#pragma once
// P2 PARTIAL: constant-density periodic MAC pressure projection following
// conservative nonlinear transport. No free surfaces, cut cells or FSI.
#include "p2_variable_mac_transport3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PeriodicMacProjectionResult3D {
    std::vector<double> u_m_s,v_m_s,w_m_s,pressure_pa;
    double max_divergence_s_inv=0.0;
    double initial_max_divergence_s_inv=0.0;
    int pcg_iterations=0;
};
struct P2PeriodicMacAdvanceResult3D {
    P2PeriodicMacProjectionResult3D projected;
    double max_outgoing_courant=0.0;
};

class P2PeriodicMacProjection3D {
    using Vec=std::vector<double>;
    static std::size_t wrap(long long a,std::size_t n) {
        const auto m=a%static_cast<long long>(n);
        return static_cast<std::size_t>(m<0?m+static_cast<long long>(n):m);
    }
    static std::size_t ci(std::size_t i,std::size_t j,std::size_t k,
                          std::size_t nx,std::size_t ny) {
        return (k*ny+j)*nx+i;
    }
    static std::size_t fi(int c,std::size_t i,std::size_t j,std::size_t k,
                          std::size_t nx,std::size_t ny) {
        if(c==0)return (k*ny+j)*(nx+1)+i;
        if(c==1)return (k*(ny+1)+j)*nx+i;
        return (k*ny+j)*nx+i;
    }
    static void validate(std::size_t nx,std::size_t ny,std::size_t nz,
                         double dx,double dt,double rho,
                         const Vec& u,const Vec& v,const Vec& w) {
        if(nx<2||ny<2||nz<2||nx>32768/ny||nx*ny>32768/nz||
           !std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0||
           !std::isfinite(rho)||rho<=0)
            throw std::invalid_argument("Invalid periodic projection parameters");
        const std::array<const Vec*,3> fields={&u,&v,&w};
        for(int c=0;c<3;++c) {
            if(fields[c]->size()!=(nx+(c==0))*(ny+(c==1))*(nz+(c==2)))
                throw std::invalid_argument("Invalid periodic projection field size");
            for(double x:*fields[c])
                if(!std::isfinite(x))throw std::invalid_argument("Nonfinite velocity");
        }
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i) {
                    if(i==0&&std::abs(u[fi(0,nx,j,k,nx,ny)]-
                                     u[fi(0,0,j,k,nx,ny)])>1e-12)
                        throw std::invalid_argument("Nonperiodic U seam");
                    if(j==0&&std::abs(v[fi(1,i,ny,k,nx,ny)]-
                                     v[fi(1,i,0,k,nx,ny)])>1e-12)
                        throw std::invalid_argument("Nonperiodic V seam");
                    if(k==0&&std::abs(w[fi(2,i,j,nz,nx,ny)]-
                                     w[fi(2,i,j,0,nx,ny)])>1e-12)
                        throw std::invalid_argument("Nonperiodic W seam");
                }
    }
    static Vec divergence(std::size_t nx,std::size_t ny,std::size_t nz,
                          double dx,const Vec& u,const Vec& v,const Vec& w) {
        Vec d(nx*ny*nz);
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i)
                    d[ci(i,j,k,nx,ny)]=(
                        u[fi(0,i+1,j,k,nx,ny)]-u[fi(0,i,j,k,nx,ny)]+
                        v[fi(1,i,j+1,k,nx,ny)]-v[fi(1,i,j,k,nx,ny)]+
                        w[fi(2,i,j,k+1,nx,ny)]-w[fi(2,i,j,k,nx,ny)])/dx;
        return d;
    }
    static double maximum(const Vec& v) {
        double m=0.0;
        for(double x:v)m=std::max(m,std::abs(x));
        return m;
    }
    static void apply_A(std::size_t nx,std::size_t ny,std::size_t nz,
                        double inv_dx2,const Vec& x,Vec& y) {
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i) {
                    const auto xm=wrap(static_cast<long long>(i)-1,nx);
                    const auto xp=(i+1)%nx;
                    const auto ym=wrap(static_cast<long long>(j)-1,ny);
                    const auto yp=(j+1)%ny;
                    const auto zm=wrap(static_cast<long long>(k)-1,nz);
                    const auto zp=(k+1)%nz;
                    y[ci(i,j,k,nx,ny)]=inv_dx2*(
                        6.0*x[ci(i,j,k,nx,ny)]
                        -x[ci(xm,j,k,nx,ny)]-x[ci(xp,j,k,nx,ny)]
                        -x[ci(i,ym,k,nx,ny)]-x[ci(i,yp,k,nx,ny)]
                        -x[ci(i,j,zm,nx,ny)]-x[ci(i,j,zp,nx,ny)]);
                }
    }
    static double dot(const Vec& a,const Vec& b) {
        double sum=0.0;
        for(std::size_t i=0;i<a.size();++i)sum+=a[i]*b[i];
        return sum;
    }
