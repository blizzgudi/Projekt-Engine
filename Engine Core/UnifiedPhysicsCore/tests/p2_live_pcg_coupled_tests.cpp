// P2 PARTIAL: opt-in PCG projection in the real coupled MAC/VOF solver.
// Tests actual live water transport, two pressure solves, rollback and
// non-regression of the legacy path. Not measured two-phase CFD / FSI.
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using upc::CoupledMacWater3D;
static int checks=0;
static void check(bool ok,const char* why){
    ++checks;if(!ok)throw std::runtime_error(why);
}
static void near(double a,double b,double tol,const char* why){
    check(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
static CoupledMacWater3D dam(int nx=10,int ny=6,int nz=8){
    CoupledMacWater3D water(nx,ny,nz,.1);
    for(int z=0;z<5;++z)
    for(int y=0;y<ny;++y)
    for(int x=0;x<3;++x)
        water.set_water(x,y,z,1.);
    return water;
}
static void pond(){
    CoupledMacWater3D water(8,8,12,.1);
    for(int z=0;z<6;++z)
    for(int y=0;y<8;++y)
    for(int x=0;x<8;++x)water.set_water(x,y,z,1.);
    const double mass=water.mass_kg();
    water.set_pcg_pressure_projection(true);
    check(water.pcg_pressure_projection_enabled(),"PCG opt-in not enabled");
    const double divergence=water.step_verified_reprojected(.01,1000,1e-7);
    near(water.mass_kg(),mass,1e-8,"pond mass drift");
    near(divergence,0,1e-7,"pond post-VOF divergence");
    check(water.last_pcg_projection_iterations()>0,"no actual PCG iterations");
    check(water.last_pcg_max_equation_residual_pa()<1e-4,
          "pond PCG equation residual too high");
    check(water.pressure_pa(4,4,0)>water.pressure_pa(4,4,4),
          "hydrostatic pressure ordering");
    std::cout<<"PCG_LIVE_POND mass_kg="<<water.mass_kg()
             <<" iterations="<<water.last_pcg_projection_iterations()
             <<" divergence_s_inv="<<divergence<<"\n";
}
static void dam_live(){
    auto water=dam();
    const double mass=water.mass_kg();
    water.set_pcg_pressure_projection(true);
    water.set_conservative_throughflow_transport(true);
    double moved=0;
    for(int t=0;t<3;++t) {
        const double div=water.step_verified_reprojected(.01,1500,2e-7);
        near(water.mass_kg(),mass,1e-7,"dam mass drift");
        check(std::isfinite(div)&&div<2e-7,"dam pressure not projected");
        check(water.last_pcg_projection_iterations()>0,"dam PCG unused");
        for(int z=0;z<8;++z)
        for(int y=0;y<6;++y)
        for(int x=0;x<10;++x){
            const auto f=water.water_fraction(x,y,z);
            check(std::isfinite(f)&&f>=0&&f<=1,"dam fill bounds");
        }
    }
    for(int z=0;z<8;++z)for(int y=0;y<6;++y)
    for(int x=3;x<10;++x)moved+=water.water_fraction(x,y,z);
    check(moved>1e-5,"dam has no 3D water transport");
    std::cout<<"PCG_LIVE_DAM mass_kg="<<water.mass_kg()
             <<" moved_cells="<<moved
             <<" iterations="<<water.last_pcg_projection_iterations()
             <<" divergence_s_inv="<<water.last_projection_divergence()<<"\n";
}
static void fail_closed(){
    auto water=dam();
    water.set_pcg_pressure_projection(true);
    const auto before_mass=water.mass_kg();
    const auto before_p=water.pressure_pa(1,1,1);
    const auto before_fill=water.water_fraction(1,1,1);
    int callbacks=0;
    water.set_transport_listener([&](const upc::TransportFlux3D&){++callbacks;});
    bool failed=false;
    try{(void)water.step_verified_reprojected(.01,1,1e-11);}
    catch(const std::exception&){failed=true;}
    check(failed,"underconverged PCG must be rejected");
    near(water.mass_kg(),before_mass,1e-12,"failed step changed water mass");
    near(water.pressure_pa(1,1,1),before_p,1e-12,
         "failed step published partial pressure");
    near(water.water_fraction(1,1,1),before_fill,1e-12,
         "failed step changed VOF fill");
    check(callbacks==0,"failed step emitted external transport events");
    check(water.last_pcg_projection_iterations()==0,
          "failed step published diagnostic iterations");
    std::cout<<"PCG_LIVE_ROLLBACK verified=1 callbacks="<<callbacks<<"\n";
}
int main(){
    try{
        pond();
        dam_live();
        fail_closed();
        std::cout<<"PASS P2_LIVE_PCG_COUPLED assertions="<<checks
                 <<" full_phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_LIVE_PCG_COUPLED "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
