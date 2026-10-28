// P2 PARTIAL: independently manufactured variable-advector periodic MAC tests.
#include "upc/p2_variable_mac_transport3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Vec=std::vector<double>;
using Solver=upc::P2VariableMacTransport3D;
std::size_t assertions=0;
constexpr double pi=3.14159265358979323846;
constexpr double mean[3]={0.2,-0.3,0.4};
constexpr double amp[3]={0.1,0.08,-0.12};
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double actual,double expected,double tol,const char* msg) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,msg);
}
std::size_t idx(int c,int x,int y,int z,int nx,int ny) {
    if(c==0)return (z*ny+y)*(nx+1)+x;
    if(c==1)return (z*(ny+1)+y)*nx+x;
    return (z*ny+y)*nx+x;
}
Vec make_field(int c,int nx,int ny,int nz) {
    const int sx=nx+(c==0),sy=ny+(c==1),sz=nz+(c==2);
    Vec result(sx*sy*sz);
    for(int k=0;k<sz;++k)for(int j=0;j<sy;++j)
        for(int i=0;i<sx;++i)
            result[idx(c,i,j,k,nx,ny)]=mean[c]+
                amp[c]*((i+j+k)%2==0?1:-1);
    return result;
}
std::array<Vec,3> shear(int nx,int ny,int nz,int sign) {
    Vec u((nx+1)*ny*nz),v(nx*(ny+1)*nz,0.0),w(nx*ny*(nz+1),0.0);
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<=nx;++i)
            u[idx(0,i,j,k,nx,ny)]=sign*(0.32+0.12*
                std::cos(2.0*pi*j/ny));
    return {u,v,w};
}
double shear_speed(int c,int j,int ny,int sign) {
    auto speed=[&](int y) {
        return sign*(0.32+0.12*std::cos(2.0*pi*y/ny));
    };
    return c==1?0.5*(speed((j+ny-1)%ny)+speed(j)):speed(j);
}
void run_shear(int nx,int ny,int nz,int sign) {
    const auto a=shear(nx,ny,nz,sign);
    auto u=make_field(0,nx,ny,nz),v=make_field(1,nx,ny,nz),
         w=make_field(2,nx,ny,nz);
    constexpr double dt=0.6;
    for(int step=1;step<=4;++step) {
        const auto out=Solver::step(nx,ny,nz,1.0,dt,
                                    a[0],a[1],a[2],u,v,w);
        u=out.u_m_s;v=out.v_m_s;w=out.w_m_s;
        near(out.max_outgoing_courant,dt*0.44,1e-14,
             "analytic shear max local CFL");
        near(out.max_cell_divergence_s_inv,0.0,1e-14,
             "analytic shear divergence");
        const std::array<Vec,3> fields={u,v,w};
        double maxerr=0.0,maxsumerr=0.0;
        for(int c=0;c<3;++c) {
            double sum=0.0;
            for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
                for(int i=0;i<nx;++i) {
                    const int parity=(i+j+k)%2==0?1:-1;
                    const double factor=1.0-2.0*dt*
                        std::abs(shear_speed(c,j,ny,sign));
                    const double expected=mean[c]+amp[c]*parity*
                        std::pow(factor,step);
                    const double actual=fields[c][idx(c,i,j,k,nx,ny)];
                    maxerr=std::max(maxerr,std::abs(actual-expected));
                    near(actual,expected,2e-12,
                         "independent variable-shear Fourier reference");
                    sum+=actual;
                }
            maxsumerr=std::max(maxsumerr,
                std::abs(sum-double(nx)*ny*nz*mean[c]));
            near(sum,double(nx)*ny*nz*mean[c],2e-9,
                 "conservative staggered momentum integral");
            for(int j:{0,ny/3,ny-1}) {
                const double sample=fields[c][idx(c,0,j,0,nx,ny)];
                std::cout<<std::setprecision(17)<<"SAMPLE "
                    <<nx<<" "<<ny<<" "<<nz<<" "<<sign<<" "<<step
                    <<" "<<c<<" "<<j<<" "<<sample<<"\n";
            }
            // Redundant normal plane must match low plane bitwise.
            for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
                for(int i=0;i<nx;++i) {
                    if(c==0&&i==0)
                        near(fields[c][idx(c,nx,j,k,nx,ny)],
                             fields[c][idx(c,0,j,k,nx,ny)],0.0,"U seam");
                    if(c==1&&j==0)
                        near(fields[c][idx(c,i,ny,k,nx,ny)],
                             fields[c][idx(c,i,0,k,nx,ny)],0.0,"V seam");
                    if(c==2&&k==0)
                        near(fields[c][idx(c,i,j,nz,nx,ny)],
                             fields[c][idx(c,i,j,0,nx,ny)],0.0,"W seam");
                }
        }
        std::cout<<std::setprecision(17)<<"SHEAR "<<nx<<" "<<ny
            <<" "<<nz<<" "<<sign<<" "<<step<<" "<<maxerr
            <<" "<<maxsumerr<<" "<<out.max_outgoing_courant<<"\n";
    }
}
std::array<Vec,3> solenoidal(int nx,int ny,int nz) {
    Vec u((nx+1)*ny*nz),v(nx*(ny+1)*nz),w(nx*ny*(nz+1));
    for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<=nx;++i)
            u[idx(0,i,j,k,nx,ny)]=0.15+0.05*std::sin(2*pi*j/ny)
                                     +0.02*std::cos(2*pi*k/nz);
    for(int k=0;k<nz;++k)for(int j=0;j<=ny;++j)
        for(int i=0;i<nx;++i)
            v[idx(1,i,j,k,nx,ny)]=-0.08+0.03*std::sin(2*pi*i/nx)
                                      +0.01*std::cos(2*pi*k/nz);
    for(int k=0;k<=nz;++k)for(int j=0;j<ny;++j)
        for(int i=0;i<nx;++i)
            w[idx(2,i,j,k,nx,ny)]=0.1+0.04*std::sin(2*pi*i/nx)
                                     +0.015*std::cos(2*pi*j/ny);
    return {u,v,w};
}
void run_self(int nx,int ny,int nz) {
    const auto a=solenoidal(nx,ny,nz);
    const auto result=Solver::self_advect(nx,ny,nz,1.0,0.5,
                                          a[0],a[1],a[2]);
    const std::array<Vec,3> next={result.u_m_s,result.v_m_s,result.w_m_s};
    double maxchange=0.0,maxsumerr=0.0;
    for(int c=0;c<3;++c) {
        double before=0.0,after=0.0;
        for(int k=0;k<nz;++k)for(int j=0;j<ny;++j)
            for(int i=0;i<nx;++i) {
                const auto pos=idx(c,i,j,k,nx,ny);
                before+=a[c][pos];after+=next[c][pos];
                maxchange=std::max(maxchange,
                                   std::abs(next[c][pos]-a[c][pos]));
            }
        maxsumerr=std::max(maxsumerr,std::abs(after-before));
        near(after,before,2e-9,"nonlinear MAC conservative integral");
    }
    check(maxchange>1e-5,"self-advection is nontrivial");
    check(result.max_outgoing_courant<1.0,"self-advection CFL");
    near(result.max_cell_divergence_s_inv,0.0,1e-13,
         "discrete solenoidal advector");
    std::cout<<std::setprecision(17)<<"SELF "<<nx<<" "<<ny<<" "<<nz
        <<" "<<maxchange<<" "<<maxsumerr<<" "
        <<result.max_cell_divergence_s_inv<<"\n";
}
void negative() {
    constexpr int nx=8,ny=10,nz=12;
    auto a=shear(nx,ny,nz,1);
    auto q=std::array<Vec,3>{make_field(0,nx,ny,nz),
                             make_field(1,nx,ny,nz),
                             make_field(2,nx,ny,nz)};
    const auto original_a=a,original_q=q;
    auto reject=[&](int x,int y,int z,double dx,double dt,
                    const std::array<Vec,3>& adv,
                    const std::array<Vec,3>& fields) {
        bool failed=false;
        try{(void)Solver::step(x,y,z,dx,dt,adv[0],adv[1],adv[2],
                               fields[0],fields[1],fields[2]);}
        catch(const std::invalid_argument&){failed=true;}
        check(failed,"invalid MAC step rejected");
    };
    reject(nx,ny,nz,1.0,0.0,a,q);
    reject(nx,ny,nz,0.0,1.0,a,q);
    reject(1,ny,nz,1.0,0.6,a,q);
    reject(nx,ny,nz,1.0,10.0,a,q);
    auto bad_a=a;bad_a[0][idx(0,2,0,0,nx,ny)]+=0.2;
    reject(nx,ny,nz,1.0,0.6,bad_a,q); // non-solenoidal
    bad_a=a;bad_a[0][idx(0,nx,0,0,nx,ny)]+=0.1;
    reject(nx,ny,nz,1.0,0.6,bad_a,q); // nonperiodic
    bad_a=a;bad_a[0][0]=std::numeric_limits<double>::quiet_NaN();
    reject(nx,ny,nz,1.0,0.6,bad_a,q);
    auto bad_q=q;bad_q[1].pop_back();
    reject(nx,ny,nz,1.0,0.6,a,bad_q);
    bad_q=q;bad_q[2][idx(2,0,0,nz,nx,ny)]+=0.1;
    reject(nx,ny,nz,1.0,0.6,a,bad_q);
    check(a==original_a&&q==original_q,"all input arrays unchanged");
    const std::array<Vec,3> constant={
        Vec(q[0].size(),0.2),Vec(q[1].size(),-0.3),Vec(q[2].size(),0.4)};
    const auto result=Solver::step(nx,ny,nz,1.0,0.6,
        a[0],a[1],a[2],constant[0],constant[1],constant[2]);
    for(double x:result.u_m_s)near(x,0.2,1e-15,"constant U");
    for(double x:result.v_m_s)near(x,-0.3,1e-15,"constant V");
    for(double x:result.w_m_s)near(x,0.4,1e-15,"constant W");
}
}
int main() {
    try {
        for(int n:{8,12,16}) {
            for(int sign:{-1,1})run_shear(n,n+2,n+4,sign);
            run_self(n,n+2,n+4);
        }
        negative();
        std::cout<<"PASS: "<<assertions
                 <<" native variable MAC transport assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
