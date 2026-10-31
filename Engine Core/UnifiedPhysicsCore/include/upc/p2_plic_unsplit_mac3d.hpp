#pragma once
// P2 PARTIAL: simultaneous periodic 3D geometric PLIC fluxes driven by
// arbitrary *prescribed* divergence-free staggered MAC face velocities.
// No pressure solve, body motion, free-surface stress or global FSI claim.
#include "upc/p2_plic_strang_periodic3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PlicUnsplitMac3D {
    std::vector<double> water_fill;
    double initial_volume_m3=0, final_volume_m3=0, mass_residual_m3=0;
    double max_cell_face_courant_sum=0, max_divergence_s_inv=0;
    std::size_t transported_faces=0, positive_faces=0, negative_faces=0;
    std::size_t reconstructed_partial_cells=0;
};

// The positive-face array for axis a stores velocity through cell i's +a
// face, not cell-centred velocity. All 6 faces contribute to each cell's
// divergence. Fluxes are computed from the *same frozen input* and applied
// atomically. The sum of absolute Courants across 6 faces must be <=1.
// If multi-axis swept slabs overlap enough to violate VOF bounds, reject
// rather than clamp, fabricate a volume correction, or claim stability.
inline P2PlicUnsplitMac3D p2_plic_unsplit_mac3d(
    const std::vector<double>& fill,
    const std::array<std::vector<double>,3>& positive_face_velocity,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>2000000||!std::isfinite(dx)||dx<=0||
       !std::isfinite(dt)||dt<=0)
        throw std::invalid_argument("Invalid unsplit MAC PLIC grid");
    const auto n=nx*ny*nz;
    if(fill.size()!=n)throw std::invalid_argument("Unsplit PLIC fill size");
    const double volume=dx*dx*dx, scale=dt/dx;
    if(!std::isfinite(volume)||volume<=0||
       !std::isfinite(scale)||scale<=0)
        throw std::invalid_argument("Invalid unsplit MAC PLIC units");
    for(double f:fill)
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid unsplit PLIC fill");
    double vmax=0;
    for(const auto& v:positive_face_velocity){
        if(v.size()!=n)throw std::invalid_argument("Unsplit MAC face size");
        for(double s:v){
            if(!std::isfinite(s))
                throw std::invalid_argument("Nonfinite unsplit MAC face");
            vmax=std::max(vmax,std::abs(s));
        }
    }
    P2PlicUnsplitMac3D out;
    out.water_fill=fill;
    double initial=0;
    for(double f:fill)initial+=f;
    out.initial_volume_m3=initial*volume;
    if(vmax==0){
        out.final_volume_m3=out.initial_volume_m3;
        return out;
    }
    const std::array<std::size_t,3> dims{nx,ny,nz};
    const auto index=[&](const std::array<std::size_t,3>& p){
        return (p[2]*ny+p[1])*nx+p[0];
    };
    // Verify true 3D MAC divergence, including periodic seams. Unlike the
    // lane-shear operator, each component may vary along its own axis.
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(p);
        double div=0,face_sum=0;
        for(int a=0;a<3;++a){
            auto prev=p;
            prev[a]=(p[a]+dims[a]-1)%dims[a];
            const double plus=positive_face_velocity[a][i];
            const double minus=positive_face_velocity[a][index(prev)];
            div+=(plus-minus)/dx;
            face_sum+=(std::abs(plus)+std::abs(minus))*scale;
        }
        out.max_divergence_s_inv=std::max(out.max_divergence_s_inv,std::abs(div));
        out.max_cell_face_courant_sum=std::max(
            out.max_cell_face_courant_sum,face_sum);
        if(std::abs(div)>1e-10*std::max(1.0,vmax/dx))
            throw std::invalid_argument("Unsplit MAC field not solenoidal");
        if(!std::isfinite(face_sum)||face_sum>1.0+1e-12)
            throw std::invalid_argument("Unsplit MAC six-face CFL exceeded");
    }
    const auto normals=p2_periodic_plic_normals3d(fill,nx,ny,nz);
    for(double f:fill)
        if(f>0&&f<1)++out.reconstructed_partial_cells;
    std::vector<double> delta(n,0.0);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(p);
        for(int a=0;a<3;++a){
            const double s=positive_face_velocity[a][i];
            if(s==0)continue;
            auto next=p;
            next[a]=(next[a]+1)%dims[a];
            const auto j=index(next);
            const auto donor=s>0?i:j;
            const auto receiver=s>0?j:i;
            const double c=std::abs(s)*scale;
            const double swept=p2_plic_outgoing_slab3d(
                fill[donor],normals[donor],a,s>0?1:-1,c);
            if(swept==0)continue;
            delta[donor]-=swept;
            delta[receiver]+=swept;
            ++out.transported_faces;
            if(s>0)++out.positive_faces;else ++out.negative_faces;
        }
    }
    double final=0;
    for(std::size_t i=0;i<n;++i){
        const double f=fill[i]+delta[i];
        if(!std::isfinite(f)||f < -1e-10||f>1+1e-10)
            throw std::runtime_error(
                "Unsplit geometric slabs overlap or overfill a cell");
        // Roundoff-only clamping; the mass residual below catches any loss.
        out.water_fill[i]=std::clamp(f,0.0,1.0);
        final+=out.water_fill[i];
    }
    out.final_volume_m3=final*volume;
    out.mass_residual_m3=out.final_volume_m3-out.initial_volume_m3;
    if(!std::isfinite(out.final_volume_m3)||
       std::abs(final-initial)>1e-10*std::max(1.0,initial))
        throw std::runtime_error("Unsplit PLIC mass imbalance");
    return out;
}
} // namespace upc
