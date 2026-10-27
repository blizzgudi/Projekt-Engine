#pragma once
// P2 PARTIAL: finite-volume geometric conservation law (GCL) for a moving
// binary voxel mask with externally supplied, time-integrated MAC face fluxes.
// This is a conservative scalar ALE-style remap, NOT a pressure/FSI solver.
// No face velocity is inferred from mask motion; the caller must provide a
// physically appropriate swept-boundary flux field.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct MovingBoundaryVofFluxResult3D {
    std::vector<double> water_fill;
    double initial_water_m3=0.0, final_water_m3=0.0;
    double mass_residual_m3=0.0;
    double max_gcl_residual_cell_volumes=0.0;
    double max_outgoing_courant=0.0;
    std::size_t active_faces=0, changed_solid_cells=0;
};

class MovingBoundaryVofFlux3D {
public:
    // old_body and new_body are binary occupancy at the beginning/end of
    // the time interval. Fixed solids remain impermeable throughout.
    // u,v,w are signed MAC FACE VELOCITIES integrated with explicit dt.
    // For each cell, the discrete geometric conservation law is:
    //   alpha_new = alpha_old - (dt/dx) * sum(outward face velocities).
    // alpha is 0 for solid and 1 for available fluid volume.
    // Transported WATER volume is donor_fill * (dt/dx)*face_velocity,
    // using OLD fluid fractions (first-order simultaneous flux update).
    // A face donor must be old-fluid, its receiver must be new-fluid;
    // the sum of outgoing Courant numbers per donor must not exceed 1.
    // A mask change alone is insufficient: an externally provided flux
    // field must satisfy the per-cell GCL, including vacated/entered cells.
    // Inputs are immutable; errors throw before a result is returned.
    static MovingBoundaryVofFluxResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,double dt_s,
        const std::vector<std::uint8_t>& old_body,
        const std::vector<std::uint8_t>& new_body,
        const std::vector<std::uint8_t>& fixed_solid,
        const std::vector<double>& old_fill,
        const std::vector<double>& u,
        const std::vector<double>& v,
        const std::vector<double>& w) {
        const auto lim=std::numeric_limits<std::size_t>::max();
        if(!nx||!ny||!nz||nx>lim/ny||nx*ny>lim/nz||
           nx==lim||ny==lim||nz==lim||
           !std::isfinite(dx_m)||dx_m<=0.0||
           !std::isfinite(dt_s)||dt_s<=0.0||
           !std::isfinite(dx_m*dx_m*dx_m)||
           !std::isfinite(dt_s/dx_m))
            throw std::invalid_argument("Invalid moving VOF domain/timestep");
        const std::size_t n=nx*ny*nz;
        if(n>2000000||nx+1>lim/(ny*nz)||
           ny+1>lim/(nx*nz)||nz+1>lim/(nx*ny))
            throw std::invalid_argument("Moving VOF grid limit");
        if(old_body.size()!=n||new_body.size()!=n||
           fixed_solid.size()!=n||old_fill.size()!=n||
           u.size()!=(nx+1)*ny*nz||
           v.size()!=nx*(ny+1)*nz||
           w.size()!=nx*ny*(nz+1))
            throw std::invalid_argument("Moving VOF array size mismatch");
        const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        const auto ui=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*(nx+1)+x;
        };
        const auto vi=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*(ny+1)+y)*nx+x;
        };
        const auto wi=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        MovingBoundaryVofFluxResult3D out;
        out.water_fill=old_fill;
        std::size_t old_count=0,new_count=0;
        double initial=0.0;
        for(std::size_t i=0;i<n;++i) {
            if(old_body[i]>1||new_body[i]>1||fixed_solid[i]>1||
               (fixed_solid[i]&&(old_body[i]||new_body[i]))||
               !std::isfinite(old_fill[i])||
               old_fill[i]<0.0||old_fill[i]>1.0||
               ((fixed_solid[i]||old_body[i])&&old_fill[i]!=0.0))
                throw std::invalid_argument("Invalid moving solid/VOF occupancy");
            old_count+=old_body[i];new_count+=new_body[i];
            out.changed_solid_cells+=(old_body[i]!=new_body[i]);
            initial+=old_fill[i];
        }
        if(old_count!=new_count)
            throw std::invalid_argument("Rigid mask must conserve solid cell count");
        for(double a:u)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite moving u face velocity");
        for(double a:v)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite moving v face velocity");
        for(double a:w)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite moving w face velocity");
        const double factor=dt_s/dx_m;
        const double cell_volume=dx_m*dx_m*dx_m;
        out.initial_water_m3=initial*cell_volume;
        std::vector<double> net_out(n,0.0),outgoing(n,0.0);
        auto flux=[&](double speed,bool has_left,std::size_t left,
                      bool has_right,std::size_t right) {
            if(speed==0.0)return;
            if(!has_left||!has_right)
                throw std::invalid_argument("Moving flux crosses outer boundary");
            const std::size_t donor=speed>0.0?left:right;
            const std::size_t receiver=speed>0.0?right:left;
            if(fixed_solid[donor]||fixed_solid[receiver]||
               old_body[donor]||new_body[receiver])
                throw std::invalid_argument("Moving flux crosses unavailable cell");
            const double courant=std::abs(speed)*factor;
            if(!std::isfinite(courant))
                throw std::invalid_argument("Nonfinite moving face Courant");
            outgoing[donor]+=courant;
            net_out[donor]+=courant;
            net_out[receiver]-=courant;
            const double water=courant*old_fill[donor];
            out.water_fill[donor]-=water;
            out.water_fill[receiver]+=water;
            ++out.active_faces;
        };
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x)
            flux(u[ui(x,y,z)],x>0,x?ci(x-1,y,z):0,
                 x<nx,x<nx?ci(x,y,z):0);
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x)
            flux(v[vi(x,y,z)],y>0,y?ci(x,y-1,z):0,
                 y<ny,y<ny?ci(x,y,z):0);
        for(std::size_t z=0;z<=nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            flux(w[wi(x,y,z)],z>0,z?ci(x,y,z-1):0,
                 z<nz,z<nz?ci(x,y,z):0);
        double final=0.0;
        for(std::size_t i=0;i<n;++i) {
            const double old_alpha=(old_body[i]||fixed_solid[i])?0.0:1.0;
            const double new_alpha=(new_body[i]||fixed_solid[i])?0.0:1.0;
            const double gcl=old_alpha-net_out[i]-new_alpha;
            out.max_gcl_residual_cell_volumes=
                std::max(out.max_gcl_residual_cell_volumes,std::abs(gcl));
            out.max_outgoing_courant=
                std::max(out.max_outgoing_courant,outgoing[i]);
            if(!std::isfinite(gcl)||std::abs(gcl)>1e-11)
                throw std::invalid_argument("Moving MAC flux violates local GCL");
            if(!std::isfinite(outgoing[i])||outgoing[i]>old_alpha+1e-12)
                throw std::invalid_argument("Moving VOF donor CFL exceeds volume");
            if(!std::isfinite(out.water_fill[i])||
               out.water_fill[i]<-1e-12||
               out.water_fill[i]>new_alpha+1e-12)
                throw std::logic_error("Moving VOF boundedness violation");
            out.water_fill[i]=std::max(0.0,std::min(new_alpha,out.water_fill[i]));
            final+=out.water_fill[i];
        }
        out.final_water_m3=final*cell_volume;
        out.mass_residual_m3=(final-initial)*cell_volume;
        if(std::abs(final-initial)>1e-10*std::max(1.0,initial))
            throw std::logic_error("Moving VOF water conservation failure");
        return out;
    }
};
} // namespace upc
