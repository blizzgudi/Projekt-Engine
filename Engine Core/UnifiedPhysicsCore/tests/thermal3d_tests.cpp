#include "upc/thermal3d.hpp"
#include <cmath>
#include <array>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int checks=0;
void require(bool yes,const char* reason){++checks;if(!yes)throw std::runtime_error(reason);}
void near(double a,double b,double tolerance,const char* reason){
    require(std::isfinite(a)&&std::abs(a-b)<=tolerance,reason);
}
void rejects(std::function<void()> action,const char* reason){
    bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}
    require(rejected,reason);
}
MaterialLibrary make_library(){
    MaterialLibrary lib;
    lib.add({1,Phase::Liquid,1000,0.001});
    lib.add({2,Phase::Solid,2500,0});
    return lib;
}
void analytical_solid_pair(){
    auto lib=make_library();
    ThermalVolume3D grid(2,1,1,0.1);
    grid.set_solid(0,0,0,2,lib,0.1,400,100,1000);
    grid.set_solid(1,0,0,2,lib,0.1,300,100,1000);
    const double e0=grid.total_enthalpy_j(),m0=grid.total_mass_kg();
    for(int step=0;step<200;++step){
        const auto result=grid.step(0.01);
        near(result.residual_j,0,1e-8,"Two-cell energy conservation");
        near(grid.total_mass_kg(),m0,1e-12,"Two-cell mass conservation");
    }
    // Continuous 2-cell Fourier exchange: d(T1-T2)/dt=-2*G/(m*cp)*(T1-T2).
    const double conductance=100*0.1;
    const double diff=100*std::exp(-2*conductance/(0.1*1000)*2);
    near(grid.temperature_k(0,0,0)-grid.temperature_k(1,0,0),
         diff,0.05,"Analytic two-cell Fourier decay");
    near(grid.total_enthalpy_j(),e0,1e-8,"Conduction creates energy");
    near((grid.temperature_k(0,0,0)+grid.temperature_k(1,0,0))/2,
         350,1e-10,"Symmetric thermal average");
    rejects([&]{grid.step(-1);},"Invalid thermal step accepted");
    rejects([&]{grid.add_heat_j(0,0,0,-1e12);},"Below zero kelvin accepted");
}
void water_phase_and_heat(){
    auto lib=make_library();
    ThermalVolume3D grid(2,1,1,0.1);
    grid.set_water(0,0,0,1,lib,1,273.15,0.6,0);
    grid.set_solid(1,0,0,2,lib,1,273.15,2.0,1000);
    auto phases=grid.water_phase(0,0,0);
    near(phases.ice,1,1e-12,"Ice initial phase");
    const double start=grid.total_enthalpy_j();
    grid.add_heat_j(0,0,0,334000*0.4);
    phases=grid.water_phase(0,0,0);
    near(phases.liquid,0.4,1e-12,"Latent fusion plateau");
    near(grid.temperature_k(0,0,0),273.15,1e-12,"Fusion isothermal plateau");
    grid.add_heat_j(0,0,0,334000*0.6);
    near(grid.water_phase(0,0,0).liquid,1,1e-12,"Fully melted water");
    grid.add_heat_j(0,0,0,4184*100);
    near(grid.temperature_k(0,0,0),373.15,1e-9,"Boiling threshold");
    grid.add_heat_j(0,0,0,2256000*0.6);
    phases=grid.water_phase(0,0,0);
    near(phases.vapour,0.6,1e-12,"Latent vaporisation plateau");
    near(grid.temperature_k(0,0,0),373.15,1e-12,"Boiling isothermal plateau");
    grid.add_heat_j(0,0,0,2256000*0.4);
    near(grid.water_phase(0,0,0).vapour,1,1e-12,"Fully vaporised");
    grid.add_heat_j(0,0,0,2000*20);
    near(grid.temperature_k(0,0,0),393.15,1e-9,"Vapour sensible heat");
    const double source=grid.supplied_heat_j();
    near(grid.total_enthalpy_j()-start,source,1e-8,"External heat source ledger");
    grid.add_heat_j(0,0,0,-source);
    near(grid.water_phase(0,0,0).ice,1,1e-10,"Condense and refreeze");
    near(grid.total_enthalpy_j(),start,1e-8,"Water thermal roundtrip");
    rejects([&]{grid.water_phase(1,0,0);},"Solid misidentified as water");
    rejects([&]{grid.set_water(1,0,0,1,lib,1,300,1);},"Double occupancy");
    rejects([&]{grid.set_water(1,0,0,2,lib,1,300,1);},"Solid ID used as water");
}
void three_dimensional_symmetry(){
    auto lib=make_library();
    ThermalVolume3D grid(5,5,5,0.1);
    for(int z=0;z<5;++z)for(int y=0;y<5;++y)for(int x=0;x<5;++x)
        grid.set_solid(x,y,z,2,lib,0.1,
                       (x==2&&y==2&&z==2)?350:300,20,1000);
    const double energy=grid.total_enthalpy_j();
    const double mass=grid.total_mass_kg();
    for(int i=0;i<100;++i){
        const auto report=grid.step(0.01);
        near(report.residual_j,0,1e-7,"3D insulated heat exchange energy drift");
    }
    near(grid.total_enthalpy_j(),energy,1e-7,"3D global energy");
    near(grid.total_mass_kg(),mass,1e-10,"3D global mass");
    const double east=grid.temperature_k(3,2,2);
    for(auto p:{std::array<int,3>{1,2,2},{2,1,2},{2,3,2},
                {2,2,1},{2,2,3}})
        near(grid.temperature_k(p[0],p[1],p[2]),east,1e-9,
             "3D heat is anisotropic");
    require(east>300,"No 3D thermal diffusion");
    require(grid.temperature_k(2,2,2)<350,"Center did not cool");
}
void interface_freezing(){
    auto lib=make_library();
    ThermalVolume3D grid(2,1,1,0.1);
    grid.set_water(0,0,0,1,lib,0.1,273.15,0.6,0.0);
    grid.set_solid(1,0,0,2,lib,0.1,330,30,1000);
    const double e0=grid.total_enthalpy_j();
    for(int i=0;i<300;++i){
        const auto r=grid.step(0.05);
        near(r.residual_j,0,1e-8,"Solid-water interface energy conservation");
    }
    require(grid.water_phase(0,0,0).liquid>0,
            "Warm solid did not melt adjacent ice");
    near(grid.temperature_k(0,0,0),273.15,1e-9,
         "Ice melting plateau did not hold during heat conduction");
    require(grid.temperature_k(1,0,0)<330,
            "Solid did not lose heat to ice");
    near(grid.total_enthalpy_j(),e0,1e-7,
         "Ice melting energy missing");
}
void runtime_snapshot(){
    UnifiedPhysicsRuntime rt(3,3,3,0.1);
    rt.core().materials().add({1,Phase::Liquid,1000,0.001});
    rt.core().materials().add({2,Phase::Solid,2500,0});
    auto& fluid=rt.add_water3d(3,3,3,0.1,1);
    rt.core().voxels().set(1,1,1,{2,1,320,0},rt.core().materials());
    fluid.set_solid(1,1,1,true);
    fluid.set_water(2,1,1,0.75);
    ThermalVolume3D grid(3,3,3,0.1);
    grid.import_runtime_snapshot(rt,fluid,1,290,0.6,15,1000);
    near(grid.total_mass_kg(),2.5+0.75,1e-12,
         "MAC volume-to-mass snapshot wrong");
    near(grid.temperature_k(1,1,1),320,1e-12,"Voxel temperature lost");
    near(grid.temperature_k(2,1,1),290,1e-12,"MAC water snapshot temperature");
    const double energy=grid.total_enthalpy_j();
    for(int i=0;i<40;++i)grid.step(0.05);
    near(grid.total_enthalpy_j(),energy,1e-7,"Snapshot heat energy");
    grid.write_solid_temperatures(rt.core().voxels(),rt.core().materials());
    require(rt.core().voxels().at(1,1,1).temperature_k<320,
            "Heated solid did not conduct to MAC snapshot water");
    near(fluid.water_fraction(2,1,1),0.75,1e-12,
         "Thermal snapshot improperly modified fluid mass");
    rejects([&]{grid.import_runtime_snapshot(rt,fluid,1,290,0.6,15,1000);},
            "Duplicate thermal snapshot accepted");
    ThermalVolume3D wrong(2,3,3,0.1);
    rejects([&]{wrong.import_runtime_snapshot(rt,fluid,1,290,0.6,15,1000);},
            "Misaligned thermal/flow grid accepted");
}
int main(){
try{
    analytical_solid_pair();
    water_phase_and_heat();
    three_dimensional_symmetry();
    interface_freezing();
    runtime_snapshot();
    std::cout<<"PASS: "<<checks<<" 3D heat conduction/latent-phase/snapshot assertions\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;
}
}
