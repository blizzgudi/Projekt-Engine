#pragma once
// P2 PARTIAL: conservative SIGNED cell-centred scalar transport through
// projected moving-box cut-cell fluxes. This is a building block for momentum,
// NOT staggered MAC momentum advection or a physical two-way FSI solver.
#include "upc/p2_moving_box_cut_cell_projection3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MovingCutCellScalarResult3D {
    std::vector<double> value_new;
    double integral_before_m3=0.0, integral_after_m3=0.0;
    double integral_residual_m3=0.0;
    double max_gcl_residual=0.0, max_donor_courant=0.0;
    std::size_t active_faces=0;
};
class P2MovingCutCellScalarTransport3D {
public:
    // Values are signed per unit OPEN fluid volume (e.g. one Cartesian
    // velocity component). q is signed, time-integrated m^3 per MAC face.
    // Finite-volume upwind: alpha_new*s_new = alpha_old*s_old - div(q*s_upwind)/dx^3.
    // The pressure projection supplies q and geometric GCL; no extra wall
    // source is added. Donor swept CFL <= 1, closed boundaries and constant
    // state preservation are checked. No input or solver state is mutated.
    static P2MovingCutCellScalarResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        const std::vector<double>& value_old,
        const P2MovingBoxCutCellProjection3D& projected) {
        if(nx<2||ny<2||nz<2||nx>32768/ny||nx*ny>32768/nz||
           !std::isfinite(dx)||dx<=0.0)
            throw std::invalid_argument("Invalid cut-cell scalar grid");
        const auto n=nx*ny*nz;
        const auto& geom=projected.wall.geometry;
        const auto& old_a=geom.cell_geometry.alpha_old;
        const auto& new_a=geom.cell_geometry.alpha_new;
        const auto& u=projected.u_fluid_m3;
        const auto& v=projected.v_fluid_m3;
        const auto& w=projected.w_fluid_m3;
        if(value_old.size()!=n||old_a.size()!=n||new_a.size()!=n||
           u.size()!=(nx+1)*ny*nz||v.size()!=nx*(ny+1)*nz||
           w.size()!=nx*ny*(nz+1))
            throw std::invalid_argument("Cut-cell scalar array shape");
        const double volume=dx*dx*dx;
        if(!std::isfinite(volume)||volume<=0.0)
            throw std::invalid_argument("Cut-cell scalar volume overflow");
        const auto ci=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        P2MovingCutCellScalarResult3D result;
        std::vector<double> mass(n,0.0),outgoing(n,0.0),net(n,0.0);
        double before=0.0,old_open=0.0,new_open=0.0;
        double low=std::numeric_limits<double>::infinity();
        double high=-std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i<n;++i) {
            if(!std::isfinite(old_a[i])||!std::isfinite(new_a[i])||
               !std::isfinite(value_old[i])||old_a[i]<0.0||
               old_a[i]>1.0||new_a[i]<0.0||new_a[i]>1.0)
                throw std::invalid_argument("Invalid cut-cell scalar occupancy");
            if(old_a[i]==0.0&&value_old[i]!=0.0)
                throw std::invalid_argument("Nonzero scalar in closed cell");
            mass[i]=old_a[i]*value_old[i];
            before+=mass[i];
            old_open+=old_a[i];new_open+=new_a[i];
            if(old_a[i]>0.0) {
                low=std::min(low,value_old[i]);
                high=std::max(high,value_old[i]);
            }
        }
        const auto face=[&](double q,bool left_exists,std::size_t left,
                            bool right_exists,std::size_t right) {
            if(!std::isfinite(q))
                throw std::invalid_argument("Nonfinite cut-cell scalar flux");
            if(q==0.0)return;
            if(!left_exists||!right_exists)
                throw std::invalid_argument("Cut-cell scalar exterior flux");
            const auto donor=q>0.0?left:right;
            const auto receiver=q>0.0?right:left;
            if(old_a[donor]<=0.0||new_a[receiver]<=0.0)
                throw std::invalid_argument("Cut-cell scalar closed donor/receiver");
            const double swept=std::abs(q)/volume;
            if(!std::isfinite(swept))
                throw std::invalid_argument("Cut-cell scalar swept overflow");
            outgoing[donor]+=swept;
            net[donor]+=swept;
            net[receiver]-=swept;
            const double moved=swept*value_old[donor];
            mass[donor]-=moved;
            mass[receiver]+=moved;
            ++result.active_faces;
        };
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x)
            face(u[(z*ny+y)*(nx+1)+x],x>0,x?ci(x-1,y,z):0,
                 x<nx,x<nx?ci(x,y,z):0);
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x)
            face(v[(z*(ny+1)+y)*nx+x],y>0,y?ci(x,y-1,z):0,
                 y<ny,y<ny?ci(x,y,z):0);
        for(std::size_t z=0;z<=nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            face(w[(z*ny+y)*nx+x],z>0,z?ci(x,y,z-1):0,
                 z<nz,z<nz?ci(x,y,z):0);
        result.value_new.assign(n,0.0);
        double after=0.0;
        for(std::size_t i=0;i<n;++i) {
            const double gcl=old_a[i]-net[i]-new_a[i];
            result.max_gcl_residual=std::max(result.max_gcl_residual,std::abs(gcl));
            if(!std::isfinite(gcl)||std::abs(gcl)>1e-10)
                throw std::invalid_argument("Cut-cell scalar GCL mismatch");
            if(!std::isfinite(outgoing[i])||
               outgoing[i]>old_a[i]+1e-11)
                throw std::invalid_argument("Cut-cell scalar donor CFL exceeded");
            if(old_a[i]>0.0)
                result.max_donor_courant=std::max(
                    result.max_donor_courant,outgoing[i]/old_a[i]);
            if(new_a[i]>1e-12) {
                const double s=mass[i]/new_a[i];
                if(!std::isfinite(s)||s<low-1e-8*std::max(1.0,std::abs(low))||
                   s>high+1e-8*std::max(1.0,std::abs(high)))
                    throw std::runtime_error("Cut-cell scalar bounds violated");
                result.value_new[i]=s;
            } else if(std::abs(mass[i])>1e-10*std::max(1.0,std::abs(before)))
                throw std::runtime_error("Cut-cell scalar trapped in closed cell");
            after+=mass[i];
        }
        result.integral_before_m3=before*volume;
        result.integral_after_m3=after*volume;
        result.integral_residual_m3=(after-before)*volume;
        if(!std::isfinite(result.integral_residual_m3)||
           std::abs(after-before)>1e-10*std::max(1.0,std::abs(before))||
           std::abs(new_open-old_open)>1e-10*std::max(1.0,old_open))
            throw std::runtime_error("Cut-cell scalar global conservation");
        return result;
    }
};
} // namespace upc
