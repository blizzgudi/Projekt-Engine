#pragma once
// P2 PARTIAL: opt-in balanced continuum-surface-force (CSF) capillary
// acceleration on the MOVING two-phase MAC/VOF field, followed by the
// existing variable-density pressure PCG. Uses a resolved diffuse VOF
// curvature stencil, NOT PLIC/height-function sharp-interface traction.
// It is not a measured surface-tension, full energy or FSI acceptance.
#include "upc/p2_two_phase_momentum_viscous_vof3d.hpp"
#include "upc/p2_vof_height_curvature_field3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2TwoPhaseCapillaryLedger3D {
    P2TwoPhaseMacPressureLedger3D pressure;
    std::size_t active_capillary_faces=0;
    std::size_t height_interface_columns=0;
    int height_normal_axis=-1;
    bool height_curvature_opt_in=false;
    double max_abs_curvature_m_inv=0;
    double interface_curvature_sum_m_inv=0;
    std::size_t interface_curvature_cells=0;
    double max_source_velocity_change_m_s=0;
    double max_post_projection_velocity_m_s=0;
    double diffuse_surface_energy_proxy_j=0;
    double capillary_source_work_j=0;
    double initial_kinetic_j=0;
    std::array<double,3> initial_momentum_kg_m_s{};
    std::array<double,3> source_impulse_kg_m_s{};
    std::array<double,3> source_projection_impulse_residual_kg_m_s{};
    double source_projection_energy_residual_j=0;
    bool sharp_plic_traction_validated=false;
    bool global_surface_energy_closed=false;
    bool full_P2_accepted=false;
};

