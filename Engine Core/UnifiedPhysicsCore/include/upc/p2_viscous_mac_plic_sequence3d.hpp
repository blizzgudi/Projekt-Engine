#pragma once
// P2 PARTIAL: bounded temporal composition of the existing periodic viscous
// nonlinear MAC/projection + passive PLIC step. Not two-phase CFD or FSI.
#include "upc/p2_viscous_nonlinear_mac_plic3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2ViscousMacPlicInterval3D {
    double dt_s=0;
    double nu_m2_s=0;
};
struct P2ViscousMacPlicIntervalReport3D {
    double dt_s=0, nu_m2_s=0;
    std::size_t viscous_substeps=0, plic_substeps=0;
    double initial_volume_m3=0, final_volume_m3=0, mass_residual_m3=0;
    double max_projected_divergence_s_inv=0;
    double viscous_face_square_sum_before=0, viscous_face_square_sum_after=0;
    std::array<double,3> viscous_unique_face_sum_delta{{0,0,0}};
};
struct P2ViscousMacPlicSequence3D {
    P2NonlinearMacPlicState3D state;
    std::vector<P2ViscousMacPlicIntervalReport3D> intervals;
    double elapsed_s=0, total_mass_residual_m3=0;
    std::size_t total_viscous_substeps=0, total_plic_substeps=0;
};

// The input is never mutated. A failed later interval throws instead of
// returning a misleading partially advanced state. The global budgets count
// actual substeps, not merely intervals. Per-step projection is recomputed.
inline P2ViscousMacPlicSequence3D p2_viscous_mac_plic_sequence3d(
    const P2NonlinearMacPlicState3D& initial,
    const std::vector<P2ViscousMacPlicInterval3D>& schedule,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double density_kg_m3,
    double target_six_face_courant=0.4,
    std::size_t max_total_plic_substeps=256,
    std::size_t max_total_viscous_substeps=256) {
    if(schedule.empty()||schedule.size()>128||
       max_total_plic_substeps==0||max_total_viscous_substeps==0||
       max_total_plic_substeps>4096||max_total_viscous_substeps>4096||
       !std::isfinite(target_six_face_courant)||
       target_six_face_courant<=0||target_six_face_courant>1)
        throw std::invalid_argument("Invalid MAC/PLIC temporal schedule or budgets");
    double duration=0;
    for(const auto& interval:schedule) {
        if(!std::isfinite(interval.dt_s)||interval.dt_s<=0||
           !std::isfinite(interval.nu_m2_s)||interval.nu_m2_s<0||
           !std::isfinite(duration+interval.dt_s))
            throw std::invalid_argument("Invalid MAC/PLIC interval");
        duration+=interval.dt_s;
    }
    P2ViscousMacPlicSequence3D out;
    out.state=initial;
    out.intervals.reserve(schedule.size());
    for(const auto& interval:schedule) {
        const auto plic_left=max_total_plic_substeps-out.total_plic_substeps;
        const auto viscous_left=max_total_viscous_substeps-out.total_viscous_substeps;
        if(plic_left==0||(interval.nu_m2_s>0&&viscous_left==0))
            throw std::invalid_argument("MAC/PLIC cumulative substep budget exhausted");
        const auto step=p2_viscous_nonlinear_mac_plic_step3d(
            out.state,nx,ny,nz,dx_m,interval.dt_s,density_kg_m3,
            interval.nu_m2_s,target_six_face_courant,plic_left,
            viscous_left==0?1:viscous_left);
        const auto plic_count=step.coupled.transport.substeps;
        if(plic_count==0||plic_count>plic_left||
           step.viscous_substeps>viscous_left)
            throw std::runtime_error("Invalid MAC/PLIC step budget accounting");
        P2ViscousMacPlicIntervalReport3D report;
        report.dt_s=interval.dt_s;
        report.nu_m2_s=interval.nu_m2_s;
        report.viscous_substeps=step.viscous_substeps;
        report.plic_substeps=plic_count;
        report.initial_volume_m3=step.coupled.transport.initial_volume_m3;
        report.final_volume_m3=step.coupled.transport.final_volume_m3;
        report.mass_residual_m3=step.coupled.transport.mass_residual_m3;
        report.max_projected_divergence_s_inv=
            step.coupled.projection.max_divergence_s_inv;
        report.viscous_face_square_sum_before=step.face_square_sum_before;
        report.viscous_face_square_sum_after=step.face_square_sum_after;
        report.viscous_unique_face_sum_delta=step.unique_face_sum_delta;
        out.total_viscous_substeps+=step.viscous_substeps;
        out.total_plic_substeps+=plic_count;
        out.total_mass_residual_m3+=report.mass_residual_m3;
        out.state=step.coupled.state;
        out.intervals.push_back(report);
        out.elapsed_s+=interval.dt_s;
    }
    return out;
}
} // namespace upc
