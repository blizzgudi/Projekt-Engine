#pragma once
// P2 PARTIAL: bounded variable-dt/viscosity periodic MAC/PLIC schedule
// with opt-in endpoint-only roundoff cleanup and per-interval mass audit.
// This is not stress-consistent two-phase CFD, FSI, or a full P2 acceptance.
#include "upc/p2_viscous_mac_plic_roundoff_sequence3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2RoundoffMacPlicInterval3D {
    double dt_s=0;
    double nu_m2_s=0;
};
struct P2RoundoffMacPlicIntervalAudit3D {
    double dt_s=0, nu_m2_s=0;
    std::size_t viscous_substeps=0, plic_substeps=0, snapped_cells=0;
    double water_before_m3=0, water_after_m3=0;
    double transport_residual_m3=0, signed_snap_m3=0, absolute_snap_m3=0;
    double ledger_residual_m3=0, max_divergence_s_inv=0;
};
struct P2RoundoffMacPlicSchedule3D {
    P2NonlinearMacPlicState3D state;
    std::vector<P2RoundoffMacPlicIntervalAudit3D> intervals;
    double elapsed_s=0, initial_volume_m3=0, final_volume_m3=0;
    double signed_snap_m3=0, absolute_snap_m3=0;
    double total_transport_residual_m3=0, total_ledger_residual_m3=0;
    double max_divergence_s_inv=0;
    std::size_t total_snapped_cells=0;
    std::size_t total_viscous_substeps=0, total_plic_substeps=0;
};

