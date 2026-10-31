// P2 bounded: independent 2x2x2 Gauss quadrature of uniform cubical cells.
#include "upc/p2_vof_uniform_cell_inertia3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int assertions=0;
void need(bool ok,const char* msg){++assertions;if(!ok)throw std::runtime_error(msg);}
void near(double got,double expected,double tol,const char* msg){
    need(std::isfinite(got)&&std::isfinite(expected)&&std::abs(got-expected)<=tol,msg);
}
template<class F>void rejects(F fn,const char* msg){
    bool caught=false;try{fn();}catch(const std::invalid_argument&){caught=true;}
    need(caught,msg);
}
using Tensor=std::array<std::array<double,3>,3>;
Tensor integrate(const std::vector<double>& fill,int n,double dx,double rho,
                 const std::array<double,3>& reference) {
    Tensor out{};
    const double h=dx/(2*std::sqrt(3.0));
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const double f=fill[(z*n+y)*n+x];
        if(f==0)continue;
        const double weight=f*rho*dx*dx*dx/8;
        for(int a:{-1,1})for(int b:{-1,1})for(int c:{-1,1}) {
            const std::array<double,3> r{
                (x+.5)*dx+a*h-reference[0],
                (y+.5)*dx+b*h-reference[1],
                (z+.5)*dx+c*h-reference[2]};
            const double norm=r[0]*r[0]+r[1]*r[1]+r[2]*r[2];
            for(int i=0;i<3;++i)for(int j=0;j<3;++j)
                out[i][j]+=weight*((i==j?norm:0)-r[i]*r[j]);
        }
    }
    return out;
}
void compare(const Tensor& actual,const Tensor& expected,double tol) {
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)
        near(actual[i][j],expected[i][j],tol,"quadrature tensor mismatch");
}
void asymmetric(int n) {
    constexpr double dx=.1,rho=1000;
    upc::MacProjection3D mac(n,n,n,dx,rho);
    std::vector<double> fill(n*n*n,0);
    std::vector<std::uint8_t> solid(n*n*n,0);
    const auto idx=[&](int x,int y,int z){return (z*n+y)*n+x;};
    fill[idx(2,2,2)]=.25;
    fill[idx(5,5,5)]=.75;
    solid[idx(4,4,4)]=1;
    mac.set_face_velocity_x(2,2,2,2);
    mac.set_face_velocity_x(3,2,2,2);
    mac.set_face_velocity_y(2,2,2,-1);
    mac.set_face_velocity_y(2,3,2,-1);
    const auto point=upc::vof_angular_inventory3d(mac,fill,solid,n,n,n,dx,rho);
    const auto finite=upc::vof_uniform_cell_inertia3d(mac,fill,solid,n,n,n,dx,rho);
    const double intrinsic=dx*dx/6;
    near(finite.mass_kg,1,1e-13,"one kg");
    for(int i=0;i<3;++i) {
        near(finite.inertia_about_com_kg_m2[i][i],
             point.inertia_about_com_kg_m2[i][i]+intrinsic,1e-13,
             "cell intrinsic inertia correction");
        near(finite.inertia_about_origin_kg_m2[i][i],
             point.inertia_about_origin_kg_m2[i][i]+intrinsic,1e-13,
             "origin intrinsic inertia correction");
        near(finite.linear_momentum_kg_m_s[i],point.linear_momentum_kg_m_s[i],0,
             "momentum must not change");
        near(finite.angular_momentum_kg_m2_s[i],point.angular_momentum_kg_m2_s[i],0,
             "angular momentum must not change");
        near(finite.centre_of_mass_m[i],point.centre_of_mass_m[i],0,
             "centre of mass must not change");
    }
    compare(finite.inertia_about_origin_kg_m2,
            integrate(fill,n,dx,rho,{0,0,0}),1e-12);
    compare(finite.inertia_about_com_kg_m2,
            integrate(fill,n,dx,rho,finite.centre_of_mass_m),1e-12);
    const std::array<double,3> origin{.1,-.2,.3};
    const auto shifted=upc::vof_uniform_cell_inertia3d(
        mac,fill,solid,n,n,n,dx,rho,origin);
    compare(shifted.inertia_about_origin_kg_m2,
            integrate(fill,n,dx,rho,origin),1e-12);
    compare(shifted.inertia_about_com_kg_m2,
            finite.inertia_about_com_kg_m2,1e-12);
    fill[idx(2,2,2)]=-1;
    rejects([&]{(void)upc::vof_uniform_cell_inertia3d(
        mac,fill,solid,n,n,n,dx,rho);},"invalid fill rejected");
    fill[idx(2,2,2)]=.25;
    rejects([&]{(void)upc::vof_uniform_cell_inertia3d(
        mac,fill,solid,n,n,n,dx,rho,
        {std::numeric_limits<double>::quiet_NaN(),0,0});},
        "invalid origin rejected");
    std::fill(fill.begin(),fill.end(),0);
    const auto empty=upc::vof_uniform_cell_inertia3d(
        mac,fill,solid,n,n,n,dx,rho);
    near(empty.inertia_about_com_kg_m2[0][0],0,0,"empty inertia");
    std::cout<<"asymmetric,"<<n<<","<<finite.mass_kg<<","
             <<finite.inertia_about_com_kg_m2[0][0]<<","
             <<finite.inertia_about_com_kg_m2[0][1]<<","
             <<point.inertia_about_com_kg_m2[0][0]<<","
             <<intrinsic<<"\n";
}
void cube(int q) {
    const int n=q+2;
    const double side=.2,dx=side/q,rho=1000;
    upc::MacProjection3D mac(n,n,n,dx,rho);
    std::vector<double> fill(n*n*n,0);
    std::vector<std::uint8_t> solid(n*n*n,0);
    for(int z=0;z<q;++z)for(int y=0;y<q;++y)for(int x=0;x<q;++x)
        fill[(z*n+y)*n+x]=1;
    const auto a=upc::vof_uniform_cell_inertia3d(
        mac,fill,solid,n,n,n,dx,rho);
    const auto point=upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho);
    const double mass=rho*side*side*side;
    const double expected=mass*side*side/6;
    near(a.mass_kg,mass,2e-12,"cube mass");
    for(int k=0;k<3;++k) {
        near(a.centre_of_mass_m[k],side/2,1e-13,"cube centroid");
        near(a.inertia_about_com_kg_m2[k][k],expected,2e-13,
             "continuum solid cube inertia");
        need(point.inertia_about_com_kg_m2[k][k]<expected,
             "point-mass inertia must omit intrinsic cell inertia");
        for(int j=0;j<3;++j)if(k!=j)
            near(a.inertia_about_com_kg_m2[k][j],0,2e-13,
                 "symmetric cube off diagonal");
    }
    compare(a.inertia_about_com_kg_m2,
            integrate(fill,n,dx,rho,a.centre_of_mass_m),2e-12);
    std::cout<<"cube,"<<q<<","<<a.mass_kg<<","
             <<a.inertia_about_com_kg_m2[0][0]<<","
             <<a.inertia_about_com_kg_m2[0][1]<<","
             <<point.inertia_about_com_kg_m2[0][0]<<","
             <<expected<<"\n";
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"case,grid,mass,Ixx,Ixy,point_Ixx,reference\n";
        asymmetric(8);asymmetric(12);
        cube(2);cube(4);cube(8);
        std::cout<<"PASS: uniform-cell VOF inertia assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: uniform-cell inertia assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
