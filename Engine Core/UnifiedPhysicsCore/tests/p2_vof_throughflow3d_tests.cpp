// P2 PARTIAL: analytically check simultaneous transport through full cells,
// and verify opt-in coupling leaves legacy mode untouched.
#include "upc/p2_vof_throughflow3d.hpp"
#include "upc/coupled_water3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int count=0;
void check(bool condition,const char* message){
    ++count;if(!condition)throw std::runtime_error(message);
}
void near(double actual,double expected,double tolerance,const char* message) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tolerance,message);
}
constexpr std::size_t nx=16,ny=8,nz=8;
const auto idx=[](std::size_t x,std::size_t y,std::size_t z){
    return (z*ny+y)*nx+x;
};
std::array<std::vector<double>,3> speed(double u) {
    std::array<std::vector<double>,3> v;
    for(auto& a:v)a.assign(nx*ny*nz,0);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x+1<nx;++x)v[0][idx(x,y,z)]=u;
    return v;
}
template<class F> void invalid(F fn) {
    bool caught=false;
    try {fn();}catch(const std::invalid_argument&){caught=true;}
    check(caught,"invalid input must reject");
}
void translate() {
    std::vector<double> fill(nx*ny*nz,0);
    std::vector<std::uint8_t> solid(fill.size(),0);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=4;x<10;++x)
        fill[idx(x,y,z)]=1.0;
    const auto original=fill;
    const auto v=speed(.2);
    const auto r=upc::p2_vof_throughflow3d(
        fill,solid,v,nx,ny,nz,.1,.05);
    check(fill==original,"initial cells immutable");
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto f=r.fill[idx(x,y,z)];
        const double expect=x==4?.9:x==10?.1:(x>4&&x<10?1.:0.);
        near(f,expect,2e-14,"exact translated saturated slug");
    }
    near(r.initial_fraction_sum,6.*ny*nz,1e-10,"initial fractional mass");
    near(r.final_fraction_sum,6.*ny*nz,1e-10,"final fractional mass");
    near(r.max_six_face_cfl,.2,1e-14,"conservative six-face CFL");
    check(r.limited_iterations==0,"solenoidal slug never capacity limited");
    check(r.transfers.size()>=6*ny*nz,
          "includes internal saturated-throughflow face transfers");
    std::cout<<"ANALYTIC_THROUGHFLOW n="<<nx<<"x"<<ny<<"x"<<nz
             <<" left_fraction="<<r.fill[idx(4,4,4)]
             <<" right_fraction="<<r.fill[idx(10,4,4)]
             <<" transferred_faces="<<r.transfers.size()<<"\n";
}
void limits() {
    std::vector<double> fill(nx*ny*nz,1);
    std::vector<std::uint8_t> solid(fill.size(),0);
    const auto v=speed(.2);
    const auto r=upc::p2_vof_throughflow3d(
        fill,solid,v,nx,ny,nz,.1,.05);
    near(r.initial_fraction_sum,r.final_fraction_sum,1e-10,
         "pure water total mass");
    for(double f:r.fill)near(f,1,1e-12,
        "solid domain full water remains bounded on closed boundaries");
    auto broken=v;
    broken[0][idx(nx-1,3,3)]=.1;
    invalid([&]{(void)upc::p2_vof_throughflow3d(
        fill,solid,broken,nx,ny,nz,.1,.05);});
    broken=v; broken[0][0]=std::numeric_limits<double>::quiet_NaN();
    invalid([&]{(void)upc::p2_vof_throughflow3d(
        fill,solid,broken,nx,ny,nz,.1,.05);});
    invalid([&]{(void)upc::p2_vof_throughflow3d(
        fill,solid,v,nx,ny,nz,.1,2.);});
    fill[0]=-0.2;
    invalid([&]{(void)upc::p2_vof_throughflow3d(
        fill,solid,v,nx,ny,nz,.1,.05);});
    fill[0]=0;
    solid[1]=1;fill[1]=1;
    invalid([&]{(void)upc::p2_vof_throughflow3d(
        fill,solid,v,nx,ny,nz,.1,.05);});
}
void optin_regression() {
    upc::CoupledMacWater3D legacy(8,6,8,.1);
    upc::CoupledMacWater3D opted(8,6,8,.1);
    for(int z=0;z<5;++z)for(int y=0;y<6;++y)
    for(int x=0;x<3;++x) {
        legacy.set_water(x,y,z,1);
        opted.set_water(x,y,z,1);
    }
    const auto initial=legacy.volume_m3();
    check(!legacy.conservative_throughflow_transport_enabled(),
          "original transport remains default");
    opted.set_conservative_throughflow_transport(true);
    check(opted.conservative_throughflow_transport_enabled(),"opt-in active");
    for(int i=0;i<5;++i) {
        legacy.step(.01,150);
        opted.step(.01,150);
    }
    near(legacy.volume_m3(),initial,1e-9,"legacy water conserved");
    near(opted.volume_m3(),initial,1e-9,"throughflow water conserved");
    for(std::size_t z=0;z<8;++z)
    for(std::size_t y=0;y<6;++y)
    for(std::size_t x=0;x<8;++x){
        const double a=opted.water_fraction(x,y,z);
        check(std::isfinite(a)&&a>=0&&a<=1,"optin bounded cell");
    }
    std::cout<<"LIVE_THROUGHFLOW legacy_volume_m3="<<legacy.volume_m3()
             <<" optin_volume_m3="<<opted.volume_m3()
             <<" total_volume_m3="<<initial<<"\n";
}
}
int main(){
    try {
        translate();
        limits();
        optin_regression();
        std::cout<<"PASS P2_THROUGHFLOW assertions="<<count
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL P2_THROUGHFLOW "<<e.what()
                 <<" assertions="<<count<<"\n";
        return 1;
    }
}