inline P2TwoPhaseCapillaryLedger3D p2_two_phase_capillary_project3d(
    P2TwoPhaseMacVofState3D& state,double sigma_n_m,
    int max_pcg_iterations=4000,int height_normal_axis=-1) {
    const auto& m=state.mac;
    const auto nx=m.nx,ny=m.ny,nz=m.nz;
    if(!(std::isfinite(sigma_n_m)&&sigma_n_m>=0)||
       nx<3||ny<3||nz<3||nx>128||ny>128||nz>128||
       nx*ny*nz>100000||!(m.dx_m>0&&m.dt_s>0)||
       !std::isfinite(m.dx_m)||!std::isfinite(m.dt_s)||
       state.water_fraction.size()!=nx*ny*nz||
       m.density_kg_m3.size()!=nx*ny*nz)
        throw std::invalid_argument("Invalid capillary two-phase geometry");
    if(height_normal_axis < -1 || height_normal_axis > 2)
        throw std::invalid_argument("Invalid capillary height normal axis");
    const double dx=m.dx_m,dt=m.dt_s,volume=dx*dx*dx;
    if(!(std::isfinite(volume)&&volume>0))
        throw std::invalid_argument("Invalid capillary cell volume");
    const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    const double drho=state.water_density_kg_m3-state.air_density_kg_m3;
    if(!(std::isfinite(drho)&&drho>0&&
         std::isfinite(state.air_density_kg_m3)&&state.air_density_kg_m3>0))
        throw std::invalid_argument("Invalid capillary phase densities");
    for(std::size_t i=0;i<state.water_fraction.size();++i){
        const double a=state.water_fraction[i];
        const double rho=state.air_density_kg_m3+drho*a;
        if(!(std::isfinite(a)&&a>=0&&a<=1&&std::isfinite(rho)&&
             std::isfinite(m.density_kg_m3[i])&&
             std::abs(m.density_kg_m3[i]-rho)<=1e-12*std::max(1.0,rho)))
            throw std::invalid_argument("Invalid capillary VOF/density");
    }
    // Capillary timestep is a necessary explicit stability guard, not a
    // sufficient condition for nonlinear two-phase CFD stability.
    if(sigma_n_m>0 && dt>std::sqrt(
          state.air_density_kg_m3*dx*dx*dx/(2*3.14159265358979323846*sigma_n_m)))
        throw std::invalid_argument("Capillary explicit timestep too large");
    const auto a=[&](std::size_t x,std::size_t y,std::size_t z){
        return state.water_fraction[ci(x,y,z)];
    };
    const auto diff=[&](const std::vector<double>& field,
                         std::size_t x,std::size_t y,std::size_t z,int axis){
        std::size_t lo=axis==0?x:(axis==1?y:z);
        const std::size_t n=axis==0?nx:(axis==1?ny:nz);
        const std::size_t p=lo==0?0:lo-1;
        const std::size_t q=lo+1<n?lo+1:lo;
        const auto at=[&](std::size_t t){
            return field[ci(axis==0?t:x,axis==1?t:y,axis==2?t:z)];
        };
        return (at(q)-at(p))/(double(q-p)*dx);
    };
    std::array<std::vector<double>,3> normal;
    for(auto& v:normal)v.resize(nx*ny*nz);
    std::vector<double> curvature(nx*ny*nz);
    P2TwoPhaseCapillaryLedger3D out;
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const auto id=ci(x,y,z);
        double grad[3];
        for(int k=0;k<3;++k)
            grad[k]=diff(state.water_fraction,x,y,z,k);
        const double norm=std::sqrt(grad[0]*grad[0]+
                                    grad[1]*grad[1]+grad[2]*grad[2]);
        if(!std::isfinite(norm))
            throw std::invalid_argument("Nonfinite capillary gradient");
        for(int k=0;k<3;++k)
            normal[k][id]=norm>1e-12/dx?grad[k]/norm:0;
        out.diffuse_surface_energy_proxy_j+=sigma_n_m*norm*volume;
    }
    if(height_normal_axis>=0){
        // Strict graph-only opt-in: no fallback to the non-convergent
        // diffuse curvature on unsupported overhangs or wall contacts.
        const auto hf=P2VofHeightCurvatureBulk3D::reconstruct(
            nx,ny,nz,dx,state.water_fraction,height_normal_axis);
        curvature=hf.cell_curvature_inv_m;
        out.height_curvature_opt_in=true;
        out.height_normal_axis=height_normal_axis;
        out.height_interface_columns=hf.interface_columns;
    }
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const auto id=ci(x,y,z);
        if(height_normal_axis<0)
            curvature[id]=-(diff(normal[0],x,y,z,0)+
                            diff(normal[1],x,y,z,1)+
                            diff(normal[2],x,y,z,2));
        if(!std::isfinite(curvature[id]))
            throw std::runtime_error("Nonfinite capillary curvature");
        if(a(x,y,z)>0.05&&a(x,y,z)<0.95){
            out.interface_curvature_sum_m_inv+=curvature[id];
            ++out.interface_curvature_cells;
            out.max_abs_curvature_m_inv=std::max(
                out.max_abs_curvature_m_inv,std::abs(curvature[id]));
        }
    }
    // Reconstruct each interior MAC face with the SAME face density as
    // the PCG. For constant curvature the source is a discrete gradient,
    // so pressure can exactly balance the capillary jump at rest.
    auto trial=state;
    const auto apply=[&](std::size_t aidx,std::size_t bidx,
                         double& face,int axis){
        const double jump=state.water_fraction[bidx]-
                          state.water_fraction[aidx];
        const double kappa=.5*(curvature[aidx]+curvature[bidx]);
        const double rho=.5*(m.density_kg_m3[aidx]+m.density_kg_m3[bidx]);
        const double delta=dt*sigma_n_m*kappa*jump/(dx*rho);
        const double old=face;
        const double updated=old+delta;
        if(!std::isfinite(updated)||!std::isfinite(delta))
            throw std::runtime_error("Nonfinite capillary MAC source");
        face=updated;
        const double mass=rho*volume;
        out.initial_momentum_kg_m_s[axis]+=mass*old;
        out.initial_kinetic_j+=.5*mass*old*old;
        out.source_impulse_kg_m_s[axis]+=mass*delta;
        out.capillary_source_work_j+=.5*mass*(updated*updated-old*old);
        out.max_source_velocity_change_m_s=std::max(
            out.max_source_velocity_change_m_s,std::abs(delta));
        if(sigma_n_m>0&&std::abs(jump)>1e-12&&std::abs(kappa)>1e-12)
            ++out.active_capillary_faces;
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=1;x<nx;++x)
        apply(ci(x-1,y,z),ci(x,y,z),
              trial.mac.u_m_s[(z*ny+y)*(nx+1)+x],0);
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        apply(ci(x,y-1,z),ci(x,y,z),
              trial.mac.v_m_s[(z*(ny+1)+y)*nx+x],1);
    for(std::size_t z=1;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        apply(ci(x,y,z-1),ci(x,y,z),
              trial.mac.w_m_s[(z*ny+y)*nx+x],2);
    out.pressure=p2_two_phase_mac_pressure_step3d(
        trial.mac,max_pcg_iterations);
    for(int k=0;k<3;++k){
        out.source_projection_impulse_residual_kg_m_s[k]=
            out.pressure.momentum_after_kg_m_s[k]-
            out.initial_momentum_kg_m_s[k]-
            out.source_impulse_kg_m_s[k]-
            out.pressure.wall_pressure_impulse_n_s[k];
        // Full source+pressure substep, NOT moving-surface energy/FSI.
        if(!std::isfinite(out.source_impulse_kg_m_s[k])||
           std::abs(out.source_projection_impulse_residual_kg_m_s[k])>
               1e-9*(1+std::abs(out.source_impulse_kg_m_s[k])))
            throw std::runtime_error("Capillary pressure impulse mismatch");
    }
    out.source_projection_energy_residual_j=
        out.pressure.kinetic_after_j-out.initial_kinetic_j-
        out.capillary_source_work_j+
        out.pressure.projection_dissipation_j;
    if(!std::isfinite(out.source_projection_energy_residual_j)||
       std::abs(out.source_projection_energy_residual_j)>
           1e-9*(1+out.pressure.kinetic_before_j))
        throw std::runtime_error("Capillary pressure energy mismatch");
    for(const auto* f:{&trial.mac.u_m_s,&trial.mac.v_m_s,&trial.mac.w_m_s})
        for(double v:*f)out.max_post_projection_velocity_m_s=
            std::max(out.max_post_projection_velocity_m_s,std::abs(v));
    state=std::move(trial);
    return out;
}

