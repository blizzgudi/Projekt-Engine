#include "upc/reactive_thermal_bridge.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int checks=0;
static void require(bool b,const char* msg){
    ++checks;if(!b)throw std::runtime_error(msg);
}
static void near(double a,double b,double eps,const char* msg){
    require(std::isfinite(a)&&std::abs(a-b)<=eps,msg);
}
int main(){
try{
    MaterialLibrary lib;
    lib.add({1,Phase::Liquid,1000,0.001});
    lib.add({2,Phase::Liquid,1000,0.001});
    lib.add({3,Phase::Solid,1000,0});
    lib.add({4,Phase::Solid,2600,0});
    ReactiveMixtureVolume3D chemical(lib,3,3,3,0.1);
    chemical.register_element(100,0.010);
    chemical.register_element(200,0.020);
    chemical.register_species({1,{{100,1}},0.010,1000,0,0});
    chemical.register_species({2,{{200,1}},0.020,1000,0,0});
    chemical.register_species({3,{{100,1},{200,1}},0.030,1000,0,-12000});
    chemical.register_reaction({{{1,-1},{2,-1},{3,1}},20,0});
    chemical.add_moles(1,1,1,1,1,300);
    chemical.add_moles(1,1,1,2,1,300);
    ThermalVolume3D thermal(3,3,3,0.1);
    thermal.set_solid(2,1,1,4,lib,1.2,298,1.8,850);
    ReactiveThermalContact3D adapter(chemical,thermal);
    const double total_before=chemical.total_energy_j()+thermal.total_enthalpy_j();
    const auto reacted=chemical.react(0.15);
    require(reacted.extent_mol>0.94,"Thermal chemistry reaction incomplete");
    near(chemical.total_energy_j()+thermal.total_enthalpy_j(),total_before,1e-9,
         "Exothermic reaction changed total chemical+thermal energy");
    require(chemical.temperature_k(1,1,1)>thermal.temperature_k(2,1,1),
            "Exothermic reaction did not heat chemistry parcel");
    double moved=0;
    for(int i=0;i<100;++i){
        const double heat=adapter.transfer_face_heat(1,1,1,2,1,1,0.8,0.05);
        require(heat>=0,"Hot chemical source sent negative heat");
        moved+=heat;
        near(chemical.total_energy_j()+thermal.total_enthalpy_j(),
             total_before,1e-7,"P4 solid/P6 reacted mixture total energy");
        require(chemical.temperature_k(1,1,1)>=thermal.temperature_k(2,1,1)-1e-7,
                "Thermal coupling overshot equilibrium");
    }
    require(moved>0,"No exothermic heat entered shared P4");
    near(chemical.total_mass_kg(),0.03,1e-12,"P4/P6 coupling lost matter");
    require(thermal.temperature_k(2,1,1)>298,"P4 solid remains cold");
    bool rejected=false;
    try{adapter.transfer_face_heat(1,1,1,1,1,1,0.8,0.05);}
    catch(const std::exception&){rejected=true;}
    require(rejected,"Non-adjacent chemistry heat contact accepted");
    std::cout<<"PASS: P6 balanced reaction chemical energy couples to separate P4"
                " neighboring solid heat, no mass or energy creation; checks="
             <<checks<<"\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;
}
}
