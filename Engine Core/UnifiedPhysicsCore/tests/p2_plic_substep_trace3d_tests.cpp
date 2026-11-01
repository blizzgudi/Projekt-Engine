// P2 PARTIAL: trace the real 16^3 translated ABC PLIC failure stage.
// The source state, pressure projection and geometry are never changed.
#include "upc/p2_plic_substep_trace3d.hpp"
#include "upc/p2_viscous_nonlinear_mac_plic3d.hpp"
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
void check(bool b,const char* why) {
    ++assertions;
    if(!b)throw std::runtime_error(why);
}
template<class F> void reject(F fn) {
    bool rejected=false;
    try {fn();}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"bad trace argument must fail");
}
using State=upc::P2NonlinearMacPlicState3D;
std::size_t face(int c,int i,int j,int k,int n) {
    if(c==0)return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
    if(c==1)return (static_cast<std::size_t>(k)*(n+1)+j)*n+i;
    return (static_cast<std::size_t>(k)*n+j)*n+i;
}
State abc(int n) {
    State s;
    for(int c=0;c<3;++c) {
        s.staggered_velocity[c].resize(
            static_cast<std::size_t>(n+(c==0))*(n+(c==1))*(n+(c==2)));
        for(int k=0;k<n+(c==2);++k)
        for(int j=0;j<n+(c==1);++j)
        for(int i=0;i<n+(c==0);++i) {
            const double x=2*pi*(i+(c==0?0.0:0.5))/n;
            const double y=2*pi*(j+(c==1?0.0:0.5))/n;
            const double z=2*pi*(k+(c==2?0.0:0.5))/n;
            const double mean[]={.25,-.15,.10};
            const double f[]={std::sin(z)+std::cos(y),
                              std::sin(x)+std::cos(z),
                              std::sin(y)+std::cos(x)};
            s.staggered_velocity[c][face(c,i,j,k,n)]=mean[c]+.05*f[c];
        }
    }
    s.water_fill.assign(static_cast<std::size_t>(n)*n*n,0);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=n/4;i<3*n/4;++i)
        s.water_fill[(k*n+j)*n+i]=1.0;
    return s;
}
std::array<std::vector<double>,3> projected_abc_faces(const State& initial,int n) {
    auto s=initial;
    const double dx=1.0/n,dt=.02,nu=.003;
    const double total_alpha=nu*dt/(dx*dx);
    const int steps=static_cast<int>(std::ceil(total_alpha*8));
    const double alpha=total_alpha/steps;
    for(int st=0;st<steps;++st) {
        auto next=s.staggered_velocity;
        for(int c=0;c<3;++c)
        for(int k=0;k<n;++k)for(int j=0;j<n;++j)
        for(int i=0;i<n;++i) {
            auto& v=s.staggered_velocity[c];
            next[c][face(c,i,j,k,n)]=v[face(c,i,j,k,n)]+alpha*(
                v[face(c,(i+1)%n,j,k,n)]+v[face(c,(i+n-1)%n,j,k,n)]+
                v[face(c,i,(j+1)%n,k,n)]+v[face(c,i,(j+n-1)%n,k,n)]+
                v[face(c,i,j,(k+1)%n,n)]+v[face(c,i,j,(k+n-1)%n,n)]-
                6*v[face(c,i,j,k,n)]);
        }
        for(int k=0;k<n;++k)for(int j=0;j<n;++j)
            next[0][face(0,n,j,k,n)]=next[0][face(0,0,j,k,n)];
        for(int k=0;k<n;++k)for(int i=0;i<n;++i)
            next[1][face(1,i,n,k,n)]=next[1][face(1,i,0,k,n)];
        for(int j=0;j<n;++j)for(int i=0;i<n;++i)
            next[2][face(2,i,j,n,n)]=next[2][face(2,i,j,0,n)];
        s.staggered_velocity=std::move(next);
    }
    auto r=upc::P2PeriodicMacProjection3D::self_advect_project(
        n,n,n,dx,dt,1000,
        s.staggered_velocity[0],s.staggered_velocity[1],
        s.staggered_velocity[2]);
    std::array<std::vector<double>,3> positive;
    for(auto& v:positive)v.resize(static_cast<std::size_t>(n)*n*n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i) {
        const auto q=(k*n+j)*n+i;
        positive[0][q]=r.projected.u_m_s[face(0,i+1,j,k,n)];
        positive[1][q]=r.projected.v_m_s[face(1,i,j+1,k,n)];
        positive[2][q]=r.projected.w_m_s[face(2,i,j,k+1,n)];
    }
    return positive;
}
void abc_probe(int n) {
    const auto start=abc(n), copy=start;
    auto positive=projected_abc_faces(start,n);
    const auto diagnostic=upc::p2_trace_periodic_plic_substeps3d(
        start.water_fill,positive,n,n,n,1.0/n,.02);
    if(n==16) {
        const auto cleaned=upc::p2_trace_periodic_plic_substeps3d(
            start.water_fill,positive,n,n,n,1.0/n,.02,.4,256,true);
        std::cout<<"TRACE_ROUNDOFF n="<<n
                 <<" completed="<<cleaned.completed
                 <<" passed="<<cleaned.completed_substeps
                 <<" snapped="<<cleaned.snapped_cells
                 <<" absolute_snap_m3="<<cleaned.absolute_snap_volume_m3
                 <<" reason="<<(cleaned.failure.empty()?"none":cleaned.failure)<<"\n";
        check(cleaned.completed,"endpoint-only cleanup must rescue 16cubed PLIC");
        const auto physical=upc::p2_viscous_nonlinear_mac_plic_step3d(
            start,n,n,n,1.0/n,.02,1000,.003,.4,256,256,true);
        check(physical.coupled.endpoint_snapped_cells==cleaned.snapped_cells,
              "actual nonlinear/viscous solver uses identical endpoint repair");
        check(std::abs(physical.coupled.endpoint_absolute_snap_volume_m3-
                       cleaned.absolute_snap_volume_m3)<1e-25,
              "actual solver and trace snap volume match");
        check(std::abs(physical.coupled.transport.final_volume_m3-0.5)<1e-10,
              "real 16cubed MAC/PLIC corrected water volume");
        check(physical.coupled.projection.max_divergence_s_inv<1e-9,
              "real 16cubed projected 3D velocity divergence");
        check(cleaned.snapped_cells>0 &&
              cleaned.absolute_snap_volume_m3<1e-12,
              "roundoff correction is explicit and tiny");
        check(std::abs(cleaned.last_volume_m3-cleaned.initial_volume_m3-
                       cleaned.signed_snap_volume_m3)<1e-10,
              "corrected trace volume ledger");
    }
    check(diagnostic.planned_substeps>=1,"planned ABC substeps");
    check(diagnostic.frames.size()==diagnostic.completed_substeps+1,
          "preflight recorded at every completed step");
    check(diagnostic.initial_volume_m3>0,"ABC water volume");
    check(start.water_fill==copy.water_fill &&
          start.staggered_velocity==copy.staggered_velocity,
          "ABC input unchanged");
    for(const auto& frame:diagnostic.frames) {
        check(std::abs(frame.water_volume_m3-diagnostic.initial_volume_m3)<1e-10,
              "stage water conservation");
        check(frame.preflight.partial_cells==
              frame.preflight.resolved_cells+frame.preflight.unresolved_cells,
              "stage classification partition");
        std::cout<<"TRACE_STAGE n="<<n<<" completed="<<frame.completed_substeps
                 <<" partial="<<frame.preflight.partial_cells
                 <<" geometric="<<frame.preflight.unresolved_geometric_cells
                 <<" endpoint="<<frame.preflight.unresolved_endpoint_roundoff_candidates
                 <<" volume="<<frame.water_volume_m3<<"\n";
    }
    if(diagnostic.completed) {
        auto full=upc::p2_viscous_nonlinear_mac_plic_step3d(
            start,n,n,n,1.0/n,.02,1000,.003);
        check(full.coupled.state.water_fill.size()==start.water_fill.size(),
              "baseline agreed on successful replay");
    } else {
        check(diagnostic.failed_substep==diagnostic.completed_substeps+1,
              "exact first failed substep");
        check(!diagnostic.failure.empty(),"explicit failure reason");
        bool original_rejected=false;
        try {
            (void)upc::p2_viscous_nonlinear_mac_plic_step3d(
                start,n,n,n,1.0/n,.02,1000,.003);
        }catch(const std::exception&){original_rejected=true;}
        check(original_rejected,"original 3D solver must reproduce failure");
    }
    std::cout<<"TRACE_ABC n="<<n<<" completed="<<diagnostic.completed
             <<" planned="<<diagnostic.planned_substeps
             <<" passed="<<diagnostic.completed_substeps
             <<" failed_substep="<<diagnostic.failed_substep
             <<" reason="<<(diagnostic.failure.empty()?"none":diagnostic.failure)
             <<"\n";
}
void synthetic() {
    const int n=16;
    std::vector<double> fill(n*n*n,0.0);
    std::array<std::vector<double>,3> v;
    for(auto& c:v)c.assign(fill.size(),0);
    const auto id=[&](int x,int y,int z){return (z*n+y)*n+x;};
    v[0].assign(fill.size(),.025);
    // A symmetric isolated half-cell is genuinely geometrically ambiguous.
    fill[id(7,8,9)]=.5;
    const auto untouched=fill;
    const auto a=upc::p2_trace_periodic_plic_substeps3d(
        fill,v,n,n,n,.1,.05);
    check(!a.completed&&a.failed_substep==1,"geometric failure traced");
    check(a.frames.front().preflight.unresolved_geometric_cells==1,
          "geometric ambiguity retained");
    check(fill==untouched,"no mutation on failure");
    bool still_rejected=false;
    try {
        (void)upc::p2_plic_endpoint_subcycle3d(fill,v,n,n,n,.1,.05);
    } catch(const std::invalid_argument&){still_rejected=true;}
    check(still_rejected,"real geometric ambiguity must remain rejected");
    std::fill(fill.begin(),fill.end(),1);
    fill[id(2,3,4)]=std::nextafter(1.0,0.0);
    const auto b=upc::p2_trace_periodic_plic_substeps3d(
        fill,v,n,n,n,.1,.05);
    check(!b.completed&&b.failed_substep==1,"endpoint failure traced");
    check(b.frames.front().preflight.unresolved_endpoint_roundoff_candidates==1,
          "endpoint rounding discriminated");
    std::fill(fill.begin(),fill.end(),1);
    const auto c=upc::p2_trace_periodic_plic_substeps3d(
        fill,v,n,n,n,.1,.05);
    check(c.completed&&c.completed_substeps==c.planned_substeps,
          "pure-water conservative replay");
    reject([&]{(void)upc::p2_trace_periodic_plic_substeps3d(
        fill,v,n,n,n,.1,.05,0);});
    reject([&]{(void)upc::p2_trace_periodic_plic_substeps3d(
        fill,v,n,n,n,.1,.05,.4,0);});
    auto bad=fill;bad[0]=-1;
    reject([&]{(void)upc::p2_trace_periodic_plic_substeps3d(
        bad,v,n,n,n,.1,.05);});
    std::cout<<"TRACE_SYNTHETIC geometric=1 endpoint=1 pure_water=PASS\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        synthetic();
        abc_probe(8);
        abc_probe(12);
        abc_probe(16);
        std::cout<<"PASS P2_PLIC_SUBSTEP_TRACE assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_PLIC_SUBSTEP_TRACE "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
