#include "upc/thermal_advection3d.hpp"
#include "upc/thermal_soil_bridge.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
static void need(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
int main(){
try{
    UnifiedPhysicsRuntime rt(8,8,8,0.1);
    auto& lib=rt.core().materials();
    lib.add({1,Phase::Liquid,1000,0.001});
    lib.add({2,Phase::Solid,1700,0});
    auto& water=rt.add_water3d(8,8,8,0.1,1);
    MaterialVolumeCoupler3D soil(rt,water);
    soil.register_porous_material(2,0.4,0.02);
    for(std::size_t y=0;y<8;++y)
     for(std::size_t x=0;x<8;++x){
         soil.set_soil_from_material(x,y,0,2);
         soil.set_soil_from_material(x,y,1,2);
         water.set_water(x,y,2,1);
         water.set_water(x,y,3,1);
     }
    ThermalVolume3D thermal(8,8,8,0.1);
    thermal.import_runtime_snapshot(rt,water,1,300,0.6,1.8,1000);
    ThermalPhaseMacBridge3D phase(thermal,water,rt.core().voxels(),lib);
    ThermalPoreExchange3D pore(soil,thermal,water);
    const double initial_water=soil.total_water_mass_kg();
    const double initial_heat=thermal.total_enthalpy_j();
    need(std::abs(pore.total_pore_mass_kg())<1e-12,"Expected empty pores");
    double infused=0;
    {
        ThermalMacAdvection3D adv(water,thermal,phase);
        for(int step=0;step<15;++step){
            adv.advect(0.01,120);
            const auto before=pore.total_thermal_gravity_j();
            const auto r=pore.exchange(0.01);
            const auto after=pore.total_thermal_gravity_j();
            infused+=r.free_to_soil_m3;
            need(std::abs(after-before)<1e-6,
                 "Gravity-to-heat ledger lost energy during soil transfer");
            phase.synchronize_after_mac_advection();
            const auto heat_before=pore.total_thermal_enthalpy_j();
            phase.step(0.01);
            need(std::abs(pore.total_thermal_enthalpy_j()-heat_before)<1e-4,
                 "Thermal diffusion changed total water/solid/pore heat");
            need(std::abs(soil.total_water_mass_kg()-initial_water)<1e-7,
                 "Integrated P2 P3 P4 water mass drift");
            need(std::abs(thermal.total_water_mass_kg()+
                          pore.total_pore_mass_kg()-initial_water)<1e-7,
                 "Free-water+soil enthalpy water inventory mismatch");
            need(std::abs(phase.represented_water_mass_kg()+
                          pore.total_pore_mass_kg()-initial_water)<1e-7,
                 "Pore integration changed phase-resolved mass");
        }
    }
    need(infused>0,"No mac->porous thermal transfer");
    need(pore.total_pore_mass_kg()>0,"Pore heat ledger lacks water");
    need(pore.total_thermal_enthalpy_j()>initial_heat,
         "Gravitational loss not released into material heat");
    std::cout<<"PASS: P2 MAC + P3 Darcy + P4 enthalpy/conduction integrated, transfer="
             <<infused<<" m3, total water="<<initial_water<<" kg\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
}
}
