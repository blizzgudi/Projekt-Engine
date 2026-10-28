#include "upc/p2_periodic_mac_vof3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
using Vec=std::vector<double>;
using Solver=upc::P2PeriodicMacVof3D;
namespace {
constexpr double pi=3.1415926535897932384626433832795;
std::size_t checks=0;
void require(bool yes,const std::string& why) {
    ++checks;
    if(!yes)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const std::string& why) {
    require(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
std::size_t id(int i,int j,int k,int nx,int ny) {
    return (k*ny+j)*nx+i;
}
std::size_t face(int c,int i,int j,int k,int nx,int ny) {
    if(c==0)return (k*ny+j)*(nx+1)+i;
    if(c==1)return (k*(ny+1)+j)*nx+i;
    return (k*ny+j)*nx+i;
}
std::array<Vec,3> constant(int nx,int ny,int nz,double x,double y,double z) {
    return {Vec((nx+1)*ny*nz,x),Vec(nx*(ny+1)*nz,y),Vec(nx*ny*(nz+1),z)};
}
std::array<Vec,3> shear(int nx,int ny,int nz) {
    auto a=constant(nx,ny,nz,0,0,0);
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<=nx;++i)
            a[0][face(0,i,j,k,nx,ny)]=0.2+0.05*std::sin(2*pi*j/ny)
                                             +0.02*std::cos(2*pi*k/nz);
    for(int k=0;k<nz;++k)for(int j=0;j<=ny;++j)
        for(int i=0;i<nx;++i)
            a[1][face(1,i,j,k,nx,ny)]=-0.1+0.04*std::sin(2*pi*k/nz)
                                              +0.03*std::cos(2*pi*i/nx);
    for(int k=0;k<=nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<nx;++i)
            a[2][face(2,i,j,k,nx,ny)]=0.15+0.03*std::sin(2*pi*i/nx)
                                             +0.02*std::cos(2*pi*j/ny);
    return a;
}
Vec water(int nx,int ny,int nz) {
    Vec a(nx*ny*nz);
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<nx;++i)
            a[id(i,j,k,nx,ny)]=0.45+0.12*std::sin(2*pi*i/nx)
                                   +0.09*std::cos(2*pi*j/ny)
                                   +0.07*std::sin(2*pi*k/nz);
    return a;
}
void constant_reference(int nx,int ny,int nz,int sign) {
    const double dx=1,dt=0.4,rho=1000;
    const std::array<double,3> speed={sign*0.25,-sign*0.2,sign*0.15};
    auto a=constant(nx,ny,nz,speed[0],speed[1],speed[2]);
    auto initial=water(nx,ny,nz);
    const double total=std::accumulate(initial.begin(),initial.end(),0.0);
    auto scalar=initial;
    for(int step=1;step<=4;++step) {
        auto result=Solver::advance(nx,ny,nz,dx,dt,rho,a[0],a[1],a[2],scalar);
        const auto& next=result.vof.water_fraction;
        require(result.mac.projected.max_divergence_s_inv<1e-12,"constant div");
        near(result.vof.max_cell_courant,dt*0.6,1e-13,"constant cell CFL");
        near(result.vof.initial_fraction_sum,total,1e-10,"initial mass");
        near(result.vof.final_fraction_sum,total,1e-10,"final mass");
        for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
            for(int i=0;i<nx;++i) {
                const double center=scalar[id(i,j,k,nx,ny)];
                const int im=(i+nx-1)%nx,ip=(i+1)%nx;
                const int jm=(j+ny-1)%ny,jp=(j+1)%ny;
                const int km=(k+nz-1)%nz,kp=(k+1)%nz;
                const double expected=center-dt*(
                    speed[0]*(speed[0]>0?center-scalar[id(im,j,k,nx,ny)]:
                                              scalar[id(ip,j,k,nx,ny)]-center)+
                    speed[1]*(speed[1]>0?center-scalar[id(i,jm,k,nx,ny)]:
                                              scalar[id(i,jp,k,nx,ny)]-center)+
                    speed[2]*(speed[2]>0?center-scalar[id(i,j,km,nx,ny)]:
                                              scalar[id(i,j,kp,nx,ny)]-center));
                near(next[id(i,j,k,nx,ny)],expected,2e-12,"independent upwind");
                require(next[id(i,j,k,nx,ny)]>=0&&next[id(i,j,k,nx,ny)]<=1,
                        "constant VOF bounds");
            }
        scalar=next;
    }
    std::cout<<std::setprecision(17)<<"CONSTANT "<<nx<<" "<<ny<<" "<<nz
             <<" "<<sign<<" "<<total<<" "
             <<std::accumulate(scalar.begin(),scalar.end(),0.0)<<"\n";
}
void nonlinear(int nx,int ny,int nz) {
    auto a=shear(nx,ny,nz);
    auto scalar=water(nx,ny,nz);
    const double total=std::accumulate(scalar.begin(),scalar.end(),0.0);
    double max_projection_div=0.0,max_cfl=0.0;
    for(int step=1;step<=4;++step) {
        auto result=Solver::advance(nx,ny,nz,1.0,0.4,1000.0,
                                    a[0],a[1],a[2],scalar);
        const auto& m=result.mac.projected;
        require(m.max_divergence_s_inv<1e-10,"nonlinear projected divergence");
        max_projection_div=std::max(max_projection_div,m.max_divergence_s_inv);
        max_cfl=std::max(max_cfl,result.vof.max_cell_courant);
        near(result.vof.initial_fraction_sum,total,2e-10,"nonlinear input mass");
        near(result.vof.final_fraction_sum,total,2e-10,"nonlinear output mass");
        for(double f:result.vof.water_fraction)
            require(f>=-1e-12&&f<=1+1e-12,"nonlinear bounds");
        // Independent FV face-by-face reference on the projected MAC field.
        for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
            for(int i=0;i<nx;++i) {
                const auto donor=[&](int x,int y,int z) {
                    return scalar[id((x+nx)%nx,(y+ny)%ny,(z+nz)%nz,nx,ny)];
                };
                const double center=donor(i,j,k);
                const double ur=m.u_m_s[face(0,i+1,j,k,nx,ny)];
                const double ul=m.u_m_s[face(0,i,j,k,nx,ny)];
                const double vr=m.v_m_s[face(1,i,j+1,k,nx,ny)];
                const double vl=m.v_m_s[face(1,i,j,k,nx,ny)];
                const double wr=m.w_m_s[face(2,i,j,k+1,nx,ny)];
                const double wl=m.w_m_s[face(2,i,j,k,nx,ny)];
                const auto f=[](double vel,double left,double right) {
                    return vel*(vel>=0?left:right);
                };
                const double expected=center-0.4*(
                    f(ur,center,donor(i+1,j,k))-f(ul,donor(i-1,j,k),center)+
                    f(vr,center,donor(i,j+1,k))-f(vl,donor(i,j-1,k),center)+
                    f(wr,center,donor(i,j,k+1))-f(wl,donor(i,j,k-1),center));
                near(result.vof.water_fraction[id(i,j,k,nx,ny)],expected,2e-12,
                     "independent nonlinear FV");
            }
        scalar=result.vof.water_fraction;
        a={m.u_m_s,m.v_m_s,m.w_m_s};
    }
    std::cout<<std::setprecision(17)<<"NONLINEAR "<<nx<<" "<<ny<<" "<<nz
             <<" "<<total<<" "
             <<std::accumulate(scalar.begin(),scalar.end(),0.0)
             <<" "<<max_projection_div<<" "<<max_cfl<<"\n";
}
template<typename F> void must_throw(F f,const char* label) {
    bool caught=false;
    try{f();}catch(const std::exception&){caught=true;}
    require(caught,label);
}
void invalid() {
    const int nx=6,ny=7,nz=8;
    auto a=constant(nx,ny,nz,0.2,0.1,0.05);
    auto fraction=water(nx,ny,nz);
    const auto original=a;
    const auto saved=fraction;
    auto broken=fraction; broken[0]=1.1;
    must_throw([&]{Solver::advance(nx,ny,nz,1,0.1,1000,
                    a[0],a[1],a[2],broken);},"fraction bounds");
    broken=fraction; broken[0]=std::numeric_limits<double>::quiet_NaN();
    must_throw([&]{Solver::advance(nx,ny,nz,1,0.1,1000,
                    a[0],a[1],a[2],broken);},"fraction NaN");
    must_throw([&]{Solver::advance(nx,ny,nz,1,0.1,-1000,
                    a[0],a[1],a[2],fraction);},"negative density");
    must_throw([&]{Solver::transport_fraction(nx,ny,nz,1,20,
                    a[0],a[1],a[2],fraction);},"cell CFL");
    auto seam=a; seam[0][face(0,nx,0,0,nx,ny)]+=0.1;
    must_throw([&]{Solver::transport_fraction(nx,ny,nz,1,0.1,
                    seam[0],seam[1],seam[2],fraction);},"seam");
    auto divergence=a; divergence[0][face(0,1,1,1,nx,ny)]+=0.1;
    must_throw([&]{Solver::transport_fraction(nx,ny,nz,1,0.1,
                    divergence[0],divergence[1],divergence[2],fraction);},
                    "non-solenoidal");
    must_throw([&]{Solver::transport_fraction(nx,ny,nz,1,0.1,
                    a[0],a[1],a[2],Vec(2));},"shape");
    require(a==original&&fraction==saved,"atomic caller state");
    std::cout<<"INVALID 7\n";
}
}
int main() {
    try {
        for(const auto dims:{std::array<int,3>{6,7,8},{8,9,10},{10,12,14}}) {
            constant_reference(dims[0],dims[1],dims[2],1);
            constant_reference(dims[0],dims[1],dims[2],-1);
            nonlinear(dims[0],dims[1],dims[2]);
        }
        invalid();
        std::cout<<"PASS: "<<checks<<" periodic MAC/VOF native assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
