#pragma once
// P2 PARTIAL: conservative 3D mass-consistent finite-volume transport of
// cell-centred momentum by the REAL projected two-phase MAC face flux.
// This is a transport primitive, not yet a MAC self-advection/FSI solver.
// No silent energy correction, nonphysical mass clipping or forged closure.
#include "upc/p2_two_phase_mac_pressure3d.hpp"
#include "upc/p2_conservative_vof_flux3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2TwoPhaseConsistentMomentum3D {
    std::vector<std::array<double,3>> velocity_m_s;
    std::vector<double> water_fraction;
    std::vector<double> density_kg_m3;
    std::array<double,3> impulse_before_kg_m_s{};
    std::array<double,3> impulse_after_kg_m_s{};
    std::array<double,3> internal_flux_impulse_residual_kg_m_s{};
    double mass_before_kg=0,mass_after_kg=0;
    double water_mass_before_kg=0,water_mass_after_kg=0;
    double air_mass_before_kg=0,air_mass_after_kg=0;
    double density_vof_consistency_max_kg_m3=0;
    double kinetic_before_j=0,kinetic_after_j=0;
    double upwind_kinetic_loss_j=0;
    double max_outgoing_courant=0;
    double max_divergence_s_inv=0;
    std::size_t active_mass_faces=0;
};
inline P2TwoPhaseConsistentMomentum3D p2_two_phase_consistent_momentum3d(
    const P2TwoPhaseMacPressureGrid3D& projected_mac,
    const std::vector<double>& water,
    const std::vector<std::array<double,3>>& cell_velocity,
    double water_rho=1000.0,double air_rho=1.2) {
    const auto& m=projected_mac;
    const std::size_t nx=m.nx,ny=m.ny,nz=m.nz;
    if(nx<2||ny<2||nz<2||nx>128||ny>128||nz>128||
       nx*ny*nz>100000||water.size()!=nx*ny*nz||
       cell_velocity.size()!=water.size()||
       m.density_kg_m3.size()!=water.size()||
       !(std::isfinite(water_rho)&&std::isfinite(air_rho)&&
         water_rho>air_rho&&air_rho>0))
        throw std::invalid_argument("Invalid two-phase momentum grid, cells or material");
    const std::size_t n=water.size();
    const double volume=m.dx_m*m.dx_m*m.dx_m;
    if(!std::isfinite(volume)||volume<=0)
        throw std::invalid_argument("Nonfinite two-phase momentum volume");
    const double drho=water_rho-air_rho;
    std::vector<double> mass_density(n), updated_density(n);
    std::vector<std::array<double,3>> momentum(n);
    P2TwoPhaseConsistentMomentum3D result;
    for(std::size_t i=0;i<n;++i) {
        if(!std::isfinite(water[i])||water[i]<0||water[i]>1)
            throw std::invalid_argument("Invalid two-phase momentum fraction");
        mass_density[i]=air_rho+drho*water[i];
        if(!std::isfinite(m.density_kg_m3[i])||
           std::abs(m.density_kg_m3[i]-mass_density[i])>
            1e-12*std::max(1.0,mass_density[i]))
            throw std::invalid_argument("Inconsistent initial momentum density");
        updated_density[i]=mass_density[i];
        const double water_mass=water_rho*water[i]*volume;
        const double air_mass=air_rho*(1-water[i])*volume;
        result.water_mass_before_kg+=water_mass;
        result.air_mass_before_kg+=air_mass;
        result.mass_before_kg+=mass_density[i]*volume;
        for(int axis=0;axis<3;++axis) {
            const double v=cell_velocity[i][axis];
            if(!std::isfinite(v)||!std::isfinite(mass_density[i]*v))
                throw std::invalid_argument("Nonfinite cell-centred momentum");
            momentum[i][axis]=mass_density[i]*v;
            result.impulse_before_kg_m_s[axis]+=momentum[i][axis]*volume;
            result.kinetic_before_j+=.5*mass_density[i]*volume*v*v;
        }
    }
    const std::vector<std::uint8_t> no_solid(n,0);
    const auto vof=ConservativeVofFlux3D::step(
        nx,ny,nz,m.dx_m,m.dt_s,no_solid,water,
        m.u_m_s,m.v_m_s,m.w_m_s);
    result.water_fraction=vof.water_fill;
    result.max_outgoing_courant=vof.max_outgoing_courant;
    result.max_divergence_s_inv=vof.max_divergence_s_inv;
    // Each interior face transfers the SAME donor mass that the VOF
    // update uses: rho_face = rho_air + (rho_water-rho_air)*alpha_donor.
    // Transfers of each Cartesian momentum component are equal/opposite
    // for the two cells. This is conservative for the cell-centred
    // inventory; velocity remapping to MAC faces is NOT in this primitive.
    const auto cell=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    const double cfl=m.dt_s/m.dx_m;
    auto transfer=[&](std::size_t left,std::size_t right,double face_v) {
        if(face_v==0)return;
        const auto donor=face_v>0?left:right,receiver=face_v>0?right:left;
        const double amount=std::abs(face_v)*cfl*mass_density[donor];
        if(!std::isfinite(amount)||amount<0)
            throw std::runtime_error("Nonfinite two-phase donor mass flux");
        updated_density[donor]-=amount;
        updated_density[receiver]+=amount;
        for(int axis=0;axis<3;++axis) {
            const double delta=amount*cell_velocity[donor][axis];
            if(!std::isfinite(delta))
                throw std::runtime_error("Nonfinite two-phase momentum flux");
            momentum[donor][axis]-=delta;
            momentum[receiver][axis]+=delta;
        }
        ++result.active_mass_faces;
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=1;x<nx;++x)
        transfer(cell(x-1,y,z),cell(x,y,z),
            m.u_m_s[(z*ny+y)*(nx+1)+x]);
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        transfer(cell(x,y-1,z),cell(x,y,z),
            m.v_m_s[(z*(ny+1)+y)*nx+x]);
    for(std::size_t z=1;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        transfer(cell(x,y,z-1),cell(x,y,z),
            m.w_m_s[(z*ny+y)*nx+x]);
    result.velocity_m_s.resize(n);
    result.density_kg_m3.resize(n);
    for(std::size_t i=0;i<n;++i) {
        const double expected=air_rho+drho*result.water_fraction[i];
        result.density_vof_consistency_max_kg_m3=std::max(
            result.density_vof_consistency_max_kg_m3,
            std::abs(updated_density[i]-expected));
        if(!std::isfinite(updated_density[i])||updated_density[i]<=0||
           std::abs(updated_density[i]-expected)>
                1e-10*std::max(1.0,expected))
            throw std::runtime_error("Mass/momentum flux disagrees with VOF");
        // A tiny numerical difference may exist between independent
        // accumulation order. Keep *transported* density, not overwrite
        // it with a fabricated correction or artificially close a ledger.
        result.density_kg_m3[i]=updated_density[i];
        result.water_mass_after_kg+=water_rho*result.water_fraction[i]*volume;
        result.air_mass_after_kg+=air_rho*(1-result.water_fraction[i])*volume;
        result.mass_after_kg+=updated_density[i]*volume;
        for(int axis=0;axis<3;++axis) {
            result.impulse_after_kg_m_s[axis]+=momentum[i][axis]*volume;
            const double speed=momentum[i][axis]/updated_density[i];
            if(!std::isfinite(speed))
                throw std::runtime_error("Nonfinite transported cell velocity");
            result.velocity_m_s[i][axis]=speed;
            result.kinetic_after_j+=.5*updated_density[i]*volume*speed*speed;
        }
    }
    result.upwind_kinetic_loss_j=result.kinetic_before_j-
                                 result.kinetic_after_j;
    const double mass_scale=std::max(1.0,result.mass_before_kg);
    if(!std::isfinite(result.upwind_kinetic_loss_j)||
       result.upwind_kinetic_loss_j< -1e-9*
           std::max(1.0,result.kinetic_before_j)||
       std::abs(result.mass_after_kg-result.mass_before_kg)>1e-10*mass_scale||
       std::abs(result.water_mass_after_kg-result.water_mass_before_kg)>
           1e-10*mass_scale||
       std::abs(result.air_mass_after_kg-result.air_mass_before_kg)>
           1e-10*mass_scale)
        throw std::runtime_error("Two-phase mass or energy monotonicity failed");
    for(int axis=0;axis<3;++axis) {
        result.internal_flux_impulse_residual_kg_m_s[axis]=
            result.impulse_after_kg_m_s[axis]-
            result.impulse_before_kg_m_s[axis];
        const double scale=std::max(1.0,
                     std::abs(result.impulse_before_kg_m_s[axis]));
        if(std::abs(result.internal_flux_impulse_residual_kg_m_s[axis])>
             1e-10*scale)
            throw std::runtime_error("Two-phase momentum flux imbalance");
    }
    return result;
}
} // namespace upc
