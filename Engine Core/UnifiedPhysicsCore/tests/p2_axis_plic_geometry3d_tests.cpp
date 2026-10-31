#include "upc/p2_axis_plic_geometry3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using V3=std::array<double,3>;
using M3=std::array<V3,3>;
int checks=0;
void require(bool ok,const char* name) {
    ++checks;
    if(!ok)throw std::runtime_error(name);
}
void near(double got,double expected,double tol,const char* name) {
    ++checks;
    if(!std::isfinite(got)||std::abs(got-expected)>tol)
        throw std::runtime_error(std::string(name)+": got="+
            std::to_string(got)+" expected="+std::to_string(expected));
}
struct Ref {
    double mass=0;
    V3 com{};
    M3 origin{},about_com{};
};
Ref gauss_reference(
    const std::vector<double>& f,const std::vector<std::uint8_t>& ax,
    const std::vector<std::int8_t>& side,int n,double dx,double rho,V3 origin) {
    // Independent 2x2x2 Gauss-Legendre integration of x_i*x_j over each
    // rectangular liquid prism. Exact for the quadratic inertia integrand.
    Ref r;
    V3 first{};
    constexpr double q=0.577350269189625764509148780501957;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=static_cast<std::size_t>((z*n+y)*n+x);
        if(f[i]==0)continue;
        V3 a{dx*x,dx*y,dx*z},b{dx*(x+1),dx*(y+1),dx*(z+1)};
        if(f[i]<1) {
            const auto k=ax[i];
            if(side[i]>0)b[k]=a[k]+dx*f[i];
            else a[k]=b[k]-dx*f[i];
        }
        const double dm=rho*(b[0]-a[0])*(b[1]-a[1])*(b[2]-a[2])/8.0;
        for(int iz=-1;iz<=1;iz+=2)
        for(int iy=-1;iy<=1;iy+=2)
        for(int ix=-1;ix<=1;ix+=2) {
            const int signs[3]={ix,iy,iz};
            V3 p{},arm{};
            for(int k=0;k<3;++k) {
                p[k]=(a[k]+b[k])*0.5+signs[k]*(b[k]-a[k])*q*0.5;
                arm[k]=p[k]-origin[k];
                first[k]+=dm*p[k];
            }
            const double r2=arm[0]*arm[0]+arm[1]*arm[1]+arm[2]*arm[2];
            for(int k=0;k<3;++k)for(int j=0;j<3;++j)
                r.origin[k][j]+=dm*((k==j?r2:0.0)-arm[k]*arm[j]);
            r.mass+=dm;
        }
    }
    if(r.mass>0)
        for(int k=0;k<3;++k)r.com[k]=first[k]/r.mass;
    else r.com=origin;
    V3 rc{};
    double rc2=0;
    for(int k=0;k<3;++k) {
        rc[k]=r.com[k]-origin[k];
        rc2+=rc[k]*rc[k];
    }
    for(int k=0;k<3;++k)for(int j=0;j<3;++j)
        r.about_com[k][j]=r.origin[k][j]-
            r.mass*((k==j?rc2:0.0)-rc[k]*rc[j]);
    return r;
}
void half_cell() {
    const double dx=.2,rho=1000;
    std::vector<double> fill{.5};
    std::vector<std::uint8_t> solid{0},axis{0};
    std::vector<std::int8_t> sign{1};
    auto r=upc::p2_axis_plic_geometry3d(
        fill,solid,axis,sign,1,1,1,dx,rho);
    near(r.mass_kg,4,1e-13,"half mass");
    near(r.centre_of_mass_m[0],.05,1e-14,"half centroid x");
    near(r.centre_of_mass_m[1],.1,1e-14,"half centroid y");
    near(r.inertia_about_com_kg_m2[0][0],4*(.04+.04)/12,1e-14,
         "half intrinsic x");
    near(r.inertia_about_com_kg_m2[1][1],4*(.01+.04)/12,1e-14,
         "half intrinsic y");
    near(r.inertia_about_com_kg_m2[2][2],4*(.01+.04)/12,1e-14,
         "half intrinsic z");
    require(r.partial_cells==1&&r.occupied_cells==1,"half counts");
    sign[0]=-1;
    auto reversed=upc::p2_axis_plic_geometry3d(
        fill,solid,axis,sign,1,1,1,dx,rho);
    near(reversed.centre_of_mass_m[0],.15,1e-14,"reversed centroid");
    for(int k=0;k<3;++k)
        near(reversed.inertia_about_com_kg_m2[k][k],
             r.inertia_about_com_kg_m2[k][k],1e-14,"reversed inertia");
    for(std::uint8_t a=1;a<3;++a) {
        axis[0]=a;sign[0]=1;
        auto t=upc::p2_axis_plic_geometry3d(
            fill,solid,axis,sign,1,1,1,dx,rho);
        near(t.centre_of_mass_m[a],.05,1e-14,"axis permutation centroid");
        near(t.inertia_about_com_kg_m2[a][a],4*(.04+.04)/12,
             1e-14,"axis permutation inertia");
    }
    std::cout<<"half,1,"<<r.mass_kg<<","<<r.centre_of_mass_m[0]
             <<","<<r.inertia_about_com_kg_m2[0][0]<<","
             <<r.inertia_about_com_kg_m2[0][1]<<"\n";
}
void cube(int n) {
    const double side=.4,dx=side/n,rho=1000;
    const auto count=static_cast<std::size_t>(n*n*n);
    std::vector<double> fill(count,1);
    std::vector<std::uint8_t> solid(count,0),axis(count,0);
    std::vector<std::int8_t> sign(count,1);
    auto r=upc::p2_axis_plic_geometry3d(
        fill,solid,axis,sign,n,n,n,dx,rho);
    const double m=rho*side*side*side;
    near(r.mass_kg,m,2e-11,"cube mass");
    require(r.partial_cells==0&&r.occupied_cells==count,"cube counts");
    for(int k=0;k<3;++k) {
        near(r.centre_of_mass_m[k],side/2,2e-13,"cube centre");
        near(r.inertia_about_com_kg_m2[k][k],m*side*side/6,
             2e-11,"cube inertia");
        for(int j=0;j<3;++j)if(k!=j)
            near(r.inertia_about_com_kg_m2[k][j],0,2e-11,
                 "cube offdiagonal");
    }
    std::cout<<"cube,"<<n<<","<<r.mass_kg<<","
             <<r.centre_of_mass_m[0]<<","
             <<r.inertia_about_com_kg_m2[0][0]<<","
             <<r.inertia_about_com_kg_m2[0][1]<<"\n";
}
void partial(int n) {
    const double dx=.025,rho=1000;
    const V3 origin{-.11,.04,.07};
    const auto count=static_cast<std::size_t>(n*n*n);
    std::vector<double> fill(count,0);
    std::vector<std::uint8_t> solid(count,0),axis(count,0);
    std::vector<std::int8_t> sign(count,1);
    for(int z=1;z<n-1;++z)for(int y=1;y<n-1;++y)
    for(int x=1;x<n-1;++x) {
        const auto i=static_cast<std::size_t>((z*n+y)*n+x);
        if((x+2*y+3*z)%7==0) {solid[i]=1;continue;}
        fill[i]=((3*x+y+z)%4+1)*.25;
        axis[i]=static_cast<std::uint8_t>((x+2*y+z)%3);
        sign[i]=((x+y+z)%2)?1:-1;
    }
    const auto r=upc::p2_axis_plic_geometry3d(
        fill,solid,axis,sign,n,n,n,dx,rho,origin);
    const auto q=gauss_reference(fill,axis,sign,n,dx,rho,origin);
    near(r.mass_kg,q.mass,1e-11,"quadrature mass");
    require(r.partial_cells>0&&r.occupied_cells>0,"partial occupied");
    for(int k=0;k<3;++k) {
        near(r.centre_of_mass_m[k],q.com[k],3e-13,"quadrature COM");
        for(int j=0;j<3;++j) {
            near(r.inertia_about_origin_kg_m2[k][j],
                 q.origin[k][j],3e-11,"quadrature origin inertia");
            near(r.inertia_about_com_kg_m2[k][j],
                 q.about_com[k][j],3e-11,"quadrature COM inertia");
        }
    }
    const V3 shifted{.21,-.07,.12};
    const auto moved=upc::p2_axis_plic_geometry3d(
        fill,solid,axis,sign,n,n,n,dx,rho,shifted);
    for(int k=0;k<3;++k) {
        near(moved.centre_of_mass_m[k],r.centre_of_mass_m[k],
             3e-13,"origin invariant centroid");
        for(int j=0;j<3;++j)
            near(moved.inertia_about_com_kg_m2[k][j],
                 r.inertia_about_com_kg_m2[k][j],
                 3e-11,"origin invariant COM inertia");
    }
    std::cout<<"partial,"<<n<<","<<r.mass_kg<<","
             <<r.centre_of_mass_m[0]<<","
             <<r.inertia_about_com_kg_m2[0][0]<<","
             <<r.inertia_about_com_kg_m2[0][1]<<"\n";
}
void invalid() {
    std::vector<double> f{.5};
    std::vector<std::uint8_t> s{0},a{0};
    std::vector<std::int8_t> sign{1};
    auto rejects=[&](const char* why) {
        bool caught=false;
        try{(void)upc::p2_axis_plic_geometry3d(
            f,s,a,sign,1,1,1,.2,1000);}
        catch(const std::invalid_argument&){caught=true;}
        require(caught,why);
    };
    f[0]=1.1;rejects("overfull");
    f[0]=-.01;rejects("negative");
    f[0]=std::numeric_limits<double>::quiet_NaN();rejects("NaN");
    f[0]=.5;s[0]=1;rejects("solid overlap");
    s[0]=0;a[0]=3;rejects("invalid axis");
    a[0]=0;sign[0]=0;rejects("invalid side");
    sign[0]=1;f.clear();rejects("size mismatch");
    f.push_back(0);
    auto empty=upc::p2_axis_plic_geometry3d(
        f,s,a,sign,1,1,1,.2,1000,{.3,.4,.5});
    near(empty.mass_kg,0,0,"empty mass");
    for(int k=0;k<3;++k)
        near(empty.centre_of_mass_m[k],.3+.1*k,1e-14,
             "empty origin convention");
    bool caught=false;
    try{(void)upc::p2_axis_plic_geometry3d(
        f,s,a,sign,1,1,1,.2,1000,
        {0,std::numeric_limits<double>::infinity(),0});}
    catch(const std::invalid_argument&){caught=true;}
    require(caught,"invalid origin");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"case,grid,mass,com_x,Ixx,Ixy\n";
        half_cell();cube(8);cube(12);partial(8);partial(12);invalid();
        std::cout<<"PASS: axis PLIC geometric assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: axis PLIC geometric assertions="<<checks
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
