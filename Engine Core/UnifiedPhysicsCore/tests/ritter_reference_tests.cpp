// P1 external-to-solver analytical reference: Ritter (1892) dam break
// into a dry, horizontal, frictionless bed, 1D shallow-water equations.
// Reference uses a closed-form similarity solution, not the Rusanov flux.
#include "upc/core.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace upc;

struct Exact {
    double h,hu;
};
Exact ritter(double x,double t,double interface_x,double h_left) {
    constexpr double g=9.81;
    const double c=std::sqrt(g*h_left);
    const double xi=(x-interface_x)/t;
    if(xi<=-c)return {h_left,0.0};
    if(xi>=2*c)return {0.0,0.0};
    const double h=(2*c-xi)*(2*c-xi)/(9*g);
    const double u=2*(c+xi)/3;
    return {h,h*u};
}
struct Metrics {
    double dx,depth_l1,momentum_l1,volume_error,max_h,max_front_error,strong_front_error;
};
Metrics run(double dx){
    const double domain=12.0,interface_x=6.0, h_left=1.0,t=0.25;
    const std::size_t nx=static_cast<std::size_t>(std::lround(domain/dx));
    if(std::abs(nx*dx-domain)>1e-9)throw std::runtime_error("Bad grid resolution");
    ShallowWater2D sim(nx,1,dx,1);
    for(std::size_t i=0;i<nx;++i){
        const double x=(i+0.5)*dx;
        if(x<interface_x)sim.set_depth(i,0,h_left);
    }
    const double initial=sim.volume_m3();
    sim.step(t);
    double sum_h=0,sum_hu=0,max_h=0;
    double front=-1e9, strong_front=-1e9;
    for(std::size_t i=0;i<nx;++i){
        const double x=(i+0.5)*dx;
        const auto actual=sim.at(i,0);
        const auto expected=ritter(x,t,interface_x,h_left);
        if(!std::isfinite(actual.h)||!std::isfinite(actual.hx)||actual.h<-1e-12)
            throw std::runtime_error("Negative/non-finite dry-front cell");
        sum_h+=std::abs(actual.h-expected.h)*dx;
        sum_hu+=std::abs(actual.hx-expected.hu)*dx;
        max_h=std::max(max_h,actual.h);
        // Diagnostic front depends on depth threshold. Report BOTH.
        if(actual.h>1e-6)front=x;
        if(actual.h>0.002)strong_front=x;
    }
    const double expected_front=interface_x+2*std::sqrt(9.81*h_left)*t;
    return {dx,sum_h/domain,sum_hu/domain,
            std::abs(sim.volume_m3()-initial),max_h,
            std::abs(front-expected_front),std::abs(strong_front-expected_front)};
}
int main(){
try {
    std::vector<Metrics> rows;
    for(double dx : {0.1,0.05,0.025})rows.push_back(run(dx));
    std::cout<<std::setprecision(12);
    std::cout<<"dx_m,depth_mean_abs_error_m,momentum_mean_abs_error_m2_s,"
                "volume_error_m3,max_depth_m,front_abs_error_m,strong_front_iso_0p002_error_m\n";
    for(const auto& r:rows)
        std::cout<<r.dx<<","<<r.depth_l1<<","<<r.momentum_l1<<","
                 <<r.volume_error<<","<<r.max_h<<","<<r.max_front_error<<','<<r.strong_front_error<<"\n";
    for(const auto& r:rows) {
        if(r.volume_error>1e-9)throw std::runtime_error("Conservation failed");
        if(r.depth_l1>0.035)throw std::runtime_error("Ritter depth error exceeds tolerance");
        if(r.momentum_l1>0.075)throw std::runtime_error("Ritter momentum error exceeds tolerance");
        if(r.max_front_error>0.35)throw std::runtime_error("Ritter low-threshold front error >35 cm");
        if(r.strong_front_error>0.36)throw std::runtime_error("Ritter 2mm iso-depth error >36 cm");
    }
    if(!(rows[2].max_front_error<0.08 &&
         rows[2].max_front_error<rows[1].max_front_error &&
         rows[1].max_front_error<rows[0].max_front_error))
        throw std::runtime_error("Ritter low-threshold dry-front fails accuracy/convergence");
    if(!(rows[2].depth_l1<rows[1].depth_l1 &&
         rows[1].depth_l1<rows[0].depth_l1))
        throw std::runtime_error("Ritter depth does not converge under refinement");
    if(!(rows[2].momentum_l1<rows[1].momentum_l1 &&
         rows[1].momentum_l1<rows[0].momentum_l1))
        throw std::runtime_error("Ritter momentum does not converge under refinement");
    std::cout<<"PASS: analytical Ritter dry-bed dam break (3 resolutions), "
                "conservation and grid convergence\n";
    return 0;
} catch(const std::exception& e) {
    std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
}
}
