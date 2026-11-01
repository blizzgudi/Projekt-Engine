// P2 PARTIAL: independent continuous 3D Arnold-Beltrami-Childress (ABC)
// exact Navier-Stokes solution against periodic viscous nonlinear MAC/PLIC.
// ABC has curl(u)=k*u, so (u.grad)u=grad(|u|^2/2); pressure absorbs
// the nonlinear term and the velocity decays as exp(-nu*k*k*t).
// This is a manufactured analytical benchmark, NOT measured 3D CFD/FSI.
#include "upc/p2_viscous_nonlinear_mac_plic3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi=3.1415926535897932384626433832795;
std::size_t assertions=0;
void check(bool condition,const char* message) {
    ++assertions;
    if(!condition)throw std::runtime_error(message);
}
using State=upc::P2NonlinearMacPlicState3D;
std::size_t face(int c,int i,int j,int k,int n) {
    if(c==0)return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
    if(c==1)return (static_cast<std::size_t>(k)*(n+1)+j)*n+i;
    return (static_cast<std::size_t>(k)*n+j)*n+i;
}
double abc(int c,int i,int j,int k,int n,double amplitude) {
    const double dx=1.0/n, wave=2*pi;
    const double x=wave*((i+(c==0?0.0:0.5))*dx);
    const double y=wave*((j+(c==1?0.0:0.5))*dx);
    const double z=wave*((k+(c==2?0.0:0.5))*dx);
    if(c==0)return amplitude*(std::sin(z)+std::cos(y));
    if(c==1)return amplitude*(std::sin(x)+std::cos(z));
    return amplitude*(std::sin(y)+std::cos(x));
}
State initial(int n,double amplitude) {
    State s;
    for(int c=0;c<3;++c) {
        s.staggered_velocity[c].resize(
            static_cast<std::size_t>(n+(c==0))*(n+(c==1))*(n+(c==2)));
        for(int k=0;k<n+(c==2);++k)
        for(int j=0;j<n+(c==1);++j)
        for(int i=0;i<n+(c==0);++i)
            s.staggered_velocity[c][face(c,i,j,k,n)]=
                abc(c,i,j,k,n,amplitude);
    }
    s.water_fill.resize(static_cast<std::size_t>(n)*n*n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)for(int i=0;i<n;++i)
        s.water_fill[(k*n+j)*n+i]=(i>=n/4&&i<n/2)?1.0:0.0;
    return s;
}
struct Metrics {
    int n=0;
    double rel_l2=0,max_abs=0,max_div=0,water_residual=0;
    double energy_ratio=0,expected_ratio=0;
};
Metrics run(int n) {
    constexpr double amplitude=.05,nu=.003,dt=.02,final_time=.02;
    constexpr int steps=1;
    const double dx=1.0/n,expected_amp=
        amplitude*std::exp(-nu*4*pi*pi*final_time);
    State s=initial(n,amplitude);
    const auto untouched=s;
    double max_div=0,water_residual=0;
    for(int step=0;step<steps;++step) {
        const auto result=upc::p2_viscous_nonlinear_mac_plic_step3d(
            s,n,n,n,dx,dt,1000.0,nu,.4,256,256);
        max_div=std::max(max_div,
            result.coupled.projection.max_divergence_s_inv);
        water_residual=std::max(water_residual,
            std::abs(result.coupled.transport.mass_residual_m3));
        check(result.viscous_substeps>0,"viscosity executed");
        check(result.coupled.momentum_predictor_courant<1,
              "stable nonlinear predictor CFL");
        s=result.coupled.state;
    }
    double error2=0,reference2=0,energy0=0,energy1=0,max_abs=0;
    for(int c=0;c<3;++c)
    for(int k=0;k<n;++k)
    for(int j=0;j<n;++j)
    for(int i=0;i<n;++i) {
        const auto id=face(c,i,j,k,n);
        const double expected=abc(c,i,j,k,n,expected_amp);
        const double actual=s.staggered_velocity[c][id];
        const double diff=actual-expected;
        error2+=diff*diff;
        reference2+=expected*expected;
        energy0+=untouched.staggered_velocity[c][id]*
                 untouched.staggered_velocity[c][id];
        energy1+=actual*actual;
        max_abs=std::max(max_abs,std::abs(diff));
        check(std::isfinite(actual),"finite ABC velocity");
    }
    for(double f:s.water_fill)
        check(std::isfinite(f)&&f>=0.0&&f<=1.0,
              "bounded water fraction");
    const double rel_l2=std::sqrt(error2/reference2);
    const double energy_ratio=energy1/energy0;
    const double expected_ratio=std::pow(expected_amp/amplitude,2);
    check(std::isfinite(rel_l2)&&rel_l2<.15,"ABC relative L2 bounded");
    check(max_div<1e-9,"projected divergence");
    check(water_residual<1e-9,"ABC water mass");
    check(energy_ratio<1.0&&energy_ratio>0.0,
          "kinetic energy decays");
    check(untouched.water_fill==initial(n,amplitude).water_fill,
          "initial water preserved");
    std::cout<<"ABC "<<n<<" "<<rel_l2<<" "<<max_abs<<" "
             <<max_div<<" "<<water_residual<<" "<<energy_ratio<<" "
             <<expected_ratio<<"\n";
    return {n,rel_l2,max_abs,max_div,water_residual,
            energy_ratio,expected_ratio};
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        const auto a=run(8),b=run(12),c=run(16);
        check(c.rel_l2<b.rel_l2&&b.rel_l2<a.rel_l2,
              "3D ABC spatial refinement improves relative error");
        std::cout<<"REFINEMENT "<<a.rel_l2/b.rel_l2<<" "
                 <<b.rel_l2/c.rel_l2<<"\n";
        std::cout<<"PASS ABC3D assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL ABC3D "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
