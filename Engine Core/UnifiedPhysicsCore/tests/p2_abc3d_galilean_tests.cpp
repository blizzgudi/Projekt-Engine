// P2 PARTIAL: Galilean-translated exact 3D ABC Navier-Stokes benchmark.
// Uniform mean flow transports a viscously decaying Beltrami wave.
// Checks genuine nonlinear advection; no free-surface stress or FSI claim.
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
constexpr double amplitude=.05, nu=.003, dt=.02;
constexpr std::array<double,3> mean{{.25,-.15,.10}};
std::size_t assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok) throw std::runtime_error(msg);
}
using State=upc::P2NonlinearMacPlicState3D;
std::size_t face(int c,int i,int j,int k,int n) {
    if(c==0) return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
    if(c==1) return (static_cast<std::size_t>(k)*(n+1)+j)*n+i;
    return (static_cast<std::size_t>(k)*n+j)*n+i;
}
// Exact Navier-Stokes solution u = U + A exp(-nu*k^2*t) F(x-U*t),
// where curl(F)=k F. Pressure absorbs the nonlinear Beltrami gradient.
double exact(int c,int i,int j,int k,int n,double t,bool advect) {
    const double wave=2*pi;
    const double dx=1.0/n;
    const double x=wave*((i+(c==0?0.0:0.5))*dx-(advect?mean[0]*t:0));
    const double y=wave*((j+(c==1?0.0:0.5))*dx-(advect?mean[1]*t:0));
    const double z=wave*((k+(c==2?0.0:0.5))*dx-(advect?mean[2]*t:0));
    const double a=amplitude*std::exp(-nu*wave*wave*t);
    if(c==0) return mean[0]+a*(std::sin(z)+std::cos(y));
    if(c==1) return mean[1]+a*(std::sin(x)+std::cos(z));
    return mean[2]+a*(std::sin(y)+std::cos(x));
}
State initial(int n) {
    State s;
    for(int c=0;c<3;++c) {
        s.staggered_velocity[c].resize(
            static_cast<std::size_t>(n+(c==0))*(n+(c==1))*(n+(c==2)));
        for(int k=0;k<n+(c==2);++k)
        for(int j=0;j<n+(c==1);++j)
        for(int i=0;i<n+(c==0);++i)
            s.staggered_velocity[c][face(c,i,j,k,n)]=
                exact(c,i,j,k,n,0,true);
    }
    s.water_fill.resize(static_cast<std::size_t>(n)*n*n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)for(int i=0;i<n;++i)
        s.water_fill[(k*n+j)*n+i]=(i>=n/4&&i<3*n/4)?1.0:0.0;
    return s;
}
struct Metrics {
    int n=0;
    double l2=0,unshifted_l2=0,max_div=0,volume=0,ledger=0;
    double max_mean_error=0,energy_ratio=0,analytic_energy_ratio=0;
    double snap=0;
    std::size_t viscous=0,plic=0;
};
Metrics run(int n) {
    const State start=initial(n);
    const auto result=upc::p2_roundoff_mac_plic_schedule3d(
        start,{{dt,nu}},n,n,n,1.0/n,1000);
    Metrics m;
    m.n=n;
    m.max_div=result.max_divergence_s_inv;
    m.volume=result.final_volume_m3;
    m.ledger=result.total_ledger_residual_m3;
    m.snap=result.absolute_snap_m3;
    m.viscous=result.total_viscous_substeps;
    m.plic=result.total_plic_substeps;
    double e2=0,wrong2=0,ref2=0,e0=0,e1=0;
    for(int c=0;c<3;++c) {
        double sum=0;
        for(int k=0;k<n;++k)for(int j=0;j<n;++j)for(int i=0;i<n;++i) {
            const auto id=face(c,i,j,k,n);
            const double actual=result.state.staggered_velocity[c][id];
            const double reference=exact(c,i,j,k,n,dt,true);
            const double wrong=exact(c,i,j,k,n,dt,false);
            check(std::isfinite(actual),"finite translated ABC face");
            e2+=(actual-reference)*(actual-reference);
            wrong2+=(actual-wrong)*(actual-wrong);
            ref2+=(reference-mean[c])*(reference-mean[c]);
            e0+=std::pow(start.staggered_velocity[c][id]-mean[c],2);
            e1+=std::pow(actual-mean[c],2);
            sum+=actual;
        }
        m.max_mean_error=std::max(m.max_mean_error,
            std::abs(sum/(static_cast<double>(n)*n*n)-mean[c]));
    }
    m.l2=std::sqrt(e2/ref2);
    m.unshifted_l2=std::sqrt(wrong2/ref2);
    m.energy_ratio=e1/e0;
    m.analytic_energy_ratio=std::exp(-2*nu*4*pi*pi*dt);
    for(double f:result.state.water_fill)
        check(std::isfinite(f)&&f>=0&&f<=1,"bounded transported VOF");
    check(result.intervals.size()==1&&m.viscous>=1&&m.plic>=1,
          "real viscosity and PLIC");
    check(std::abs(m.volume-(double(3*n/4-n/4)/n))<1e-10,"conserved water volume");
    check(std::abs(m.ledger)<1e-10,"closed water ledger");
    check(m.snap<1e-12,"only endpoint roundoff correction");
    check(m.max_div<1e-9,"divergence-free projected field");
    check(m.l2<.1,"translated ABC analytic L2 bound");
    check(m.energy_ratio>0&&m.energy_ratio<1,
          "mean-subtracted kinetic energy decays");
    check(start.water_fill==initial(n).water_fill,
          "caller state unchanged");
    std::cout<<"ABC_GALILEAN n="<<n<<" rel_l2="<<m.l2
             <<" unshifted_l2="<<m.unshifted_l2
             <<" mean_error="<<m.max_mean_error
             <<" energy_ratio="<<m.energy_ratio
             <<" analytic_energy_ratio="<<m.analytic_energy_ratio
             <<" volume="<<m.volume<<" ledger="<<m.ledger
             <<" snap="<<m.snap<<" divergence="<<m.max_div
             <<" viscous="<<m.viscous<<" plic="<<m.plic<<"\n";
    return m;
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        const auto a=run(8),b=run(10),c=run(12);
        check(c.l2<b.l2&&b.l2<a.l2,
              "3D translated ABC error decreases on spatial refinement");
        check(c.l2<c.unshifted_l2,
              "solver closer to advected than stationary ABC reference");
        check(b.l2<b.unshifted_l2,
              "nonlinear advection detected on medium grid");
        std::cout<<"ABC_GALILEAN_REFINEMENT coarse="<<a.l2
                 <<" medium="<<b.l2<<" fine="<<c.l2
                 <<" spatial_ratio_8_10="<<a.l2/b.l2
                 <<" spatial_ratio_10_12="<<b.l2/c.l2<<"\n";
        std::cout<<"PASS ABC_GALILEAN assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL ABC_GALILEAN "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
