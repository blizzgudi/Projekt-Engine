#pragma once
// P2 PARTIAL: bounded CFL subcycling of a *prescribed*, frozen, periodic
// 3D staggered-MAC velocity field through the geometric PLIC flux operator.
// This is not a Navier-Stokes time integrator, FSI or a CFD validation.
#include "upc/p2_plic_unsplit_mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PlicAdaptiveSubcycle3D {
    std::vector<double> water_fill;
    std::size_t substeps=0, transported_faces=0;
    std::size_t reconstructed_partial_cell_visits=0;
    double max_six_face_courant_per_substep=0;
    double max_divergence_s_inv=0;
    double initial_volume_m3=0, final_volume_m3=0, mass_residual_m3=0;
};

// No silent truncation of the time interval: fail closed if the CFL
// requirement exceeds the explicit work budget. Each substep recomputes
// PLIC normals from the *updated* VOF field, and the underlying kernel
// still rejects overlapping geometric slabs, bad divergence and bad mass.
inline P2PlicAdaptiveSubcycle3D p2_plic_adaptive_subcycle3d(
    const std::vector<double>& fill,
    const std::array<std::vector<double>,3>& positive_face_velocity,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
    double target_six_face_courant=0.4,std::size_t max_substeps=256) {
    if(!std::isfinite(target_six_face_courant) ||
       target_six_face_courant<=0 || target_six_face_courant>1 ||
       max_substeps==0 || max_substeps>4096)
        throw std::invalid_argument("Invalid adaptive PLIC subcycling budget");
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>2000000||!std::isfinite(dx)||dx<=0||
       !std::isfinite(dt)||dt<=0)
        throw std::invalid_argument("Invalid adaptive PLIC grid or units");
    const std::size_t n=nx*ny*nz;
    if(fill.size()!=n)throw std::invalid_argument("Adaptive PLIC fill size");
    for(const auto& v:positive_face_velocity){
        if(v.size()!=n)throw std::invalid_argument("Adaptive MAC face size");
        for(double speed:v)
            if(!std::isfinite(speed))
                throw std::invalid_argument("Nonfinite adaptive MAC face");
    }
    const double scale=dt/dx;
    if(!std::isfinite(scale)||scale<=0)
        throw std::invalid_argument("Invalid adaptive PLIC Courant scale");
    const std::array<std::size_t,3> dims{nx,ny,nz};
    const auto index=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    double max_six_face=0;
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(x,y,z);
        double sum=0;
        for(int a=0;a<3;++a){
            auto prev=p;
            prev[a]=(prev[a]+dims[a]-1)%dims[a];
            sum+=std::abs(positive_face_velocity[a][i]);
            sum+=std::abs(positive_face_velocity[a][index(
                prev[0],prev[1],prev[2])]);
        }
        max_six_face=std::max(max_six_face,sum);
    }
    const double required=max_six_face*scale/target_six_face_courant;
    if(!std::isfinite(required) ||
       required>static_cast<double>(max_substeps))
        throw std::invalid_argument("Adaptive PLIC substep budget exceeded");
    const auto count=std::max<std::size_t>(
        1,static_cast<std::size_t>(std::ceil(required)));
    const double sub_dt=dt/static_cast<double>(count);
    if(!std::isfinite(sub_dt)||sub_dt<=0)
        throw std::invalid_argument("Adaptive PLIC substep underflow");
    P2PlicAdaptiveSubcycle3D out;
    out.water_fill=fill;
    out.substeps=count;
    for(std::size_t step=0;step<count;++step){
        const auto r=p2_plic_unsplit_mac3d(
            out.water_fill,positive_face_velocity,nx,ny,nz,dx,sub_dt);
        if(step==0)out.initial_volume_m3=r.initial_volume_m3;
        out.water_fill=r.water_fill;
        out.final_volume_m3=r.final_volume_m3;
        out.transported_faces+=r.transported_faces;
        out.reconstructed_partial_cell_visits+=r.reconstructed_partial_cells;
        out.max_six_face_courant_per_substep=std::max(
            out.max_six_face_courant_per_substep,
            r.max_cell_face_courant_sum);
        out.max_divergence_s_inv=std::max(
            out.max_divergence_s_inv,r.max_divergence_s_inv);
    }
    out.mass_residual_m3=out.final_volume_m3-out.initial_volume_m3;
    if(!std::isfinite(out.mass_residual_m3)||
       std::abs(out.mass_residual_m3)>1e-10*
          std::max(1.0,out.initial_volume_m3))
        throw std::runtime_error("Adaptive PLIC cumulative mass imbalance");
    return out;
}
} // namespace upc
