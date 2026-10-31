#pragma once
// P2 bounded diagnostic: uniform-in-cell finite-volume VOF inertia.
// A partially filled cell is approximated by a uniformly distributed
// fractional density, NOT its unknown actual free-surface geometry.
// Does not model subcell vorticity, rigid-body motion or FSI torques.
#include "upc/p2_vof_angular_inventory3d.hpp"
#include <cmath>
#include <stdexcept>

namespace upc {
// Adds the exact intrinsic inertia of a uniformly filled cubic cell to the
// existing cell-centre point-mass inertia tensor. The correction is
// (m*dx^2/6) on each diagonal, both about the origin and the COM.
// Mass, centroid, linear and angular momentum remain unchanged.
inline VofAngularInventory3D vof_uniform_cell_inertia3d(
    const MacProjection3D& mac,
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double rho,
    const std::array<double,3>& origin_m={0,0,0}) {
    auto out=vof_angular_inventory3d(
        mac,fill,solid,nx,ny,nz,dx,rho,origin_m);
    const double correction=out.mass_kg*(dx*dx/6.0);
    if(!std::isfinite(correction))
        throw std::runtime_error("Nonfinite uniform-cell VOF inertia");
    for(int k=0;k<3;++k) {
        out.inertia_about_origin_kg_m2[k][k]+=correction;
        out.inertia_about_com_kg_m2[k][k]+=correction;
        if(!std::isfinite(out.inertia_about_origin_kg_m2[k][k])||
           !std::isfinite(out.inertia_about_com_kg_m2[k][k]))
            throw std::runtime_error("Nonfinite corrected VOF inertia");
    }
    return out;
}
} // namespace upc