struct P2TwoPhaseCapillaryMomentumViscousLedger3D {
    P2TwoPhaseCapillaryLedger3D capillary;
    P2TwoPhaseMomentumViscousVofLedger3D live;
    bool capillary_applied=false;
    bool full_P2_accepted=false;
};
inline P2TwoPhaseCapillaryMomentumViscousLedger3D
p2_two_phase_capillary_momentum_viscous_vof_step3d(
    P2TwoPhaseMacVofState3D& state,double sigma_n_m,
    int max_pcg_iterations=4000,
    std::array<double,3> gravity_m_s2={0,0,0},
    double water_dynamic_viscosity_pa_s=.001,
    double air_dynamic_viscosity_pa_s=.000018,
    int height_normal_axis=-1){
    if(!(std::isfinite(sigma_n_m)&&sigma_n_m>=0))
        throw std::invalid_argument("Invalid live capillary tension");
    auto trial=state;
    P2TwoPhaseCapillaryMomentumViscousLedger3D out;
    if(sigma_n_m>0){
        out.capillary=p2_two_phase_capillary_project3d(
            trial,sigma_n_m,max_pcg_iterations,height_normal_axis);
        out.capillary_applied=true;
    }
    out.live=p2_two_phase_momentum_viscous_vof_step3d(
        trial,max_pcg_iterations,gravity_m_s2,
        water_dynamic_viscosity_pa_s,air_dynamic_viscosity_pa_s);
    state=std::move(trial);
    return out;
}
} // namespace upc
