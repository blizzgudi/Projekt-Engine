// P2 PARTIAL: periodic manufactured linear advection of MAC components.
// Constant transport speed; no moving walls, pressure, FSI or nonlinear CFD.
#include "upc/p2_periodic_mac_transport3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
std::size_t assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
std::size_t idx(int c,int x,int y,int z,int nx,int ny) {
    if(c==0)return (z*ny+y)*(nx+1)+x;
    if(c==1)return (z*(ny+1)+y)*nx+x;
    return (z*ny+y)*nx+x;
}
using Solver=upc::P2PeriodicMacTransport3D;
using Fields=upc::P2PeriodicMacTransportResult3D;
constexpr double mean[3]={0.2,-0.3,0.4};
constexpr double amp[3]={0.1,0.08,-0.12};
std::vector<double> make_field(int c,int nx,int ny,int nz) {
    const int sx=nx+(c==0),sy=ny+(c==1),sz=nz+(c==2);
    std::vector<double> out(sx*sy*sz,0.0);
    for(int z=0;z<sz;++z)for(int y=0;y<sy;++y)
        for(int x=0;x<sx;++x) {
            const int parity=(x+y+z)%2==0?1:-1;
            out[idx(c,x,y,z,nx,ny)]=mean[c]+amp[c]*parity;
        }
    return out;
}
void inspect(int c,const std::vector<double>& values,int nx,int ny,int nz,
             int step,double& max_error,double& max_sum_error,
             double& variance_ratio) {
    const double factor=std::pow(0.38,step);
    double sum=0.0,variance=0.0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
        for(int x=0;x<nx;++x) {
            const int parity=(x+y+z)%2==0?1:-1;
            const double actual=values[idx(c,x,y,z,nx,ny)];
            const double expected=mean[c]+amp[c]*factor*parity;
            max_error=std::max(max_error,std::abs(actual-expected));
            near(actual,expected,1e-12,"analytic upwind checkerboard eigenmode");
            sum+=actual;
            variance+=(actual-mean[c])*(actual-mean[c]);
        }
    const double cells=double(nx)*ny*nz;
    const double sum_error=std::abs(sum-cells*mean[c]);
    max_sum_error=std::max(max_sum_error,sum_error);
    near(sum,cells*mean[c],2e-9,"periodic MAC momentum integral");
    const double initial_variance=cells*amp[c]*amp[c];
    const double ratio=variance/initial_variance;
    near(ratio,std::pow(0.38,2*step),2e-12,
         "analytic L2 dissipation of first-order upwind");
    variance_ratio=std::max(variance_ratio,ratio);
    // The high normal face must equal the low face exactly.
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
        for(int x=0;x<nx;++x) {
            if(c==0&&x==0)
                near(values[idx(c,nx,y,z,nx,ny)],
                     values[idx(c,0,y,z,nx,ny)],0.0,"U periodic seam");
            if(c==1&&y==0)
                near(values[idx(c,x,ny,z,nx,ny)],
                     values[idx(c,x,0,z,nx,ny)],0.0,"V periodic seam");
            if(c==2&&z==0)
                near(values[idx(c,x,y,nz,nx,ny)],
                     values[idx(c,x,y,0,nx,ny)],0.0,"W periodic seam");
        }
}
void run(int nx,int ny,int nz,int sx,int sy,int sz) {
    auto u=make_field(0,nx,ny,nz);
    auto v=make_field(1,nx,ny,nz);
    auto w=make_field(2,nx,ny,nz);
    const std::array<double,3> transport={
        sx*0.07,sy*0.11,sz*0.13};
    for(int step=1;step<=5;++step) {
        auto result=Solver::step(nx,ny,nz,1.0,1.0,transport,u,v,w);
        near(result.courant_sum,0.31,1e-14,"3D upwind CFL");
        u=std::move(result.u_m_s);
        v=std::move(result.v_m_s);
        w=std::move(result.w_m_s);
        double max_error=0.0,max_sum_error=0.0,variance_ratio=0.0;
        inspect(0,u,nx,ny,nz,step,max_error,max_sum_error,variance_ratio);
        inspect(1,v,nx,ny,nz,step,max_error,max_sum_error,variance_ratio);
        inspect(2,w,nx,ny,nz,step,max_error,max_sum_error,variance_ratio);
        std::cout<<std::setprecision(17)
            <<"TRANSPORT "<<nx<<" "<<ny<<" "<<nz<<" "
            <<sx<<" "<<sy<<" "<<sz<<" "<<step<<" "
            <<max_error<<" "<<max_sum_error<<" "<<variance_ratio<<"\n";
    }
}
void negative() {
    auto u=make_field(0,8,10,12);
    auto v=make_field(1,8,10,12);
    auto w=make_field(2,8,10,12);
    const auto old_u=u,old_v=v,old_w=w;
    const std::array<double,3> speed={0.07,0.11,0.13};
    auto rejects=[&](int nx,int ny,int nz,double dx,double dt,
                     const std::array<double,3>& vel,
                     const std::vector<double>& a,
                     const std::vector<double>& b,
                     const std::vector<double>& c) {
        bool threw=false;
        try{(void)Solver::step(nx,ny,nz,dx,dt,vel,a,b,c);}
        catch(const std::invalid_argument&){threw=true;}
        check(threw,"invalid transport rejected");
    };
    rejects(8,10,12,1.0,0.0,speed,u,v,w);
    rejects(8,10,12,0.0,1.0,speed,u,v,w);
    rejects(8,10,12,1.0,1.0,{1.0,1.0,1.0},u,v,w);
    rejects(8,10,12,1.0,1.0,
            {std::numeric_limits<double>::quiet_NaN(),0.0,0.0},u,v,w);
    rejects(1,10,12,1.0,1.0,speed,u,v,w);
    auto bad_u=u;bad_u.pop_back();
    rejects(8,10,12,1.0,1.0,speed,bad_u,v,w);
    bad_u=u;bad_u[idx(0,8,0,0,8,10)]+=0.01;
    rejects(8,10,12,1.0,1.0,speed,bad_u,v,w);
    auto bad_v=v;bad_v[idx(1,0,10,0,8,10)]+=0.01;
    rejects(8,10,12,1.0,1.0,speed,u,bad_v,w);
    auto bad_w=w;bad_w[idx(2,0,0,12,8,10)]+=0.01;
    rejects(8,10,12,1.0,1.0,speed,u,v,bad_w);
    bad_u=u;bad_u[0]=std::numeric_limits<double>::infinity();
    rejects(8,10,12,1.0,1.0,speed,bad_u,v,w);
    check(u==old_u&&v==old_v&&w==old_w,
          "invalid steps never modify input state");
    const auto zero=Solver::step(8,10,12,1.0,1.0,{0,0,0},u,v,w);
    check(zero.u_m_s==u&&zero.v_m_s==v&&zero.w_m_s==w,
          "zero-speed transport bitwise identity");
    const auto constant_u=std::vector<double>(u.size(),0.2);
    const auto constant_v=std::vector<double>(v.size(),-0.3);
    const auto constant_w=std::vector<double>(w.size(),0.4);
    const auto uniform=Solver::step(8,10,12,1.0,1.0,speed,
                                    constant_u,constant_v,constant_w);
    for(double x:uniform.u_m_s)near(x,0.2,1e-15,"constant U invariance");
    for(double x:uniform.v_m_s)near(x,-0.3,1e-15,"constant V invariance");
    for(double x:uniform.w_m_s)near(x,0.4,1e-15,"constant W invariance");
}
}
int main() {
    try {
        for(int n:{8,12,16})for(int sx:{-1,1})
            for(int sy:{-1,1})for(int sz:{-1,1})
                run(n,n+2,n+4,sx,sy,sz);
        negative();
        std::cout<<"PASS: "<<assertions
                 <<" native periodic MAC linear-transport assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
