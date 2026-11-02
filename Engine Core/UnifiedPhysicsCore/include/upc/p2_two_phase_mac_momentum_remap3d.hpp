#pragma once
// P2 PARTIAL: physically mass-weighted cell-to-staggered MAC reconstruction
// from conserved 3D two-phase cell momenta. A standard MAC face owns the
// half-cell masses of its two neighbours, so its velocity is their
// mass-weighted average, NOT the old unweighted velocity average.
// Exact partition identity includes the omitted virtual boundary half-faces;
// it does not claim those missing wall momentum terms are a real wall force.
// The subsequent PCG pressure impulse remains a separate explicit ledger.
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
struct P2TwoPhaseMacMomentumRemap3D {
    std::vector<double> u_m_s,v_m_s,w_m_s;
    std::array<double,3> cell_impulse_kg_m_s{};
    std::array<double,3> interior_mac_impulse_kg_m_s{};
    std::array<double,3> omitted_boundary_half_face_impulse_kg_m_s{};
    std::array<double,3> exact_partition_residual_kg_m_s{};
    double cell_kinetic_j=0;
    double interior_mac_kinetic_j=0;
    double omitted_boundary_half_face_kinetic_j=0;
    double interior_interpolation_loss_j=0;
    double max_density_weight_disagreement_m_s=0;
    std::size_t interior_face_count=0;
};
inline P2TwoPhaseMacMomentumRemap3D p2_two_phase_mac_momentum_remap3d(
    const P2TwoPhaseMacPressureGrid3D& mac,
    const std::vector<std::array<double,3>>& cell_velocity) {
    const auto nx=mac.nx,ny=mac.ny,nz=mac.nz;
    if(nx<2||ny<2||nz<2||nx>128||ny>128||nz>128||
       nx*ny*nz>100000||
       !std::isfinite(mac.dx_m)||mac.dx_m<=0||
       mac.density_kg_m3.size()!=nx*ny*nz||
       cell_velocity.size()!=nx*ny*nz||
       mac.u_m_s.size()!=(nx+1)*ny*nz||
       mac.v_m_s.size()!=nx*(ny+1)*nz||
       mac.w_m_s.size()!=nx*ny*(nz+1))
        throw std::invalid_argument("Invalid two-phase MAC momentum remap dimensions");
    const double volume=mac.dx_m*mac.dx_m*mac.dx_m;
    if(!std::isfinite(volume)||volume<=0)
        throw std::invalid_argument("Invalid two-phase MAC remap cell volume");
    P2TwoPhaseMacMomentumRemap3D out;
    out.u_m_s.assign(mac.u_m_s.size(),0);
    out.v_m_s.assign(mac.v_m_s.size(),0);
    out.w_m_s.assign(mac.w_m_s.size(),0);
    const auto cell=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    for(std::size_t i=0;i<cell_velocity.size();++i){
        const double rho=mac.density_kg_m3[i];
        if(!std::isfinite(rho)||rho<=0)
            throw std::invalid_argument("Invalid two-phase MAC remap material density");
        for(int axis=0;axis<3;++axis){
            const double velocity=cell_velocity[i][axis];
            if(!std::isfinite(velocity))
                throw std::invalid_argument("Nonfinite cell momentum during remap");
            out.cell_impulse_kg_m_s[axis]+=rho*volume*velocity;
            out.cell_kinetic_j+=.5*rho*volume*velocity*velocity;
        }
    }
    // Each physical closed boundary owns a half cell of the staggered
    // dual control volume. The virtual face's momentum is explicitly
    // BOOKED, never injected as a nonphysical wall-normal velocity.
    const auto boundary=[&](std::size_t c,int axis){
        const double mass=.5*mac.density_kg_m3[c]*volume;
        const double v=cell_velocity[c][axis];
        out.omitted_boundary_half_face_impulse_kg_m_s[axis]+=mass*v;
        out.omitted_boundary_half_face_kinetic_j+=.5*mass*v*v;
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y){
        boundary(cell(0,y,z),0);
        boundary(cell(nx-1,y,z),0);
    }
    for(std::size_t z=0;z<nz;++z)for(std::size_t x=0;x<nx;++x){
        boundary(cell(x,0,z),1);
        boundary(cell(x,ny-1,z),1);
    }
    for(std::size_t y=0;y<ny;++y)for(std::size_t x=0;x<nx;++x){
        boundary(cell(x,y,0),2);
        boundary(cell(x,y,nz-1),2);
    }
    const auto interior=[&](std::size_t a,std::size_t b,int axis,
                            double& target) {
        const double rho_a=mac.density_kg_m3[a],rho_b=mac.density_kg_m3[b];
        const double m_a=.5*rho_a*volume,m_b=.5*rho_b*volume;
        const double mass=m_a+m_b;
        const double numerator=m_a*cell_velocity[a][axis]+
                               m_b*cell_velocity[b][axis];
        const double vel=numerator/mass;
        if(!std::isfinite(mass)||mass<=0||!std::isfinite(vel))
            throw std::runtime_error("Nonfinite mass-weighted MAC remap face");
        target=vel;
        out.interior_mac_impulse_kg_m_s[axis]+=mass*vel;
        out.interior_mac_kinetic_j+=.5*mass*vel*vel;
        out.max_density_weight_disagreement_m_s=std::max(
            out.max_density_weight_disagreement_m_s,
            std::abs(vel-.5*(cell_velocity[a][axis]+cell_velocity[b][axis])));
        ++out.interior_face_count;
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=1;x<nx;++x)
        interior(cell(x-1,y,z),cell(x,y,z),0,
                 out.u_m_s[(z*ny+y)*(nx+1)+x]);
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        interior(cell(x,y-1,z),cell(x,y,z),1,
                 out.v_m_s[(z*(ny+1)+y)*nx+x]);
    for(std::size_t z=1;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        interior(cell(x,y,z-1),cell(x,y,z),2,
                 out.w_m_s[(z*ny+y)*nx+x]);
    out.interior_interpolation_loss_j=
        out.cell_kinetic_j-out.interior_mac_kinetic_j-
        out.omitted_boundary_half_face_kinetic_j;
    if(!std::isfinite(out.interior_interpolation_loss_j)||
       out.interior_interpolation_loss_j<
         -1e-10*std::max(1.0,out.cell_kinetic_j))
        throw std::runtime_error("Two-phase MAC remap unphysical kinetic creation");
    for(int k=0;k<3;++k){
        out.exact_partition_residual_kg_m_s[k]=
            out.cell_impulse_kg_m_s[k]-
            out.interior_mac_impulse_kg_m_s[k]-
            out.omitted_boundary_half_face_impulse_kg_m_s[k];
        if(!std::isfinite(out.exact_partition_residual_kg_m_s[k])||
           std::abs(out.exact_partition_residual_kg_m_s[k])>
              1e-10*std::max(1.0,std::abs(out.cell_impulse_kg_m_s[k])))
            throw std::runtime_error("Two-phase MAC remap mass partition failed");
    }
    return out;
}
} // namespace upc
