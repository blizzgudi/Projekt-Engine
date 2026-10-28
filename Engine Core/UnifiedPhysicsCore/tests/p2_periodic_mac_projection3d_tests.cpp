#include "upc/p2_periodic_mac_projection3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using Vec=std::vector<double>;
using Solver=upc::P2PeriodicMacProjection3D;
namespace {
std::size_t assertions=0;
void check(bool ok,const std::string& label) {
    ++assertions;
    if(!ok)throw std::runtime_error(label);
}
void near(double actual,double expected,double tolerance,const std::string& label) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tolerance,label);
}
std::size_t idx(int c,int i,int j,int k,int nx,int ny) {
    if(c==0)return (k*ny+j)*(nx+1)+i;
    if(c==1)return (k*(ny+1)+j)*nx+i;
    return (k*ny+j)*nx+i;
}
std::size_t cell(int i,int j,int k,int nx,int ny) {
    return (k*ny+j)*nx+i;
}
std::array<Vec,3> make(int nx,int ny,int nz) {
    constexpr double pi=3.1415926535897932384626433832795;
    Vec u((nx+1)*ny*nz),v(nx*(ny+1)*nz),w(nx*ny*(nz+1));
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<=nx;++i)
            u[idx(0,i,j,k,nx,ny)]=0.2+0.05*std::sin(2*pi*j/ny)
                                       +0.02*std::cos(2*pi*k/nz);
    for(int k=0;k<nz;++k)for(int j=0;j<=ny;++j)
        for(int i=0;i<nx;++i)
            v[idx(1,i,j,k,nx,ny)]=-0.1+0.04*std::sin(2*pi*k/nz)
                                        +0.03*std::cos(2*pi*i/nx);
    for(int k=0;k<=nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<nx;++i)
            w[idx(2,i,j,k,nx,ny)]=0.15+0.03*std::sin(2*pi*i/nx)
                                        +0.02*std::cos(2*pi*j/ny);
    return {u,v,w};
}
double component_sum(const Vec& q,int c,int nx,int ny,int nz) {
    double s=0;
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<nx;++i)s+=q[idx(c,i,j,k,nx,ny)];
    return s;
}
void manufactured(int nx,int ny,int nz) {
    constexpr double pi=3.1415926535897932384626433832795;
    constexpr double dt=0.4,rho=1000.0,dx=0.5;
    auto a=make(nx,ny,nz);
    Vec pressure(nx*ny*nz);
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<nx;++i)
            pressure[cell(i,j,k,nx,ny)]=
                100.0*std::cos(2*pi*i/nx)+
                40.0*std::cos(2*pi*j/ny)+
                20.0*std::cos(2*pi*k/nz);
    const auto p=[&](int i,int j,int k) {
        return pressure[cell((i+nx)%nx,(j+ny)%ny,(k+nz)%nz,nx,ny)];
    };
    const double factor=dt/(rho*dx);
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<nx;++i) {
            a[0][idx(0,i,j,k,nx,ny)]+=factor*(p(i,j,k)-p(i-1,j,k));
            a[1][idx(1,i,j,k,nx,ny)]+=factor*(p(i,j,k)-p(i,j-1,k));
            a[2][idx(2,i,j,k,nx,ny)]+=factor*(p(i,j,k)-p(i,j,k-1));
        }
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        a[0][idx(0,nx,j,k,nx,ny)]=a[0][idx(0,0,j,k,nx,ny)];
    for(int k=0;k<nz;++k)for(int i=0;i<nx;++i)
        a[1][idx(1,i,ny,k,nx,ny)]=a[1][idx(1,i,0,k,nx,ny)];
    for(int j=0;j<ny;++j)for(int i=0;i<nx;++i)
        a[2][idx(2,i,j,nz,nx,ny)]=a[2][idx(2,i,j,0,nx,ny)];
    const auto result=Solver::project(nx,ny,nz,dx,dt,rho,a[0],a[1],a[2]);
    check(result.pcg_iterations>0,"PCG nonzero");
    check(result.initial_max_divergence_s_inv>1e-5,"manufactured divergence");
    check(result.max_divergence_s_inv<1e-11,"manufactured projection divergence");
    double maxerr=0;
    const auto original=make(nx,ny,nz);
    const std::array<Vec,3> corrected={result.u_m_s,result.v_m_s,result.w_m_s};
    for(int c=0;c<3;++c)
        for(std::size_t i=0;i<corrected[c].size();++i) {
            near(corrected[c][i],original[c][i],1e-10,"gradient removed");
            maxerr=std::max(maxerr,std::abs(corrected[c][i]-original[c][i]));
        }
    for(std::size_t i=0;i<pressure.size();++i)
        near(result.pressure_pa[i],pressure[i],1e-7,"manufactured pressure recovered");
    const double p000=result.pressure_pa[cell(0,0,0,nx,ny)];
    const double px=result.pressure_pa[cell(nx/2,0,0,nx,ny)];
    const double py=result.pressure_pa[cell(0,ny/2,0,nx,ny)];
    const double pz=result.pressure_pa[cell(0,0,nz/2,nx,ny)];
    std::cout<<std::setprecision(17)<<"MODE "<<nx<<" "<<ny<<" "<<nz
             <<" "<<p000<<" "<<px<<" "<<py<<" "<<pz
             <<" "<<maxerr<<" "<<result.max_divergence_s_inv<<"\n";
}
void nonlinear(int nx,int ny,int nz) {
    auto a=make(nx,ny,nz);
    double largest_before=0,largest_after=0;
    for(int step=1;step<=4;++step) {
        const auto original=a;
        const auto result=Solver::self_advect_project(nx,ny,nz,1.0,0.4,1000.0,
                                                      a[0],a[1],a[2]);
        a={result.projected.u_m_s,result.projected.v_m_s,result.projected.w_m_s};
        check(result.max_outgoing_courant<1.0,"CFL");
        check(result.projected.max_divergence_s_inv<1e-10,"solenoidal step");
        check(result.projected.pcg_iterations>0,"nonlinear PCG used");
        largest_before=std::max(largest_before,result.projected.initial_max_divergence_s_inv);
        largest_after=std::max(largest_after,result.projected.max_divergence_s_inv);
        for(int c=0;c<3;++c)
            near(component_sum(a[c],c,nx,ny,nz),
                 component_sum(original[c],c,nx,ny,nz),2e-9,
                 "periodic component momentum sum");
        std::cout<<std::setprecision(17)<<"STEP "<<nx<<" "<<ny<<" "<<nz
                 <<" "<<step<<" "<<result.projected.initial_max_divergence_s_inv
                 <<" "<<result.projected.max_divergence_s_inv<<"\n";
    }
    check(largest_before>1e-6,"nonlinear transport produces divergence");
    check(largest_after<1e-10,"pressure removes divergence");
}
void negative() {
    auto a=make(8,10,12);
    const auto before=a;
    auto reject=[&](int nx,double dt,double rho,const std::array<Vec,3>& b) {
        bool failed=false;
        try{(void)Solver::project(nx,10,12,1.0,dt,rho,b[0],b[1],b[2]);}
        catch(const std::invalid_argument&){failed=true;}
        check(failed,"invalid input rejected");
    };
    reject(1,0.4,1000,a);
    reject(8,0.0,1000,a);
    reject(8,0.4,0.0,a);
    auto b=a;b[0].pop_back();reject(8,0.4,1000,b);
    b=a;b[1][0]=std::numeric_limits<double>::quiet_NaN();
    reject(8,0.4,1000,b);
    b=a;b[2][idx(2,0,0,12,8,10)]+=0.1;
    reject(8,0.4,1000,b);
    check(a==before,"input rollback");
    const auto identity=Solver::project(8,10,12,1.0,0.4,1000,
                                        a[0],a[1],a[2]);
    check(identity.pcg_iterations==0,"solenoidal identity projection");
    for(int c=0;c<3;++c) {
        const std::array<Vec,3> q={identity.u_m_s,identity.v_m_s,identity.w_m_s};
        for(std::size_t i=0;i<q[c].size();++i)
            near(q[c][i],a[c][i],1e-12,"identity velocity");
    }
}
}
int main() {
    try {
        for(int n:{8,12,16}) {
            manufactured(n,n+2,n+4);
            nonlinear(n,n+2,n+4);
        }
        negative();
        std::cout<<"PASS: "<<assertions
                 <<" native periodic projected MAC assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
