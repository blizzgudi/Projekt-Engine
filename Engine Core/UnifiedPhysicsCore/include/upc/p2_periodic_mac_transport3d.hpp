#pragma once
// P2 PARTIAL: first-order conservative linear momentum transport on a
// periodic 3D staggered MAC lattice. Constant prescribed transport velocity;
// NOT nonlinear Navier-Stokes advection, cut-cell FSI or a free-surface solver.
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PeriodicMacTransportResult3D {
    std::vector<double> u_m_s,v_m_s,w_m_s;
    double courant_sum=0.0;
};

class P2PeriodicMacTransport3D {
    static std::size_t index(int component,std::size_t i,std::size_t j,
                             std::size_t k,std::size_t nx,std::size_t ny) {
        if(component==0)return (k*ny+j)*(nx+1)+i;
        if(component==1)return (k*(ny+1)+j)*nx+i;
        return (k*ny+j)*nx+i;
    }
    static std::vector<double> advect(int component,std::size_t nx,
        std::size_t ny,std::size_t nz,const std::vector<double>& src,
        const std::array<double,3>& a) {
        const std::size_t sx=nx+(component==0);
        const std::size_t sy=ny+(component==1);
        const std::size_t sz=nz+(component==2);
        std::vector<double> dst(sx*sy*sz,0.0);
        auto at=[&](std::size_t i,std::size_t j,std::size_t k)->double {
            return src[index(component,i,j,k,nx,ny)];
        };
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i) {
                    const double center=at(i,j,k);
                    const double x=at(a[0]>=0?(i+nx-1)%nx:(i+1)%nx,j,k);
                    const double y=at(i,a[1]>=0?(j+ny-1)%ny:(j+1)%ny,k);
                    const double z=at(i,j,a[2]>=0?(k+nz-1)%nz:(k+1)%nz);
                    const double updated=(1.0-std::abs(a[0])-
                        std::abs(a[1])-std::abs(a[2]))*center+
                        std::abs(a[0])*x+std::abs(a[1])*y+std::abs(a[2])*z;
                    if(!std::isfinite(updated))
                        throw std::invalid_argument("Nonfinite MAC transport update");
                    dst[index(component,i,j,k,nx,ny)]=updated;
                }
        // Periodic MAC stores a redundant high-side plane in its normal
        // direction. Copy, never transport this plane as a separate cell.
        if(component==0)
            for(std::size_t k=0;k<nz;++k)
                for(std::size_t j=0;j<ny;++j)
                    dst[index(0,nx,j,k,nx,ny)]=dst[index(0,0,j,k,nx,ny)];
        if(component==1)
            for(std::size_t k=0;k<nz;++k)
                for(std::size_t i=0;i<nx;++i)
                    dst[index(1,i,ny,k,nx,ny)]=dst[index(1,i,0,k,nx,ny)];
        if(component==2)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i)
                    dst[index(2,i,j,nz,nx,ny)]=dst[index(2,i,j,0,nx,ny)];
        return dst;
    }
    static void validate_field(int component,std::size_t nx,
        std::size_t ny,std::size_t nz,const std::vector<double>& v) {
        const std::size_t expected=(nx+(component==0))*
            (ny+(component==1))*(nz+(component==2));
        if(v.size()!=expected)
            throw std::invalid_argument("Periodic MAC component shape mismatch");
        for(double x:v)
            if(!std::isfinite(x))
                throw std::invalid_argument("Nonfinite periodic MAC velocity");
        // Do not silently accept a discontinuous redundant periodic plane.
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i) {
                    if(component==0&&i==0&&
                       std::abs(v[index(0,nx,j,k,nx,ny)]-
                                v[index(0,0,j,k,nx,ny)])>1e-12)
                        throw std::invalid_argument("Nonperiodic U seam");
                    if(component==1&&j==0&&
                       std::abs(v[index(1,i,ny,k,nx,ny)]-
                                v[index(1,i,0,k,nx,ny)])>1e-12)
                        throw std::invalid_argument("Nonperiodic V seam");
                    if(component==2&&k==0&&
                       std::abs(v[index(2,i,j,nz,nx,ny)]-
                                v[index(2,i,j,0,nx,ny)])>1e-12)
                        throw std::invalid_argument("Nonperiodic W seam");
                }
    }
public:
    static P2PeriodicMacTransportResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,const std::array<double,3>& transport_m_s,
        const std::vector<double>& u,const std::vector<double>& v,
        const std::vector<double>& w) {
        if(nx<2||ny<2||nz<2||nx>32768/ny||nx*ny>32768/nz||
           !std::isfinite(dx_m)||dx_m<=0.0||
           !std::isfinite(dt_s)||dt_s<=0.0)
            throw std::invalid_argument("Invalid periodic MAC grid or time step");
        std::array<double,3> a{};
        double courant=0.0;
        for(int d=0;d<3;++d) {
            if(!std::isfinite(transport_m_s[d]))
                throw std::invalid_argument("Nonfinite transport speed");
            a[d]=transport_m_s[d]*dt_s/dx_m;
            if(!std::isfinite(a[d]))
                throw std::invalid_argument("Nonfinite transport Courant number");
            courant+=std::abs(a[d]);
        }
        if(!std::isfinite(courant)||courant>1.0+1e-13)
            throw std::invalid_argument("Periodic upwind CFL exceeded");
        validate_field(0,nx,ny,nz,u);
        validate_field(1,nx,ny,nz,v);
        validate_field(2,nx,ny,nz,w);
        return {advect(0,nx,ny,nz,u,a),advect(1,nx,ny,nz,v,a),
                advect(2,nx,ny,nz,w,a),courant};
    }
};
} // namespace upc
