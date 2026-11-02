#pragma once
// P2 PARTIAL: transactional moving water/air VOF fraction with a shared
// variable-density 3D MAC pressure projection. Both phases carry velocity.
// No momentum advection, interfacial stress, moving solid or full FSI.
#include "upc/p2_two_phase_mac_pressure3d.hpp"
#include "upc/p2_conservative_vof_flux3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2TwoPhaseMacVofState3D {
    P2TwoPhaseMacPressureGrid3D mac;
    std::vector<double> water_fraction; // cell volume fraction in [0,1]
    double water_density_kg_m3=1000.0;
    double air_density_kg_m3=1.2;
};
struct P2TwoPhaseMacVofLedger3D {
    P2TwoPhaseMacPressureLedger3D pressure;
    ConservativeVofFluxResult3D vof;
    double water_mass_before_kg=0,water_mass_after_kg=0;
    double air_mass_before_kg=0,air_mass_after_kg=0;
    double total_mass_residual_kg=0;
    double max_density_change_kg_m3=0;
};
inline P2TwoPhaseMacVofLedger3D p2_two_phase_mac_vof_step3d(
    P2TwoPhaseMacVofState3D& state,int max_pcg_iterations=4000) {
    const auto& m=state.mac;
    if(!(std::isfinite(state.water_density_kg_m3)&&
         std::isfinite(state.air_density_kg_m3)&&
         state.water_density_kg_m3>state.air_density_kg_m3&&
         state.air_density_kg_m3>0))
        throw std::invalid_argument("Invalid two-phase material densities");
    if(m.nx<2||m.ny<2||m.nz<2||m.nx>128||m.ny>128||m.nz>128||
       m.nx*m.ny*m.nz>100000||state.water_fraction.size()!=m.nx*m.ny*m.nz||
       m.density_kg_m3.size()!=state.water_fraction.size())
        throw std::invalid_argument("Invalid two-phase VOF dimensions");
    const double drho=state.water_density_kg_m3-state.air_density_kg_m3;
    for(std::size_t i=0;i<state.water_fraction.size();++i) {
        const double a=state.water_fraction[i];
        if(!std::isfinite(a)||a<0||a>1)
            throw std::invalid_argument("Invalid water fraction");
        const double rho=state.air_density_kg_m3+drho*a;
        if(!std::isfinite(rho)||!std::isfinite(m.density_kg_m3[i])||
           std::abs(m.density_kg_m3[i]-rho)>1e-12*std::max(1.0,rho))
            throw std::invalid_argument("Inconsistent VOF mixture density");
    }
    // Projection and conservative transport execute on a local copy.
    // An invalid PCG, wall flux, divergence or VOF CFL cannot mutate state.
    auto next=state;
    P2TwoPhaseMacVofLedger3D out;
    out.pressure=p2_two_phase_mac_pressure_step3d(
        next.mac,max_pcg_iterations);
    const std::vector<std::uint8_t> no_solids(next.water_fraction.size(),0);
    out.vof=ConservativeVofFlux3D::step(
        m.nx,m.ny,m.nz,m.dx_m,m.dt_s,no_solids,next.water_fraction,
        next.mac.u_m_s,next.mac.v_m_s,next.mac.w_m_s);
    const double cell_volume=m.dx_m*m.dx_m*m.dx_m;
    const double box_volume=double(m.nx*m.ny*m.nz)*cell_volume;
    out.water_mass_before_kg=out.vof.initial_volume_m3*state.water_density_kg_m3;
    out.water_mass_after_kg=out.vof.final_volume_m3*state.water_density_kg_m3;
    out.air_mass_before_kg=(box_volume-out.vof.initial_volume_m3)*state.air_density_kg_m3;
    out.air_mass_after_kg=(box_volume-out.vof.final_volume_m3)*state.air_density_kg_m3;
    out.total_mass_residual_kg=(out.water_mass_after_kg+out.air_mass_after_kg)-
        (out.water_mass_before_kg+out.air_mass_before_kg);
    const double mass_scale=std::max(1.0,out.pressure.total_mass_kg);
    if(!std::isfinite(out.total_mass_residual_kg)||
       std::abs(out.total_mass_residual_kg)>1e-10*mass_scale||
       std::abs(out.pressure.total_mass_kg-
           (out.water_mass_before_kg+out.air_mass_before_kg))>1e-10*mass_scale)
        throw std::runtime_error("Two-phase VOF mass inventory did not close");
    next.water_fraction=std::move(out.vof.water_fill);
    for(std::size_t i=0;i<next.water_fraction.size();++i) {
        const double rho=state.air_density_kg_m3+drho*next.water_fraction[i];
        if(!std::isfinite(rho))
            throw std::runtime_error("Nonfinite transported mixture density");
        out.max_density_change_kg_m3=std::max(out.max_density_change_kg_m3,
            std::abs(rho-m.density_kg_m3[i]));
        next.mac.density_kg_m3[i]=rho;
    }
    state=std::move(next);
    return out;
}
} // namespace upc