inline P2RoundoffMacPlicSchedule3D p2_roundoff_mac_plic_schedule3d(
    const P2NonlinearMacPlicState3D& initial,
    const std::vector<P2RoundoffMacPlicInterval3D>& schedule,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double density_kg_m3,
    double target_six_face_courant=0.4,
    std::size_t max_total_plic_substeps=4096,
    std::size_t max_total_viscous_substeps=4096) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>32768||initial.water_fill.size()!=nx*ny*nz||
       !std::isfinite(dx_m)||dx_m<=0||
       !std::isfinite(density_kg_m3)||density_kg_m3<=0||
       !std::isfinite(target_six_face_courant)||
       target_six_face_courant<=0||target_six_face_courant>1||
       schedule.empty()||schedule.size()>256||
       max_total_plic_substeps==0||max_total_viscous_substeps==0||
       max_total_plic_substeps>4096||max_total_viscous_substeps>4096)
        throw std::invalid_argument("Invalid roundoff MAC/PLIC schedule");
    const double cell_volume=dx_m*dx_m*dx_m;
    if(!std::isfinite(cell_volume)||cell_volume<=0)
        throw std::invalid_argument("Invalid roundoff schedule cell volume");
    double duration=0;
    for(const auto& interval:schedule) {
        if(!std::isfinite(interval.dt_s)||interval.dt_s<=0||
           !std::isfinite(interval.nu_m2_s)||interval.nu_m2_s<0||
           !std::isfinite(duration+interval.dt_s))
            throw std::invalid_argument("Invalid roundoff MAC/PLIC interval");
        duration+=interval.dt_s;
    }
    auto volume_cells=[](const std::vector<double>& fill) {
        long double sum=0;
        for(double f:fill) {
            if(!std::isfinite(f)||f<0||f>1)
                throw std::invalid_argument("Invalid roundoff schedule water fraction");
            sum+=f;
        }
        return sum;
    };
    P2RoundoffMacPlicSchedule3D out;
    out.state=initial;
    out.intervals.reserve(schedule.size());
    const long double original_cells=volume_cells(initial.water_fill);
    out.initial_volume_m3=static_cast<double>(original_cells)*cell_volume;
    long double signed_snaps=0,absolute_snaps=0;
    const auto cleanup=[&]() {
        auto fixed=p2_plic_endpoint_roundoff3d(out.state.water_fill);
        out.state.water_fill=std::move(fixed.fill);
        out.total_snapped_cells+=fixed.snapped_cells;
        signed_snaps+=fixed.signed_correction_cells;
        absolute_snaps+=fixed.absolute_correction_cells;
        if(absolute_snaps>1e-10L*std::max(1.0L,original_cells))
            throw std::runtime_error("Roundoff schedule snap budget exceeded");
        return fixed;
    };
    cleanup(); // Initial correction is included in the global inventory.
    for(const auto& interval:schedule) {
        const std::size_t plic_left=max_total_plic_substeps-out.total_plic_substeps;
        const std::size_t visc_left=max_total_viscous_substeps-out.total_viscous_substeps;
        if(plic_left==0||(interval.nu_m2_s>0&&visc_left==0))
            throw std::invalid_argument("Roundoff schedule substep budget exhausted");
        const long double before=volume_cells(out.state.water_fill);
        auto step=p2_viscous_nonlinear_mac_plic_step3d(
            out.state,nx,ny,nz,dx_m,interval.dt_s,density_kg_m3,
            interval.nu_m2_s,target_six_face_courant,
            std::min<std::size_t>(plic_left,256),
            std::min<std::size_t>(std::max<std::size_t>(visc_left,1),256));
        const auto plic_count=step.coupled.transport.substeps;
        if(plic_count==0||plic_count>plic_left||
           step.viscous_substeps>visc_left)
            throw std::runtime_error("Invalid roundoff schedule substep accounting");
        out.state=std::move(step.coupled.state);
        const auto fixed=cleanup();
        const long double after=volume_cells(out.state.water_fill);
        P2RoundoffMacPlicIntervalAudit3D audit;
        audit.dt_s=interval.dt_s;
        audit.nu_m2_s=interval.nu_m2_s;
        audit.viscous_substeps=step.viscous_substeps;
        audit.plic_substeps=plic_count;
        audit.snapped_cells=fixed.snapped_cells;
        audit.water_before_m3=static_cast<double>(before)*cell_volume;
        audit.water_after_m3=static_cast<double>(after)*cell_volume;
        audit.transport_residual_m3=step.coupled.transport.mass_residual_m3;
        audit.signed_snap_m3=fixed.signed_correction_cells*cell_volume;
        audit.absolute_snap_m3=fixed.absolute_correction_cells*cell_volume;
        audit.ledger_residual_m3=audit.water_after_m3-audit.water_before_m3-
            audit.transport_residual_m3-audit.signed_snap_m3;
        audit.max_divergence_s_inv=step.coupled.projection.max_divergence_s_inv;
        const double tolerance=1e-10*std::max(1.0,out.initial_volume_m3);
        if(!std::isfinite(audit.ledger_residual_m3)||
           std::abs(audit.ledger_residual_m3)>tolerance)
            throw std::runtime_error("Roundoff schedule interval mass ledger invalid");
        out.intervals.push_back(audit);
        out.total_viscous_substeps+=audit.viscous_substeps;
        out.total_plic_substeps+=audit.plic_substeps;
        out.total_transport_residual_m3+=audit.transport_residual_m3;
        out.max_divergence_s_inv=std::max(
            out.max_divergence_s_inv,audit.max_divergence_s_inv);
        out.elapsed_s+=interval.dt_s;
    }
    out.final_volume_m3=static_cast<double>(
        volume_cells(out.state.water_fill))*cell_volume;
    out.signed_snap_m3=static_cast<double>(signed_snaps)*cell_volume;
    out.absolute_snap_m3=static_cast<double>(absolute_snaps)*cell_volume;
    out.total_ledger_residual_m3=out.final_volume_m3-out.initial_volume_m3-
        out.signed_snap_m3-out.total_transport_residual_m3;
    if(!std::isfinite(out.total_ledger_residual_m3)||
       std::abs(out.total_ledger_residual_m3)>
           1e-10*std::max(1.0,out.initial_volume_m3))
        throw std::runtime_error("Roundoff schedule total mass ledger invalid");
    return out;
}
} // namespace upc
