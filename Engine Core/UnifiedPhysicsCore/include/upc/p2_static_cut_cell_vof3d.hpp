#pragma once
// P2 PARTIAL: first-order conservative 3D donor-cell VOF transport through
// stationary box cut-cell apertures using prescribed solenoidal MAC velocities.
// NOT a Navier-Stokes step, free-surface stress, or two-way moving-body FSI.
#include "upc/p2_moving_box_mac_apertures3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2StaticCutCellVofResult3D {
    std::vector<double> fraction_after;
    double water_before_m3=0.0;
    double water_after_m3=0.0;
    double max_cell_volume_divergence_m3=0.0;
    double max_donor_cfl=0.0;
    std::size_t substeps_used=1;
};
class P2StaticCutCellVof3D {
public:
    static P2StaticCutCellVofResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s) {
        if(!std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0)
            throw std::invalid_argument("Invalid static VOF dx/dt");
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,stationary_box,stationary_box);
        const std::size_t n=nx*ny*nz;
        if(fraction.size()!=n||u_m_s.size()!=(nx+1)*ny*nz||
           v_m_s.size()!=nx*(ny+1)*nz||w_m_s.size()!=nx*ny*(nz+1))
            throw std::invalid_argument("Static VOF array size mismatch");
        const double cell=dx*dx*dx;
        const double tol=1e-11*cell;
        std::vector<double> volume(n),change(n,0.0),divergence(n,0.0),
                            outgoing(n,0.0);
        P2StaticCutCellVofResult3D out;
        out.fraction_after.assign(n,0.0);
        for(std::size_t i=0;i<n;++i) {
            if(!std::isfinite(fraction[i])||fraction[i]<0||fraction[i]>1)
                throw std::invalid_argument("Invalid VOF fraction");
            volume[i]=geom.cell_geometry.alpha_old[i]*cell;
            if(volume[i]<=tol&&fraction[i]!=0)
                throw std::invalid_argument("Water fraction inside solid");
            out.water_before_m3+=volume[i]*fraction[i];
        }
        const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        const auto apply=[&](double velocity,double aperture,bool boundary,
                             std::size_t a,std::size_t b) {
            if(!std::isfinite(velocity))
                throw std::invalid_argument("Nonfinite MAC velocity");
            if(boundary||aperture<=1e-14) {
                if(std::abs(velocity)>1e-12)
                    throw std::invalid_argument("Flow through sealed face");
                return;
            }
            const double q=velocity*aperture*dx*dx*dt;
            if(!std::isfinite(q))
                throw std::invalid_argument("Nonfinite integrated volume flux");
            const double water=q*(q>=0?fraction[a]:fraction[b]);
            change[a]-=water;
            change[b]+=water;
            divergence[a]+=q;
            divergence[b]-=q;
            if(q>=0)outgoing[a]+=q;
            else outgoing[b]-=q;
        };
        std::size_t face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x,++face)
            apply(u_m_s[face],geom.u_open[face],x==0||x==nx,
                  x? id(x-1,y,z):0,x<nx?id(x,y,z):0);
        face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            apply(v_m_s[face],geom.v_open[face],y==0||y==ny,
                  y?id(x,y-1,z):0,y<ny?id(x,y,z):0);
        face=0;
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            apply(w_m_s[face],geom.w_open[face],z==0||z==nz,
                  z?id(x,y,z-1):0,z<nz?id(x,y,z):0);
        for(std::size_t i=0;i<n;++i) {
            out.max_cell_volume_divergence_m3=std::max(
                out.max_cell_volume_divergence_m3,std::abs(divergence[i]));
            if(std::abs(divergence[i])>tol)
                throw std::invalid_argument("Non-solenoidal MAC volume flux");
            if(outgoing[i]>volume[i]+tol)
                throw std::invalid_argument("Static VOF donor CFL exceeded");
            if(volume[i]>tol)
                out.max_donor_cfl=std::max(out.max_donor_cfl,
                                          outgoing[i]/volume[i]);
            const double next=volume[i]*fraction[i]+change[i];
            if(!std::isfinite(next)||next < -tol||next>volume[i]+tol)
                throw std::invalid_argument("Static VOF fraction not bounded");
            if(volume[i]>tol)
                out.fraction_after[i]=std::clamp(next/volume[i],0.0,1.0);
            else if(std::abs(next)>tol)
                throw std::invalid_argument("Water inside closed cell");
            out.water_after_m3+=volume[i]*out.fraction_after[i];
        }
        if(std::abs(out.water_after_m3-out.water_before_m3)>
           1e-9*std::max(1.0,out.water_before_m3))
            throw std::logic_error("Static VOF water mass changed");
        return out;
    }
    // Explicit temporal subcycling; prescribed MAC velocity and fixed geometry.
    // This is not a dynamic Navier-Stokes or two-way FSI integration.
    static P2StaticCutCellVofResult3D step_subcycled(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,std::size_t substeps) {
        if(substeps==0||substeps>1024)
            throw std::invalid_argument("Invalid VOF substep count");
        const double h=dt/static_cast<double>(substeps);
        if(!std::isfinite(dt)||dt<=0||!std::isfinite(h)||h<=0)
            throw std::invalid_argument("Invalid VOF subcycle time");
        auto state=fraction;
        P2StaticCutCellVofResult3D total;
        for(std::size_t k=0;k<substeps;++k) {
            auto next=step(nx,ny,nz,dx,h,stationary_box,state,u_m_s,v_m_s,w_m_s);
            if(k==0)total.water_before_m3=next.water_before_m3;
            total.water_after_m3=next.water_after_m3;
            total.max_donor_cfl=std::max(total.max_donor_cfl,next.max_donor_cfl);
            total.max_cell_volume_divergence_m3=std::max(
                total.max_cell_volume_divergence_m3,
                next.max_cell_volume_divergence_m3);
            state=std::move(next.fraction_after);
        }
        total.fraction_after=std::move(state);
        total.substeps_used=substeps;
        return total;
    }
    // Select a safe number of substeps from the *geometric* outgoing
    // donor-CFL, independent of the evolving water fraction. A pilot
    // step at dt/max_substeps validates the same sealed-face, cut-cell,
    // and solenoidal constraints as the production step. No solver CFL
    // or pressure dynamics are inferred from this passive VOF bound.
    static P2StaticCutCellVofResult3D step_auto_subcycled(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,double target_donor_cfl=0.8,
        std::size_t max_substeps=1024) {
        if(!std::isfinite(target_donor_cfl)||target_donor_cfl<=0||
           target_donor_cfl>1||max_substeps==0||max_substeps>1024)
            throw std::invalid_argument("Invalid automatic VOF CFL controls");
        if(!std::isfinite(dt)||dt<=0)
            throw std::invalid_argument("Invalid automatic VOF dt");
        const double pilot_dt=dt/static_cast<double>(max_substeps);
        if(!std::isfinite(pilot_dt)||pilot_dt<=0)
            throw std::invalid_argument("Invalid automatic VOF pilot dt");
        const auto pilot=step(nx,ny,nz,dx,pilot_dt,stationary_box,
                              fraction,u_m_s,v_m_s,w_m_s);
        const double full_cfl=pilot.max_donor_cfl*
                              static_cast<double>(max_substeps);
        const double ratio=full_cfl/target_donor_cfl;
        if(!std::isfinite(ratio)||ratio>static_cast<double>(max_substeps))
            throw std::invalid_argument("Automatic VOF CFL budget exceeded");
        const auto count=std::max<std::size_t>(
            1,static_cast<std::size_t>(std::ceil(ratio)));
        return step_subcycled(nx,ny,nz,dx,dt,stationary_box,fraction,
                              u_m_s,v_m_s,w_m_s,count);
    }
};
} // namespace upc
