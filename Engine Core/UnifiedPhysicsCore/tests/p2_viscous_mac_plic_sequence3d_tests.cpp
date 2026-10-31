// P2 PARTIAL: independent multistep Fourier shear and live PLIC regression.
#include "upc/p2_viscous_mac_plic_sequence3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
constexpr double pi=3.14159265358979323846;
std::size_t assertions=0;
void check(bool b,const char* msg){++assertions;if(!b)throw std::runtime_error(msg);}
void near(double a,double b,double tol,const char* msg){
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
using State=upc::P2NonlinearMacPlicState3D;
using Interval=upc::P2ViscousMacPlicInterval3D;
State blank(int n) {
    State s;
    s.staggered_velocity={
        std::vector<double>((n+1)*n*n,0),
        std::vector<double>(n*(n+1)*n,0),
        std::vector<double>(n*n*(n+1),0)};
    s.water_fill.assign(n*n*n,1.0);
    return s;
}
std::size_t uface(int i,int j,int k,int n){
    return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
}
void shear(State& s,int n,double mean,double amplitude) {
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i)
        s.staggered_velocity[0][uface(i,j,k,n)]=
            mean+amplitude*std::sin(2*pi*j/n);
}
const std::vector<Interval> schedule{{.2,.002},{.4,.004},{.15,0.0}};
void eigen(int n) {
    const double dx=.1,mean=.02,amplitude=.04;
    auto s=blank(n);
    shear(s,n,mean,amplitude);
    const auto original=s;
    const auto r=upc::p2_viscous_mac_plic_sequence3d(
        s,schedule,n,n,n,dx,1000,.4,128,128);
    double factor=1;
    std::size_t expected_viscous=0;
    for(std::size_t t=0;t<schedule.size();++t) {
        const auto& interval=schedule[t];
        const auto& report=r.intervals[t];
        std::size_t sub=0;
        if(interval.nu_m2_s>0) {
            const long double a=static_cast<long double>(interval.nu_m2_s)*
                interval.dt_s/(static_cast<long double>(dx)*dx);
            sub=static_cast<std::size_t>(std::ceil(a*8));
            const double alpha=static_cast<double>(a/sub);
            factor*=std::pow(1-4*alpha*std::pow(std::sin(pi/n),2),
                             static_cast<int>(sub));
        }
        expected_viscous+=sub;
        check(report.viscous_substeps==sub,"viscosity budget per interval");
        check(report.plic_substeps>=1,"PLIC step exists");
        near(report.mass_residual_m3,0,2e-11,"interval mass");
        check(report.max_projected_divergence_s_inv<1e-10,"interval divergence");
        if(sub>0)
            check(report.viscous_face_square_sum_after<
                  report.viscous_face_square_sum_before,"dissipating viscous step");
        else
            near(report.viscous_face_square_sum_before,0,0,"zero viscosity bypass");
    }
    double maxerr=0;
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i) {
        const double expected=mean+amplitude*factor*std::sin(2*pi*j/n);
        const double actual=r.state.staggered_velocity[0][uface(i,j,k,n)];
        maxerr=std::max(maxerr,std::abs(actual-expected));
        near(actual,expected,3e-11,"multistep Fourier eigenmode");
    }
    for(int c=1;c<3;++c)
        for(double v:r.state.staggered_velocity[c])
            near(v,0,3e-11,"no transverse mode");
    for(double f:r.state.water_fill)
        near(f,1,2e-12,"full-water invariant");
    check(r.total_viscous_substeps==expected_viscous,"cumulative viscosity");
    check(r.total_plic_substeps>=schedule.size(),"cumulative PLIC");
    near(r.elapsed_s,.75,1e-14,"elapsed duration");
    near(r.total_mass_residual_m3,0,2e-11,"total water residual");
    check(s.water_fill==original.water_fill&&
          s.staggered_velocity==original.staggered_velocity,"immutable input");
    std::cout<<"EIGEN "<<n<<" "<<expected_viscous<<" "
             <<factor<<" "<<maxerr<<" "<<r.total_plic_substeps
             <<" "<<r.total_mass_residual_m3<<"\n";
}
void coupled() {
    constexpr int n=12;
    auto s=blank(n);
    shear(s,n,.025,.01);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i)
        s.water_fill[(k*n+j)*n+i]=(i>=2&&i<=4)?1.0:0.0;
    const auto original=s;
    const std::vector<Interval> steps{{.04,.005},{.03,.002},{.02,0}};
    const auto r=upc::p2_viscous_mac_plic_sequence3d(
        s,steps,n,n,n,.1,1000,.4,128,128);
    auto manual=s;
    std::size_t sum_v=0,sum_p=0;
    for(std::size_t t=0;t<steps.size();++t) {
        const auto m=upc::p2_viscous_nonlinear_mac_plic_step3d(
            manual,n,n,n,.1,steps[t].dt_s,1000,steps[t].nu_m2_s);
        manual=m.coupled.state;
        sum_v+=m.viscous_substeps;
        sum_p+=m.coupled.transport.substeps;
        check(r.intervals[t].plic_substeps==m.coupled.transport.substeps,
              "per-step PLIC budget parity");
    }
    check(r.state.water_fill==manual.water_fill,"manual multistep VOF parity");
    check(r.state.staggered_velocity==manual.staggered_velocity,
          "manual multistep MAC parity");
    check(r.total_viscous_substeps==sum_v&&r.total_plic_substeps==sum_p,
          "manual budget parity");
    double moved=0;
    for(std::size_t i=0;i<r.state.water_fill.size();++i) {
        moved+=std::abs(r.state.water_fill[i]-s.water_fill[i]);
        check(r.state.water_fill[i]>=0&&r.state.water_fill[i]<=1,
              "bounded water");
    }
    check(moved>1e-5,"water transported in repeated coupled steps");
    near(r.intervals.front().initial_volume_m3,.432,1e-11,"3D water inventory");
    near(r.intervals.back().final_volume_m3,.432,1e-11,"3D final water");
    near(r.total_mass_residual_m3,0,1e-11,"cumulative water conservation");
    check(s.water_fill==original.water_fill&&
          s.staggered_velocity==original.staggered_velocity,"coupled input immutable");
    std::cout<<"COUPLED "<<moved<<" "<<sum_v<<" "<<sum_p<<" "
             <<r.total_mass_residual_m3<<"\n";
}
void negative() {
    auto s=blank(8);
    shear(s,8,.02,.01);
    std::size_t rejected=0;
    auto reject=[&](auto f) {
        try{f();}catch(const std::invalid_argument&){++rejected;return;}
        throw std::runtime_error("invalid sequence accepted");
    };
    auto run=[&](const std::vector<Interval>& a,std::size_t p=128,
                 std::size_t v=128) {
        return upc::p2_viscous_mac_plic_sequence3d(
            s,a,8,8,8,.1,1000,.4,p,v);
    };
    reject([&]{run({});});
    reject([&]{run({{0,.002}});});
    reject([&]{run({{.1,-.001}});});
    reject([&]{run({{.1,std::numeric_limits<double>::quiet_NaN()}});});
    reject([&]{run({{.1,.002}},0);});
    reject([&]{run({{.1,.002}},128,0);});
    reject([&]{run({{.2,.002},{.4,.004}},128,1);});
    reject([&]{run({{.1,.002},{.1,.002}},1,128);});
    reject([&]{run({{.1,.002},{std::numeric_limits<double>::infinity(),0}});});
    check(rejected==9,"nine invalid cases rejected");
    std::cout<<"NEGATIVE "<<rejected<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        eigen(8);
        eigen(12);
        coupled();
        negative();
        std::cout<<"PASS: viscous MAC PLIC sequence assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
