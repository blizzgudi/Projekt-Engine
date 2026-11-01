// P2 PARTIAL: time-varying viscous 3D ABC/MAC/PLIC with roundoff-only
// interface sanitation, independent analytic decay and interval ledger.
#include "upc/p2_viscous_mac_plic_roundoff_schedule3d.hpp"
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
std::size_t assertions=0;
void check(bool ok,const char* message) {
    ++assertions;
    if(!ok)throw std::runtime_error(message);
}
template<class F> void rejects(F fn,const char* message) {
    bool thrown=false;
    try{fn();}catch(const std::exception&){thrown=true;}
    check(thrown,message);
}
using State=upc::P2NonlinearMacPlicState3D;
using Interval=upc::P2RoundoffMacPlicInterval3D;
std::size_t face(int c,int i,int j,int k,int n) {
    if(c==0)return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
    if(c==1)return (static_cast<std::size_t>(k)*(n+1)+j)*n+i;
    return (static_cast<std::size_t>(k)*n+j)*n+i;
}
double abc(int c,int i,int j,int k,int n,double amp) {
    const double w=2*pi/n;
    const double x=w*(i+(c==0?0.0:0.5));
    const double y=w*(j+(c==1?0.0:0.5));
    const double z=w*(k+(c==2?0.0:0.5));
    if(c==0)return amp*(std::sin(z)+std::cos(y));
    if(c==1)return amp*(std::sin(x)+std::cos(z));
    return amp*(std::sin(y)+std::cos(x));
}
State initial(int n,double amp) {
    State s;
    for(int c=0;c<3;++c) {
        s.staggered_velocity[c].resize(
            static_cast<std::size_t>(n+(c==0))*(n+(c==1))*(n+(c==2)));
        for(int k=0;k<n+(c==2);++k)
        for(int j=0;j<n+(c==1);++j)
        for(int i=0;i<n+(c==0);++i)
            s.staggered_velocity[c][face(c,i,j,k,n)]=abc(c,i,j,k,n,amp);
    }
    s.water_fill.resize(static_cast<std::size_t>(n)*n*n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)for(int i=0;i<n;++i)
        s.water_fill[(k*n+j)*n+i]=(i>=n/4&&i<n/2)?1.0:0.0;
    return s;
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        constexpr int n=12;
        constexpr double amp=.05,dx=1.0/n;
        const auto start=initial(n,amp);
        const std::vector<Interval> schedule{
            {.012,.002}, {.018,.003}, {.020,.004}, {.014,.0025}, {.036,.0035}};
        // Positional aggregate initialization is valid in C++17.
        const auto result=upc::p2_roundoff_mac_plic_schedule3d(
            start,schedule,n,n,n,dx,1000);
        check(result.intervals.size()==5,"all variable intervals executed");
        check(result.total_viscous_substeps>=5&&result.total_plic_substeps>=5,
              "actual coupled viscous and geometric transport substeps");
        check(result.total_snapped_cells>0,"endpoint sanitation exercised");
        check(result.absolute_snap_m3>0&&result.absolute_snap_m3<1e-12,
              "only numerical endpoint roundoff was modified");
        check(std::abs(result.total_ledger_residual_m3)<1e-10,
              "global snap-aware water ledger");
        check(std::abs(result.final_volume_m3-.25)<1e-10,
              "quarter-domain water volume");
        check(result.max_divergence_s_inv<1e-9,
              "periodic pressure projection");
        check(std::abs(result.elapsed_s-.1)<1e-14,
              "variable interval duration");
        double integrated_nu=0,energy_before=0,energy_after=0;
        for(const auto& item:schedule)integrated_nu+=item.dt_s*item.nu_m2_s;
        const double expected_amp=amp*std::exp(-4*pi*pi*integrated_nu);
        double err2=0,ref2=0,max_diff=0;
        for(int c=0;c<3;++c)for(int k=0;k<n;++k)
        for(int j=0;j<n;++j)for(int i=0;i<n;++i) {
            const auto id=face(c,i,j,k,n);
            const double value=result.state.staggered_velocity[c][id];
            const double reference=abc(c,i,j,k,n,expected_amp);
            err2+=(value-reference)*(value-reference);
            ref2+=reference*reference;
            energy_before+=start.staggered_velocity[c][id]*
                           start.staggered_velocity[c][id];
            energy_after+=value*value;
            max_diff=std::max(max_diff,std::abs(value-reference));
            check(std::isfinite(value),"finite 3D velocity");
        }
        for(double f:result.state.water_fill)
            check(std::isfinite(f)&&f>=0&&f<=1,"bounded VOF fractions");
        const double rel=std::sqrt(err2/ref2);
        check(rel<.02,"variable viscosity ABC analytical L2 reference");
        check(energy_after>0&&energy_after<energy_before,
              "viscous kinetic energy decreases");
        std::size_t summed_snap=0,sum_visc=0,sum_plic=0;
        double sum_mass=0,sum_correction=0;
        for(const auto& audit:result.intervals) {
            check(std::abs(audit.ledger_residual_m3)<1e-10,
                  "each interval closes its mass ledger");
            check(audit.max_divergence_s_inv<1e-9,
                  "each interval pressure projection");
            check(audit.water_after_m3>0&&audit.water_before_m3>0,
                  "nonempty water inventory each interval");
            summed_snap+=audit.snapped_cells;
            sum_visc+=audit.viscous_substeps;
            sum_plic+=audit.plic_substeps;
            sum_mass+=audit.transport_residual_m3;
            sum_correction+=audit.signed_snap_m3;
        }
        check(sum_visc==result.total_viscous_substeps&&
              sum_plic==result.total_plic_substeps,
              "substep totals equal interval records");
        check(summed_snap<=result.total_snapped_cells,
              "initial snap counted separately");
        check(std::abs(sum_mass-result.total_transport_residual_m3)<1e-14,
              "transport residual accumulation");
        check(std::abs(sum_correction-result.signed_snap_m3)<1e-14,
              "snap accumulation including initial correction");
        // Composition check: splitting the schedule into five independent
        // calls must produce the same state as the single schedule.
        State chained=start;
        for(const auto& item:schedule) {
            const auto single=upc::p2_roundoff_mac_plic_schedule3d(
                chained,{item},n,n,n,dx,1000);
            chained=single.state;
        }
        double max_chain=0;
        for(int c=0;c<3;++c)
            for(std::size_t i=0;i<chained.staggered_velocity[c].size();++i)
                max_chain=std::max(max_chain,std::abs(
                    chained.staggered_velocity[c][i]-
                    result.state.staggered_velocity[c][i]));
        for(std::size_t i=0;i<chained.water_fill.size();++i)
            max_chain=std::max(max_chain,std::abs(
                chained.water_fill[i]-result.state.water_fill[i]));
        check(max_chain<1e-14,"schedule and individually chained steps agree");
        // Constant-interval compatibility with the prior ABC5 adapter.
        const std::vector<Interval> constant(5,{.02,.003});
        const auto variable=upc::p2_roundoff_mac_plic_schedule3d(
            start,constant,n,n,n,dx,1000);
        const auto legacy=upc::p2_viscous_mac_plic_roundoff_sequence3d(
            start,n,n,n,dx,.02,1000,.003,5);
        double max_legacy=0;
        for(int c=0;c<3;++c)
            for(std::size_t i=0;i<legacy.state.staggered_velocity[c].size();++i)
                max_legacy=std::max(max_legacy,std::abs(
                    legacy.state.staggered_velocity[c][i]-
                    variable.state.staggered_velocity[c][i]));
        for(std::size_t i=0;i<legacy.state.water_fill.size();++i)
            max_legacy=std::max(max_legacy,std::abs(
                legacy.state.water_fill[i]-variable.state.water_fill[i]));
        check(max_legacy<1e-14,"constant schedule matches prior ABC5 adapter");
        check(start.water_fill==initial(n,amp).water_fill,
              "caller state is immutable");
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            start,{},n,n,n,dx,1000);},"reject empty schedule");
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            start,{{0,.003}},n,n,n,dx,1000);},"reject zero dt");
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            start,{{.02,-.003}},n,n,n,dx,1000);},"reject negative nu");
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            start,{{.02,std::numeric_limits<double>::quiet_NaN()}},
            n,n,n,dx,1000);},"reject NaN nu");
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            start,schedule,n,n,n,dx,1000,.4,1);},"reject cumulative PLIC budget");
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            start,schedule,n,n,n,dx,1000,.4,4096,1);},
            "reject cumulative viscous budget");
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            start,schedule,n,n,n,dx,1000,1.1);},"reject invalid CFL");
        auto invalid=start;
        invalid.water_fill[0]=.5;
        invalid.water_fill[1]=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            invalid,schedule,n,n,n,dx,1000);},"reject invalid fraction");
        invalid=start;
        invalid.staggered_velocity[0][0]=
            std::numeric_limits<double>::infinity();
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            invalid,schedule,n,n,n,dx,1000);},"reject nonfinite velocity");
        invalid=start;
        std::fill(invalid.water_fill.begin(),invalid.water_fill.end(),0.0);
        invalid.water_fill[(4*n+4)*n+4]=.5;
        rejects([&]{upc::p2_roundoff_mac_plic_schedule3d(
            invalid,schedule,n,n,n,dx,1000);},
            "genuine unresolved PLIC geometry must still fail");
        check(start.water_fill==initial(n,amp).water_fill,
              "failed calls never mutate input");
        std::cout<<"ABC_VARIABLE rel_l2="<<rel
                 <<" max_abs="<<max_diff
                 <<" energy_ratio="<<energy_after/energy_before
                 <<" analytic_energy_ratio="<<std::pow(expected_amp/amp,2)
                 <<" volume_m3="<<result.final_volume_m3
                 <<" ledger_m3="<<result.total_ledger_residual_m3
                 <<" snap_abs_m3="<<result.absolute_snap_m3
                 <<" snap_cells="<<result.total_snapped_cells
                 <<" divergence="<<result.max_divergence_s_inv
                 <<" chain_diff="<<max_chain
                 <<" legacy_diff="<<max_legacy
                 <<" viscous_substeps="<<result.total_viscous_substeps
                 <<" plic_substeps="<<result.total_plic_substeps<<"\n";
        std::cout<<"PASS ABC_VARIABLE assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL ABC_VARIABLE "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
