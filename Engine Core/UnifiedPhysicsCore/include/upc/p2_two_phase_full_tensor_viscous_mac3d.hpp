#pragma once
// P2 PARTIAL: conservative 3D symmetric-rate-of-strain viscous MAC
// stress divergence, including all three shear components and variable
// water/air viscosity. Face forces are pairwise shared; exterior normal
// velocities are zero, exterior tangential shear is free-slip.
// Heterogeneous viscosity is a CELL-FRACTION PROXY, not geometrically
// sharp SDF/PLIC interface stress, surface tension or full CFD/FSI.
// Explicit energy budget: kinetic_before - kinetic_after = dt*D - dt²*Q/2.
#include "upc/p2_two_phase_mac_pressure3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2TwoPhaseFullTensorViscousLedger3D {
    double kinetic_before_j=0,kinetic_after_j=0;
    double stress_dissipation_rate_w=0;
    double explicit_temporal_correction_j=0;
    double energy_residual_j=0;
    double max_velocity_change_m_s=0;
    std::array<double,3> momentum_before_kg_m_s{};
    std::array<double,3> momentum_after_kg_m_s{};
    std::array<double,3> wall_normal_traction_impulse_n_s{};
    std::array<double,3> momentum_residual_kg_m_s{};
    std::size_t normal_stress_cells=0;
    std::size_t xy_shear_edges=0,xz_shear_edges=0,yz_shear_edges=0;
    bool physical_full_two_phase_stress_validated=false;
};
inline P2TwoPhaseFullTensorViscousLedger3D
p2_two_phase_full_tensor_viscous_mac_step3d(
    P2TwoPhaseMacPressureGrid3D& mac,
    const std::vector<double>& water_fraction,
    double water_dynamic_viscosity_pa_s=.001,
    double air_dynamic_viscosity_pa_s=.000018) {
    const std::size_t nx=mac.nx,ny=mac.ny,nz=mac.nz;
    if(nx<2||ny<2||nz<2||nx>128||ny>128||nz>128||
       nx*ny*nz>100000||water_fraction.size()!=nx*ny*nz||
       mac.density_kg_m3.size()!=water_fraction.size()||
       mac.u_m_s.size()!=(nx+1)*ny*nz||
       mac.v_m_s.size()!=nx*(ny+1)*nz||
       mac.w_m_s.size()!=nx*ny*(nz+1)||
       !std::isfinite(mac.dx_m)||mac.dx_m<=0||
       !std::isfinite(mac.dt_s)||mac.dt_s<=0||
       !std::isfinite(water_dynamic_viscosity_pa_s)||
       !std::isfinite(air_dynamic_viscosity_pa_s)||
       water_dynamic_viscosity_pa_s<=0||air_dynamic_viscosity_pa_s<=0)
        throw std::invalid_argument("Invalid viscous two-phase MAC mesh/material");
    const std::size_t n=nx*ny*nz;
    const double dx=mac.dx_m,dt=mac.dt_s,volume=dx*dx*dx,area=dx*dx;
    if(!std::isfinite(volume)||volume<=0||!std::isfinite(dt/dx))
        throw std::invalid_argument("Nonfinite viscous MAC finite volume");
    std::vector<double> viscosity(n);
    double min_rho=std::numeric_limits<double>::max(),max_mu=0;
    for(std::size_t i=0;i<n;++i) {
        const double alpha=water_fraction[i];
        const double rho=mac.density_kg_m3[i];
        if(!std::isfinite(alpha)||alpha<0||alpha>1||
           !std::isfinite(rho)||rho<=0)
            throw std::invalid_argument("Invalid water/air fraction or density");
        const double mu=air_dynamic_viscosity_pa_s+
            alpha*(water_dynamic_viscosity_pa_s-air_dynamic_viscosity_pa_s);
        if(!std::isfinite(mu)||mu<=0)
            throw std::invalid_argument("Invalid mixed dynamic viscosity");
        viscosity[i]=mu;
        min_rho=std::min(min_rho,rho);
        max_mu=std::max(max_mu,mu);
    }
    // Conservative explicit spectral safety bound, not an accuracy proof.
    if(dt>min_rho*dx*dx/(24*max_mu))
        throw std::invalid_argument("Viscous MAC explicit timestep unstable");
    for(double vel:mac.u_m_s)if(!std::isfinite(vel))
        throw std::invalid_argument("Nonfinite u face");
    for(double vel:mac.v_m_s)if(!std::isfinite(vel))
        throw std::invalid_argument("Nonfinite v face");
    for(double vel:mac.w_m_s)if(!std::isfinite(vel))
        throw std::invalid_argument("Nonfinite w face");
    const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    const auto ui=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*(nx+1)+x;
    };
    const auto vi=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*(ny+1)+y)*nx+x;
    };
    const auto wi=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        if(mac.u_m_s[ui(0,y,z)]!=0||mac.u_m_s[ui(nx,y,z)]!=0)
            throw std::invalid_argument("Invalid viscous x wall normal");
    for(std::size_t z=0;z<nz;++z)for(std::size_t x=0;x<nx;++x)
        if(mac.v_m_s[vi(x,0,z)]!=0||mac.v_m_s[vi(x,ny,z)]!=0)
            throw std::invalid_argument("Invalid viscous y wall normal");
    for(std::size_t y=0;y<ny;++y)for(std::size_t x=0;x<nx;++x)
        if(mac.w_m_s[wi(x,y,0)]!=0||mac.w_m_s[wi(x,y,nz)]!=0)
            throw std::invalid_argument("Invalid viscous z wall normal");
    const auto xy=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*(ny+1)+y)*(nx+1)+x;
    };
    const auto xz=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*(nx+1)+x;
    };
    const auto yz=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*(ny+1)+y)*nx+x;
    };
    std::vector<double> tau_xx(n),tau_yy(n),tau_zz(n);
    std::vector<double> tau_xy((nx+1)*(ny+1)*nz,0);
    std::vector<double> tau_xz((nx+1)*ny*(nz+1),0);
    std::vector<double> tau_yz(nx*(ny+1)*(nz+1),0);
    P2TwoPhaseFullTensorViscousLedger3D result;
    // Build all six independent symmetric strain components at their
    // natural MAC stress locations. The same edge shear is used in
    // both adjacent momentum equations (equal/opposite work).
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto i=ci(x,y,z);
        const double ux=(mac.u_m_s[ui(x+1,y,z)]-mac.u_m_s[ui(x,y,z)])/dx;
        const double vy=(mac.v_m_s[vi(x,y+1,z)]-mac.v_m_s[vi(x,y,z)])/dx;
        const double wz=(mac.w_m_s[wi(x,y,z+1)]-mac.w_m_s[wi(x,y,z)])/dx;
        tau_xx[i]=2*viscosity[i]*ux;
        tau_yy[i]=2*viscosity[i]*vy;
        tau_zz[i]=2*viscosity[i]*wz;
        result.stress_dissipation_rate_w+=
            2*viscosity[i]*(ux*ux+vy*vy+wz*wz)*volume;
        ++result.normal_stress_cells;
    }
    // Harmonic shear viscosity across four actual physical cells.
    // All outer edges carry free-slip τ_tn=0 (an explicit BC).
    const auto harmonic=[&](std::array<std::size_t,4> idx) {
        double denom=0;
        for(auto i:idx)denom+=1/viscosity[i];
        return 4/denom;
    };
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=1;x<nx;++x) {
        const double strain=(
            mac.u_m_s[ui(x,y,z)]-mac.u_m_s[ui(x,y-1,z)]+
            mac.v_m_s[vi(x,y,z)]-mac.v_m_s[vi(x-1,y,z)])/dx;
        const double mu=harmonic({ci(x-1,y-1,z),ci(x,y-1,z),
                                  ci(x-1,y,z),ci(x,y,z)});
        tau_xy[xy(x,y,z)]=mu*strain;
        result.stress_dissipation_rate_w+=mu*strain*strain*volume;
        ++result.xy_shear_edges;
    }
    for(std::size_t z=1;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=1;x<nx;++x) {
        const double strain=(
            mac.u_m_s[ui(x,y,z)]-mac.u_m_s[ui(x,y,z-1)]+
            mac.w_m_s[wi(x,y,z)]-mac.w_m_s[wi(x-1,y,z)])/dx;
        const double mu=harmonic({ci(x-1,y,z-1),ci(x,y,z-1),
                                  ci(x-1,y,z),ci(x,y,z)});
        tau_xz[xz(x,y,z)]=mu*strain;
        result.stress_dissipation_rate_w+=mu*strain*strain*volume;
        ++result.xz_shear_edges;
    }
    for(std::size_t z=1;z<nz;++z)
    for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double strain=(
            mac.v_m_s[vi(x,y,z)]-mac.v_m_s[vi(x,y,z-1)]+
            mac.w_m_s[wi(x,y,z)]-mac.w_m_s[wi(x,y-1,z)])/dx;
        const double mu=harmonic({ci(x,y-1,z-1),ci(x,y,z-1),
                                  ci(x,y-1,z),ci(x,y,z)});
        tau_yz[yz(x,y,z)]=mu*strain;
        result.stress_dissipation_rate_w+=mu*strain*strain*volume;
        ++result.yz_shear_edges;
    }
    auto new_u=mac.u_m_s,new_v=mac.v_m_s,new_w=mac.w_m_s;
    const auto update=[&](double old,double& target,double force_density,
                          double rho,int axis) {
        const double acceleration=force_density/rho;
        target=old+dt*acceleration;
        if(!std::isfinite(target)||!std::isfinite(acceleration))
            throw std::runtime_error("Nonfinite full-tensor viscous acceleration");
        result.momentum_before_kg_m_s[axis]+=rho*volume*old;
        result.momentum_after_kg_m_s[axis]+=rho*volume*target;
        result.kinetic_before_j+=.5*rho*volume*old*old;
        result.kinetic_after_j+=.5*rho*volume*target*target;
        result.explicit_temporal_correction_j+=
            .5*rho*volume*dt*dt*acceleration*acceleration;
        result.max_velocity_change_m_s=std::max(
            result.max_velocity_change_m_s,std::abs(target-old));
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=1;x<nx;++x) {
        const double force=(
            tau_xx[ci(x,y,z)]-tau_xx[ci(x-1,y,z)]+
            tau_xy[xy(x,y+1,z)]-tau_xy[xy(x,y,z)]+
            tau_xz[xz(x,y,z+1)]-tau_xz[xz(x,y,z)])/dx;
        const double rho=.5*(mac.density_kg_m3[ci(x-1,y,z)]+
                              mac.density_kg_m3[ci(x,y,z)]);
        const auto i=ui(x,y,z);
        update(mac.u_m_s[i],new_u[i],force,rho,0);
    }
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double force=(
            tau_xy[xy(x+1,y,z)]-tau_xy[xy(x,y,z)]+
            tau_yy[ci(x,y,z)]-tau_yy[ci(x,y-1,z)]+
            tau_yz[yz(x,y,z+1)]-tau_yz[yz(x,y,z)])/dx;
        const double rho=.5*(mac.density_kg_m3[ci(x,y-1,z)]+
                              mac.density_kg_m3[ci(x,y,z)]);
        const auto i=vi(x,y,z);
        update(mac.v_m_s[i],new_v[i],force,rho,1);
    }
    for(std::size_t z=1;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double force=(
            tau_xz[xz(x+1,y,z)]-tau_xz[xz(x,y,z)]+
            tau_yz[yz(x,y+1,z)]-tau_yz[yz(x,y,z)]+
            tau_zz[ci(x,y,z)]-tau_zz[ci(x,y,z-1)])/dx;
        const double rho=.5*(mac.density_kg_m3[ci(x,y,z-1)]+
                              mac.density_kg_m3[ci(x,y,z)]);
        const auto i=wi(x,y,z);
        update(mac.w_m_s[i],new_w[i],force,rho,2);
    }
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        result.wall_normal_traction_impulse_n_s[0]+=dt*area*(
            tau_xx[ci(nx-1,y,z)]-tau_xx[ci(0,y,z)]);
    for(std::size_t z=0;z<nz;++z)for(std::size_t x=0;x<nx;++x)
        result.wall_normal_traction_impulse_n_s[1]+=dt*area*(
            tau_yy[ci(x,ny-1,z)]-tau_yy[ci(x,0,z)]);
    for(std::size_t y=0;y<ny;++y)for(std::size_t x=0;x<nx;++x)
        result.wall_normal_traction_impulse_n_s[2]+=dt*area*(
            tau_zz[ci(x,y,nz-1)]-tau_zz[ci(x,y,0)]);
    const double imp_scale=std::max(1.0, std::abs(result.momentum_before_kg_m_s[0])+
        std::abs(result.momentum_before_kg_m_s[1])+
        std::abs(result.momentum_before_kg_m_s[2]));
    for(int axis=0;axis<3;++axis) {
        result.momentum_residual_kg_m_s[axis]=
            result.momentum_after_kg_m_s[axis]-
            result.momentum_before_kg_m_s[axis]-
            result.wall_normal_traction_impulse_n_s[axis];
        if(!std::isfinite(result.momentum_residual_kg_m_s[axis])||
           std::abs(result.momentum_residual_kg_m_s[axis])>1e-9*imp_scale)
            throw std::runtime_error(
                "Full-tensor MAC viscous momentum/wall traction mismatch");
    }
    result.energy_residual_j=result.kinetic_before_j-
        result.kinetic_after_j-dt*result.stress_dissipation_rate_w+
        result.explicit_temporal_correction_j;
    const double energy_scale=std::max(1.0,result.kinetic_before_j);
    if(!std::isfinite(result.energy_residual_j)||
       result.stress_dissipation_rate_w<0||
       result.kinetic_after_j>result.kinetic_before_j+1e-10*energy_scale||
       std::abs(result.energy_residual_j)>1e-9*energy_scale)
        throw std::runtime_error("Full-tensor MAC viscous energy ledger mismatch");
    mac.u_m_s.swap(new_u);mac.v_m_s.swap(new_v);mac.w_m_s.swap(new_w);
    return result;
}
} // namespace upc
