#pragma once
// P2 PARTIAL: LIVE moving 3D two-phase MAC/VOF + 6 symmetric viscous
// stress components + variable-density pressure correction. Actual
// water/air mixture viscosities are approximated in partial cells.
// Post-VOF viscous/pressure impulse and kinetic ledger are separately
// closed; total physical Navier-Stokes/FSI/experimental energy is NOT.
#include "upc/p2_two_phase_mac_vof3d.hpp"
#include "upc/p2_two_phase_full_tensor_viscous_mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace upc {
struct P2TwoPhaseFullTensorMacVofLedger3D {
    P2TwoPhaseMacVofLedger3D moving;
    P2TwoPhaseFullTensorViscousLedger3D viscous;
    P2TwoPhaseMacPressureLedger3D post_viscosity_projection;
    std::array<double,3> actual_combined_wall_impulse_kg_m_s{};
    std::array<double,3> actual_combined_impulse_change_kg_m_s{};
    std::array<double,3> post_viscosity_impulse_residual_kg_m_s{};
    double post_viscosity_energy_residual_j=0;
    double post_viscosity_energy_before_j=0;
    double post_viscosity_energy_after_j=0;
    double water_mass_before_kg=0,water_mass_after_kg=0;
    double air_mass_before_kg=0,air_mass_after_kg=0;
    double maximum_wet_divergence_s_inv=0;
    bool physical_total_energy_validated=false;
    bool sharp_stress_interface_validated=false;
    bool two_way_FSI_validated=false;
};
inline P2TwoPhaseFullTensorMacVofLedger3D
p2_two_phase_full_tensor_mac_vof_step3d(
    P2TwoPhaseMacVofState3D& state,
    int pressure_iterations=4000,
    std::array<double,3> gravity_m_s2={0,0,0},
    double water_dynamic_viscosity_pa_s=.001,
    double air_dynamic_viscosity_pa_s=.000018) {
    P2TwoPhaseMacVofState3D candidate=state;
    P2TwoPhaseFullTensorMacVofLedger3D result;
    result.moving=p2_two_phase_mac_vof_step3d(
        candidate,pressure_iterations,gravity_m_s2);
    result.viscous=p2_two_phase_full_tensor_viscous_mac_step3d(
        candidate.mac,candidate.water_fraction,
        water_dynamic_viscosity_pa_s,air_dynamic_viscosity_pa_s);
    result.post_viscosity_projection=p2_two_phase_mac_pressure_step3d(
        candidate.mac,pressure_iterations);
    result.water_mass_before_kg=result.moving.water_mass_before_kg;
    result.water_mass_after_kg=result.moving.water_mass_after_kg;
    result.air_mass_before_kg=result.moving.air_mass_before_kg;
    result.air_mass_after_kg=result.moving.air_mass_after_kg;
    result.maximum_wet_divergence_s_inv=
        result.post_viscosity_projection.divergence_after_s_inv;
    const double volume=candidate.mac.dx_m*candidate.mac.dx_m*candidate.mac.dx_m;
    double total_mass=0;
    for(std::size_t i=0;i<candidate.water_fraction.size();++i) {
        const double expected=candidate.air_density_kg_m3+
            (candidate.water_density_kg_m3-candidate.air_density_kg_m3)*
             candidate.water_fraction[i];
        if(!std::isfinite(expected)||!std::isfinite(candidate.mac.density_kg_m3[i])||
           std::abs(candidate.mac.density_kg_m3[i]-expected)>
                1e-10*std::max(1.0,expected))
            throw std::runtime_error(
                "Moving MAC full-tensor stress invalidated physical phase mass");
        total_mass+=candidate.mac.density_kg_m3[i]*volume;
    }
    if(std::abs(total_mass-result.water_mass_after_kg-
                result.air_mass_after_kg)>1e-10*std::max(1.0,total_mass))
        throw std::runtime_error("Moving MAC symmetric stress total mass mismatch");
    for(int axis=0;axis<3;++axis) {
        result.actual_combined_wall_impulse_kg_m_s[axis]=
            result.viscous.wall_normal_traction_impulse_n_s[axis]+
            result.post_viscosity_projection.wall_pressure_impulse_n_s[axis];
        result.actual_combined_impulse_change_kg_m_s[axis]=
            result.post_viscosity_projection.momentum_after_kg_m_s[axis]-
            result.viscous.momentum_before_kg_m_s[axis];
        result.post_viscosity_impulse_residual_kg_m_s[axis]=
            result.actual_combined_impulse_change_kg_m_s[axis]-
            result.actual_combined_wall_impulse_kg_m_s[axis];
        if(!std::isfinite(result.post_viscosity_impulse_residual_kg_m_s[axis])||
           std::abs(result.post_viscosity_impulse_residual_kg_m_s[axis])>
               1e-9*std::max(1.0,
                   std::abs(result.viscous.momentum_before_kg_m_s[axis])))
            throw std::runtime_error(
                "Moving MAC viscous plus pressure physical wall impulse failed");
    }
    result.post_viscosity_energy_before_j=result.viscous.kinetic_before_j;
    result.post_viscosity_energy_after_j=
        result.post_viscosity_projection.kinetic_after_j;
    const double pressure_initial_diff=std::abs(
        result.post_viscosity_projection.kinetic_before_j-
        result.viscous.kinetic_after_j);
    result.post_viscosity_energy_residual_j=
        result.post_viscosity_energy_before_j-
        result.post_viscosity_energy_after_j-
        candidate.mac.dt_s*result.viscous.stress_dissipation_rate_w+
        result.viscous.explicit_temporal_correction_j-
        result.post_viscosity_projection.projection_dissipation_j;
    if(pressure_initial_diff>1e-9*std::max(1.0,
                result.viscous.kinetic_after_j)||
       !std::isfinite(result.post_viscosity_energy_residual_j)||
       std::abs(result.post_viscosity_energy_residual_j)>
          1e-9*std::max(1.0,result.post_viscosity_energy_before_j))
        throw std::runtime_error(
            "Moving MAC full-tensor viscous/pressure energy ledger failed");
    state=std::move(candidate);
    return result;
}
} // namespace upc
