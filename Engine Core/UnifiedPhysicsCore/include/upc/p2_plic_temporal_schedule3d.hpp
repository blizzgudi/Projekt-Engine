#pragma once
// P2 PARTIAL: prescribed piecewise-constant, divergence-free periodic
// 3D MAC face velocities drive geometric PLIC transport across time intervals.
// This is NOT a Navier-Stokes velocity update, measured CFD, or FSI.
#include "upc/p2_plic_adaptive_subcycle3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PlicTemporalSegment3D {
    double duration_s=0;
    std::array<std::vector<double>,3> positive_face_velocity;
};
struct P2PlicTemporalSchedule3D {
    std::vector<double> water_fill;
    std::vector<std::size_t> substeps_per_segment;
    std::size_t segments=0, substeps=0, transported_faces=0;
    double total_time_s=0, initial_volume_m3=0, final_volume_m3=0;
    double mass_residual_m3=0, max_six_face_courant_per_substep=0;
    double max_divergence_s_inv=0;
};

// Reject an invalid schedule before any work; each segment's velocity is
// frozen only for its own interval. Inputs remain unchanged on all failures.
// A returned result is atomic; an exception exposes no partially moved water.
inline P2PlicTemporalSchedule3D p2_plic_temporal_schedule3d(
    const std::vector<double>& fill,
    const std::vector<P2PlicTemporalSegment3D>& schedule,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,
    double target_six_face_courant=0.4,
    std::size_t max_total_substeps=1024) {
    if(schedule.empty() || schedule.size()>256 ||
       max_total_substeps==0 || max_total_substeps>4096 ||
       !std::isfinite(target_six_face_courant) ||
       target_six_face_courant<=0 || target_six_face_courant>1)
        throw std::invalid_argument("Invalid temporal PLIC schedule budget");
    if(nx<3||ny<3||nz<3 ||
       nx>std::numeric_limits<std::size_t>::max()/ny ||
       nx*ny>std::numeric_limits<std::size_t>::max()/nz ||
       nx*ny*nz>2000000 || fill.size()!=nx*ny*nz ||
       !std::isfinite(dx)||dx<=0)
        throw std::invalid_argument("Invalid temporal PLIC grid");
    double total_time=0;
    for(const auto& seg:schedule) {
        if(!std::isfinite(seg.duration_s)||seg.duration_s<=0)
            throw std::invalid_argument("Invalid temporal PLIC duration");
        total_time+=seg.duration_s;
        if(!std::isfinite(total_time))
            throw std::invalid_argument("Temporal PLIC duration overflow");
        for(const auto& v:seg.positive_face_velocity) {
            if(v.size()!=fill.size())
                throw std::invalid_argument("Invalid temporal MAC face size");
            for(double x:v)
                if(!std::isfinite(x))
                    throw std::invalid_argument("Nonfinite temporal MAC face");
        }
    }
    P2PlicTemporalSchedule3D out;
    out.water_fill=fill;
    out.segments=schedule.size();
    out.total_time_s=total_time;
    out.substeps_per_segment.reserve(schedule.size());
    for(const auto& seg:schedule) {
        const auto remaining=max_total_substeps-out.substeps;
        if(remaining==0)
            throw std::invalid_argument("Temporal PLIC work budget exhausted");
        const auto r=p2_plic_adaptive_subcycle3d(
            out.water_fill,seg.positive_face_velocity,nx,ny,nz,dx,
            seg.duration_s,target_six_face_courant,remaining);
        if(out.substeps==0)out.initial_volume_m3=r.initial_volume_m3;
        out.water_fill=r.water_fill;
        out.final_volume_m3=r.final_volume_m3;
        out.substeps+=r.substeps;
        out.substeps_per_segment.push_back(r.substeps);
        out.transported_faces+=r.transported_faces;
        out.max_six_face_courant_per_substep=std::max(
            out.max_six_face_courant_per_substep,
            r.max_six_face_courant_per_substep);
        out.max_divergence_s_inv=std::max(
            out.max_divergence_s_inv,r.max_divergence_s_inv);
    }
    out.mass_residual_m3=out.final_volume_m3-out.initial_volume_m3;
    if(!std::isfinite(out.mass_residual_m3) ||
       std::abs(out.mass_residual_m3)>
           1e-10*std::max(1.0,out.initial_volume_m3))
        throw std::runtime_error("Temporal PLIC cumulative mass imbalance");
    return out;
}
} // namespace upc
