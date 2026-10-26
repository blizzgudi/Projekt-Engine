#include "upc/viscous_mac3d.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace upc;
static std::size_t checks=0;
static void req(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
static void near(double a,double b,double tol,const char* why){
    req(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
constexpr double pi=3.141592653589793238462643383279502884;
int main(){
try{
    constexpr std::size_t nx=12,ny=10,nz=8;
    constexpr double dx=0.1,nu=0.012,dt=0.03;
    MacProjection3D mac(nx,ny,nz,dx);
    ViscousMacDiffusion3D viscosity(nx,ny,nz,dx);
    // Independent discrete Laplacian eigenmode for no-slip boundaries.
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<=nx;++x)
        mac.set_face_velocity_x(x,y,z,
            std::sin(pi*x/nx)*std::sin(pi*(y+1)/(ny+1))*
            std::sin(pi*(z+1)/(nz+1)));
    const double before=viscosity.face_energy_proxy(mac);
    const double alpha=nu*dt/(dx*dx);
    const double lambda=2*(std::cos(pi/nx)-1)+
        2*(std::cos(pi/(ny+1))-1)+2*(std::cos(pi/(nz+1))-1);
    const double exact_factor=1+alpha*lambda;
    req(viscosity.step(mac,dt,nu)==1,"single stable viscosity step");
    double maxerr=0;
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<=nx;++x){
        const double expected=exact_factor*std::sin(pi*x/nx)*
             std::sin(pi*(y+1)/(ny+1))*std::sin(pi*(z+1)/(nz+1));
        const double actual=mac.face_velocity_x(x,y,z);
        maxerr=std::max(maxerr,std::abs(actual-expected));
        near(actual,expected,1e-12,"3D eigenmode analytic discrete diffusion");
    }
    const double after=viscosity.face_energy_proxy(mac);
    req(after>0&&after<before,"viscous energy proxy must decrease");
    double previous=after;
    for(int i=0;i<60;++i){
        req(viscosity.step(mac,dt,nu)==1,"unexpected viscosity substeps");
        const double energy=viscosity.face_energy_proxy(mac);
        req(energy>=0&&energy<=previous+1e-14,"non-monotonic diffusion energy");
        previous=energy;
    }
    // Axis symmetry on cubic domain: equal x/y/z impulses diffuse equally.
    MacProjection3D symmetry(12,12,12,dx);
    ViscousMacDiffusion3D symop(12,12,12,dx);
    symmetry.set_face_velocity_x(6,5,5,1);
    symmetry.set_face_velocity_y(5,6,5,1);
    symmetry.set_face_velocity_z(5,5,6,1);
    for(int i=0;i<25;++i)symop.step(symmetry,dt,nu);
    const auto a=symmetry.face_velocity_x(6,5,5);
    const auto b=symmetry.face_velocity_y(5,6,5);
    const auto c=symmetry.face_velocity_z(5,5,6);
    near(a,b,1e-13,"3D viscosity axis x/y symmetry");
    near(b,c,1e-13,"3D viscosity axis y/z symmetry");
    req(a>0&&a<1,"3D impulse failed to spread");
    // Large timestep is split automatically, never silently unstable.
    MacProjection3D big(12,12,12,dx);
    big.set_face_velocity_x(6,5,5,1);
    const auto steps=symop.step(big,0.2,1);
    req(steps>100&&steps<200,"explicit diffusion CFL substeps");
    req(symop.face_energy_proxy(big)<0.5*dx*dx*dx,"large dt energy growth");
    // No viscosity means no changes to pre-existing MAC face data.
    MacProjection3D zero(12,12,12,dx);
    zero.set_face_velocity_x(6,5,5,1.23456789);
    req(symop.step(zero,0.1,0)==0,"zero-viscosity unexpectedly substepped");
    near(zero.face_velocity_x(6,5,5),1.23456789,0,"inviscid mode changed");
    for(double bad:{-1.0,-0.01}){
        bool threw=false;
        try{symop.step(zero,0.01,bad);}catch(const std::invalid_argument&){threw=true;}
        req(threw,"negative viscosity accepted");
    }
    bool threw=false;
    try{symop.step(zero,20,1);}catch(const std::invalid_argument&){threw=true;}
    req(threw,"unsafe >10000 explicit substeps accepted");
    threw=false;
    try{symop.step(zero,0,nu);}catch(const std::invalid_argument&){threw=true;}
    req(threw,"zero timestep accepted");
    threw=false;
    try{ViscousMacDiffusion3D invalid(1,3,3,dx);(void)invalid;}
    catch(const std::invalid_argument&){threw=true;}
    req(threw,"invalid viscosity domain accepted");
    std::cout.precision(14);
    std::cout<<"PASS: P2 optional 3D MAC viscosity; checks="<<checks
       <<" analytic_eigenmode_max_error="<<maxerr
       <<" energy_before="<<before<<" energy_after="<<after
       <<" energy_61_steps="<<previous
       <<" adaptive_substeps="<<steps
       <<" isotropic_face_value="<<a<<"\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL: P2 viscosity check "<<checks<<": "<<e.what()<<"\n";
    return 1;
}
}
