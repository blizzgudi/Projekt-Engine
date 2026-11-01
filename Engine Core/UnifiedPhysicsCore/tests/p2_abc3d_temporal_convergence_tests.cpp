// P2 PARTIAL: temporal self-convergence of variable-viscosity 3D ABC MAC/PLIC.
// This is a numerical verification against an exact periodic Navier-Stokes
// solution, not an external measured CFD reference or two-phase/FSI proof.
#include "upc/p2_viscous_mac_plic_roundoff_schedule3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi=3.1415926535897932384626433832795;
constexpr int n=12;
constexpr double amplitude=0.05, dx=1.0/n;
std::size_t assertions=0;
void check(bool condition,const char* message) {
    ++assertions;
    if(!condition) throw std::runtime_error(message);
}
using State=upc::P2NonlinearMacPlicState3D;
using Interval=upc::P2RoundoffMacPlicInterval3D;
using Result=upc::P2RoundoffMacPlicSchedule3D;
std::size_t face(int c,int i,int j,int k) {
    if(c==0) return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
    if(c==1) return (static_cast<std::size_t>(k)*(n+1)+j)*n+i;
    return (static_cast<std::size_t>(k)*n+j)*n+i;
}
double abc(int c,int i,int j,int k,double a) {
    const double w=2*pi/n;
    const double x=w*(i+(c==0?0.0:0.5));
    const double y=w*(j+(c==1?0.0:0.5));
    const double z=w*(k+(c==2?0.0:0.5));
    if(c==0) return a*(std::sin(z)+std::cos(y));
    if(c==1) return a*(std::sin(x)+std::cos(z));
    return a*(std::sin(y)+std::cos(x));
}
State initial() {
    State s;
    for(int c=0;c<3;++c) {
        s.staggered_velocity[c].resize(
            static_cast<std::size_t>(n+(c==0))*(n+(c==1))*(n+(c==2)));
        for(int k=0;k<n+(c==2);++k)
        for(int j=0;j<n+(c==1);++j)
        for(int i=0;i<n+(c==0);++i)
            s.staggered_velocity[c][face(c,i,j,k)]=
                abc(c,i,j,k,amplitude);
    }
    s.water_fill.resize(static_cast<std::size_t>(n)*n*n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)for(int i=0;i<n;++i)
        s.water_fill[(k*n+j)*n+i]=(i>=n/4&&i<n/2)?1.0:0.0;
    return s;
}
const std::array<Interval,5> base{{
    {0.012,0.002},{0.018,0.003},{0.020,0.004},
    {0.014,0.0025},{0.036,0.0035}
}};
std::vector<Interval> refined(int subdivision) {
    std::vector<Interval> v;
    for(const auto& b:base)
        for(int j=0;j<subdivision;++j)
            v.push_back({b.dt_s/subdivision,b.nu_m2_s});
    return v;
}
struct Measurement {
    Result result;
    double relative_l2=0,energy_ratio=0,analytic_ratio=0;
};
Measurement run(const State& start,int subdivision) {
    const auto schedule=refined(subdivision);
    Measurement m;
    m.result=upc::p2_roundoff_mac_plic_schedule3d(
        start,schedule,n,n,n,dx,1000);
    double integrated_nu=0;
    for(const auto& step:schedule) integrated_nu+=step.dt_s*step.nu_m2_s;
    const double exact_amp=amplitude*std::exp(-4*pi*pi*integrated_nu);
    double err2=0,ref2=0,energy0=0,energy1=0;
    for(int c=0;c<3;++c)
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)for(int i=0;i<n;++i) {
        const auto id=face(c,i,j,k);
        const double actual=m.result.state.staggered_velocity[c][id];
        const double exact=abc(c,i,j,k,exact_amp);
        check(std::isfinite(actual),"all three velocity components finite");
        err2+=(actual-exact)*(actual-exact);
        ref2+=exact*exact;
        energy0+=start.staggered_velocity[c][id]*
                 start.staggered_velocity[c][id];
        energy1+=actual*actual;
    }
    m.relative_l2=std::sqrt(err2/ref2);
    m.energy_ratio=energy1/energy0;
    m.analytic_ratio=std::pow(exact_amp/amplitude,2);
    check(m.result.intervals.size()==schedule.size(),"all steps completed");
    check(m.result.total_plic_substeps>=schedule.size(),"real PLIC transport");
    check(m.result.total_viscous_substeps>=schedule.size(),"real viscosity");
    check(std::abs(m.result.elapsed_s-.1)<1e-13,"fixed final time");
    check(std::abs(m.result.final_volume_m3-.25)<1e-10,"water inventory");
    check(std::abs(m.result.total_ledger_residual_m3)<1e-10,"water ledger");
    check(m.result.absolute_snap_m3<1e-12,"endpoint correction bounded");
    check(m.result.max_divergence_s_inv<1e-9,"projected divergence");
    check(m.relative_l2<.02,"analytic 3D ABC velocity error bounded");
    check(m.energy_ratio>0&&m.energy_ratio<1,"numerical kinetic decay");
    for(double fill:m.result.state.water_fill)
        check(std::isfinite(fill)&&fill>=0&&fill<=1,"VOF bounded");
    return m;
}
double velocity_distance(const State& a,const State& b) {
    double sum=0;
    for(int c=0;c<3;++c)
        for(std::size_t i=0;i<a.staggered_velocity[c].size();++i) {
            const double d=a.staggered_velocity[c][i]-
                           b.staggered_velocity[c][i];
            sum+=d*d;
        }
    return std::sqrt(sum);
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        const auto start=initial();
        const auto coarse=run(start,1);
        const auto medium=run(start,2);
        const auto fine=run(start,4);
        const double d_coarse_medium=velocity_distance(
            coarse.result.state,medium.result.state);
        const double d_medium_fine=velocity_distance(
            medium.result.state,fine.result.state);
        const double ratio=d_coarse_medium/d_medium_fine;
        check(std::isfinite(ratio)&&d_medium_fine>0,
              "nonzero finite temporal self-convergence difference");
        check(ratio>1.7&&ratio<2.4,
              "measured first-order temporal self-convergence band");
        check(fine.relative_l2<medium.relative_l2&&
              medium.relative_l2<coarse.relative_l2,
              "analytical ABC error decreases on time refinement");
        check(std::abs(fine.energy_ratio-medium.energy_ratio)<
              std::abs(medium.energy_ratio-coarse.energy_ratio),
              "energy diagnostic approaches time-refined limit");
        check(coarse.result.state.water_fill.size()==start.water_fill.size(),
              "water state retained");
        check(start.water_fill==initial().water_fill,"caller state unchanged");
        std::cout<<"ABC_TEMPORAL coarse_l2="<<coarse.relative_l2
                 <<" medium_l2="<<medium.relative_l2
                 <<" fine_l2="<<fine.relative_l2
                 <<" diff_5_10="<<d_coarse_medium
                 <<" diff_10_20="<<d_medium_fine
                 <<" self_ratio="<<ratio
                 <<" coarse_energy="<<coarse.energy_ratio
                 <<" medium_energy="<<medium.energy_ratio
                 <<" fine_energy="<<fine.energy_ratio
                 <<" analytic_energy="<<fine.analytic_ratio
                 <<" coarse_volume="<<coarse.result.final_volume_m3
                 <<" fine_volume="<<fine.result.final_volume_m3
                 <<" fine_divergence="<<fine.result.max_divergence_s_inv
                 <<" fine_ledger="<<fine.result.total_ledger_residual_m3
                 <<" fine_snap="<<fine.result.absolute_snap_m3
                 <<" fine_viscous="<<fine.result.total_viscous_substeps
                 <<" fine_plic="<<fine.result.total_plic_substeps<<"\n";
        std::cout<<"PASS ABC_TEMPORAL assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL ABC_TEMPORAL "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
