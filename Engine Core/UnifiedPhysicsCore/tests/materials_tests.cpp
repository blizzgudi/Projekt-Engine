#include "upc/core.hpp"
#include "upc/materials_ext.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace upc;
int n=0;
void check(bool yes,const char* why){++n;if(!yes)throw std::runtime_error(why);}
void near(double a,double b,double tol,const char* msg){
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
void rejects(const std::function<void()>& f,const char* why){
    bool threw=false;try{f();}catch(const std::exception&){threw=true;}
    check(threw,why);
}
int main(){
try{
    MaterialLibrary lib;
    lib.add({1,Phase::Liquid,1000,0.001}); // water
    lib.add({2,Phase::Solid,2500,0});     // gravel
    lib.add({3,Phase::Solid,1800,0});     // clay
    lib.add({4,Phase::Solid,2100,0});     // fired clay
    // P4: Latent heat; no mass lost by changing phase.
    WaterThermalState water(2,273.15,0);
    auto st=water.state();
    near(st.ice,1,1e-12,"Initial ice state");
    near(st.liquid,0,1e-12,"Initial ice liquid fraction");
    const double e0=water.total_enthalpy_j();
    water.add_heat_j(2*334000); // melting latent heat
    st=water.state();
    near(st.temperature_k,273.15,1e-12,"Melting plateau temperature");
    near(st.liquid,1,1e-12,"Ice-to-water transition");
    water.add_heat_j(2*4184*100); // 100C
    near(water.state().temperature_k,373.15,1e-9,"Heating to boiling");
    water.add_heat_j(2*2256000);
    st=water.state();
    near(st.vapour,1,1e-12,"Evaporation");
    near(st.temperature_k,373.15,1e-9,"Boiling plateau");
    near(water.mass_kg(),2,1e-12,"Phase mass conservation");
    water.add_heat_j(-2*(2256000+4184*100+334000));
    near(water.total_enthalpy_j(),e0,1e-7,"Thermal energy roundtrip");
    near(water.state().ice,1,1e-12,"Freeze after removing heat");
    rejects([&]{water.add_heat_j(-1e12);},"Below absolute zero accepted");
    // P6: mixture as composition, not an invented chemical law.
    MaterialMixture clay,gravel;
    clay.add(3,7,lib); // 7kg clay
    gravel.add(2,3,lib);
    clay.combine(gravel);
    near(clay.total_mass_kg(),10,1e-12,"Mix mass");
    near(clay.mass_kg(3),7,1e-12,"Clay mass");
    near(clay.mass_kg(2),3,1e-12,"Gravel mass");
    near(clay.ideal_volume_m3(lib),7./1800+3./2500,1e-12,"Ideal volumes");
    const double m0=clay.total_mass_kg();
    clay.transform(3,4,2,lib);
    near(clay.total_mass_kg(),m0,1e-12,"Reaction conservation");
    near(clay.mass_kg(3),5,1e-12,"Reactant mass");
    near(clay.mass_kg(4),2,1e-12,"Product mass");
    rejects([&]{clay.transform(3,4,6,lib);},"Overconsumption accepted");
    // P3: water-to-soil infiltration is a conservative local exchange.
    ShallowWater2D w(2,2,0.5,1);
    w.set_depth(0,0,0.8);
    PorousSoilCell soil{0.06,0.01,0.015};
    const double total0=w.volume_m3()+soil.stored_water_m3;
    double absorbed=infiltrate_water(w,0,0,soil,0.5,2.0);
    near(absorbed,0.03,1e-12,"Infiltration rate");
    near(w.volume_m3()+soil.stored_water_m3,total0,1e-12,"Infiltration mass");
    absorbed=infiltrate_water(w,0,0,soil,0.5,20.0);
    near(absorbed,0.02,1e-12,"Pore capacity limit");
    near(soil.stored_water_m3,0.06,1e-12,"Pore saturation");
    near(w.volume_m3()+soil.stored_water_m3,total0,1e-12,"Saturated total mass");
    rejects([&]{infiltrate_water(w,0,0,soil,0.5,-1.0);},"Negative duration accepted");
    // P7: sparse chunks, including negative coordinate edge cases.
    SparseVoxelChunks sparse;
    check(sparse.allocated_chunks()==0,"Sparse starts nonempty");
    sparse.set(0,0,0,1);
    sparse.set(15,15,15,2);
    check(sparse.allocated_chunks()==1,"Same chunk not shared");
    sparse.set(16,0,0,2);
    sparse.set(-1,-1,-1,3);
    sparse.set(-16,-16,-16,4);
    check(sparse.allocated_chunks()==3,"Negative chunk address");
    check(sparse.get(0,0,0)==1&&sparse.get(15,15,15)==2,"Chunk values");
    check(sparse.get(-1,-1,-1)==3&&sparse.get(-16,-16,-16)==4,"Negative read");
    check(sparse.get(-17,0,0)==0,"Empty chunk should read zero");
    check(sparse.active_voxels()==5,"Sparse active count");
    sparse.set(16,0,0,0);
    check(sparse.allocated_chunks()==2,"Empty chunk must release memory");
    sparse.set(-1,-1,-1,0);
    sparse.set(-16,-16,-16,0);
    check(sparse.allocated_chunks()==1,"Chunk deletion");
    std::cout<<"PASS: "<<n<<" extended assertions (porous soil, thermal phase change, material blending, sparse chunks)\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<n<<" checks: "<<e.what()<<"\n";return 1;
}
}
