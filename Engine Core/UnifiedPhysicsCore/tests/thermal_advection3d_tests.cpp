#include "upc/thermal_advection3d.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
static void need(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
static bool close(double a,double b,double tol){return std::isfinite(a)&&std::abs(a-b)<tol;}
int main(){
try{
    UnifiedPhysicsRuntime rt(12,6,8,0.1);
    rt.core().materials().add({1,Phase::Liquid,1000,0.001});
    rt.core().materials().add({2,Phase::Solid,2300,0});
    auto& water=rt.add_water3d(12,6,8,0.1,1);
    for(int z=0;z<4;++z)
      for(int y=0;y<6;++y)
        for(int x=0;x<4;++x)water.set_water(x,y,z,1);
    auto& grid=rt.core().voxels();
    grid.set(10,3,3,{2,1,320,0},rt.core().materials());
    water.set_solid(10,3,3);
    ThermalVolume3D thermal(12,6,8,0.1);
    thermal.import_runtime_snapshot(rt,water,1,290,0.6,8,1000);
    ThermalPhaseMacBridge3D phase(thermal,water,grid,rt.core().materials());
    thermal.add_heat_j(3,3,1,20000);
    const double m0=thermal.total_mass_kg();
    const double e0=thermal.total_enthalpy_j();
    const double w0=phase.represented_water_mass_kg();
    need(close(w0,water.mass_kg(),1e-8),"Initial liquid mass mismatch");
    {
        ThermalMacAdvection3D advection(water,thermal,phase);
        for(int i=0;i<10;++i){
            advection.advect(0.015,180);
            need(close(advection.energy_residual_j(),0,1e-5),
                 "Liquid enthalpy transport lost energy");
            need(close(advection.mass_residual_kg(),0,1e-8),
                 "Liquid transport lost mass");
        }
    }
    need(close(thermal.total_enthalpy_j(),e0,1e-5),"Total enthalpy not conserved");
    need(close(thermal.total_mass_kg(),m0,1e-8),"Total thermal mass not conserved");
    need(close(phase.represented_water_mass_kg(),water.mass_kg(),1e-7),
         "Thermal inventory and MAC mass do not agree");
    need(close(phase.represented_water_mass_kg(),w0,1e-7),
         "Water escaped closed MAC tank");
    double outside_liquid=0;
    for(int z=0;z<8;++z)for(int y=0;y<6;++y)for(int x=4;x<12;++x)
        outside_liquid+=water.water_fraction(x,y,z);
    need(outside_liquid>0.5,"Dam break no water transport beyond initial region");
    // A subsequent thermal solver step must accept the moved water and update
    // the shared solid's temperature without changing water mass/total energy.
    const auto before_conduction=thermal.total_enthalpy_j();
    phase.step(0.02);
    need(close(thermal.total_enthalpy_j(),before_conduction,1e-5),
         "Live-advection thermal conduction energy drift");
    need(close(phase.represented_water_mass_kg(),w0,1e-7),
         "Live thermal phase bridge lost water");
    // Verify guard against silent mass insertion from an external fluid edit.
    water.set_water(0,0,0,0.02);
    bool failed=false;
    try{phase.step(0.005);}catch(const std::exception&){failed=true;}
    need(failed,"Out-of-band fluid change must reject thermal step");
    std::cout<<"PASS: live 3D MAC transport carries water mass+enthalpy, Fourier phase bridge remains conservative\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
}
}
