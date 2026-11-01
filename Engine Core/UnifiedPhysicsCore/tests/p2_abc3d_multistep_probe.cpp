// P2 PARTIAL: reproduce a real one-ULP VOF normal failure, then validate
// opt-in roundoff-only endpoint sanitation over five 3D ABC steps.
// This is not a measured CFD, free-surface stress or FSI validation.
#include "upc/p2_viscous_mac_plic_roundoff_sequence3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
constexpr double pi=3.1415926535897932384626433832795;
std::size_t assertions=0;
void check(bool yes,const char* msg) {
    ++assertions;
    if(!yes)throw std::runtime_error(msg);
}
template<class Fn> void rejects(Fn fn,const char* msg) {
    bool thrown=false;
    try{fn();}catch(const std::exception&){thrown=true;}
    check(thrown,msg);
}
using State=upc::P2NonlinearMacPlicState3D;
std::size_t idx(int c,int i,int j,int k,int n) {
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
            s.staggered_velocity[c][idx(c,i,j,k,n)]=abc(c,i,j,k,n,amp);
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
        constexpr double dt=.02,nu=.003,amp=.05,dx=1.0/n;
        const State start=initial(n,amp);
        // Exact reproduction of the previously documented blocker:
        // one-ULP near-full cells with zero centered gradient.
        const auto first=upc::p2_viscous_nonlinear_mac_plic_step3d(
            start,n,n,n,dx,dt,1000,nu,.4,256,256);
        std::size_t nearfull=0,flat=0;
        const auto& f=first.coupled.state.water_fill;
        for(int k=0;k<n;++k)for(int j=0;j<n;++j)for(int i=0;i<n;++i) {
            const auto id=(k*n+j)*n+i;
            if(f[id]==std::nextafter(1.0,0.0))++nearfull;
            if(f[id]<=0||f[id]>=1)continue;
            const auto at=[&](int x,int y,int z) {
                return f[((z+n)%n*n+(y+n)%n)*n+(x+n)%n];
            };
            const double gx=.5*(at(i+1,j,k)-at(i-1,j,k));
            const double gy=.5*(at(i,j+1,k)-at(i,j-1,k));
            const double gz=.5*(at(i,j,k+1)-at(i,j,k-1));
            if(std::max({std::abs(gx),std::abs(gy),std::abs(gz)})<=1e-12)++flat;
        }
        check(nearfull>0&&flat>0,"real one-ULP unresolved cells reproduced");
        rejects([&]{
            upc::p2_viscous_nonlinear_mac_plic_step3d(
                first.coupled.state,n,n,n,dx,dt,1000,nu,.4,256,256);
        },"unmodified second step must still reject unresolved normals");
        const auto run=upc::p2_viscous_mac_plic_roundoff_sequence3d(
            start,n,n,n,dx,dt,1000,nu,5,.4,256,256);
        check(run.steps==5&&run.viscous_substeps>=5,"all five viscous steps executed");
        check(run.plic_substeps>=5,"all five geometric transports executed");
        check(run.snapped_cells>0,"roundoff sanitation actually used");
        check(run.absolute_snap_volume_m3>0&&
              run.absolute_snap_volume_m3<1e-12,"only negligible endpoint roundoff");
        check(std::abs(run.total_mass_residual_m3)<1e-10,
              "mass after accounting signed snap corrections");
        check(std::abs(run.final_water_volume_m3-run.initial_water_volume_m3)<1e-10,
              "total physical water inventory remains stable");
        check(run.max_projected_divergence_s_inv<1e-9,"MAC pressure projection");
        check(start.water_fill==initial(n,amp).water_fill,
              "input water field remains unchanged");
        double e0=0,e1=0,err2=0,ref2=0;
        const double expected_amp=amp*std::exp(-nu*4*pi*pi*5*dt);
        for(int c=0;c<3;++c)for(int k=0;k<n;++k)
        for(int j=0;j<n;++j)for(int i=0;i<n;++i) {
            const auto id=idx(c,i,j,k,n);
            const double actual=run.state.staggered_velocity[c][id];
            const double expected=abc(c,i,j,k,n,expected_amp);
            err2+=(actual-expected)*(actual-expected);
            ref2+=expected*expected;
            e0+=start.staggered_velocity[c][id]*start.staggered_velocity[c][id];
            e1+=actual*actual;
            check(std::isfinite(actual),"all 3D velocity components finite");
        }
        for(double x:run.state.water_fill)
            check(std::isfinite(x)&&x>=0&&x<=1,"bounded final water");
        const double rel=std::sqrt(err2/ref2);
        check(rel<.02,"five-step analytic ABC L2 accuracy bounded");
        check(e1<e0&&e1>0,"viscous ABC kinetic energy decreases");
        std::cout<<"ABC5 rel_l2="<<rel<<" energy_ratio="<<e1/e0
                 <<" analytic_energy_ratio="<<std::pow(expected_amp/amp,2)
                 <<" mass_m3="<<run.final_water_volume_m3
                 <<" mass_residual_m3="<<run.total_mass_residual_m3
                 <<" snap_cells="<<run.snapped_cells
                 <<" snap_abs_m3="<<run.absolute_snap_volume_m3
                 <<" divergence="<<run.max_projected_divergence_s_inv
                 <<" nearfull="<<nearfull<<" flat="<<flat<<"\n";
        // Endpoint cleanup must not hide physically meaningful small fills.
        const double eps=std::numeric_limits<double>::epsilon();
        const auto cleaned=upc::p2_plic_endpoint_roundoff3d(
            {0,1,eps,1-eps,1e-10,1-1e-10,.5});
        check(cleaned.snapped_cells==2,"only two roundoff endpoints snapped");
        check(cleaned.fill[2]==0&&cleaned.fill[3]==1,"endpoint cleanup");
        check(cleaned.fill[4]==1e-10&&cleaned.fill[5]==1-1e-10,
              "physical subcell fractions remain unchanged");
        rejects([&]{upc::p2_plic_endpoint_roundoff3d({});},
                "reject empty endpoint array");
        rejects([&]{upc::p2_plic_endpoint_roundoff3d({-eps});},
                "reject negative fractions, do not clamp");
        rejects([&]{upc::p2_plic_endpoint_roundoff3d({1+eps});},
                "reject overfilled fractions, do not clamp");
        rejects([&]{upc::p2_plic_endpoint_roundoff3d(
                {std::numeric_limits<double>::quiet_NaN()});},
                "reject NaN fraction");
        std::vector<double> genuinely_unresolved(n*n*n,0);
        genuinely_unresolved[(4*n+4)*n+4]=.5;
        const auto untouched=upc::p2_plic_endpoint_roundoff3d(
            genuinely_unresolved);
        check(untouched.snapped_cells==0,"no invented normal for isolated half-cell");
        rejects([&]{upc::p2_periodic_plic_normals3d(
            untouched.fill,n,n,n);},"genuine unresolved interface still rejected");
        rejects([&]{upc::p2_viscous_mac_plic_roundoff_sequence3d(
            start,n,n,n,dx,dt,1000,nu,0);},"reject zero steps");
        rejects([&]{upc::p2_viscous_mac_plic_roundoff_sequence3d(
            start,n,n,n,dx,-dt,1000,nu,5);},"reject negative dt");
        rejects([&]{upc::p2_viscous_mac_plic_roundoff_sequence3d(
            start,n,n,n,dx,dt,1000,-nu,5);},"reject negative viscosity");
        rejects([&]{upc::p2_viscous_mac_plic_roundoff_sequence3d(
            start,n,n,n,dx,dt,1000,nu,257);},"reject oversized sequence");
        rejects([&]{upc::p2_viscous_mac_plic_roundoff_sequence3d(
            start,n,n,n,dx,dt,1000,nu,5,.4,0);},"reject exhausted PLIC budget");
        check(start.water_fill==initial(n,amp).water_fill,
              "failed paths did not mutate input");
        std::cout<<"PASS ABC5_ROUNDOFF assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL ABC5_ROUNDOFF "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
