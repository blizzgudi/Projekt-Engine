#pragma once
// P2 PARTIAL: live 3D water/air capillary + momentum + full tensor
// energy inventory. Surface area is a diffuse |grad(alpha)| proxy, NOT
// geometrically reconstructed PLIC area. The global residual is reported,
// never forced to zero or interpreted as a P2 full acceptance.
#include "upc/p2_two_phase_capillary_mac_vof3d.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace upc {
struct P2CapillaryEnergyInventory3D {
    double water_mass_kg=0,air_mass_kg=0;
    double mac_kinetic_j=0,gravity_potential_j=0;
    double diffuse_area_m2=0,surface_energy_proxy_j=0;
    double total_proxy_j=0;
};
inline P2CapillaryEnergyInventory3D p2_capillary_energy_inventory3d(
    const P2TwoPhaseMacVofState3D& state,double sigma_n_m,
    std::array<double,3> gravity_m_s2={0,0,0}) {
    const auto& m=state.mac;
    const auto nx=m.nx,ny=m.ny,nz=m.nz;
    if(nx<3||ny<3||nz<3||nx>128||ny>128||nz>128||
       nx*ny*nz>100000||!(std::isfinite(m.dx_m)&&m.dx_m>0)||
       !(std::isfinite(sigma_n_m)&&sigma_n_m>=0)||
       state.water_fraction.size()!=nx*ny*nz||
       m.density_kg_m3.size()!=nx*ny*nz||
       m.u_m_s.size()!=(nx+1)*ny*nz||
       m.v_m_s.size()!=nx*(ny+1)*nz||
       m.w_m_s.size()!=nx*ny*(nz+1)||
       !(std::isfinite(state.water_density_kg_m3)&&
         std::isfinite(state.air_density_kg_m3)&&
         state.water_density_kg_m3>state.air_density_kg_m3&&
         state.air_density_kg_m3>0))
        throw std::invalid_argument("Invalid P2 capillary energy geometry");
    for(double g:gravity_m_s2)if(!std::isfinite(g))
        throw std::invalid_argument("Nonfinite gravity in energy audit");
    const double dx=m.dx_m,vol=dx*dx*dx;
    if(!(std::isfinite(vol)&&vol>0))
        throw std::invalid_argument("Invalid capillary energy volume");
    const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    const auto a=[&](std::size_t x,std::size_t y,std::size_t z){
        return state.water_fraction[ci(x,y,z)];
    };
    P2CapillaryEnergyInventory3D out;
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const auto i=ci(x,y,z);
        const double f=a(x,y,z),rho=m.density_kg_m3[i];
        const double expected=state.air_density_kg_m3+
            (state.water_density_kg_m3-state.air_density_kg_m3)*f;
        if(!(std::isfinite(f)&&f>=0&&f<=1&&std::isfinite(rho)&&
             std::abs(rho-expected)<=1e-12*std::max(1.0,expected)))
            throw std::invalid_argument("Inconsistent energy VOF/density");
        out.water_mass_kg+=vol*f*state.water_density_kg_m3;
        out.air_mass_kg+=vol*(1-f)*state.air_density_kg_m3;
        const double gdotr=dx*(gravity_m_s2[0]*(x+.5)+
            gravity_m_s2[1]*(y+.5)+gravity_m_s2[2]*(z+.5));
        out.gravity_potential_j-=rho*vol*gdotr;
        double grad2=0;
        for(int axis=0;axis<3;++axis){
            const auto pos=axis==0?x:(axis==1?y:z);
            const auto size=axis==0?nx:(axis==1?ny:nz);
            const auto lo=pos==0?0:pos-1;
            const auto hi=pos+1<size?pos+1:pos;
            const double left=a(axis==0?lo:x,axis==1?lo:y,axis==2?lo:z);
            const double right=a(axis==0?hi:x,axis==1?hi:y,axis==2?hi:z);
            const double d=(right-left)/(double(hi-lo)*dx);
            grad2+=d*d;
        }
        out.diffuse_area_m2+=std::sqrt(grad2)*vol;
    }
    const auto face=[&](std::size_t aidx,std::size_t bidx,double speed){
        if(!std::isfinite(speed))
            throw std::invalid_argument("Nonfinite MAC velocity in energy audit");
        const double rho=.5*(m.density_kg_m3[aidx]+m.density_kg_m3[bidx]);
        out.mac_kinetic_j+=.5*rho*vol*speed*speed;
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=1;x<nx;++x)
        face(ci(x-1,y,z),ci(x,y,z),m.u_m_s[(z*ny+y)*(nx+1)+x]);
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        face(ci(x,y-1,z),ci(x,y,z),m.v_m_s[(z*(ny+1)+y)*nx+x]);
    for(std::size_t z=1;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        face(ci(x,y,z-1),ci(x,y,z),m.w_m_s[(z*ny+y)*nx+x]);
    out.surface_energy_proxy_j=sigma_n_m*out.diffuse_area_m2;
    out.total_proxy_j=out.mac_kinetic_j+out.gravity_potential_j+
        out.surface_energy_proxy_j;
    if(!(std::isfinite(out.total_proxy_j)&&
         std::isfinite(out.diffuse_area_m2)&&
         std::isfinite(out.water_mass_kg)&&std::isfinite(out.air_mass_kg)))
        throw std::runtime_error("Nonfinite global capillary energy inventory");
    return out;
}
struct P2CapillaryGlobalEnergyLedger3D {
    P2CapillaryEnergyInventory3D before,after;
    P2TwoPhaseCapillaryMomentumViscousLedger3D live;
    double water_mass_residual_kg=0,air_mass_residual_kg=0;
    double surface_area_delta_m2=0,surface_energy_delta_j=0;
    double kinetic_delta_j=0,potential_delta_j=0;
    double total_energy_delta_j=0;
    double final_viscous_pressure_loss_j=0;
    double unresolved_global_energy_j=0;
    bool global_physical_energy_closed=false;
    bool full_P2_accepted=false;
};
inline P2CapillaryGlobalEnergyLedger3D p2_capillary_global_energy_step3d(
    P2TwoPhaseMacVofState3D& state,double sigma_n_m,
    int max_pcg_iterations=4000,
    std::array<double,3> gravity_m_s2={0,0,0},
    double water_dynamic_viscosity_pa_s=.001,
    double air_dynamic_viscosity_pa_s=.000018) {
    P2CapillaryGlobalEnergyLedger3D out;
    out.before=p2_capillary_energy_inventory3d(state,sigma_n_m,gravity_m_s2);
    auto trial=state;
    out.live=p2_two_phase_capillary_momentum_viscous_vof_step3d(
        trial,sigma_n_m,max_pcg_iterations,gravity_m_s2,
        water_dynamic_viscosity_pa_s,air_dynamic_viscosity_pa_s);
    out.after=p2_capillary_energy_inventory3d(trial,sigma_n_m,gravity_m_s2);
    out.water_mass_residual_kg=out.after.water_mass_kg-out.before.water_mass_kg;
    out.air_mass_residual_kg=out.after.air_mass_kg-out.before.air_mass_kg;
    if(std::abs(out.water_mass_residual_kg)>
            1e-9*std::max(1.0,out.before.water_mass_kg)||
       std::abs(out.air_mass_residual_kg)>
            1e-9*std::max(1.0,out.before.air_mass_kg))
        throw std::runtime_error("Capillary global inventory mass drift");
    out.surface_area_delta_m2=out.after.diffuse_area_m2-out.before.diffuse_area_m2;
    out.surface_energy_delta_j=out.after.surface_energy_proxy_j-
        out.before.surface_energy_proxy_j;
    out.kinetic_delta_j=out.after.mac_kinetic_j-out.before.mac_kinetic_j;
    out.potential_delta_j=out.after.gravity_potential_j-
        out.before.gravity_potential_j;
    out.total_energy_delta_j=out.after.total_proxy_j-out.before.total_proxy_j;
    // Only the LAST viscous+pressure split is independently dissipative.
    // Earlier projections, upwind advection, remap and moving surface work
    // are unresolved: do not label this as a global energy conservation test.
    out.final_viscous_pressure_loss_j=
        trial.mac.dt_s*out.live.live.viscous.stress_dissipation_rate_w-
        out.live.live.viscous.explicit_temporal_correction_j+
        out.live.live.final_projection.projection_dissipation_j;
    out.unresolved_global_energy_j=out.total_energy_delta_j+
        out.final_viscous_pressure_loss_j;
    if(!std::isfinite(out.unresolved_global_energy_j))
        throw std::runtime_error("Nonfinite global capillary energy gap");
    state=std::move(trial);
    return out;
}
} // namespace upc
