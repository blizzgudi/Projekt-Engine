#include "upc/p2_periodic_mac_vof_energy3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>
using Vec=std::vector<double>;
using Solver=upc::P2PeriodicMacVofEnergy3D;
static int assertions=0;
static void check(bool v,const char* msg) {++assertions;if(!v)throw std::runtime_error(msg);}
static void near(double a,double b,double t,const char* msg) {
    check(std::isfinite(a)&&std::abs(a-b)<=t,msg);
}
static std::size_t face(int c,int i,int j,int k,int nx,int ny) {
    if(c==0)return (k*ny+j)*(nx+1)+i;
    if(c==1)return (k*(ny+1)+j)*nx+i;
    return (k*ny+j)*nx+i;
}
static std::array<Vec,3> shear(int nx,int ny,int nz) {
    constexpr double pi=3.14159265358979323846;
    std::array<Vec,3> a={Vec((nx+1)*ny*nz),Vec(nx*(ny+1)*nz),Vec(nx*ny*(nz+1))};
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)for(int i=0;i<=nx;++i)
        a[0][face(0,i,j,k,nx,ny)]=0.2+0.05*std::sin(2*pi*j/ny)+0.02*std::cos(2*pi*k/nz);
    for(int k=0;k<nz;++k)for(int j=0;j<=ny;++j)for(int i=0;i<nx;++i)
        a[1][face(1,i,j,k,nx,ny)]=-0.1+0.04*std::sin(2*pi*k/nz)+0.03*std::cos(2*pi*i/nx);
    for(int k=0;k<=nz;++k)for(int j=0;j<ny;++j)for(int i=0;i<nx;++i)
        a[2][face(2,i,j,k,nx,ny)]=0.15+0.03*std::sin(2*pi*i/nx)+0.02*std::cos(2*pi*j/ny);
    return a;
}
static void nonlinear(int nx,int ny,int nz) {
    auto a=shear(nx,ny,nz);
    Vec fraction(nx*ny*nz,0.5);
    const double mass=std::accumulate(fraction.begin(),fraction.end(),0.0);
    double max_identity=0,max_orthogonality=0,pressure_loss=0;
    for(int step=0;step<4;++step) {
        auto r=Solver::advance(nx,ny,nz,1.0,0.4,1000,a[0],a[1],a[2],fraction);
        const auto& e=r.ledger;
        auto reference=upc::P2PeriodicMacVof3D::advance(
            nx,ny,nz,1.0,0.4,1000,a[0],a[1],a[2],fraction);
        near(e.projected_kinetic_j,
             e.advected_kinetic_j+e.projection_change_j,2e-9,"energy delta");
        near(e.projection_change_j,
             e.pressure_work_j+e.correction_energy_j,2e-9,"pressure work");
        near(e.projected_gradient_orthogonality_j,0,2e-8,"projection orthogonality");
        near(e.max_pressure_gradient_velocity_residual_m_s,0,1e-10,"pressure gradient correction");
        check(e.projection_change_j<=2e-8,"projection energy nonincrease");
        check(e.advection_change_j<=2e-8,"upwind energy nonincrease");
        near(r.state.vof.final_fraction_sum,mass,1e-9,"passive mass");
        near(r.state.mac.projected.max_divergence_s_inv,0,1e-9,"projected divergence");
        for(int c=0;c<3;++c) {
            const auto& x=c==0?r.state.mac.projected.u_m_s:
                          c==1?r.state.mac.projected.v_m_s:r.state.mac.projected.w_m_s;
            const auto& y=c==0?reference.mac.projected.u_m_s:
                          c==1?reference.mac.projected.v_m_s:reference.mac.projected.w_m_s;
            check(x.size()==y.size(),"same MAC size");
            for(std::size_t i=0;i<x.size();++i)near(x[i],y[i],1e-13,"same MAC state");
        }
        for(std::size_t i=0;i<fraction.size();++i)
            near(r.state.vof.water_fraction[i],reference.vof.water_fraction[i],1e-13,"same VOF state");
        max_identity=std::max(max_identity,std::abs(e.pressure_identity_residual_j));
        max_orthogonality=std::max(max_orthogonality,std::abs(e.projected_gradient_orthogonality_j));
        pressure_loss+=-e.projection_change_j;
        a={r.state.mac.projected.u_m_s,r.state.mac.projected.v_m_s,
           r.state.mac.projected.w_m_s};
        fraction=r.state.vof.water_fraction;
    }
    std::cout<<std::setprecision(17)<<"ENERGY "<<nx<<" "<<ny<<" "<<nz<<" "
             <<mass<<" "<<max_identity<<" "<<max_orthogonality<<" "<<pressure_loss<<"\n";
}
static void manufactured(int nx,int ny,int nz) {
    constexpr double pi=3.14159265358979323846;
    const double dt=0.2,rho=1000,dx=1.0;
    Vec p(nx*ny*nz);
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)for(int i=0;i<nx;++i)
        p[(k*ny+j)*nx+i]=100*std::cos(2*pi*i/nx)+80*std::sin(2*pi*j/ny)
                         +60*std::cos(2*pi*k/nz);
    auto a=shear(nx,ny,nz);
    for(auto& field:a)std::fill(field.begin(),field.end(),0.0);
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)for(int i=0;i<nx;++i) {
        const auto at=[&](int x,int y,int z) {
            return p[((z+nz)%nz*ny+(y+ny)%ny)*nx+(x+nx)%nx];
        };
        a[0][face(0,i,j,k,nx,ny)]=dt/rho*(at(i,j,k)-at(i-1,j,k))/dx;
        a[1][face(1,i,j,k,nx,ny)]=dt/rho*(at(i,j,k)-at(i,j-1,k))/dx;
        a[2][face(2,i,j,k,nx,ny)]=dt/rho*(at(i,j,k)-at(i,j,k-1))/dx;
    }
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        a[0][face(0,nx,j,k,nx,ny)]=a[0][face(0,0,j,k,nx,ny)];
    for(int k=0;k<nz;++k)for(int i=0;i<nx;++i)
        a[1][face(1,i,ny,k,nx,ny)]=a[1][face(1,i,0,k,nx,ny)];
    for(int j=0;j<ny;++j)for(int i=0;i<nx;++i)
        a[2][face(2,i,j,nz,nx,ny)]=a[2][face(2,i,j,0,nx,ny)];
    auto projected=upc::P2PeriodicMacProjection3D::project(
        nx,ny,nz,dx,dt,rho,a[0],a[1],a[2]);
    auto e=Solver::audit(nx,ny,nz,dx,dt,rho,
        a[0],a[1],a[2],a[0],a[1],a[2],projected);
    near(e.projected_kinetic_j,0,1e-10,"pure gradient removed");
    near(e.projection_change_j,-e.advected_kinetic_j,1e-10,"gradient energy loss");
    near(e.pressure_work_j,-2*e.correction_energy_j,1e-10,"pressure work analytic");
    near(e.pressure_identity_residual_j,0,1e-10,"manufactured closure");
    auto corrupt=projected;
    corrupt.pressure_pa[0]+=10.0;
    bool caught=false;
    try {Solver::audit(nx,ny,nz,dx,dt,rho,
        a[0],a[1],a[2],a[0],a[1],a[2],corrupt);}
    catch(const std::runtime_error&){caught=true;}
    check(caught,"corrupted pressure gradient rejected");
    auto bad_seam=projected;
    bad_seam.u_m_s[face(0,nx,0,0,nx,ny)]+=0.1;
    caught=false;
    try {Solver::audit(nx,ny,nz,dx,dt,rho,
        a[0],a[1],a[2],a[0],a[1],a[2],bad_seam);}
    catch(const std::invalid_argument&){caught=true;}
    check(caught,"corrupted periodic seam rejected");
    std::cout<<std::setprecision(17)<<"GRADIENT "<<nx<<" "<<ny<<" "<<nz<<" "
             <<e.advected_kinetic_j<<" "<<e.projected_kinetic_j<<" "
             <<e.pressure_identity_residual_j<<"\n";
}
int main() {
    try {
        for(auto dims:{std::array<int,3>{6,7,8},{8,9,10},{10,12,14}}) {
            nonlinear(dims[0],dims[1],dims[2]);
            manufactured(dims[0],dims[1],dims[2]);
        }
        auto a=shear(6,7,8);
        Vec water(6*7*8,0.5);
        bool rejected=false;
        try{Solver::advance(6,7,8,1,0.4,-1,a[0],a[1],a[2],water);}
        catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"reject negative density");
        rejected=false;
        auto bad=water;bad[0]=std::numeric_limits<double>::quiet_NaN();
        try{Solver::advance(6,7,8,1,0.4,1000,a[0],a[1],a[2],bad);}
        catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&water[0]==0.5,"invalid VOF atomic");
        std::cout<<"PASS: "<<assertions<<" periodic MAC energy native assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
