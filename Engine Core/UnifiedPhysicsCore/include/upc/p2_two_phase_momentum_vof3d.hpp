#pragma once
// P2 PARTIAL: optional LIVE projected-MAC -> cell FV momentum transport ->
// staggered-MAC feedback -> second variable-density pressure projection.
// Unlike a passive diagnostic, transported velocities feed the next step.
// Cell FV mass/momentum transport is exactly conservative. Cell-to-MAC
// interpolation/projection is NOT globally conservative; report its
// numerical impulse/energy delta explicitly. No free-surface stress or FSI.
#include "upc/p2_two_phase_mac_vof3d.hpp"
#include "upc/p2_two_phase_consistent_momentum3d.hpp"
#include "upc/p2_two_phase_mac_momentum_remap3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2TwoPhaseMomentumVofLedger3D {
    P2TwoPhaseMacVofLedger3D fluid;
    P2TwoPhaseConsistentMomentum3D convective;
    P2TwoPhaseMacMomentumRemap3D density_weighted_remap;
    P2TwoPhaseMacPressureLedger3D feedback_projection;
    std::array<double,3> projected_mac_impulse_before_kg_m_s{};
    std::array<double,3> projected_mac_impulse_after_kg_m_s{};
    // Isolate the non-conservative cell->MAC remap from the legitimate
    // physical impulse of the CLOSED box's pressure reaction.
    std::array<double,3> remapped_mac_impulse_kg_m_s{};
    std::array<double,3> remap_only_impulse_change_kg_m_s{};
    std::array<double,3> feedback_pressure_impulse_change_kg_m_s{};
    std::array<double,3> feedback_pressure_wall_impulse_n_s{};
    std::array<double,3> feedback_pressure_impulse_residual_kg_m_s{};
    std::array<double,3> remap_plus_feedback_impulse_change_kg_m_s{};
    double mac_energy_before_remap_j=0;
    double mac_energy_after_remap_j=0;
    double mac_energy_after_remap_and_projection_j=0;
    double remap_only_energy_change_j=0;
    double feedback_pressure_energy_change_j=0;
    double feedback_pressure_energy_residual_j=0;
    double remap_plus_feedback_energy_change_j=0;
    double max_density_agreement_kg_m3=0;
    bool cell_to_mac_is_conservative=false;
};

