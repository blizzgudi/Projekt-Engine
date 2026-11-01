// P2 PARTIAL: exact uniform-phase invariance in periodic 3D MAC/PLIC.
// Full/empty 16^3 fields must not create unresolved one-ULP interfaces.
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
constexpr int n=16;
constexpr double pi=3.1415926535897932384626433832795;
std::size_t assertions=0;
void check(bool value,const char* message) {
    ++assertions;
    if(!value)throw std::runtime_error(message);
}
template<class F> void reject(F f,const char* message) {
    bool thrown=false;
    try {f();}catch(const std::invalid_argument&){thrown=true;}
    check(thrown,message);
}
std::size_t cell(int i,int j,int k) {return (static_cast<std::size_t>(k)*n+j)*n+i;}
std::size_t face(int a,int i,int j,int k) {
    if(a==0)return (static_cast<std::size_t>(k)*n+j)*(n+1)+i;
    if(a==1)return (static_cast<std::size_t>(k)*(n+1)+j)*n+i;
    return (static_cast<std::size_t>(k)*n+j)*n+i;
}
double velocity(int a,int i,int j,int k) {
    const double x=2*pi*(i+(a==0?0.0:0.5))/n;
    const double y=2*pi*(j+(a==1?0.0:0.5))/n;
    const double z=2*pi*(k+(a==2?0.0:0.5))/n;
    if(a==0)return .25+.05*(std::sin(z)+std::cos(y));
    if(a==1)return -.15+.05*(std::sin(x)+std::cos(z));
    return .10+.05*(std::sin(y)+std::cos(x));
}
using State=upc::P2NonlinearMacPlicState3D;
State initial(double fraction) {
    State state;
    state.water_fill.assign(n*n*n,fraction);
    for(int a=0;a<3;++a) {
        state.staggered_velocity[a].resize(
            static_cast<std::size_t>(n+(a==0))*(n+(a==1))*(n+(a==2)));
        for(int k=0;k<n+(a==2);++k)
        for(int j=0;j<n+(a==1);++j)
        for(int i=0;i<n+(a==0);++i)
            state.staggered_velocity[a][face(a,i,j,k)]=velocity(a,i,j,k);
    }
    return state;
}
std::array<std::vector<double>,3> positive_faces() {
    std::array<std::vector<double>,3> out;
    for(auto& v:out)v.resize(n*n*n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)for(int i=0;i<n;++i) {
        const auto id=cell(i,j,k);
        out[0][id]=velocity(0,i+1,j,k);
        out[1][id]=velocity(1,i,j+1,k);
        out[2][id]=velocity(2,i,j,k+1);
    }
    return out;
}
void test_pure(double fraction) {
    const auto state=initial(fraction);
    const auto faces=positive_faces();
    const auto transport=upc::p2_plic_unsplit_mac3d(
        state.water_fill,faces,n,n,n,1.0/n,.02);
    check(transport.water_fill==state.water_fill,"exact direct pure-phase invariance");
    check(transport.transported_faces==0,"no artificial geometric flux");
    check(transport.reconstructed_partial_cells==0,"no artificial interface");
    check(transport.mass_residual_m3==0,"direct mass conservation");
    check(transport.max_cell_face_courant_sum>0,"nontrivial MAC advection field");
    check(transport.max_divergence_s_inv<1e-12,"solenoidal ABC MAC faces");
    const auto result=upc::p2_roundoff_mac_plic_schedule3d(
        state,{{.02,.003},{.02,.003}},n,n,n,1.0/n,1000);
    check(result.intervals.size()==2,"two real coupled steps");
    check(result.total_viscous_substeps>=2,"real viscous steps");
    check(result.total_plic_substeps>=2,"real PLIC steps");
    check(result.state.water_fill==state.water_fill,"coupled pure-phase invariance");
    check(result.initial_volume_m3==fraction,"initial pure-phase volume");
    check(result.final_volume_m3==fraction,"final pure-phase volume");
    check(result.absolute_snap_m3==0,"no endpoint corrections");
    check(result.total_ledger_residual_m3==0,"exact coupled water ledger");
    check(result.max_divergence_s_inv<1e-9,"projected divergence bound");
    check(state.water_fill==initial(fraction).water_fill,"caller state unchanged");
    std::cout<<"PURE16 fill="<<fraction
             <<" volume="<<result.final_volume_m3
             <<" plic="<<result.total_plic_substeps
             <<" viscous="<<result.total_viscous_substeps
             <<" divergence="<<result.max_divergence_s_inv
             <<" snap="<<result.absolute_snap_m3
             <<" ledger="<<result.total_ledger_residual_m3<<"\n";
}
void test_rejections() {
    auto faces=positive_faces();
    const std::vector<double> full(n*n*n,1.0);
    faces[0][0]+=.02;
    reject([&]{(void)upc::p2_plic_unsplit_mac3d(full,faces,n,n,n,1.0/n,.02);},
           "pure-phase path must reject divergent MAC field");
    faces=positive_faces();
    reject([&]{(void)upc::p2_plic_unsplit_mac3d(full,faces,n,n,n,1.0/n,1.0);},
           "pure-phase path must reject CFL violation");
    std::vector<double> unresolved(n*n*n,.5);
    reject([&]{(void)upc::p2_plic_unsplit_mac3d(unresolved,faces,n,n,n,1.0/n,.02);},
           "uniform partial water must not bypass interface reconstruction");
    unresolved=full;
    unresolved[cell(8,8,8)]=.5;
    reject([&]{(void)upc::p2_plic_unsplit_mac3d(unresolved,faces,n,n,n,1.0/n,.02);},
           "isolated partial water must still fail closed");
    unresolved=full;
    unresolved[0]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{(void)upc::p2_plic_unsplit_mac3d(unresolved,faces,n,n,n,1.0/n,.02);},
           "nonfinite fraction must still fail closed");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        test_pure(1.0);
        test_pure(0.0);
        test_rejections();
        std::cout<<"PASS PURE16 assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL PURE16 "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}