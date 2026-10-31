// P2 partial: stage-resolved MAC face-square energy, NOT continuum kinetic energy.
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
namespace {
int assertions=0;
void need(bool x,const char* why){++assertions;if(!x)throw std::runtime_error(why);}
void near(double a,double b,double tol,const char* why){
    need(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
double stages(const upc::MacFaceEnergyStages3D& e){
    return e.advection_delta_j+e.viscosity_delta_j+e.gravity_delta_j+
           e.projection_delta_j+e.vof_transport_delta_j+e.post_projection_delta_j;
}
void run(int n){
    constexpr double dx=0.1,rho=1000,dt=0.01,g=9.81;
    const int c=n/2;
    upc::CoupledMacWater3D a(n,n,n,dx,rho);
    a.set_water(c,c,c,1);
    a.set_water(c,c,c+1,1);
    const auto original=a;
    a.enable_face_energy_stages(true);
    need(a.face_energy_stages_enabled(),"enabled");
    near(a.current_face_energy_proxy_j(),0,0,"initial zero");
    const double div=a.step_verified_reprojected(dt,4000,1e-8);
    const auto e=a.last_face_energy_stages();
    const double independent_gravity=0.5*rho*dx*dx*dx*3*g*g*dt*dt;
    near(e.initial_j,0,0,"initial stage");
    near(e.advection_delta_j,0,0,"initial zero advection");
    near(e.viscosity_delta_j,0,0,"zero viscosity");
    near(e.gravity_delta_j,independent_gravity,1e-12,"three active vertical faces");
    near(e.vof_transport_delta_j,0,0,"VOF does not modify MAC velocity");
    near(e.final_j,a.current_face_energy_proxy_j(),1e-14,"final read-only");
    near(e.final_j-e.initial_j,stages(e),1e-11,"telescoping stage sum");
    near(e.closure_residual_j,0,1e-11,"closure");
    need(e.substeps==a.last_substeps()&&e.substeps==1,"single substep");
    need(div<=1e-8,"wet divergence");
    near(a.mass_kg(),2,1e-10,"mass");
    // Disabled diagnostics must not change the solver result.
    auto legacy=original;
    legacy.step_verified_reprojected(dt,4000,1e-8);
    need(!legacy.face_energy_stages_enabled(),"disabled default");
    near(legacy.current_face_energy_proxy_j(),e.final_j,1e-12,"instrumentation parity");
    near(legacy.mass_kg(),a.mass_kg(),1e-12,"mass parity");
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        near(legacy.water_fraction(x,y,z),a.water_fraction(x,y,z),0,"VOF parity");
        near(legacy.pressure_pa(x,y,z),a.pressure_pa(x,y,z),0,"pressure parity");
    }
    const auto snapshot=a.last_face_energy_stages();
    bool rejected=false;
    try{a.step_verified_reprojected(0.02,1,1e-14);}
    catch(const std::runtime_error&){rejected=true;}
    need(rejected,"underconverged candidate rejected");
    const auto rolled=a.last_face_energy_stages();
    near(rolled.gravity_delta_j,snapshot.gravity_delta_j,0,"gravity rollback");
    near(rolled.final_j,snapshot.final_j,0,"energy rollback");
    near(a.current_face_energy_proxy_j(),snapshot.final_j,0,"state rollback");
    a.enable_face_energy_stages(false);
    a.step_verified_reprojected(dt,4000,1e-8);
    const auto off=a.last_face_energy_stages();
    near(off.final_j,0,0,"disabled budget reset");
    near(off.gravity_delta_j,0,0,"disabled stage reset");
    std::cout<<"case,"<<n<<","<<independent_gravity<<","<<e.gravity_delta_j
             <<","<<e.projection_delta_j<<","<<e.post_projection_delta_j
             <<","<<e.final_j<<","<<e.closure_residual_j<<","<<div
             <<","<<e.substeps<<"\n";
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,analytic_gravity,measured_gravity,projection,post_projection,final,closure,divergence,substeps\n";
        run(8);run(12);
        std::cout<<"PASS: face stage energy assertions="<<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" after "<<assertions<<" assertions\n";
        return 1;
    }
}
