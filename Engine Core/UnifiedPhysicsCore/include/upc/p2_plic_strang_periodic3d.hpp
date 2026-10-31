#pragma once
// P2 PARTIAL: symmetric direction-split 3D geometric PLIC transport.
// Each prescribed periodic MAC face component is constant along its own
// axis (transverse shear permitted). Not an unsplit arbitrary 3D MAC solver.
#include "upc/p2_plic_sheared_periodic3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PlicStrangPeriodic3D {
    std::vector<double> water_fill;
    double initial_volume_m3=0, final_volume_m3=0, mass_residual_m3=0;
    double max_stage_courant=0;
    std::size_t active_stages=0, reconstructed_partial_cells=0;
    std::size_t transported_faces=0;
};

// Central differences use periodic neighbors, including across all seams.
// A partial cell with no resolved gradient is rejected, never assigned an
// invented interface normal. Normal points from water to air.
inline std::vector<std::array<double,3>> p2_periodic_plic_normals3d(
    const std::vector<double>& fill,
    std::size_t nx,std::size_t ny,std::size_t nz) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<2||ny<2||nz<2||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>2000000||fill.size()!=nx*ny*nz)
        throw std::invalid_argument("Invalid periodic PLIC normal grid");
    std::vector<std::array<double,3>> normals(fill.size());
    const std::array<std::size_t,3> dims{nx,ny,nz};
    const auto index=[&](const std::array<std::size_t,3>& p){
        return (p[2]*ny+p[1])*nx+p[0];
    };
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(p);
        const double f=fill[i];
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid periodic PLIC fill");
        if(f==0||f==1)continue;
        std::array<double,3> g{};
        for(int a=0;a<3;++a){
            auto left=p,right=p;
            left[a]=(p[a]+dims[a]-1)%dims[a];
            right[a]=(p[a]+1)%dims[a];
            g[a]=0.5*(fill[index(right)]-fill[index(left)]);
        }
        const double scale=std::max({std::abs(g[0]),std::abs(g[1]),std::abs(g[2])});
        if(!std::isfinite(scale)||scale<=1e-12)
            throw std::invalid_argument("Unresolved periodic PLIC interface");
        for(double& v:g)v/=scale;
        const double length=std::sqrt(g[0]*g[0]+g[1]*g[1]+g[2]*g[2]);
        for(int a=0;a<3;++a)normals[i][a]=-g[a]/length;
    }
    return normals;
}

// Strang order X/2,Y/2,Z,Y/2,X/2; geometric normals are recomputed
// from the current VOF field before each active stage. Face velocities
// are prescribed and frozen; no pressure projection or fluid/solid forces.
inline P2PlicStrangPeriodic3D p2_plic_strang_periodic3d(
    const std::vector<double>& fill,
    const std::array<std::vector<double>,3>& positive_face_velocity,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<2||ny<2||nz<2||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>2000000||!std::isfinite(dx)||dx<=0||
       !std::isfinite(dt)||dt<=0)
        throw std::invalid_argument("Invalid Strang PLIC grid");
    const auto n=nx*ny*nz;
    if(fill.size()!=n)throw std::invalid_argument("Strang PLIC fill size");
    for(const auto& v:positive_face_velocity){
        if(v.size()!=n)throw std::invalid_argument("Strang PLIC velocity size");
        for(double s:v)if(!std::isfinite(s))
            throw std::invalid_argument("Nonfinite Strang PLIC velocity");
    }
    const double cell_volume=dx*dx*dx;
    if(!std::isfinite(cell_volume)||cell_volume<=0)
        throw std::invalid_argument("Invalid Strang PLIC cell volume");
    P2PlicStrangPeriodic3D out;
    out.water_fill=fill;
    double initial=0;
    for(double f:fill){
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid Strang PLIC fraction");
        initial+=f;
    }
    out.initial_volume_m3=initial*cell_volume;
    const std::array<int,5> axes{0,1,2,1,0};
    const std::array<double,5> fractions{0.5,0.5,1.0,0.5,0.5};
    for(std::size_t k=0;k<axes.size();++k){
        const int a=axes[k];
        const auto& velocities=positive_face_velocity[a];
        if(std::all_of(velocities.begin(),velocities.end(),
                       [](double s){return s==0;}))continue;
        const auto normals=p2_periodic_plic_normals3d(out.water_fill,nx,ny,nz);
        for(std::size_t i=0;i<n;++i)
            if(out.water_fill[i]>0&&out.water_fill[i]<1)
                ++out.reconstructed_partial_cells;
        auto stage=p2_plic_sheared_periodic3d(
            out.water_fill,normals,velocities,nx,ny,nz,
            dx,dt*fractions[k],a);
        out.max_stage_courant=std::max(out.max_stage_courant,stage.max_courant);
        out.transported_faces+=stage.transported_faces;
        ++out.active_stages;
        out.water_fill=std::move(stage.water_fill);
    }
    double final=0;
    for(double f:out.water_fill)final+=f;
    out.final_volume_m3=final*cell_volume;
    out.mass_residual_m3=out.final_volume_m3-out.initial_volume_m3;
    if(!std::isfinite(out.final_volume_m3)||
       std::abs(final-initial)>1e-10*std::max(1.0,initial))
        throw std::runtime_error("Strang PLIC volume imbalance");
    return out;
}
} // namespace upc
