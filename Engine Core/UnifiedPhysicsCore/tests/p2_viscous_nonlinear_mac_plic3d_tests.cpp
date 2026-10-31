// P2 PARTIAL: periodic viscous MAC -> nonlinear predictor/projection -> PLIC.
// Independent discrete Fourier eigenvalue and zero-viscosity regression.
#include "upc/p2_viscous_nonlinear_mac_plic3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi=3.1415926535897932384626433832795;
std::size_t checks=0;
void require(bool b,const char* msg) {
    ++checks;
    if(!b)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    require(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
using State=upc::P2NonlinearMacPlicState3D;
State blank(int n) {
    State s;
    s.staggered_velocity={
        std::vector<double>((n+1)*n*n,0),
        std::vector<double>(n*(n+1)*n,0),
        std::vector<double>(n*n*(n+1),0)};
    s.water_fill.assign(n*n*n,1);
    return s;
}
std::size_t uface(int i,int j,int k,int n) {
    return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
}
void fill_shear(State& s,int n,double mean,double amplitude) {
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i)
        s.staggered_velocity[0][uface(i,j,k,n)]=
            mean+amplitude*std::sin(2*pi*j/n);
}
void eigen(int n,double dt) {
    auto s=blank(n);
    const double dx=.1,nu=.002,mean=.02,amplitude=.04;
    fill_shear(s,n,mean,amplitude);
    const auto original=s;
    const auto r=upc::p2_viscous_nonlinear_mac_plic_step3d(
        s,n,n,n,dx,dt,1000,nu,.4,128,128);
    const double total_alpha=nu*dt/(dx*dx);
    const std::size_t substeps=static_cast<std::size_t>(
        std::ceil(static_cast<long double>(nu)*dt/(dx*dx)*8));
    const double alpha=total_alpha/substeps;
    const double eigenvalue=4*std::pow(std::sin(pi/n),2);
    const double factor=std::pow(1-alpha*eigenvalue,
                                  static_cast<int>(substeps));
    double max_error=0;
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i) {
        const double expected=mean+amplitude*factor*std::sin(2*pi*j/n);
        const double actual=r.coupled.state.staggered_velocity[0][
            uface(i,j,k,n)];
        max_error=std::max(max_error,std::abs(expected-actual));
        near(actual,expected,2e-11,"independent discrete Fourier eigenmode");
    }
    for(int c=1;c<3;++c)
        for(double v:r.coupled.state.staggered_velocity[c])
            near(v,0,2e-11,"no transverse shear velocity generated");
    require(r.viscous_substeps==substeps,"viscous CFL subcycling");
    require(r.face_square_sum_after<r.face_square_sum_before,
            "viscous energy proxy dissipates");
    for(double delta:r.unique_face_sum_delta)
        near(delta,0,2e-10,"periodic face momentum sum conserved");
    for(double f:r.coupled.state.water_fill)
        near(f,1,2e-12,"full-water volume fraction invariant");
    near(r.coupled.transport.mass_residual_m3,0,2e-11,
         "full-water geometric PLIC mass conservation");
    require(r.coupled.projection.max_divergence_s_inv<1e-10,
            "projected MAC divergence");
    require(s.staggered_velocity==original.staggered_velocity &&
            s.water_fill==original.water_fill,"input unchanged");
    std::cout<<"EIGEN "<<n<<" "<<dt<<" "<<substeps<<" "
             <<max_error<<" "<<r.face_square_sum_before<<" "
             <<r.face_square_sum_after<<" "
             <<r.coupled.transport.mass_residual_m3<<" "
             <<r.coupled.projection.max_divergence_s_inv<<"\n";
}
void coupled() {
    const int n=12;
    auto s=blank(n);
    const double dx=.1,dt=.04,nu=.005;
    fill_shear(s,n,.025,.01);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i)
        s.water_fill[(k*n+j)*n+i]=(i>=2&&i<=4)?1.0:0.0;
    const auto initial=s;
    const auto r=upc::p2_viscous_nonlinear_mac_plic_step3d(
        s,n,n,n,dx,dt,1000,nu);
    double moved=0;
    for(std::size_t i=0;i<s.water_fill.size();++i) {
        moved+=std::abs(r.coupled.state.water_fill[i]-s.water_fill[i]);
        require(r.coupled.state.water_fill[i]>=0&&
                r.coupled.state.water_fill[i]<=1,"bounded PLIC");
    }
    require(moved>1e-5,"viscous velocity actually advects water");
    require(r.face_square_sum_after<r.face_square_sum_before,
            "viscous dissipation in coupled flow");
    near(r.coupled.transport.initial_volume_m3,
         3*n*n*dx*dx*dx,1e-11,"initial 3D slab volume");
    near(r.coupled.transport.final_volume_m3,
         3*n*n*dx*dx*dx,1e-11,"final 3D slab volume");
    near(r.coupled.transport.mass_residual_m3,0,1e-11,
         "coupled PLIC water mass");
    require(r.coupled.projection.max_divergence_s_inv<1e-10,
            "coupled projected divergence");
    require(s.water_fill==initial.water_fill,"coupled input immutable");
    std::cout<<"COUPLED "<<moved<<" "
             <<r.coupled.transport.mass_residual_m3<<" "
             <<r.coupled.projection.max_divergence_s_inv<<"\n";
}
void zero() {
    const int n=8;
    auto s=blank(n);
    fill_shear(s,n,.02,.01);
    const auto base=upc::p2_nonlinear_mac_plic_step3d(
        s,n,n,n,.1,.04,1000);
    const auto got=upc::p2_viscous_nonlinear_mac_plic_step3d(
        s,n,n,n,.1,.04,1000,0);
    require(got.viscous_substeps==0,"nu=0 bypasses viscosity");
    require(base.state.water_fill==got.coupled.state.water_fill,
            "nu=0 water bitwise parity");
    require(base.state.staggered_velocity==
            got.coupled.state.staggered_velocity,
            "nu=0 velocity bitwise parity");
    std::cout<<"ZERO 1\n";
}
void negative() {
    int failures=0;
    auto s=blank(8);
    fill_shear(s,8,.02,.01);
    auto reject=[&](auto fn) {
        try{fn();}catch(const std::invalid_argument&){++failures;return;}
        throw std::runtime_error("invalid viscous input accepted");
    };
    auto run=[&](const State& state,double nu,std::size_t maxstep=256) {
        return upc::p2_viscous_nonlinear_mac_plic_step3d(
            state,8,8,8,.1,.04,1000,nu,.4,256,maxstep);
    };
    reject([&]{run(s,-.001);});
    reject([&]{run(s,std::numeric_limits<double>::quiet_NaN());});
    reject([&]{run(s,1.0,1);});
    reject([&]{run(s,.001,0);});
    auto bad=s;
    bad.staggered_velocity[0][uface(8,0,0,8)]+=.2;
    reject([&]{run(bad,.001);});
    bad=s;
    bad.staggered_velocity[0].pop_back();
    reject([&]{run(bad,.001);});
    require(failures==6,"all six negative cases");
    std::cout<<"NEGATIVE "<<failures<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        eigen(8,.2);
        eigen(12,.8);
        coupled();
        zero();
        negative();
        std::cout<<"PASS: viscous nonlinear MAC PLIC assertions="
                 <<checks<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<checks<<"\n";
        return 1;
    }
}
