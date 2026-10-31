#include "upc/p2_vof_affine_spin3d.hpp"
#include <algorithm>
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
using V=std::array<double,3>;
int checks=0;
void require(bool ok,const char* what) {
    ++checks;
    if(!ok)throw std::runtime_error(what);
}
void near(double a,double b,double tol,const char* what) {
    ++checks;
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tol)
        throw std::runtime_error(std::string(what)+" actual="+std::to_string(a)+
                                 " expected="+std::to_string(b));
}
V cross(V a,V b) {
    return {a[1]*b[2]-a[2]*b[1],
            a[2]*b[0]-a[0]*b[2],
            a[0]*b[1]-a[1]*b[0]};
}
V add(V a,V b) {return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
V sub(V a,V b) {return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V mul(V a,double s) {return {a[0]*s,a[1]*s,a[2]*s};}
V omega{.7,-1.1,1.3},translation{.5,-.2,.3};
V velocity(V p,V pivot) {return add(translation,cross(omega,sub(p,pivot)));}
void configure(upc::MacProjection3D& mac,int n,double dx,V pivot) {
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<=n;++x)
        mac.set_face_velocity_x(x,y,z,
            velocity({x*dx,(y+.5)*dx,(z+.5)*dx},pivot)[0]);
    for(int z=0;z<n;++z)for(int y=0;y<=n;++y)for(int x=0;x<n;++x)
        mac.set_face_velocity_y(x,y,z,
            velocity({(x+.5)*dx,y*dx,(z+.5)*dx},pivot)[1]);
    for(int z=0;z<=n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        mac.set_face_velocity_z(x,y,z,
            velocity({(x+.5)*dx,(y+.5)*dx,z*dx},pivot)[2]);
}
void cube(int n) {
    const double side=.4,dx=side/n,rho=1000;
    const V pivot{side/2,side/2,side/2},origin{.03,-.07,.1};
    upc::MacProjection3D mac(n,n,n,dx,rho);
    configure(mac,n,dx,pivot);
    std::vector<double> fill(n*n*n,1);
    std::vector<std::uint8_t> solid(fill.size(),0);
    const auto a=upc::vof_affine_spin_inventory3d(
        mac,fill,solid,n,n,n,dx,rho,origin);
    const auto point=upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho,origin);
    const double mass=rho*side*side*side;
    const double I=mass*side*side/6;
    const V expected_origin=add(mul(omega,I),
        cross(sub(pivot,origin),mul(translation,mass)));
    near(a.inventory.mass_kg,mass,2e-11,"cube mass");
    for(int k=0;k<3;++k) {
        near(a.inventory.centre_of_mass_m[k],pivot[k],2e-14,"cube COM");
        near(a.inventory.linear_momentum_kg_m_s[k],mass*translation[k],
             2e-11,"cube linear momentum");
        near(a.inventory.inertia_about_com_kg_m2[k][k],I,3e-12,
             "cube exact continuum inertia");
        near(a.intrinsic_spin_kg_m2_s[k],mass*dx*dx/6*omega[k],3e-13,
             "cube intrinsic spin");
        near(a.inventory.angular_momentum_kg_m2_s[k],expected_origin[k],
             2e-11,"cube angular momentum about origin");
        near(a.angular_momentum_about_com_kg_m2_s[k],I*omega[k],2e-11,
             "cube angular momentum about COM");
        near(a.inventory.angular_momentum_kg_m2_s[k]-
             point.angular_momentum_kg_m2_s[k],
             a.intrinsic_spin_kg_m2_s[k],2e-13,"spin correction isolated");
    }
    std::cout<<"cube,"<<n<<","<<mass<<","<<I<<","
             <<a.angular_momentum_about_com_kg_m2_s[2]<<"\n";
}
void asymmetric(int n) {
    const double dx=.025,rho=1000;
    const V pivot{.13,.17,.08},origin{-.04,.02,.03};
    upc::MacProjection3D mac(n,n,n,dx,rho);
    configure(mac,n,dx,pivot);
    std::vector<double> fill(n*n*n,0);
    std::vector<std::uint8_t> solid(fill.size(),0);
    for(int z=1;z<n-1;++z)for(int y=1;y<n-1;++y)for(int x=1;x<n-1;++x)
        if((3*x+5*y+7*z)%4!=0)
            fill[(z*n+y)*n+x]=((x+2*y+z)%3+1)*.25;
    const auto a=upc::vof_affine_spin_inventory3d(
        mac,fill,solid,n,n,n,dx,rho,origin);
    // Independent 2x2x2 Gauss-Legendre volume integration of exact affine
    // velocity, including partial cell density and arbitrary origin.
    const double q=dx/(2*std::sqrt(3.0));
    double mass=0;
    V first{},momentum{},angular{};
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const double f=fill[(z*n+y)*n+x];
        if(f==0)continue;
        const double dm=f*rho*dx*dx*dx/8;
        for(int iz=-1;iz<=1;iz+=2)
        for(int iy=-1;iy<=1;iy+=2)
        for(int ix=-1;ix<=1;ix+=2) {
            const V pos{(x+.5)*dx+ix*q,(y+.5)*dx+iy*q,
                        (z+.5)*dx+iz*q};
            const V vel=velocity(pos,pivot);
            const V arm=sub(pos,origin);
            const V torque=cross(arm,vel);
            mass+=dm;
            for(int k=0;k<3;++k) {
                first[k]+=dm*pos[k];
                momentum[k]+=dm*vel[k];
                angular[k]+=dm*torque[k];
            }
        }
    }
    const V com=mul(first,1/mass);
    const V comL=sub(angular,cross(sub(com,origin),momentum));
    near(a.inventory.mass_kg,mass,2e-11,"partial mass quadrature");
    for(int k=0;k<3;++k) {
        near(a.inventory.centre_of_mass_m[k],com[k],2e-13,
             "partial COM quadrature");
        near(a.inventory.linear_momentum_kg_m_s[k],momentum[k],2e-11,
             "partial momentum quadrature");
        near(a.inventory.angular_momentum_kg_m2_s[k],angular[k],2e-11,
             "partial angular quadrature");
        near(a.angular_momentum_about_com_kg_m2_s[k],comL[k],2e-11,
             "partial COM angular quadrature");
    }
    auto shifted=upc::vof_affine_spin_inventory3d(
        mac,fill,solid,n,n,n,dx,rho,{0,0,0});
    for(int k=0;k<3;++k) {
        near(shifted.intrinsic_spin_kg_m2_s[k],a.intrinsic_spin_kg_m2_s[k],
             2e-13,"spin origin invariance");
        near(shifted.angular_momentum_about_com_kg_m2_s[k],
             a.angular_momentum_about_com_kg_m2_s[k],2e-11,
             "COM angular origin invariance");
    }
    std::cout<<"partial,"<<n<<","<<mass<<",0,"
             <<a.angular_momentum_about_com_kg_m2_s[2]<<"\n";
}
void negatives() {
    const int n=3;
    upc::MacProjection3D mac(n,n,n,.1,1000);
    std::vector<double> fill(n*n*n,0);
    std::vector<std::uint8_t> solid(fill.size(),0);
    auto empty=upc::vof_affine_spin_inventory3d(
        mac,fill,solid,n,n,n,.1,1000);
    near(empty.inventory.mass_kg,0,0,"empty mass");
    for(double v:empty.intrinsic_spin_kg_m2_s)
        near(v,0,0,"empty spin");
    auto rejects=[&](const char* label) {
        bool threw=false;
        try{(void)upc::vof_affine_spin_inventory3d(
            mac,fill,solid,n,n,n,.1,1000);}
        catch(const std::invalid_argument&){threw=true;}
        require(threw,label);
    };
    fill[0]=1.1;rejects("overfull fraction rejected");
    fill[0]=-.01;rejects("negative fraction rejected");
    fill[0]=1;solid[0]=1;rejects("solid overlap rejected");
    solid[0]=0;fill[0]=0;
    bool threw=false;
    try{(void)upc::vof_affine_spin_inventory3d(
        mac,fill,solid,n,n,n,.1,1000,
        {0,std::numeric_limits<double>::infinity(),0});}
    catch(const std::invalid_argument&){threw=true;}
    require(threw,"nonfinite origin rejected");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"case,grid,mass,Icom,Lcom_z\n";
        cube(8);cube(12);cube(16);
        asymmetric(8);asymmetric(12);
        negatives();
        std::cout<<"PASS: affine MAC subcell spin assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: affine MAC subcell spin assertions="<<checks
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