inline std::vector<std::array<double,3>> p2_two_phase_sample_centered_mac3d(
    const P2TwoPhaseMacPressureGrid3D& mac) {
    const auto nx=mac.nx,ny=mac.ny,nz=mac.nz;
    if(nx<2||ny<2||nz<2||nx>128||ny>128||nz>128||
       nx*ny*nz>100000||
       mac.u_m_s.size()!=(nx+1)*ny*nz||
       mac.v_m_s.size()!=nx*(ny+1)*nz||
       mac.w_m_s.size()!=nx*ny*(nz+1))
        throw std::invalid_argument("Invalid MAC-to-cell momentum sample");
    std::vector<std::array<double,3>> q(nx*ny*nz);
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto i=(z*ny+y)*nx+x;
        q[i][0]=.5*(mac.u_m_s[(z*ny+y)*(nx+1)+x]+
                    mac.u_m_s[(z*ny+y)*(nx+1)+x+1]);
        q[i][1]=.5*(mac.v_m_s[(z*(ny+1)+y)*nx+x]+
                    mac.v_m_s[(z*(ny+1)+y+1)*nx+x]);
        q[i][2]=.5*(mac.w_m_s[(z*ny+y)*nx+x]+
                    mac.w_m_s[((z+1)*ny+y)*nx+x]);
        for(double value:q[i])if(!std::isfinite(value))
            throw std::invalid_argument("Nonfinite sampled cell velocity");
    }
    return q;
}
inline P2TwoPhaseMomentumVofLedger3D p2_two_phase_momentum_vof_step3d(
    P2TwoPhaseMacVofState3D& state,int max_pcg_iterations=4000,
    std::array<double,3> gravity_m_s2={0,0,0}) {
    auto candidate=state; // full state remains untouched on any exception
    P2TwoPhaseMomentumVofLedger3D report;
    report.fluid=p2_two_phase_mac_vof_step3d(
        candidate,max_pcg_iterations,gravity_m_s2);
    // The first projection supplies the unique mass flux shared by VOF
    // and conservative cell-centred momentum advection.
    auto pre_advective_mac=candidate.mac;
    pre_advective_mac.density_kg_m3=state.mac.density_kg_m3;
    const auto projected_cell_velocity=
        p2_two_phase_sample_centered_mac3d(candidate.mac);
    report.convective=p2_two_phase_consistent_momentum3d(
        pre_advective_mac,state.water_fraction,projected_cell_velocity,
        state.water_density_kg_m3,state.air_density_kg_m3);
    for(std::size_t i=0;i<candidate.water_fraction.size();++i) {
        const auto delta_f=std::abs(candidate.water_fraction[i]-
                                    report.convective.water_fraction[i]);
        const auto delta_rho=std::abs(candidate.mac.density_kg_m3[i]-
                                      report.convective.density_kg_m3[i]);
        if(delta_f>1e-12||delta_rho>
           1e-10*std::max(1.0,candidate.mac.density_kg_m3[i]))
            throw std::runtime_error("VOF/momentum transport flux mismatch");
        report.max_density_agreement_kg_m3=std::max(
            report.max_density_agreement_kg_m3,delta_rho);
    }
    const auto nx=candidate.mac.nx,ny=candidate.mac.ny,nz=candidate.mac.nz;
    const auto vol=candidate.mac.dx_m*candidate.mac.dx_m*candidate.mac.dx_m;
    auto impulse=[&](const P2TwoPhaseMacPressureGrid3D& grid) {
        std::array<double,3> momentum{};
        double energy=0;
        const auto cell=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        const auto visit=[&](std::size_t a,std::size_t b,
                             double v,int axis) {
            const double face_rho=.5*(grid.density_kg_m3[a]+
                                      grid.density_kg_m3[b]);
            momentum[axis]+=face_rho*vol*v;
            energy+=.5*face_rho*vol*v*v;
        };
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=1;x<nx;++x)
            visit(cell(x-1,y,z),cell(x,y,z),
                  grid.u_m_s[(z*ny+y)*(nx+1)+x],0);
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=1;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            visit(cell(x,y-1,z),cell(x,y,z),
                  grid.v_m_s[(z*(ny+1)+y)*nx+x],1);
        for(std::size_t z=1;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            visit(cell(x,y,z-1),cell(x,y,z),
                  grid.w_m_s[(z*ny+y)*nx+x],2);
        return std::pair<std::array<double,3>,double>{momentum,energy};
    };
    {
        auto [momentum,energy]=impulse(candidate.mac);
        report.projected_mac_impulse_before_kg_m_s=momentum;
        report.mac_energy_before_remap_j=energy;
    }
    // New mass-weighted half-cell dual-volume remap. For each axis,
    // total CELL impulse = interior MAC-face impulse + omitted physical
    // boundary half-face impulse, exactly. The missing boundary term
    // is a geometric quadrature residual, NOT an assumed wall force.
    // The subsequent pressure reaction remains independently audited.
    auto& mac=candidate.mac;
    report.density_weighted_remap=p2_two_phase_mac_momentum_remap3d(
        mac,report.convective.velocity_m_s);
    mac.u_m_s=report.density_weighted_remap.u_m_s;
    mac.v_m_s=report.density_weighted_remap.v_m_s;
    mac.w_m_s=report.density_weighted_remap.w_m_s;
    {
        auto [momentum,energy]=impulse(mac);
        report.remapped_mac_impulse_kg_m_s=momentum;
        report.mac_energy_after_remap_j=energy;
    }
    for(int axis=0;axis<3;++axis) {
        const double difference=report.remapped_mac_impulse_kg_m_s[axis]-
            report.density_weighted_remap.interior_mac_impulse_kg_m_s[axis];
        if(!std::isfinite(difference)||
           std::abs(difference)>1e-9*std::max(
               1.0,std::abs(report.remapped_mac_impulse_kg_m_s[axis])))
            throw std::runtime_error("Dual MAC remap inventory inconsistent");
    }
    report.feedback_projection=p2_two_phase_mac_pressure_step3d(
        mac,max_pcg_iterations);
    {
        auto [momentum,energy]=impulse(mac);
        report.projected_mac_impulse_after_kg_m_s=momentum;
        report.mac_energy_after_remap_and_projection_j=energy;
    }
    for(int axis=0;axis<3;++axis) {
        report.remap_only_impulse_change_kg_m_s[axis]=
            report.remapped_mac_impulse_kg_m_s[axis]-
            report.projected_mac_impulse_before_kg_m_s[axis];
        report.feedback_pressure_impulse_change_kg_m_s[axis]=
            report.projected_mac_impulse_after_kg_m_s[axis]-
            report.remapped_mac_impulse_kg_m_s[axis];
        report.feedback_pressure_wall_impulse_n_s[axis]=
            report.feedback_projection.wall_pressure_impulse_n_s[axis];
        report.feedback_pressure_impulse_residual_kg_m_s[axis]=
            report.feedback_pressure_impulse_change_kg_m_s[axis]-
            report.feedback_pressure_wall_impulse_n_s[axis];
        report.remap_plus_feedback_impulse_change_kg_m_s[axis]=
            report.projected_mac_impulse_after_kg_m_s[axis]-
            report.projected_mac_impulse_before_kg_m_s[axis];
        const double tolerance=1e-9*std::max(
            1.0,std::abs(report.remapped_mac_impulse_kg_m_s[axis]));
        if(!std::isfinite(report.remap_only_impulse_change_kg_m_s[axis])||
           !std::isfinite(report.feedback_pressure_impulse_residual_kg_m_s[axis])||
           std::abs(report.feedback_pressure_impulse_residual_kg_m_s[axis])>tolerance||
           std::abs(report.feedback_projection.momentum_before_kg_m_s[axis]-
                    report.remapped_mac_impulse_kg_m_s[axis])>tolerance||
           std::abs(report.feedback_projection.momentum_after_kg_m_s[axis]-
                    report.projected_mac_impulse_after_kg_m_s[axis])>tolerance)
            throw std::runtime_error(
                "Feedback projection impulse must match measured wall reaction");
    }
    report.remap_only_energy_change_j=
        report.mac_energy_after_remap_j-report.mac_energy_before_remap_j;
    report.feedback_pressure_energy_change_j=
        report.mac_energy_after_remap_and_projection_j-
        report.mac_energy_after_remap_j;
    report.feedback_pressure_energy_residual_j=
        report.feedback_pressure_energy_change_j+
        report.feedback_projection.projection_dissipation_j;
    report.remap_plus_feedback_energy_change_j=
        report.mac_energy_after_remap_and_projection_j-
        report.mac_energy_before_remap_j;
    const double energy_tolerance=1e-9*
        std::max(1.0,std::abs(report.mac_energy_after_remap_j));
    if(!std::isfinite(report.remap_only_energy_change_j)||
       !std::isfinite(report.feedback_pressure_energy_residual_j)||
       !std::isfinite(report.remap_plus_feedback_energy_change_j)||
       std::abs(report.feedback_pressure_energy_residual_j)>energy_tolerance||
       std::abs(report.feedback_projection.kinetic_before_j-
                report.mac_energy_after_remap_j)>energy_tolerance||
       std::abs(report.feedback_projection.kinetic_after_j-
                report.mac_energy_after_remap_and_projection_j)>energy_tolerance)
        throw std::runtime_error(
            "Feedback projection energy must match independent MAC inventory");
    // The density-weighted half-cell remap closes the cell/interior/boundary
    // partition but does NOT close actual wall forces or total fluid energy.
    // Subsequent pressure reaction is measured, not corrected away; global
    // physical fluid + boundary/FSI closure remains unvalidated.
    state=std::move(candidate);
    return report;
}
} // namespace upc
