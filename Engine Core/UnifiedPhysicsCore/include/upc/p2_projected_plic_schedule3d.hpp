#pragma once
// P2 PARTIAL: constant-density periodic pressure projection of prescribed
// staggered MAC fields before geometric VOF/PLIC transport.
// NOT a Navier-Stokes predictor, two-phase pressure solve, moving FSI or CFD.
#include "upc/p2_periodic_mac_projection3d.hpp"
#include "upc/p2_plic_temporal_schedule3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2ProjectedPlicSegment3D {
    double duration_s=0;
    // Conventional staggered MAC arrays: (nx+1)*ny*nz,
    // nx*(ny+1)*nz, nx*ny*(nz+1), including periodic duplicate seams.
    std::array<std::vector<double>,3> staggered_velocity;
};
struct P2ProjectedPlicSchedule3D {
    P2PlicTemporalSchedule3D transport;
    std::vector<double> divergence_before_s_inv, divergence_after_s_inv;
    std::vector<int> projection_iterations;
    double max_divergence_before_s_inv=0, max_divergence_after_s_inv=0;
};

// Each prescribed interval is projected independently, then its corrected
// face field is mapped to the +face convention used by the PLIC kernel.
// No input mutation. Projection/transport exceptions cannot leak partial output.
// The pressure projection uses the interval duration only as a scale factor:
// it does not predict a velocity from forces or fluid/solid stresses.
inline P2ProjectedPlicSchedule3D p2_projected_plic_schedule3d(
    const std::vector<double>& fill,
    const std::vector<P2ProjectedPlicSegment3D>& schedule,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx,double density_kg_m3,
    double target_six_face_courant=0.4,
    std::size_t max_total_substeps=1024) {
    if(nx<3||ny<3||nz<3||ny>32768/nx||nz>32768/(nx*ny) ||
       !std::isfinite(dx)||dx<=0||
       !std::isfinite(density_kg_m3)||density_kg_m3<=0 ||
       fill.size()!=nx*ny*nz)
        throw std::invalid_argument("Invalid projected PLIC grid/density");
    if(schedule.empty()||schedule.size()>256||
       max_total_substeps==0||max_total_substeps>4096||
       !std::isfinite(target_six_face_courant)||
       target_six_face_courant<=0||target_six_face_courant>1)
        throw std::invalid_argument("Invalid projected PLIC schedule budget");
    const std::array<std::size_t,3> sizes={
        (nx+1)*ny*nz,nx*(ny+1)*nz,nx*ny*(nz+1)};
    for(const auto& seg:schedule) {
        if(!std::isfinite(seg.duration_s)||seg.duration_s<=0)
            throw std::invalid_argument("Invalid projected PLIC interval");
        for(int a=0;a<3;++a)
            if(seg.staggered_velocity[a].size()!=sizes[a])
                throw std::invalid_argument("Invalid staggered MAC field size");
    }
    P2ProjectedPlicSchedule3D out;
    std::vector<P2PlicTemporalSegment3D> projected;
    projected.reserve(schedule.size());
    out.divergence_before_s_inv.reserve(schedule.size());
    out.divergence_after_s_inv.reserve(schedule.size());
    out.projection_iterations.reserve(schedule.size());
    for(const auto& seg:schedule) {
        const auto& vel=seg.staggered_velocity;
        const auto r=P2PeriodicMacProjection3D::project(
            nx,ny,nz,dx,seg.duration_s,density_kg_m3,
            vel[0],vel[1],vel[2]);
        if(!std::isfinite(r.max_divergence_s_inv)||
           r.max_divergence_s_inv>1e-10)
            throw std::runtime_error("Projected PLIC divergence too large");
        P2PlicTemporalSegment3D item;
        item.duration_s=seg.duration_s;
        for(auto& component:item.positive_face_velocity)
            component.resize(nx*ny*nz);
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i) {
            const auto cell=(k*ny+j)*nx+i;
            item.positive_face_velocity[0][cell]=
                r.u_m_s[(k*ny+j)*(nx+1)+(i+1)];
            item.positive_face_velocity[1][cell]=
                r.v_m_s[(k*(ny+1)+(j+1))*nx+i];
            item.positive_face_velocity[2][cell]=
                r.w_m_s[((k+1)*ny+j)*nx+i];
        }
        // Remove only projection roundoff on mathematically zero faces.
        // The downstream unsplit operator independently checks divergence.
        // This is not a physical velocity threshold or mass correction.
        for(auto& component:item.positive_face_velocity)
            for(double& speed:component)
                if(std::abs(speed)<1e-13)speed=0.0;
        out.max_divergence_before_s_inv=std::max(
            out.max_divergence_before_s_inv,r.initial_max_divergence_s_inv);
        out.max_divergence_after_s_inv=std::max(
            out.max_divergence_after_s_inv,r.max_divergence_s_inv);
        out.divergence_before_s_inv.push_back(r.initial_max_divergence_s_inv);
        out.divergence_after_s_inv.push_back(r.max_divergence_s_inv);
        out.projection_iterations.push_back(r.pcg_iterations);
        projected.push_back(std::move(item));
    }
    out.transport=p2_plic_temporal_schedule3d(
        fill,projected,nx,ny,nz,dx,
        target_six_face_courant,max_total_substeps);
    return out;
}
} // namespace upc
