// P4 numerical verification against the discrete Neumann heat-equation eigenmode.
// Fixed material properties and insulated boundaries; 3D grid with 1D cosine
// initial profile, transverse invariance, no advection/pressure/phase-volume feedback.
#include "upc/thermal3d.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace upc;
static constexpr double pi=3.1415926535897932384626433832795;
int main(){
try{
    MaterialLibrary lib;
    lib.add({2,Phase::Solid,2500,0});
    std::cout<<std::setprecision(14);
    std::cout<<"scenario,nx,ny,nz,dx_m,sim_seconds,expected_amplitude_k,"
                "observed_amplitude_k,max_profile_error_k,"
                "relative_energy_residual,elapsed_ms\n";
    for(const int n:{8,12,16,24}){
        constexpr double dx=0.1,conductivity=100,mass=0.1,cp=1000;
        ThermalVolume3D grid(n,n,n,dx);
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
            const double mode=std::cos(pi*(x+0.5)/n);
            grid.set_solid(x,y,z,2,lib,mass,300+50*mode,conductivity,cp);
        }
        const double energy0=grid.total_enthalpy_j();
        const auto started=std::chrono::steady_clock::now();
        constexpr int steps=200;
        constexpr double dt=0.05;
        for(int s=0;s<steps;++s)grid.step(dt);
        const double ms=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-started).count();
        const double lambda=4*(conductivity*dx/(mass*cp))*
             std::pow(std::sin(pi/(2*n)),2);
        const double expected=50*std::exp(-lambda*steps*dt);
        double maximum=0,estimated=0,mode_norm=0;
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
            const double mode=std::cos(pi*(x+0.5)/n);
            const double t=grid.temperature_k(x,y,z);
            maximum=std::max(maximum,std::abs(t-(300+expected*mode)));
            estimated+=(t-300)*mode;
            mode_norm+=mode*mode;
        }
        estimated/=mode_norm;
        const double relative=std::abs(grid.total_enthalpy_j()-energy0)/
                              std::max(1.0,std::abs(energy0));
        if(maximum>0.05||relative>1e-10)
            throw std::runtime_error("3D Neumann heat analytical benchmark failed");
        std::cout<<"neumann_fourier_3d,"<<n<<","<<n<<","<<n<<","<<dx<<","
                 <<steps*dt<<","<<expected<<","<<estimated<<","
                 <<maximum<<","<<relative<<","<<ms<<"\n";
    }
    return 0;
}catch(const std::exception& e){
    std::cerr<<"P4 THERMAL REFERENCE FAILED: "<<e.what()<<"\n";return 1;
}
}
