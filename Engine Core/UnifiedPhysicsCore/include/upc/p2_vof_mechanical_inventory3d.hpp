#pragma once
// P2 partial: VOF-mass-weighted cell-centred mechanical inventory.
// This is a sampled kinetic-energy diagnostic, NOT an exact staggered-grid
// kinetic-energy quadrature or a conservation proof for moving-body FSI.
#include "upc/mac3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct VofMechanicalInventory3D {
    double mass_kg=0;
    std::array<double,3> momentum_kg_m_s{};
    double kinetic_j=0;
    double gravitational_j=0;
    double total_j=0;
    std::size_t occupied_cells=0;
};

inline VofMechanicalInventory3D vof_mechanical_inventory3d(
    const MacProjection3D& mac,
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double rho) {
    if(!nx||!ny||!nz||
       nx>std::numeric_limits<std::size_t>::max()/ny||
       nx*ny>std::numeric_limits<std::size_t>::max()/nz||
       fill.size()!=nx*ny*nz||solid.size()!=fill.size()||
       !std::isfinite(dx)||dx<=0||!std::isfinite(rho)||rho<=0)
        throw std::invalid_argument("Invalid VOF inventory domain");
    const double cell_mass=rho*dx*dx*dx;
    if(!std::isfinite(cell_mass)||cell_mass<=0)
        throw std::invalid_argument("Invalid VOF inventory cell mass");
    VofMechanicalInventory3D out;
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const std::size_t i=(z*ny+y)*nx+x;
        const double f=fill[i];
        if(!std::isfinite(f)||f<0||f>1||(solid[i]&&f>0))
            throw std::invalid_argument("Invalid VOF inventory occupancy");
        if(f==0)continue; // Dry-cell/ghost-face energy is excluded.
        const double v[3]={
            0.5*(mac.face_velocity_x(x,y,z)+mac.face_velocity_x(x+1,y,z)),
            0.5*(mac.face_velocity_y(x,y,z)+mac.face_velocity_y(x,y+1,z)),
            0.5*(mac.face_velocity_z(x,y,z)+mac.face_velocity_z(x,y,z+1))
        };
        const double m=f*cell_mass;
        out.mass_kg+=m;
        double speed2=0;
        for(int axis=0;axis<3;++axis) {
            if(!std::isfinite(v[axis]))
                throw std::runtime_error("Nonfinite VOF inventory velocity");
            out.momentum_kg_m_s[axis]+=m*v[axis];
            speed2+=v[axis]*v[axis];
        }
        out.kinetic_j+=0.5*m*speed2;
        out.gravitational_j+=m*9.81*(static_cast<double>(z)+0.5)*dx;
        ++out.occupied_cells;
    }
    out.total_j=out.kinetic_j+out.gravitational_j;
    if(!std::isfinite(out.mass_kg)||!std::isfinite(out.kinetic_j)||
       !std::isfinite(out.gravitational_j)||!std::isfinite(out.total_j)||
       out.kinetic_j<0)
        throw std::runtime_error("Nonfinite VOF inventory budget");
    for(double p:out.momentum_kg_m_s)
        if(!std::isfinite(p))
            throw std::runtime_error("Nonfinite VOF inventory momentum");
    return out;
}
} // namespace upc
