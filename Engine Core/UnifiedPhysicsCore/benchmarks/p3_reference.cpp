// P3 quantitative reference: analytic single-cell Darcy flux + gravity
// energy and CPU scaling in coupled 3D MAC/porous voxel domain.
// This is a finite-volume CPU-MVP test, NOT full Richards/Stokes-Darcy/FSI.
#include "upc/material_coupling3d.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace upc;
static void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static void add(UnifiedPhysicsRuntime& r){
    r.core().materials().add({1,Phase::Liquid,1000.0,0.001});
    r.core().materials().add({2,Phase::Solid,1800.0,0});
}
int main(){
try{
    std::cout<<std::setprecision(14);
    std::cout<<"scenario,nx,ny,nz,cell_m,conductivity_m_s,expected_flux_m3,"
             <<"observed_flux_m3,volume_initial_m3,volume_final_m3,"
             <<"energy_balance_error_j,elapsed_ms\n";
    for(const double dx:{0.1,0.05,0.025}){
        for(const double k:{0.0,0.002,0.004}){
            UnifiedPhysicsRuntime engine(3,3,4,dx);
            add(engine);
            auto& water=engine.add_water3d(3,3,4,dx,1);
            MaterialVolumeCoupler3D coupling(engine,water);
            coupling.set_soil(1,1,1,2,0.35,k);
            water.set_water(1,1,2,1);
            const double before=coupling.total_water_volume_m3();
            const auto a=std::chrono::steady_clock::now();
            const auto result=coupling.exchange(0.05);
            const double millis=std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-a).count();
            const double analytic=k*dx*dx*0.05;
            require(std::abs(result.free_to_soil_m3-analytic)<1e-12,
                    "Single-face Darcy flux mismatch");
            require(std::abs(coupling.total_water_volume_m3()-before)<1e-12,
                    "Reference lost fluid volume");
            require(std::abs(result.potential_released_j-1000*9.81*analytic*dx)<1e-9,
                    "Reference gravitational work mismatch");
            std::cout<<"darcy_analytic,3,3,4,"<<dx<<","<<k<<","<<analytic<<","
                     <<result.free_to_soil_m3<<","<<before<<","
                     <<coupling.total_water_volume_m3()<<","
                     <<result.gravitational_energy_residual_j<<","<<millis<<"\n";
        }
    }
    for(const int n:{8,12,16}){
        const double dx=0.1;
        UnifiedPhysicsRuntime engine(n,n,n,dx);
        add(engine);
        auto& water=engine.add_water3d(n,n,n,dx,1);
        MaterialVolumeCoupler3D coupling(engine,water);
        for(int y=0;y<n;++y)for(int x=0;x<n;++x){
            coupling.set_soil(x,y,0,2,0.4,0.008);
            coupling.set_soil(x,y,1,2,0.4,0.008);
            water.set_water(x,y,2,1);
            water.set_water(x,y,3,1);
        }
        const double before=coupling.total_water_volume_m3();
        const auto start=std::chrono::steady_clock::now();
        double energy_err_max=0,transferred=0;
        for(int frame=0;frame<20;++frame){
            const auto change=coupling.step(0.01,160);
            transferred+=change.free_to_soil_m3;
            energy_err_max=std::max(energy_err_max,
                                    std::abs(change.gravitational_energy_residual_j));
            require(std::abs(coupling.total_water_volume_m3()-before)<1e-9,
                    "3D CPU water/soil mass drift");
        }
        const double millis=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-start).count();
        require(transferred>1e-6,"Integrated soil failed to infiltrate");
        require(energy_err_max<1e-7,"Soil transfer energy ledger");
        std::cout<<"mac_vof_soil_3d,"<<n<<","<<n<<","<<n<<","<<dx
                 <<",0.008,0,"<<transferred<<","<<before<<","
                 <<coupling.total_water_volume_m3()<<","
                 <<energy_err_max<<","<<millis<<"\n";
    }
    return 0;
}catch(const std::exception& e){
    std::cerr<<"P3 REFERENCE FAILED: "<<e.what()<<"\n";
    return 1;
}
}
