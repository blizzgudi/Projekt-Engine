// P2 PARTIAL: sequential nonlinear MAC momentum/projection drives geometric PLIC.
// Periodic constant-density, passive VOF only. No free-surface stress or FSI.
#include "upc/p2_nonlinear_mac_plic3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {
using Vec=std::vector<double>;
using State=upc::P2NonlinearMacPlicState3D;
constexpr double pi=3.1415926535897932384626433832795;
std::size_t assertions=0;
void check(bool b,const char* what) {
    ++assertions;
    if(!b)throw std::runtime_error(what);
}
void near(double x,double y,double tol,const char* what) {
    check(std::isfinite(x)&&std::abs(x-y)<=tol,what);
}
std::size_t cell(int i,int j,int k,int n) {
    return (static_cast<std::size_t>(k)*n+j)*n+i;
}
std::size_t face(int axis,int i,int j,int k,int n) {
    if(axis==0)return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
    if(axis==1)return (static_cast<std::size_t>(k)*(n+1)+j)*n+i;
    return (static_cast<std::size_t>(k)*n+j)*n+i;
}
State blank(int n) {
    State s;
    s.staggered_velocity={
        Vec((n+1)*n*n,0),Vec(n*(n+1)*n,0),Vec(n*n*(n+1),0)};
    s.water_fill.assign(n*n*n,0);
    return s;
}
void slab(int n,int axis,int sign) {
    auto s=blank(n);
    const double dx=.1,dt=.04,speed=sign*.1;
    std::fill(s.staggered_velocity[axis].begin(),
              s.staggered_velocity[axis].end(),speed);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i) {
        const int a=axis==0?i:axis==1?j:k;
        if(a>=2&&a<=4)s.water_fill[cell(i,j,k,n)]=1;
    }
    const auto start=s;
    const double volume=3.0*n*n*dx*dx*dx;
    double max_error=0,divergence=0;
    for(int step=1;step<=3;++step) {
        const auto result=upc::p2_nonlinear_mac_plic_step3d(
            s,n,n,n,dx,dt,1000);
        s=result.state;
        near(result.transport.initial_volume_m3,volume,1e-11,
             "slab initial volume");
        near(result.transport.final_volume_m3,volume,1e-11,
             "slab final volume");
        near(result.transport.mass_residual_m3,0,1e-11,
             "slab mass closure");
        near(result.momentum_predictor_courant,.04,1e-12,
             "slab momentum CFL");
        check(result.transport.substeps==1,"slab one geometric step");
        check(result.projection.pcg_iterations==0,
              "uniform velocity needs no pressure correction");
        divergence=std::max(divergence,
            result.projection.max_divergence_s_inv);
        for(int k=0;k<n;++k)for(int j=0;j<n;++j)
        for(int i=0;i<n;++i) {
            const int a=axis==0?i:axis==1?j:k;
            const double shift=.04*step;
            double expected=a>=2&&a<=4?1.0:0.0;
            if(sign>0) {
                if(a==2)expected=1-shift;
                if(a==5)expected=shift;
            }else {
                if(a==4)expected=1-shift;
                if(a==1)expected=shift;
            }
            const double actual=s.water_fill[cell(i,j,k,n)];
            max_error=std::max(max_error,std::abs(actual-expected));
            near(actual,expected,2e-9,"independent slab translation");
            check(actual>=0&&actual<=1,"slab fill bounds");
        }
        for(int c=0;c<3;++c)
            for(double v:s.staggered_velocity[c])
                near(v,c==axis?speed:0,2e-12,
                     "constant MAC velocity preserved");
    }
    check(start.water_fill!=s.water_fill,"actual water displacement");
    std::cout<<"SLAB "<<n<<" "<<axis<<" "<<sign<<" "<<volume
             <<" "<<max_error<<" "<<divergence<<" 3\n";
}
void vortex(int n) {
    auto s=blank(n);
    const double dx=.1,dt=.02;
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i)
        s.staggered_velocity[0][face(0,i,j,k,n)]=
            .02+.012*std::sin(2*pi*j/n)+.008*std::cos(2*pi*k/n);
    for(int k=0;k<n;++k)for(int j=0;j<=n;++j)
    for(int i=0;i<n;++i)
        s.staggered_velocity[1][face(1,i,j,k,n)]=
            .01+.009*std::sin(2*pi*k/n)+.006*std::cos(2*pi*i/n);
    for(int k=0;k<=n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i)
        s.staggered_velocity[2][face(2,i,j,k,n)]=
            -.01+.007*std::sin(2*pi*i/n)+.005*std::cos(2*pi*j/n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i)
        s.water_fill[cell(i,j,k,n)]=(i>=2&&i<5)?1.0:0.0;
    const auto initial=s;
    const double volume=3.0*n*n*dx*dx*dx;
    double max_div=0,max_mass=0,max_parity=0,max_before=0;
    int total_pcg=0;
    for(int step=1;step<=3;++step) {
        const auto manual=upc::P2PeriodicMacProjection3D::self_advect_project(
            n,n,n,dx,dt,1000,s.staggered_velocity[0],
            s.staggered_velocity[1],s.staggered_velocity[2]);
        const auto got=upc::p2_nonlinear_mac_plic_step3d(
            s,n,n,n,dx,dt,1000,.4,64);
        for(int c=0;c<3;++c) {
            const auto& actual=got.state.staggered_velocity[c];
            const auto& expected=c==0?manual.projected.u_m_s:
                                 c==1?manual.projected.v_m_s:
                                      manual.projected.w_m_s;
            check(actual.size()==expected.size(),"MAC output shape");
            for(std::size_t i=0;i<actual.size();++i)
                near(actual[i],expected[i],1e-13,
                     "nonlinear predictor/projection parity");
            // Periodic momentum component integral (numerical, not physical FSI).
            const auto sum_unique=[&](const Vec& v) {
                double total=0;
                for(int k=0;k<n;++k)for(int j=0;j<n;++j)
                for(int i=0;i<n;++i)total+=v[face(c,i,j,k,n)];
                return total;
            };
            near(sum_unique(actual),sum_unique(s.staggered_velocity[c]),
                 1e-9,"periodic component sum");
        }
        std::array<Vec,3> positive{Vec(n*n*n),Vec(n*n*n),Vec(n*n*n)};
        for(int k=0;k<n;++k)for(int j=0;j<n;++j)
        for(int i=0;i<n;++i) {
            const auto id=cell(i,j,k,n);
            positive[0][id]=manual.projected.u_m_s[face(0,i+1,j,k,n)];
            positive[1][id]=manual.projected.v_m_s[face(1,i,j+1,k,n)];
            positive[2][id]=manual.projected.w_m_s[face(2,i,j,k+1,n)];
        }
        const auto reference=upc::p2_plic_adaptive_subcycle3d(
            s.water_fill,positive,n,n,n,dx,dt,.4,64);
        for(std::size_t i=0;i<reference.water_fill.size();++i) {
            const double a=got.state.water_fill[i];
            const double b=reference.water_fill[i];
            max_parity=std::max(max_parity,std::abs(a-b));
            near(a,b,1e-13,"manual geometric PLIC composition");
            check(a>=0&&a<=1,"vortex bounded fill");
        }
        near(got.transport.final_volume_m3,volume,1e-10,
             "vortex water mass");
        max_mass=std::max(max_mass,std::abs(
            got.transport.final_volume_m3-volume));
        max_div=std::max(max_div,got.projection.max_divergence_s_inv);
        max_before=std::max(max_before,
            got.projection.initial_max_divergence_s_inv);
        total_pcg+=got.projection.pcg_iterations;
        check(got.transport.substeps>=1&&got.transport.substeps<=64,
              "bounded PLIC subcycles");
        s=got.state;
    }
    check(total_pcg>0&&max_before>1e-7,
          "nonlinear predictor actually needed projection");
    check(max_div<1e-10,"nonlinear projected divergence");
    check(s.water_fill!=initial.water_fill,"nonlinear water actually moved");
    std::cout<<"VORTEX "<<n<<" "<<max_before<<" "<<max_div<<" "
             <<max_mass<<" "<<max_parity<<" "<<total_pcg<<" 3\n";
}
void negative() {
    auto s=blank(8);
    std::fill(s.staggered_velocity[0].begin(),
              s.staggered_velocity[0].end(),.1);
    s.water_fill.assign(8*8*8,.5);
    const auto saved=s;
    auto fail=[&](const State& q,double dt,double rho,double target,
                  std::size_t budget) {
        bool rejected=false;
        try{(void)upc::p2_nonlinear_mac_plic_step3d(
            q,8,8,8,.1,dt,rho,target,budget);}
        catch(const std::exception&){rejected=true;}
        check(rejected,"invalid input rejected atomically");
    };
    fail(s,0,1000,.4,64);
    fail(s,.02,0,.4,64);
    fail(s,.02,1000,0,64);
    fail(s,.02,1000,.4,0);
    fail(s,20,1000,.4,64);
    auto bad=s;bad.water_fill[0]=1.01;
    fail(bad,.02,1000,.4,64);
    bad=s;bad.water_fill[0]=std::numeric_limits<double>::quiet_NaN();
    fail(bad,.02,1000,.4,64);
    bad=s;bad.staggered_velocity[0].pop_back();
    fail(bad,.02,1000,.4,64);
    bad=s;bad.staggered_velocity[0][face(0,8,0,0,8)]+=.01;
    fail(bad,.02,1000,.4,64);
    check(s.water_fill==saved.water_fill&&
          s.staggered_velocity==saved.staggered_velocity,
          "caller input untouched after all failures");
    std::cout<<"NEGATIVE 9\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        for(int n:{8,12})for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})slab(n,axis,sign);
        for(int n:{8,12})vortex(n);
        negative();
        std::cout<<"PASS: nonlinear MAC PLIC assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: nonlinear MAC PLIC assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
