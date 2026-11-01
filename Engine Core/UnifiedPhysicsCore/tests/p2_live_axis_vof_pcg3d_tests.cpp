// P2 PARTIAL: opt-in axis-aligned VOF geometry in the LIVE 3D MAC/PCG
// coupled water step, including rollback and stale-frame rejection.
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool ok,const char* why) {
    ++checks;
    if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
template<class F> void reject(F fn) {
    bool rejected=false;
    try{fn();}catch(const std::exception&){rejected=true;}
    check(rejected,"Expected fail-closed live geometry rejection");
}
upc::CoupledMacWater3D slab(int n,bool partial=true) {
    upc::CoupledMacWater3D water(n,n,n,1.0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        water.set_water(x,y,z,z==0?1.0:(z==1?(partial?.75:1.0):0.0));
    water.set_pcg_pressure_projection(true);
    return water;
}
void live_axis_case(int n) {
    auto axis=slab(n);
    auto legacy=axis;
    reject([&]{axis.set_axis_vof_pcg_geometry(true);axis.step(.001);});
    check(axis.last_axis_vof_resolved_faces()==0,"Unverified must not mutate geometry");
    const auto mass=axis.mass_kg();
    reject([&]{axis.set_pcg_pressure_projection(false);});
    check(axis.pcg_pressure_projection_enabled(),"PCG cannot be disabled while axis mode active");
    const auto d=axis.step_verified(.001,1500,1e-8);
    const auto b=legacy.step_verified(.001,1500,1e-8);
    check(d<=1e-8&&b<=1e-8,"PCG wet divergence");
    check(axis.last_axis_vof_resolved_faces()==std::size_t(n*n),"Every plane cut resolved");
    check(axis.last_axis_vof_unresolved_pure_faces()==0,"No pure-face fallback");
    near(axis.mass_kg(),mass,1e-8,"Axis geometry water mass");
    near(legacy.mass_kg(),mass,1e-8,"Baseline water mass");
    // The optional geometric pressure condition must be active in the
    // coupled live solve, not just a standalone geometry construction.
    const auto a=axis.pressure_pa(n/2,n/2,1);
    const auto c=legacy.pressure_pa(n/2,n/2,1);
    check(std::isfinite(a)&&std::isfinite(c),"Finite live pressure");
    // Independent hydrostatic pressure at the top wet cell: rho*g*theta*dx.
    near(a,1000.0*9.81*.25,1e-5,"Fractional hydrostatic pressure");
    near(c,1000.0*9.81*.5,1e-5,"Legacy half-cell hydrostatic pressure");
    check(std::abs(a-c)>1e-8,"Fractional PCG must alter live pressure");
    std::cout<<"LIVE_AXIS n="<<n<<" mass_kg="<<mass
             <<" pressure_fractional_pa="<<a<<" pressure_legacy_pa="<<c
             <<" faces="<<axis.last_axis_vof_resolved_faces()
             <<" divergence="<<d<<"\n";
    // Reusing the same object must not carry the previous theta into
    // a pure VOF frame. Reset the field explicitly before the new step.
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        axis.set_water(x,y,z,z<2?1.0:0.0);
    const auto mass2=axis.mass_kg();
    const auto d2=axis.step_verified(.00001,1500,1e-8);
    check(d2<=1e-8,"Second-frame PCG divergence");
    check(axis.last_axis_vof_resolved_faces()==0,"Old fractional cuts must clear");
    check(axis.last_axis_vof_unresolved_pure_faces()==std::size_t(n*n),
          "Pure interface must be explicitly unresolved");
    near(axis.mass_kg(),mass2,1e-8,"Pure-frame water mass");
}
void rollback_cases() {
    auto water=slab(5);
    reject([&]{auto w=slab(5);w.set_pcg_pressure_projection(false);
               w.set_axis_vof_pcg_geometry(true);});
    water.set_axis_vof_pcg_geometry(true);
    water.set_water(2,2,1,.9); // y gradient -> non-axis normal
    const auto mass=water.mass_kg();
    const auto pressure=water.pressure_pa(2,2,1);
    reject([&]{water.step_verified(.001,1500,1e-8);});
    near(water.mass_kg(),mass,1e-12,"Oblique failure rolls back mass");
    near(water.pressure_pa(2,2,1),pressure,1e-12,"Oblique failure rolls back pressure");
    check(water.last_axis_vof_resolved_faces()==0,"Oblique failure rolls back diagnostics");
    water.set_water(2,2,1,.75);
    water.set_water(2,2,2,.25); // two partial cells around the wet boundary
    const auto mass2=water.mass_kg();
    reject([&]{water.step_verified(.001,1500,1e-8);});
    near(water.mass_kg(),mass2,1e-12,"Two-partial failure rolls back mass");
    check(water.last_axis_vof_resolved_faces()==0,"Two-partial failure rolls back diagnostics");
    water.set_axis_vof_pcg_geometry(false);
    check(!water.axis_vof_pcg_geometry_enabled(),"Disable axis mode");
}
}
int main() {
    try {
        live_axis_case(5);
        live_axis_case(9);
        rollback_cases();
        std::cout<<"PASS P2_LIVE_AXIS_VOF_PCG assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL P2_LIVE_AXIS_VOF_PCG "<<e.what()<<"\n";
        return 1;
    }
}
