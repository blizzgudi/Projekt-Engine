#include "upc/thermal_phase_bridge.hpp"
#include "upc/thermal_volume_estimates.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int assertions=0;
void check(bool yes,const char* msg){++assertions;if(!yes)throw std::runtime_error(msg);}
void near(double a,double b,double tolerance,const char* msg){
    check(std::isfinite(a)&&std::abs(a-b)<=tolerance,msg);
}
void rejects(std::function<void()> f,const char* msg){
    bool threw=false;try{f();}catch(const std::exception&){threw=true;}
    check(threw,msg);
}
void run(){
    UnifiedPhysicsRuntime rt(3,3,3,0.1);
    rt.core().materials().add({1,Phase::Liquid,1000,0.001});
    rt.core().materials().add({2,Phase::Solid,2500,0});
    auto& water=rt.add_water3d(3,3,3,0.1,1);
    auto& voxel=rt.core().voxels();
    water.set_water(1,1,1,1);
    voxel.set(2,1,1,{2,1,330,0},rt.core().materials());
    water.set_solid(2,1,1);
    ThermalVolume3D thermal(3,3,3,0.1);
    thermal.import_runtime_snapshot(rt,water,1,290,0.6,20,1000);
    ThermalPhaseMacBridge3D bridge(thermal,water,voxel,rt.core().materials());
    const double m0=thermal.total_mass_kg(),water0=bridge.inventory().liquid_kg;
    near(water0,1.0,1e-12,"Initial one kilogram water");
    near(bridge.represented_water_mass_kg(),1,1e-12,"Initial MAC mass");
    const double total0=thermal.total_enthalpy_j();
    // Transfer external heat to water. The heat source must be mirrored in the
    // audit total energy; insulated internal conduction adds no energy.
    const double freeze_target=1*(-2100.0*5); // 5 K below melting ice enthalpy
    const double to_freeze=freeze_target-thermal.at(1,1,1).enthalpy_j;
    thermal.add_heat_j(1,1,1,to_freeze);
    const double e_before=thermal.total_enthalpy_j();
    auto audit=bridge.step(0.001);
    near(audit.residual_j,0,1e-7,"Freeze conduction conservation");
    check(water.is_solid(1,1,1),"Fully frozen water must become MAC solid");
    near(water.water_fraction(1,1,1),0,1e-12,"Frozen water still flowing");
    check(bridge.inventory().ice_kg>0.999,"Ice mass not retained");
    const auto icevol=phase_volumes_at_pressure(bridge.inventory(),268.15);
    near(icevol.solid_ice_m3,1.0/917.0,1e-12,"Ice density volume");
    check(icevol.solid_ice_m3>0.001,"Ice expansion must be diagnosed");
    near(bridge.represented_water_mass_kg(),1,1e-8,"Freeze lost water mass");
    near(thermal.total_mass_kg(),m0,1e-8,"Freeze lost solid+fluid mass");
    near(thermal.total_enthalpy_j(),e_before,1e-6,"Freeze lost enthalpy");
    // Melt ice by a source whose heat is explicit, not via arbitrary mass birth.
    const double melted_target=334000 + 4184.0*(300-273.15);
    thermal.add_heat_j(1,1,1,melted_target-thermal.at(1,1,1).enthalpy_j);
    bridge.step(0.001);
    check(!water.is_solid(1,1,1),"Water should unblock MAC after thaw");
    check(bridge.inventory().liquid_kg>0.999,"Thaw did not restore liquid");
    near(water.water_fraction(1,1,1),1,1e-5,"Thaw liquid volume not re-created");
    // Boil to superheated vapor: vapor is mass inventory, not a gas solver.
    const double vapor_target=334000 + 4184.0*100+2256000+2000*10;
    thermal.add_heat_j(1,1,1,vapor_target-thermal.at(1,1,1).enthalpy_j);
    bridge.step(0.001);
    check(bridge.inventory().vapor_kg>0.999,"Vaporization failed");
    const auto steamvol=phase_volumes_at_pressure(bridge.inventory(),383.15);
    near(steamvol.vapor_m3,461.5*383.15/101325.0,1e-10,"Ideal water vapor gas volume");
    check(steamvol.vapor_m3>1000*0.001,"Vapor expansion diagnostic");
    near(water.water_fraction(1,1,1),0,1e-12,"Gas incorrectly retained as liquid");
    near(bridge.represented_water_mass_kg(),1,1e-8,"Boiling lost water");
    // Recondensation reintroduces liquid volume and conserves mass.
    thermal.add_heat_j(1,1,1,melted_target-thermal.at(1,1,1).enthalpy_j);
    bridge.step(0.001);
    check(bridge.inventory().liquid_kg>0.999,"Condensation failed");
    const auto fluidvol=phase_volumes_at_pressure(bridge.inventory(),300);
    near(fluidvol.liquid_m3,0.001,1e-12,"Condensed liquid volume");
    near(water.water_fraction(1,1,1),1,1e-5,"Condensation failed to restore MAC volume");
    check(voxel.at(2,1,1).temperature_k<330,"Heat from water not coupled to solid");
    near(thermal.total_mass_kg(),m0,1e-8,"Mass changed after thermal cycles");
    const double injected=thermal.supplied_heat_j();
    near(thermal.total_enthalpy_j()-total0,injected,1e-6,
         "External heat ledger missing after four transitions");
    // Detect unsupported concurrent 3D fluid transport rather than silently
    // manufacturing or deleting heat/mass on resynchronization.
    water.set_water(1,1,1,0.5);
    rejects([&]{bridge.step(0.01);},"Unsynchronized MAC advection accepted");
}
int main(){
try{
    run();
    std::cout<<"PASS: "<<assertions<<" thermal 3D water/ice/vapor<->MAC/voxel bridge assertions\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<assertions<<": "<<e.what()<<"\n";
    return 1;
}
}
