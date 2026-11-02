#pragma once
// P2 PARTIAL: real opt-in moving two-phase 3D MAC/VOF + conservative
// cell-centred momentum + mass-weighted staggered feedback + full 3D
// symmetric viscous tensor (normal XX/YY/ZZ and XY/XZ/YZ shear) +
// final density-weighted MAC pressure projection.
// This is not a validated sharp PLIC surface-stress, interface curvature,
// dynamic rigid/deformable FSI or experimental MARIN full-phase solver.
// Every substage is transactional and keeps genuine wall impulse/energy
// budgets separately; no fabricated global physical balance is asserted.
#include "upc/p2_two_phase_momentum_vof3d.hpp"
#include "upc/p2_two_phase_full_tensor_viscous_mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace upc {
struct P2TwoPhaseMomentumViscousVofLedger3D {
    P2TwoPhaseMomentumVofLedger3D momentum;
    P2TwoPhaseFullTensorViscousLedger3D viscous;
    P2TwoPhaseMacPressureLedger3D final_projection;
    std::array<double,3> viscous_plus_projection_impulse_kg_m_s{};
    std::array<double,3> viscous_plus_projection_wall_impulse_n_s{};
    std::array<double,3> viscous_plus_projection_impulse_residual_kg_m_s{};
    double kinetic_before_viscosity_j=0;
    double kinetic_after_viscosity_and_projection_j=0;
    double measured_kinetic_change_j=0;
    double predicted_kinetic_change_j=0;
    double kinetic_change_residual_j=0;
    double water_mass_residual_kg=0;
    double air_mass_residual_kg=0;
    double last_divergence_s_inv=0;
    bool moving_sharp_interface_stress_validated=false;
    bool global_physical_energy_impulse_closed=false;
    bool full_P2_accepted=false;
};
inline P2TwoPhaseMomentumViscousVofLedger3D
p2_two_phase_momentum_viscous_vof_step3d(
    P2TwoPhaseMacVofState3D& state,
    int max_pcg_iterations=4000,
    std::array<double,3> gravity_m_s2={0,0,0},
    double water_dynamic_viscosity_pa_s=.001,
    double air_dynamic_viscosity_pa_s=.000018) {
    // Never publish momentum/VOF/pressure state if the subsequent viscous
    // stress or its second post-stress pressure projection is invalid.
    auto trial=state;
    P2TwoPhaseMomentumViscousVofLedger3D out;
    out.momentum=p2_two_phase_momentum_vof_step3d(
        trial,max_pcg_iterations,gravity_m_s2);
    const double dx=trial.mac.dx_m;
    const double volume=dx*dx*dx;
    const auto inventories=[&](const P2TwoPhaseMacVofState3D& s) {
        double water=0,air=0;
        for(double fraction:s.water_fraction) {
            water+=fraction*s.water_density_kg_m3*volume;
            air+=(1-fraction)*s.air_density_kg_m3*volume;
        }
        return std::pair<double,double>{water,air};
    };
    const auto [water_before,air_before]=inventories(state);
    const auto [water_after,air_after]=inventories(trial);
    const double box_volume=volume*double(trial.water_fraction.size());
    out.water_mass_residual_kg=water_after-water_before;
    out.air_mass_residual_kg=air_after-air_before;
    const double mass_scale=std::max(1.0,water_before);
    if(!std::isfinite(out.water_mass_residual_kg)||
       !std::isfinite(out.air_mass_residual_kg)||
       std::abs(out.water_mass_residual_kg)>1e-10*mass_scale||
       std::abs(out.air_mass_residual_kg)>1e-10*std::max(1.0,air_before)||
       !(box_volume>0&&std::isfinite(box_volume)))
        throw std::runtime_error("Moving two-phase MAC/VOF lost water/air mass");
    out.viscous=p2_two_phase_full_tensor_viscous_mac_step3d(
        trial.mac,trial.water_fraction,
        water_dynamic_viscosity_pa_s,air_dynamic_viscosity_pa_s);
    out.kinetic_before_viscosity_j=out.viscous.kinetic_before_j;
    if(std::abs(out.momentum.mac_energy_after_remap_and_projection_j-
                out.viscous.kinetic_before_j)>
            1e-9*std::max(1.0,out.viscous.kinetic_before_j))
        throw std::runtime_error(
            "Full tensor stress starts from wrong live MAC kinetic state");
    out.final_projection=p2_two_phase_mac_pressure_step3d(
        trial.mac,max_pcg_iterations);
    out.last_divergence_s_inv=out.final_projection.divergence_after_s_inv;
    out.kinetic_after_viscosity_and_projection_j=
        out.final_projection.kinetic_after_j;
    if(std::abs(out.final_projection.kinetic_before_j-
                out.viscous.kinetic_after_j)>
            1e-9*std::max(1.0,out.viscous.kinetic_after_j))
        throw std::runtime_error("Final PCG did not use current viscous MAC faces");
    for(int axis=0;axis<3;++axis){
        out.viscous_plus_projection_impulse_kg_m_s[axis]=
            out.final_projection.momentum_after_kg_m_s[axis]-
            out.viscous.momentum_before_kg_m_s[axis];
        out.viscous_plus_projection_wall_impulse_n_s[axis]=
            out.viscous.wall_normal_traction_impulse_n_s[axis]+
            out.final_projection.wall_pressure_impulse_n_s[axis];
        out.viscous_plus_projection_impulse_residual_kg_m_s[axis]=
            out.viscous_plus_projection_impulse_kg_m_s[axis]-
            out.viscous_plus_projection_wall_impulse_n_s[axis];
        const double scale=std::max(
            1.0,std::abs(out.viscous.momentum_before_kg_m_s[axis]));
        if(!std::isfinite(
               out.viscous_plus_projection_impulse_residual_kg_m_s[axis])||
           std::abs(
               out.viscous_plus_projection_impulse_residual_kg_m_s[axis])>
               1e-9*scale)
            throw std::runtime_error("Viscous+pressure wall impulse ledger failed");
    }
    out.measured_kinetic_change_j=
        out.kinetic_after_viscosity_and_projection_j-
        out.kinetic_before_viscosity_j;
    out.predicted_kinetic_change_j=
        -trial.mac.dt_s*out.viscous.stress_dissipation_rate_w+
        out.viscous.explicit_temporal_correction_j-
        out.final_projection.projection_dissipation_j;
    out.kinetic_change_residual_j=
        out.measured_kinetic_change_j-out.predicted_kinetic_change_j;
    if(!std::isfinite(out.kinetic_change_residual_j)||
       std::abs(out.kinetic_change_residual_j)>
           1e-9*std::max(1.0,out.kinetic_before_viscosity_j))
        throw std::runtime_error("Real viscous+pressure MAC energy ledger failed");
    // The source+gravity/VOF/advective split and fixed boundary quadrature
    // still have unresolved global mechanical work and impulse. These
    // exactly verified SUBSTEP ledgers are not a physical P2 full gate.
    state=std::move(trial);
    return out;
}
} // namespace upc
