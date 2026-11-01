#pragma once
// P2 PARTIAL: opt-in endpoint-roundoff sanitation between periodic
// viscous MAC/PLIC steps. This addresses one-ULP near-full cells whose
// centered VOF gradient vanishes. It does NOT reconstruct genuinely
// unresolved interfaces or validate physical two-phase Navier-Stokes.
#include "upc/p2_viscous_nonlinear_mac_plic3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2PlicEndpointRoundoff3D {
    std::vector<double> fill;
    std::size_t snapped_cells=0;
    double signed_correction_cells=0;
    double absolute_correction_cells=0;
};

// Only fractions within eight machine epsilons of 0 or 1 are eligible.
// No mass redistribution, clamping of invalid inputs, or arbitrary
// normal-direction fallback is permitted.
inline P2PlicEndpointRoundoff3D p2_plic_endpoint_roundoff3d(
    const std::vector<double>& input) {
    if(input.empty()||input.size()>2000000)
        throw std::invalid_argument("Invalid PLIC endpoint cleanup size");
    constexpr double tolerance=8*std::numeric_limits<double>::epsilon();
    P2PlicEndpointRoundoff3D out;
    out.fill=input;
    long double signed_sum=0,absolute_sum=0;
    for(double& f:out.fill) {
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid PLIC endpoint fraction");
        const double before=f;
        if(f>0&&f<=tolerance)f=0;
        else if(f<1&&1-f<=tolerance)f=1;
        if(f!=before) {
            ++out.snapped_cells;
            const long double change=static_cast<long double>(f)-before;
            signed_sum+=change;
            absolute_sum+=std::abs(change);
        }
    }
    out.signed_correction_cells=static_cast<double>(signed_sum);
    out.absolute_correction_cells=static_cast<double>(absolute_sum);
    return out;
}

struct P2ViscousMacPlicRoundoffSequence3D {
    P2NonlinearMacPlicState3D state;
    std::size_t steps=0, snapped_cells=0;
    std::size_t viscous_substeps=0, plic_substeps=0;
    double initial_water_volume_m3=0, final_water_volume_m3=0;
    double signed_snap_volume_m3=0, absolute_snap_volume_m3=0;
    double transport_mass_residual_m3=0, total_mass_residual_m3=0;
    double max_projected_divergence_s_inv=0;
};

// Repeated constant-dt/nu periodic steps, with bounded endpoint-only
// sanitation before the first step and after each step. Fail closed if
// the sum of ABSOLUTE snap changes exceeds a tiny relative mass budget.
// The input state remains untouched even when a later step fails.
inline P2ViscousMacPlicRoundoffSequence3D
p2_viscous_mac_plic_roundoff_sequence3d(
    const P2NonlinearMacPlicState3D& initial,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double dt_s,double density_kg_m3,double nu_m2_s,
    std::size_t steps,double target_six_face_courant=0.4,
    std::size_t max_plic_substeps=256,
    std::size_t max_viscous_substeps=256) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>32768||initial.water_fill.size()!=nx*ny*nz||
       !std::isfinite(dx_m)||dx_m<=0||
       !std::isfinite(dt_s)||dt_s<=0||
       !std::isfinite(density_kg_m3)||density_kg_m3<=0||
       !std::isfinite(nu_m2_s)||nu_m2_s<0||
       steps==0||steps>256)
        throw std::invalid_argument("Invalid roundoff MAC/PLIC sequence");
    const double cell_volume=dx_m*dx_m*dx_m;
    if(!std::isfinite(cell_volume)||cell_volume<=0)
        throw std::invalid_argument("Invalid roundoff MAC cell volume");
    long double initial_cells=0;
    for(double f:initial.water_fill) {
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid roundoff MAC water fraction");
        initial_cells+=f;
    }
    P2ViscousMacPlicRoundoffSequence3D out;
    out.steps=steps;
    out.initial_water_volume_m3=static_cast<double>(initial_cells)*cell_volume;
    out.state=initial;
    long double signed_snap_cells=0,absolute_snap_cells=0;
    const auto sanitize=[&]() {
        auto cleanup=p2_plic_endpoint_roundoff3d(out.state.water_fill);
        out.state.water_fill=std::move(cleanup.fill);
        out.snapped_cells+=cleanup.snapped_cells;
        signed_snap_cells+=cleanup.signed_correction_cells;
        absolute_snap_cells+=cleanup.absolute_correction_cells;
        // Explicit cumulative absolute correction budget in cell fractions.
        if(absolute_snap_cells>1e-10L*std::max(1.0L,initial_cells))
            throw std::runtime_error("PLIC endpoint snap budget exceeded");
    };
    sanitize();
    for(std::size_t i=0;i<steps;++i) {
        auto result=p2_viscous_nonlinear_mac_plic_step3d(
            out.state,nx,ny,nz,dx_m,dt_s,density_kg_m3,nu_m2_s,
            target_six_face_courant,max_plic_substeps,max_viscous_substeps);
        out.state=std::move(result.coupled.state);
        out.viscous_substeps+=result.viscous_substeps;
        out.plic_substeps+=result.coupled.transport.substeps;
        out.transport_mass_residual_m3+=result.coupled.transport.mass_residual_m3;
        out.max_projected_divergence_s_inv=std::max(
            out.max_projected_divergence_s_inv,
            result.coupled.projection.max_divergence_s_inv);
        sanitize();
    }
    long double final_cells=0;
    for(double f:out.state.water_fill)final_cells+=f;
    out.final_water_volume_m3=static_cast<double>(final_cells)*cell_volume;
    out.signed_snap_volume_m3=static_cast<double>(signed_snap_cells)*cell_volume;
    out.absolute_snap_volume_m3=static_cast<double>(absolute_snap_cells)*cell_volume;
    out.total_mass_residual_m3=out.final_water_volume_m3-
        out.initial_water_volume_m3-out.signed_snap_volume_m3;
    const double tolerance=1e-10*std::max(1.0,out.initial_water_volume_m3);
    if(!std::isfinite(out.total_mass_residual_m3)||
       std::abs(out.total_mass_residual_m3)>tolerance)
        throw std::runtime_error("Roundoff MAC/PLIC cumulative water imbalance");
    return out;
}
} // namespace upc
