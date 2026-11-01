#pragma once
// P2 PARTIAL: water-fraction-weighted cell-centered MAC inventory.
// This is a diagnostic quadrature, NOT a conservative two-phase momentum
// equation, subcell interface reconstruction, or physical energy closure.
#include "upc/p2_viscous_mac_plic_sequence3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace upc {
struct P2WaterMacInventory3D {
    double water_volume_m3=0;
    double water_mass_kg=0;
    std::array<double,3> water_momentum_kg_m_s{{0,0,0}};
    double water_kinetic_energy_j=0;
};
struct P2WaterMacEndpointDelta3D {
    P2WaterMacInventory3D before, after;
    double water_mass_residual_kg=0;
    std::array<double,3> momentum_change_kg_m_s{{0,0,0}};
    double kinetic_energy_change_j=0;
};

// The two adjacent staggered MAC faces are averaged at the cell center.
// For a partially filled cell the entire cell-centered velocity is weighted
// by its VOF fill; interface-local velocity structure is NOT resolved.
inline P2WaterMacInventory3D p2_water_mac_inventory3d(
    const P2NonlinearMacPlicState3D& state,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double density_kg_m3) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>32768||!std::isfinite(dx_m)||dx_m<=0||
       !std::isfinite(density_kg_m3)||density_kg_m3<=0)
        throw std::invalid_argument("Invalid water MAC inventory grid or units");
    const double volume=dx_m*dx_m*dx_m;
    const double cell_mass=volume*density_kg_m3;
    if(!std::isfinite(cell_mass)||cell_mass<=0||!std::isfinite(volume)||volume<=0)
        throw std::invalid_argument("Invalid water MAC inventory cell volume/mass");
    const std::array<std::size_t,3> sizes{{
        (nx+1)*ny*nz,nx*(ny+1)*nz,nx*ny*(nz+1)}};
    if(state.water_fill.size()!=nx*ny*nz)
        throw std::invalid_argument("Invalid water MAC inventory fill dimensions");
    for(int c=0;c<3;++c) {
        const auto& v=state.staggered_velocity[c];
        if(v.size()!=sizes[c])
            throw std::invalid_argument("Invalid water MAC inventory face dimensions");
        for(double x:v)
            if(!std::isfinite(x))
                throw std::invalid_argument("Nonfinite water MAC inventory velocity");
    }
    const auto& u=state.staggered_velocity[0];
    const auto& v=state.staggered_velocity[1];
    const auto& w=state.staggered_velocity[2];
    auto ux=[&](std::size_t i,std::size_t j,std::size_t k) {
        return (k*ny+j)*(nx+1)+i;
    };
    auto vy=[&](std::size_t i,std::size_t j,std::size_t k) {
        return (k*(ny+1)+j)*nx+i;
    };
    auto wz=[&](std::size_t i,std::size_t j,std::size_t k) {
        return (k*ny+j)*nx+i;
    };
    auto seam_ok=[](double a,double b) {
        return std::abs(a-b)<=1e-12*std::max({1.0,std::abs(a),std::abs(b)});
    };
    P2WaterMacInventory3D out;
    for(std::size_t k=0;k<nz;++k)
    for(std::size_t j=0;j<ny;++j)
    for(std::size_t i=0;i<nx;++i) {
        const std::size_t id=(k*ny+j)*nx+i;
        const double f=state.water_fill[id];
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid water MAC inventory fraction");
        if(i==0&&!seam_ok(u[ux(0,j,k)],u[ux(nx,j,k)]))
            throw std::invalid_argument("Nonperiodic water MAC U seam");
        if(j==0&&!seam_ok(v[vy(i,0,k)],v[vy(i,ny,k)]))
            throw std::invalid_argument("Nonperiodic water MAC V seam");
        if(k==0&&!seam_ok(w[wz(i,j,0)],w[wz(i,j,nz)]))
            throw std::invalid_argument("Nonperiodic water MAC W seam");
        const std::array<double,3> uc{{
            .5*(u[ux(i,j,k)]+u[ux(i+1,j,k)]),
            .5*(v[vy(i,j,k)]+v[vy(i,j+1,k)]),
            .5*(w[wz(i,j,k)]+w[wz(i,j,k+1)])}};
        const double m=cell_mass*f;
        out.water_volume_m3+=volume*f;
        out.water_mass_kg+=m;
        double speed2=0;
        for(int c=0;c<3;++c) {
            out.water_momentum_kg_m_s[c]+=m*uc[c];
            speed2+=uc[c]*uc[c];
        }
        out.water_kinetic_energy_j+=.5*m*speed2;
    }
    if(!std::isfinite(out.water_volume_m3)||
       !std::isfinite(out.water_mass_kg)||
       !std::isfinite(out.water_kinetic_energy_j))
        throw std::runtime_error("Nonfinite water MAC inventory");
    for(double p:out.water_momentum_kg_m_s)
        if(!std::isfinite(p))
            throw std::runtime_error("Nonfinite water MAC momentum");
    return out;
}
inline P2WaterMacEndpointDelta3D p2_water_mac_endpoint_delta3d(
    const P2NonlinearMacPlicState3D& before,
    const P2NonlinearMacPlicState3D& after,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double density_kg_m3) {
    P2WaterMacEndpointDelta3D out;
    out.before=p2_water_mac_inventory3d(before,nx,ny,nz,dx_m,density_kg_m3);
    out.after=p2_water_mac_inventory3d(after,nx,ny,nz,dx_m,density_kg_m3);
    out.water_mass_residual_kg=out.after.water_mass_kg-out.before.water_mass_kg;
    out.kinetic_energy_change_j=
        out.after.water_kinetic_energy_j-out.before.water_kinetic_energy_j;
    for(int c=0;c<3;++c)
        out.momentum_change_kg_m_s[c]=
            out.after.water_momentum_kg_m_s[c]-
            out.before.water_momentum_kg_m_s[c];
    return out;
}
inline P2WaterMacEndpointDelta3D p2_water_mac_sequence_endpoints3d(
    const P2NonlinearMacPlicState3D& initial,
    const P2ViscousMacPlicSequence3D& sequence,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double density_kg_m3) {
    if(sequence.intervals.empty())
        throw std::invalid_argument("Empty water MAC sequence");
    const auto out=p2_water_mac_endpoint_delta3d(
        initial,sequence.state,nx,ny,nz,dx_m,density_kg_m3);
    const double tol=1e-10*std::max(1.0,out.before.water_mass_kg);
    if(std::abs(out.before.water_mass_kg-sequence.initial_water_mass_kg)>tol||
       std::abs(out.after.water_mass_kg-sequence.final_water_mass_kg)>tol)
        throw std::runtime_error("Water MAC endpoint/sequence mass mismatch");
    return out;
}
} // namespace upc
