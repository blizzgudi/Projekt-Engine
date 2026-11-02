#pragma once
// P2 PARTIAL: transactional moving water/air VOF fraction with a shared
// variable-density 3D MAC pressure projection. Both phases carry velocity.
// No momentum advection, interfacial stress, moving solid or full FSI.
#include "upc/p2_two_phase_mac_pressure3d.hpp"
#include "upc/p2_conservative_vof_flux3d.hpp"
#include <algorithm>
#include <array>
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
    // Source/projection staggered-face quadrature, not a full physical
    // two-phase kinetic/potential energy budget after VOF remapping.
    std::array<double,3> gravity_source_impulse_kg_m_s{};
    std::array<double,3> gravity_source_projection_momentum_residual_kg_m_s{};
    double gravity_source_work_j=0;
    double gravity_source_projection_energy_residual_j=0;
    // True SI mass-weighted gravitational potential at cell centers.
    // A conservative VOF face-flux identity is checked independently of
    // the old source/projection face-kinetic ledger.
    double potential_before_j=0,potential_after_j=0;
    double potential_delta_direct_j=0,potential_delta_face_flux_j=0;
    double potential_flux_residual_j=0;
    // Density remap changes the MAC face-mass quadrature while velocity is
    // frozen. This is NOT momentum advection or a physical energy closure.
    double kinetic_after_density_remap_j=0;
    double kinetic_density_remap_delta_j=0;
    double unresolved_mechanical_energy_j=0;
};
inline P2TwoPhaseMacVofLedger3D p2_two_phase_mac_vof_step3d(
    P2TwoPhaseMacVofState3D& state,int max_pcg_iterations=4000,
    std::array<double,3> gravity_m_s2={0,0,0}) {
    const auto& m=state.mac;
    for(double g:gravity_m_s2)
        if(!std::isfinite(g))throw std::invalid_argument("Nonfinite gravity acceleration");
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
    const double dt=m.dt_s,volume=m.dx_m*m.dx_m*m.dx_m;
    std::array<double,3> momentum_before_source{};
    double kinetic_before_source=0;
    const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*m.ny+y)*m.nx+x;
    };
    const auto source_face=[&](double& velocity,std::size_t a,std::size_t b,int axis){
        const double rho=.5*(m.density_kg_m3[a]+m.density_kg_m3[b]);
        const double face_mass=rho*volume;
        const double old=velocity;
        const double dv=dt*gravity_m_s2[axis];
        const double updated=old+dv;
        if(!(std::isfinite(face_mass)&&face_mass>0&&std::isfinite(updated)))
            throw std::invalid_argument("Nonfinite gravity source or face density");
        momentum_before_source[axis]+=face_mass*old;
        kinetic_before_source+=.5*face_mass*old*old;
        out.gravity_source_impulse_kg_m_s[axis]+=face_mass*dv;
        out.gravity_source_work_j+=.5*face_mass*(updated*updated-old*old);
        velocity=updated;
    };
    // Apply uniform acceleration only to interior MAC faces. Closed-box
    // normal wall velocities remain zero. The projection balances the
    // hydrostatic force for horizontally stratified mixture density.
    for(std::size_t z=0;z<m.nz;++z)for(std::size_t y=0;y<m.ny;++y)
    for(std::size_t x=1;x<m.nx;++x)
        source_face(next.mac.u_m_s[(z*m.ny+y)*(m.nx+1)+x],
                    ci(x-1,y,z),ci(x,y,z),0);
    for(std::size_t z=0;z<m.nz;++z)for(std::size_t y=1;y<m.ny;++y)
    for(std::size_t x=0;x<m.nx;++x)
        source_face(next.mac.v_m_s[(z*(m.ny+1)+y)*m.nx+x],
                    ci(x,y-1,z),ci(x,y,z),1);
    for(std::size_t z=1;z<m.nz;++z)for(std::size_t y=0;y<m.ny;++y)
    for(std::size_t x=0;x<m.nx;++x)
        source_face(next.mac.w_m_s[(z*m.ny+y)*m.nx+x],
                    ci(x,y,z-1),ci(x,y,z),2);
    out.pressure=p2_two_phase_mac_pressure_step3d(
        next.mac,max_pcg_iterations);
    for(int k=0;k<3;++k)
        out.gravity_source_projection_momentum_residual_kg_m_s[k]=
            out.pressure.momentum_after_kg_m_s[k]-momentum_before_source[k]-
            out.gravity_source_impulse_kg_m_s[k]-
            out.pressure.wall_pressure_impulse_n_s[k];
    out.gravity_source_projection_energy_residual_j=
        out.pressure.kinetic_after_j-kinetic_before_source-
        out.gravity_source_work_j+out.pressure.projection_dissipation_j;
    double impulse_scale=1,energy_scale=1+std::abs(kinetic_before_source)+
        std::abs(out.gravity_source_work_j);
    for(int k=0;k<3;++k) {
        impulse_scale+=std::abs(momentum_before_source[k])+
            std::abs(out.gravity_source_impulse_kg_m_s[k]);
        if(std::abs(out.gravity_source_projection_momentum_residual_kg_m_s[k])>
           1e-9*impulse_scale)
            throw std::runtime_error("Gravity source/projection momentum ledger failed");
    }
    if(!std::isfinite(out.gravity_source_projection_energy_residual_j)||
       std::abs(out.gravity_source_projection_energy_residual_j)>1e-9*energy_scale)
        throw std::runtime_error("Gravity source/projection energy ledger failed");
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
    // Global gravitational inventory and independent donor-face flux audit.
    // Constant air background cancels from the potential-energy difference.
    // Coordinates are cell centers; changing the origin cannot change delta U
    // in this impermeable, mass-conserving box.
    for(std::size_t z=0;z<m.nz;++z)for(std::size_t y=0;y<m.ny;++y)
    for(std::size_t x=0;x<m.nx;++x) {
        const auto i=ci(x,y,z);
        const double gdotr=m.dx_m*(
            gravity_m_s2[0]*(double(x)+.5)+
            gravity_m_s2[1]*(double(y)+.5)+
            gravity_m_s2[2]*(double(z)+.5));
        const double delta=next.water_fraction[i]-state.water_fraction[i];
        out.potential_before_j-=m.density_kg_m3[i]*volume*gdotr;
        out.potential_after_j-=next.mac.density_kg_m3[i]*volume*gdotr;
        out.potential_delta_direct_j-=drho*volume*delta*gdotr;
    }
    const auto flux_potential=[&](std::size_t a,std::size_t b,
                                  double face_velocity,int axis) {
        const double donor_alpha=state.water_fraction[face_velocity>=0?a:b];
        out.potential_delta_face_flux_j-=drho*volume*dt*
            face_velocity*donor_alpha*gravity_m_s2[axis];
    };
    const auto kinetic_remap=[&](std::size_t a,std::size_t b,double face_velocity) {
        const double face_rho=.5*(next.mac.density_kg_m3[a]+
                                  next.mac.density_kg_m3[b]);
        out.kinetic_after_density_remap_j+=
            .5*face_rho*volume*face_velocity*face_velocity;
    };
    for(std::size_t z=0;z<m.nz;++z)for(std::size_t y=0;y<m.ny;++y)
    for(std::size_t x=1;x<m.nx;++x) {
        const auto a=ci(x-1,y,z),b=ci(x,y,z);
        const double v=next.mac.u_m_s[(z*m.ny+y)*(m.nx+1)+x];
        flux_potential(a,b,v,0);kinetic_remap(a,b,v);
    }
    for(std::size_t z=0;z<m.nz;++z)for(std::size_t y=1;y<m.ny;++y)
    for(std::size_t x=0;x<m.nx;++x) {
        const auto a=ci(x,y-1,z),b=ci(x,y,z);
        const double v=next.mac.v_m_s[(z*(m.ny+1)+y)*m.nx+x];
        flux_potential(a,b,v,1);kinetic_remap(a,b,v);
    }
    for(std::size_t z=1;z<m.nz;++z)for(std::size_t y=0;y<m.ny;++y)
    for(std::size_t x=0;x<m.nx;++x) {
        const auto a=ci(x,y,z-1),b=ci(x,y,z);
        const double v=next.mac.w_m_s[(z*m.ny+y)*m.nx+x];
        flux_potential(a,b,v,2);kinetic_remap(a,b,v);
    }
    out.potential_flux_residual_j=out.potential_delta_direct_j-
        out.potential_delta_face_flux_j;
    out.kinetic_density_remap_delta_j=
        out.kinetic_after_density_remap_j-out.pressure.kinetic_after_j;
    const double kinetic_initial=out.pressure.kinetic_before_j-
        out.gravity_source_work_j;
    // Closed-box total mechanical-energy change (including numerical
    // projection effects). Report, do NOT force to zero: momentum advection
    // and interfacial work are still missing from the model.
    out.unresolved_mechanical_energy_j=
        out.kinetic_after_density_remap_j+out.potential_after_j-
        kinetic_initial-out.potential_before_j;
    const double potential_scale=1+std::abs(out.potential_before_j)+
        std::abs(out.potential_after_j)+std::abs(out.potential_delta_face_flux_j);
    if(!std::isfinite(out.potential_flux_residual_j)||
       !std::isfinite(out.unresolved_mechanical_energy_j)||
       !std::isfinite(out.kinetic_after_density_remap_j)||
       std::abs(out.potential_flux_residual_j)>1e-9*potential_scale)
        throw std::runtime_error("Two-phase gravity VOF potential flux audit failed");
    state=std::move(next);
    return out;
}
} // namespace upc
