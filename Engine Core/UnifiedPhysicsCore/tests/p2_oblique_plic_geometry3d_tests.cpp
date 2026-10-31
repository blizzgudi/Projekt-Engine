#include "upc/p2_oblique_plic_geometry3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using V=std::array<double,3>;
int assertions=0;
void check(bool pass,const char* name) {
    ++assertions;
    if(!pass)throw std::runtime_error(name);
}
void near(double got,double want,double tol,const char* name) {
    ++assertions;
    if(!std::isfinite(got)||std::abs(got-want)>tol)
        throw std::runtime_error(std::string(name)+" got="+
            std::to_string(got)+" want="+std::to_string(want));
}
auto cell(double f,V normal,double dx=.2,double rho=1000,
          V origin={0,0,0}) {
    return upc::p2_oblique_plic_geometry3d(
        {f},{0},{normal},1,1,1,dx,rho,origin);
}
void tetrahedron() {
    const double dx=.2,m=8.0/6.0;
    auto r=cell(1.0/6.0,{1,1,1});
    near(r.mass_kg,m,2e-13,"tetra mass");
    for(int a=0;a<3;++a) {
        near(r.centre_of_mass_m[a],dx/4,2e-13,"tetra COM");
        near(r.inertia_about_com_kg_m2[a][a],m*dx*dx*3.0/40,
             2e-13,"tetra inertia diagonal");
        for(int b=0;b<3;++b)if(a!=b)
            near(r.inertia_about_com_kg_m2[a][b],m*dx*dx/80,
                 2e-13,"tetra inertia offdiagonal");
    }
    check(r.partial_cells==1&&r.occupied_cells==1,"tetra count");
    auto reverse=cell(1.0/6.0,{-1,-1,-1});
    for(int a=0;a<3;++a) {
        near(reverse.centre_of_mass_m[a],3*dx/4,2e-13,"reverse COM");
        for(int b=0;b<3;++b)
            near(reverse.inertia_about_com_kg_m2[a][b],
                 r.inertia_about_com_kg_m2[a][b],2e-13,"reverse inertia");
    }
    std::cout<<"tetra,1,"<<r.mass_kg<<","<<r.centre_of_mass_m[0]
             <<","<<r.inertia_about_com_kg_m2[0][0]<<","
             <<r.inertia_about_com_kg_m2[0][1]<<"\n";
}
void prism() {
    const double dx=.2,m=4;
    auto r=cell(.5,{1,1,0});
    near(r.mass_kg,m,2e-13,"prism mass");
    near(r.centre_of_mass_m[0],dx/3,2e-13,"prism COM x");
    near(r.centre_of_mass_m[1],dx/3,2e-13,"prism COM y");
    near(r.centre_of_mass_m[2],dx/2,2e-13,"prism COM z");
    near(r.inertia_about_com_kg_m2[0][0],m*dx*dx*5.0/36,
         2e-13,"prism Ixx");
    near(r.inertia_about_com_kg_m2[2][2],m*dx*dx/9,
         2e-13,"prism Izz");
    near(r.inertia_about_com_kg_m2[0][1],m*dx*dx/36,
         2e-13,"prism Ixy");
    near(r.inertia_about_com_kg_m2[0][2],0,2e-13,"prism Ixz");
    std::cout<<"prism,1,"<<r.mass_kg<<","<<r.centre_of_mass_m[0]
             <<","<<r.inertia_about_com_kg_m2[0][0]<<","
             <<r.inertia_about_com_kg_m2[0][1]<<"\n";
}
void axis() {
    for(int a=0;a<3;++a)for(int side:{-1,1}) {
        V n{};n[a]=side;
        auto r=cell(.25,n);
        std::vector<double> fill{.25};
        std::vector<std::uint8_t> solid{0},ax{static_cast<std::uint8_t>(a)};
        std::vector<std::int8_t> sign{static_cast<std::int8_t>(side)};
        auto q=upc::p2_axis_plic_geometry3d(
            fill,solid,ax,sign,1,1,1,.2,1000);
        // Axis API side=+1 fills low end, whereas plane normal +a
        // keeps low end. Same convention.
        near(r.mass_kg,q.mass_kg,2e-13,"axis mass");
        for(int k=0;k<3;++k) {
            near(r.centre_of_mass_m[k],q.centre_of_mass_m[k],
                 2e-13,"axis COM");
            for(int j=0;j<3;++j)
                near(r.inertia_about_com_kg_m2[k][j],
                     q.inertia_about_com_kg_m2[k][j],2e-13,
                     "axis inertia");
        }
    }
}
void multi(int n) {
    const double dx=.025,rho=1000;
    const auto count=static_cast<std::size_t>(n*n*n);
    std::vector<double> f(count,0);
    std::vector<std::uint8_t> solid(count,0);
    std::vector<V> normals(count,V{0,0,0});
    double expected=0;
    int partial=0,occupied=0;
    for(int z=1;z<n-1;++z)for(int y=1;y<n-1;++y)
    for(int x=1;x<n-1;++x) {
        const auto i=static_cast<std::size_t>((z*n+y)*n+x);
        if((x+2*y+3*z)%11==0){solid[i]=1;continue;}
        f[i]=((x+3*y+5*z)%6+1)/7.0;
        normals[i]=V{static_cast<double>(x%3+1),
                     static_cast<double>(-y%3-1),
                     static_cast<double>(z%4+1)};
        expected+=f[i]*rho*dx*dx*dx;
        ++partial;++occupied;
    }
    V origin{-.1,.03,.11};
    auto r=upc::p2_oblique_plic_geometry3d(
        f,solid,normals,n,n,n,dx,rho,origin);
    near(r.mass_kg,expected,2e-11,"multi VOF mass");
    check(r.partial_cells==static_cast<std::size_t>(partial)&&
          r.occupied_cells==static_cast<std::size_t>(occupied),
          "multi counts");
    auto shifted=upc::p2_oblique_plic_geometry3d(
        f,solid,normals,n,n,n,dx,rho,{.23,-.08,.07});
    for(int a=0;a<3;++a) {
        near(shifted.centre_of_mass_m[a],r.centre_of_mass_m[a],
             2e-12,"multi invariant COM");
        check(r.inertia_about_com_kg_m2[a][a]>0,"multi positive inertia");
        for(int b=0;b<3;++b) {
            near(r.inertia_about_com_kg_m2[a][b],
                 r.inertia_about_com_kg_m2[b][a],1e-12,
                 "multi symmetric");
            near(shifted.inertia_about_com_kg_m2[a][b],
                 r.inertia_about_com_kg_m2[a][b],3e-11,
                 "multi Steiner invariant");
        }
    }
    std::cout<<"multi,"<<n<<","<<r.mass_kg<<","
             <<r.centre_of_mass_m[0]<<","
             <<r.inertia_about_com_kg_m2[0][0]<<","
             <<r.inertia_about_com_kg_m2[0][1]<<"\n";
}
void invalid() {
    std::vector<double> f{.5};
    std::vector<std::uint8_t> s{0};
    std::vector<V> n{V{1,1,1}};
    auto rejects=[&](const char* name){
        bool caught=false;
        try{(void)upc::p2_oblique_plic_geometry3d(
            f,s,n,1,1,1,.2,1000);}
        catch(const std::invalid_argument&){caught=true;}
        check(caught,name);
    };
    f[0]=1.1;rejects("overfill");
    f[0]=-.01;rejects("negative");
    f[0]=std::numeric_limits<double>::quiet_NaN();rejects("nan");
    f[0]=.5;s[0]=1;rejects("solid overlap");
    s[0]=0;n[0]={0,0,0};rejects("zero normal");
    n[0]={1,std::numeric_limits<double>::infinity(),0};
    rejects("infinite normal");
    n[0]={1,1,1};f.clear();rejects("bad length");
    f.push_back(0);
    auto empty=upc::p2_oblique_plic_geometry3d(
        f,s,n,1,1,1,.2,1000,{.3,.4,.5});
    near(empty.mass_kg,0,0,"empty");
    near(empty.centre_of_mass_m[0],.3,0,"empty COM");
    f[0]=1;n[0]={0,0,0};
    auto full=upc::p2_oblique_plic_geometry3d(
        f,s,n,1,1,1,.2,1000);
    near(full.mass_kg,8,1e-13,"full");
    near(full.inertia_about_com_kg_m2[0][0],
         8*.04/6,2e-13,"full inertia");
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"case,grid,mass,com_x,Ixx,Ixy\n";
        tetrahedron();prism();axis();multi(8);multi(12);invalid();
        std::cout<<"PASS: oblique PLIC assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: oblique PLIC assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
