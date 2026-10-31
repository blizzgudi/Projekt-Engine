#include "upc/p2_projected_plic_schedule3d.hpp"
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
using Vec=std::vector<double>;
using Fields=std::array<Vec,3>;
using Segment=upc::P2ProjectedPlicSegment3D;
std::size_t checks=0;
constexpr int N=8;
constexpr double DX=.1;
constexpr double PI=3.14159265358979323846;
void require(bool b,const char* what) {
    ++checks;
    if(!b)throw std::runtime_error(what);
}
void near(double a,double b,double tol,const char* what) {
    require(std::isfinite(a)&&std::abs(a-b)<=tol,what);
}
std::size_t cell(int i,int j,int k) {
    return (static_cast<std::size_t>(k)*N+j)*N+i;
}
std::size_t face(int a,int i,int j,int k) {
    if(a==0)return (static_cast<std::size_t>(k)*N+j)*(N+1)+i;
    if(a==1)return (static_cast<std::size_t>(k)*(N+1)+j)*N+i;
    return (static_cast<std::size_t>(k)*N+j)*N+i;
}
Fields constant(const std::array<double,3>& base) {
    return {Vec((N+1)*N*N,base[0]),
            Vec(N*(N+1)*N,base[1]),
            Vec(N*N*(N+1),base[2])};
}
Vec block() {
    Vec f(N*N*N,0.0);
    for(int k=2;k<5;++k)for(int j=2;j<5;++j)
        for(int i=2;i<5;++i)f[cell(i,j,k)]=1.0;
    return f;
}
double phi(int i,int j,int k) {
    return .002*(std::cos(2*PI*i/N)+
                 .7*std::cos(2*PI*j/N)+
                 .4*std::cos(2*PI*k/N));
}
Fields gradient_perturbed(std::array<double,3> base) {
    auto a=constant(base);
    for(int k=0;k<N;++k)for(int j=0;j<N;++j)
    for(int i=0;i<N;++i) {
        a[0][face(0,i,j,k)]+=(phi(i,j,k)-phi(i-1,j,k))/DX;
        a[1][face(1,i,j,k)]+=(phi(i,j,k)-phi(i,j-1,k))/DX;
        a[2][face(2,i,j,k)]+=(phi(i,j,k)-phi(i,j,k-1))/DX;
    }
    for(int k=0;k<N;++k)for(int j=0;j<N;++j)
        a[0][face(0,N,j,k)]=a[0][face(0,0,j,k)];
    for(int k=0;k<N;++k)for(int i=0;i<N;++i)
        a[1][face(1,i,N,k)]=a[1][face(1,i,0,k)];
    for(int j=0;j<N;++j)for(int i=0;i<N;++i)
        a[2][face(2,i,j,N)]=a[2][face(2,i,j,0)];
    return a;
}
std::array<Vec,3> positive(const Fields& f) {
    std::array<Vec,3> out{Vec(N*N*N),Vec(N*N*N),Vec(N*N*N)};
    for(int k=0;k<N;++k)for(int j=0;j<N;++j)
    for(int i=0;i<N;++i) {
        const auto id=cell(i,j,k);
        out[0][id]=f[0][face(0,i+1,j,k)];
        out[1][id]=f[1][face(1,i,j+1,k)];
        out[2][id]=f[2][face(2,i,j,k+1)];
    }
    return out;
}
void compare(const Vec& a,const Vec& b,double tol) {
    require(a.size()==b.size(),"VOF dimensions");
    for(std::size_t i=0;i<a.size();++i) {
        near(a[i],b[i],tol,"independent reference VOF");
        require(a[i]>=0&&a[i]<=1,"bounded water fill");
    }
}
void manufactured_gradient() {
    const auto fill=block();
    const std::array<double,3> base1{.12,0,0};
    const std::array<double,3> base2{.08,0,0};
    const std::vector<Segment> s{
        {.05,gradient_perturbed(base1)},
        {.07,gradient_perturbed(base2)}};
    const auto diag=upc::P2PeriodicMacProjection3D::project(
        N,N,N,DX,.05,1000,s[0].staggered_velocity[0],
        s[0].staggered_velocity[1],s[0].staggered_velocity[2]);
    double max_pressure_error=0;
    for(int k=0;k<N;++k)for(int j=0;j<N;++j)
    for(int i=0;i<N;++i) {
        const auto c=cell(i,j,k);
        const double p_ref=1000.0/.05*phi(i,j,k);
        max_pressure_error=std::max(max_pressure_error,
            std::abs(diag.pressure_pa[c]-p_ref));
        near(diag.pressure_pa[c],p_ref,1e-8,
             "independent manufactured pressure");
        near(diag.u_m_s[face(0,i,j,k)],.12,1e-10,
             "manufactured gradient removed U");
        near(diag.v_m_s[face(1,i,j,k)],0,1e-10,
             "manufactured gradient removed V");
        near(diag.w_m_s[face(2,i,j,k)],0,1e-10,
             "manufactured gradient removed W");
    }
    const auto got=upc::p2_projected_plic_schedule3d(
        fill,s,N,N,N,DX,1000.0,.4);
    const std::vector<upc::P2PlicTemporalSegment3D> ref{
        {.05,positive(constant(base1))},
        {.07,positive(constant(base2))}};
    const auto expected=upc::p2_plic_temporal_schedule3d(
        fill,ref,N,N,N,DX,.4);
    compare(got.transport.water_fill,expected.water_fill,1e-10);
    require(got.projection_iterations.size()==2,"two projections");
    require(got.projection_iterations[0]>0&&
            got.projection_iterations[1]>0,"actual pressure solves");
    require(got.divergence_before_s_inv[0]>.01,"gradient has divergence");
    require(got.divergence_before_s_inv[1]>.01,"second gradient divergence");
    require(got.max_divergence_after_s_inv<1e-10,"solenoidal output");
    require(got.transport.substeps==expected.substeps,"substep parity");
    require(got.transport.transported_faces==expected.transported_faces,
            "face transport parity");
    near(got.transport.initial_volume_m3,.027,1e-12,"initial water volume");
    near(got.transport.final_volume_m3,.027,1e-12,"conserved water volume");
    near(got.transport.mass_residual_m3,0,1e-12,"mass closure");
    require(s[0].staggered_velocity==gradient_perturbed(base1),
            "input velocity immutable");
    std::cout<<"gradient,"<<got.max_divergence_before_s_inv<<","
             <<got.max_divergence_after_s_inv<<","
             <<got.transport.mass_residual_m3<<","
             <<got.transport.substeps<<","<<max_pressure_error<<"\n";
}
void solenoidal_shear() {
    const auto fill=block();
    auto f=constant({.04,0,0});
    for(int k=0;k<N;++k)for(int j=0;j<N;++j)
    for(int i=0;i<=N;++i)
        f[0][face(0,i,j,k)]+=.01*std::sin(2*PI*j/N);
    const auto got=upc::p2_projected_plic_schedule3d(
        fill,{{.08,f}},N,N,N,DX,1000,.4);
    const auto expected=upc::p2_plic_temporal_schedule3d(
        fill,{{.08,positive(f)}},N,N,N,DX,.4);
    compare(got.transport.water_fill,expected.water_fill,1e-10);
    require(got.max_divergence_before_s_inv<1e-12,"shear divergence before");
    require(got.max_divergence_after_s_inv<1e-12,"shear divergence after");
    near(got.transport.mass_residual_m3,0,1e-12,"shear mass");
    std::cout<<"shear,"<<got.max_divergence_before_s_inv<<","
             <<got.max_divergence_after_s_inv<<","
             <<got.transport.mass_residual_m3<<","
             <<got.transport.substeps<<",0\n";
}
void negative() {
    const auto fill=block();
    const auto base=constant({.04,0,0});
    const auto bad=[&](const std::vector<Segment>& s,
                       double density,double target,std::size_t budget){
        bool rejected=false;
        try{(void)upc::p2_projected_plic_schedule3d(
            fill,s,N,N,N,DX,density,target,budget);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"invalid input rejected");
    };
    bad({},1000,.4,1024);
    bad({{.1,base}},0,.4,1024);
    bad({{.1,base}},1000,0,1024);
    bad({{.1,base}},1000,.4,0);
    bad({{0,base}},1000,.4,1024);
    auto nonfinite=base;
    nonfinite[0][0]=std::numeric_limits<double>::quiet_NaN();
    bad({{.1,nonfinite}},1000,.4,1024);
    auto wrong=base;
    wrong[1].pop_back();
    bad({{.1,wrong}},1000,.4,1024);
    auto seam=base;
    seam[0][face(0,N,0,0)]+=.01;
    bad({{.1,seam}},1000,.4,1024);
    bad({{10,base}},1000,.4,1);
    require(fill==block(),"negative input fill unchanged");
}
}
int main(){
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"case,divergence_before,divergence_after,mass_residual,substeps,pressure_max_error\n";
        manufactured_gradient();
        solenoidal_shear();
        negative();
        std::cout<<"PASS: projected MAC PLIC assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: projected MAC PLIC assertions="<<checks
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