public:
    static P2PeriodicMacProjectionResult3D project(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,double density_kg_m3,
        const Vec& u,const Vec& v,const Vec& w,
        int max_iterations=4000) {
        validate(nx,ny,nz,dx_m,dt_s,density_kg_m3,u,v,w);
        if(max_iterations<1||max_iterations>10000)
            throw std::invalid_argument("Invalid PCG iteration limit");
        const auto d=divergence(nx,ny,nz,dx_m,u,v,w);
        const double initial=maximum(d);
        Vec b(d.size()),p(d.size(),0.0),r(d.size()),z(d.size()),q(d.size());
        // A=-L, A*p=-rho/dt*div(u*). Mean zero fixes periodic gauge.
        const double scale=-density_kg_m3/dt_s;
        for(std::size_t i=0;i<b.size();++i)b[i]=scale*d[i];
        const double mean=std::accumulate(b.begin(),b.end(),0.0)/
                          static_cast<double>(b.size());
        for(double& x:b)x-=mean;
        r=b;
        const double inv_dx2=1.0/(dx_m*dx_m);
        const double jacobi=dx_m*dx_m/6.0;
        for(std::size_t i=0;i<r.size();++i)z[i]=jacobi*r[i];
        Vec direction=z;
        double rz=dot(r,z);
        const double bnorm=std::sqrt(dot(b,b));
        int iterations=0;
        const double tolerance=1e-12*bnorm;
        while(std::sqrt(std::max(0.0,dot(r,r)))>tolerance) {
            if(iterations>=max_iterations)
                throw std::runtime_error("Periodic MAC pressure PCG not converged");
            apply_A(nx,ny,nz,inv_dx2,direction,q);
            const double denom=dot(direction,q);
            if(!std::isfinite(denom)||denom<=0.0||!std::isfinite(rz))
                throw std::runtime_error("Periodic MAC pressure PCG breakdown");
            const double alpha=rz/denom;
            for(std::size_t i=0;i<p.size();++i) {
                p[i]+=alpha*direction[i];
                r[i]-=alpha*q[i];
                z[i]=jacobi*r[i];
            }
            const double next_rz=dot(r,z);
            if(!std::isfinite(next_rz))
                throw std::runtime_error("Periodic MAC pressure PCG nonfinite");
            if(next_rz==0.0){++iterations;break;}
            const double beta=next_rz/rz;
            for(std::size_t i=0;i<direction.size();++i)
                direction[i]=z[i]+beta*direction[i];
            rz=next_rz;
            ++iterations;
        }
        const double pmean=std::accumulate(p.begin(),p.end(),0.0)/
                           static_cast<double>(p.size());
        for(double& x:p)x-=pmean;
        Vec out_u=u,out_v=v,out_w=w;
        const double factor=dt_s/(density_kg_m3*dx_m);
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i) {
                    const auto id=ci(i,j,k,nx,ny);
                    const auto ix=ci(wrap(static_cast<long long>(i)-1,nx),j,k,nx,ny);
                    const auto iy=ci(i,wrap(static_cast<long long>(j)-1,ny),k,nx,ny);
                    const auto iz=ci(i,j,wrap(static_cast<long long>(k)-1,nz),nx,ny);
                    out_u[fi(0,i,j,k,nx,ny)]-=factor*(p[id]-p[ix]);
                    out_v[fi(1,i,j,k,nx,ny)]-=factor*(p[id]-p[iy]);
                    out_w[fi(2,i,j,k,nx,ny)]-=factor*(p[id]-p[iz]);
                }
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                out_u[fi(0,nx,j,k,nx,ny)]=out_u[fi(0,0,j,k,nx,ny)];
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t i=0;i<nx;++i)
                out_v[fi(1,i,ny,k,nx,ny)]=out_v[fi(1,i,0,k,nx,ny)];
        for(std::size_t j=0;j<ny;++j)
            for(std::size_t i=0;i<nx;++i)
                out_w[fi(2,i,j,nz,nx,ny)]=out_w[fi(2,i,j,0,nx,ny)];
        const double residual=maximum(divergence(nx,ny,nz,dx_m,out_u,out_v,out_w));
        if(!std::isfinite(residual)||residual>1e-9*std::max(1.0,initial))
            throw std::runtime_error("Periodic MAC projection divergence residual");
        return {std::move(out_u),std::move(out_v),std::move(out_w),
                std::move(p),residual,initial,iterations};
    }
    static P2PeriodicMacAdvanceResult3D self_advect_project(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,double density_kg_m3,
        const Vec& u,const Vec& v,const Vec& w) {
        // Transport validates the incoming solenoidal advector and CFL.
        const auto predicted=P2VariableMacTransport3D::self_advect(
            nx,ny,nz,dx_m,dt_s,u,v,w);
        auto projected=project(nx,ny,nz,dx_m,dt_s,density_kg_m3,
            predicted.u_m_s,predicted.v_m_s,predicted.w_m_s);
        return {std::move(projected),predicted.max_outgoing_courant};
    }
};
} // namespace upc
