#pragma once
// P2 PARTIAL: opt-in periodic, constant-density 3D MAC momentum advection,
// pressure projection and conservative passive VOF-fraction transport.
// This is NOT a free-surface stress solver, moving cut-cell or FSI model.
#include "upc/p2_periodic_mac_projection3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2PeriodicMacVofTransport3D {
    std::vector<double> water_fraction;
    double max_cell_courant=0.0;
    double initial_fraction_sum=0.0;
    double final_fraction_sum=0.0;
};
struct P2PeriodicMacVofAdvance3D {
    P2PeriodicMacAdvanceResult3D mac;
    P2PeriodicMacVofTransport3D vof;
};

class P2PeriodicMacVof3D {
    using Vec=std::vector<double>;
    static std::size_t cell(std::size_t i,std::size_t j,std::size_t k,
                            std::size_t nx,std::size_t ny) {
        return (k*ny+j)*nx+i;
    }
    static std::size_t uface(std::size_t i,std::size_t j,std::size_t k,
                             std::size_t nx,std::size_t ny) {
        return (k*ny+j)*(nx+1)+i;
    }
    static std::size_t vface(std::size_t i,std::size_t j,std::size_t k,
                             std::size_t nx,std::size_t ny) {
        return (k*(ny+1)+j)*nx+i;
    }
    static std::size_t wface(std::size_t i,std::size_t j,std::size_t k,
                             std::size_t nx,std::size_t ny) {
        return (k*ny+j)*nx+i;
    }
public:
    // MAC field must already be discretely divergence-free and periodic.
    // A first-order upwind FV step: face flux = velocity * donor fraction.
    // No clamping: an invalid bound/conservation condition fails atomically.
    static P2PeriodicMacVofTransport3D transport_fraction(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,
        const Vec& u,const Vec& v,const Vec& w,const Vec& fraction) {
        if(nx<2||ny<2||nz<2||ny>32768/nx||nz>32768/(nx*ny)||
           !std::isfinite(dx_m)||dx_m<=0.0||
           !std::isfinite(dt_s)||dt_s<=0.0||
           fraction.size()!=nx*ny*nz||
           u.size()!=(nx+1)*ny*nz||
           v.size()!=nx*(ny+1)*nz||
           w.size()!=nx*ny*(nz+1))
            throw std::invalid_argument("Invalid periodic MAC VOF dimensions");
        for(double a:fraction)
            if(!std::isfinite(a)||a<0.0||a>1.0)
                throw std::invalid_argument("Invalid water fraction");
        for(const Vec* field:{&u,&v,&w})
            for(double x:*field)
                if(!std::isfinite(x))
                    throw std::invalid_argument("Nonfinite MAC velocity");
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i) {
            if(i==0&&std::abs(u[uface(nx,j,k,nx,ny)]-u[uface(0,j,k,nx,ny)])>1e-12)
                throw std::invalid_argument("Nonperiodic U seam");
            if(j==0&&std::abs(v[vface(i,ny,k,nx,ny)]-v[vface(i,0,k,nx,ny)])>1e-12)
                throw std::invalid_argument("Nonperiodic V seam");
            if(k==0&&std::abs(w[wface(i,j,nz,nx,ny)]-w[wface(i,j,0,nx,ny)])>1e-12)
                throw std::invalid_argument("Nonperiodic W seam");
        }
        const double lambda=dt_s/dx_m;
        if(!std::isfinite(lambda))
            throw std::invalid_argument("Invalid VOF time scale");
        P2PeriodicMacVofTransport3D result;
        result.water_fraction.resize(fraction.size());
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i) {
            const auto at=[&](std::size_t x,std::size_t y,std::size_t z) {
                return fraction[cell(x,y,z,nx,ny)];
            };
            const double c=at(i,j,k);
            const double xl=at((i+nx-1)%nx,j,k),xr=at((i+1)%nx,j,k);
            const double yl=at(i,(j+ny-1)%ny,k),yr=at(i,(j+1)%ny,k);
            const double zl=at(i,j,(k+nz-1)%nz),zr=at(i,j,(k+1)%nz);
            const double ux0=u[uface(i,j,k,nx,ny)],ux1=u[uface(i+1,j,k,nx,ny)];
            const double vy0=v[vface(i,j,k,nx,ny)],vy1=v[vface(i,j+1,k,nx,ny)];
            const double wz0=w[wface(i,j,k,nx,ny)],wz1=w[wface(i,j,k+1,nx,ny)];
            const double div=(ux1-ux0+vy1-vy0+wz1-wz0)/dx_m;
            const double speed=std::max({std::abs(ux0),std::abs(ux1),
                                         std::abs(vy0),std::abs(vy1),
                                         std::abs(wz0),std::abs(wz1)});
            if(std::abs(div)>1e-9*std::max(1.0,speed/dx_m))
                throw std::invalid_argument("VOF advector not divergence-free");
            const double outgoing=lambda*(std::max(ux1,0.0)+std::max(-ux0,0.0)+
                std::max(vy1,0.0)+std::max(-vy0,0.0)+
                std::max(wz1,0.0)+std::max(-wz0,0.0));
            if(!std::isfinite(outgoing)||outgoing>1.0+1e-12)
                throw std::invalid_argument("VOF outgoing CFL exceeded");
            result.max_cell_courant=std::max(result.max_cell_courant,outgoing);
            const double flux=(ux1*(ux1>=0?c:xr)-ux0*(ux0>=0?xl:c))+
                              (vy1*(vy1>=0?c:yr)-vy0*(vy0>=0?yl:c))+
                              (wz1*(wz1>=0?c:zr)-wz0*(wz0>=0?zl:c));
            const double next=c-lambda*flux;
            if(!std::isfinite(next)||next< -1e-10||next>1.0+1e-10)
                throw std::runtime_error("VOF update outside physical bounds");
            result.water_fraction[cell(i,j,k,nx,ny)]=next;
            result.initial_fraction_sum+=c;
            result.final_fraction_sum+=next;
        }
        if(std::abs(result.final_fraction_sum-result.initial_fraction_sum)>
           1e-9*std::max(1.0,result.initial_fraction_sum))
            throw std::runtime_error("VOF periodic mass not conserved");
        return result;
    }

    // Projection is used for the velocity transporting the VOF fraction.
    // Inputs are const: failed momentum/CFL/VOF steps never alter caller state.
    static P2PeriodicMacVofAdvance3D advance(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,double density_kg_m3,
        const Vec& u,const Vec& v,const Vec& w,const Vec& fraction) {
        // Validate water before doing the expensive nonlinear PCG solve.
        if(nx<2||ny<2||nz<2||ny>32768/nx||nz>32768/(nx*ny)||
           fraction.size()!=nx*ny*nz)
            throw std::invalid_argument("Invalid MAC VOF water dimensions");
        for(double a:fraction)
            if(!std::isfinite(a)||a<0.0||a>1.0)
                throw std::invalid_argument("Invalid MAC VOF water fraction");
        auto mac=P2PeriodicMacProjection3D::self_advect_project(
            nx,ny,nz,dx_m,dt_s,density_kg_m3,u,v,w);
        auto vof=transport_fraction(nx,ny,nz,dx_m,dt_s,
            mac.projected.u_m_s,mac.projected.v_m_s,mac.projected.w_m_s,fraction);
        return {std::move(mac),std::move(vof)};
    }
};
} // namespace upc
