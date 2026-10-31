#pragma once
// P2 PARTIAL: read-only, local finite-difference VOF normal reconstruction.
// This estimates interface orientation from neighboring liquid fractions;
// it is NOT an exact PLIC reconstruction, geometric advection, surface
// tension, physical CFD validation, or two-way fluid/solid coupling.
#include "upc/p2_oblique_plic_geometry3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2VofNormalField3D {
    std::vector<std::array<double,3>> normals;
    std::size_t partial_cells=0;
    std::size_t one_sided_axis_estimates=0;
};

// The normal points FROM liquid TOWARD air (opposite the fill gradient),
// matching p2_oblique_plic_geometry3d. Central differences use both valid
// neighboring fluid cells; at a domain or solid boundary, one-sided
// differences are used. A partial cell with no measurable gradient fails
// explicitly rather than silently inventing an interface orientation.
// This is a first-order local estimate near discontinuities and obstacles.
inline P2VofNormalField3D p2_reconstruct_vof_normals3d(
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx) {
    if(nx==0||ny==0||nz==0||
       nx>std::numeric_limits<std::size_t>::max()/ny||
       nx*ny>std::numeric_limits<std::size_t>::max()/nz||
       !std::isfinite(dx)||dx<=0.0)
        throw std::invalid_argument("Invalid VOF normal grid");
    const auto n=nx*ny*nz;
    if(fill.size()!=n||solid.size()!=n)
        throw std::invalid_argument("VOF normal array sizes");
    for(std::size_t i=0;i<n;++i)
        if(!std::isfinite(fill[i])||fill[i]<0.0||fill[i]>1.0||
           solid[i]>1||(solid[i]&&fill[i]>0.0))
            throw std::invalid_argument("Invalid VOF normal cell state");
    P2VofNormalField3D out;
    out.normals.resize(n);
    const std::array<std::size_t,3> sizes{nx,ny,nz};
    const std::array<std::size_t,3> strides{1,nx,nx*ny};
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto i=(z*ny+y)*nx+x;
        if(fill[i]<=0.0||fill[i]>=1.0)continue;
        const std::array<std::size_t,3> pos{x,y,z};
        std::array<double,3> gradient{};
        for(int a=0;a<3;++a) {
            const bool has_left=pos[a]>0&&!solid[i-strides[a]];
            const bool has_right=pos[a]+1<sizes[a]&&!solid[i+strides[a]];
            if(has_left&&has_right)
                gradient[a]=(fill[i+strides[a]]-fill[i-strides[a]])*0.5;
            else if(has_right) {
                gradient[a]=fill[i+strides[a]]-fill[i];
                ++out.one_sided_axis_estimates;
            } else if(has_left) {
                gradient[a]=fill[i]-fill[i-strides[a]];
                ++out.one_sided_axis_estimates;
            }
        }
        // Work in cell-normalized differences to avoid a 1/dx overflow.
        const double scale=std::max({std::abs(gradient[0]),
                                     std::abs(gradient[1]),
                                     std::abs(gradient[2])});
        if(!std::isfinite(scale)||scale<=1e-12)
            throw std::invalid_argument("Unresolvable partial VOF normal");
        const std::array<double,3> unit{
            gradient[0]/scale,gradient[1]/scale,gradient[2]/scale};
        const double len=std::sqrt(unit[0]*unit[0]+
                                   unit[1]*unit[1]+unit[2]*unit[2]);
        for(int a=0;a<3;++a)out.normals[i][a]=-unit[a]/len;
        ++out.partial_cells;
    }
    return out;
}

struct P2ReconstructedPlicGeometry3D {
    P2AxisPlicGeometry3D geometry;
    std::size_t reconstructed_partial_cells=0;
    std::size_t one_sided_axis_estimates=0;
};

// Read-only bridge to the previously tested oblique plane moment integrator.
// Neither the VOF fractions nor the live MAC solver are modified.
inline P2ReconstructedPlicGeometry3D p2_vof_reconstructed_plic_geometry3d(
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx,double rho,
    const std::array<double,3>& origin_m={0,0,0}) {
    auto field=p2_reconstruct_vof_normals3d(fill,solid,nx,ny,nz,dx);
    P2ReconstructedPlicGeometry3D out;
    out.geometry=p2_oblique_plic_geometry3d(
        fill,solid,field.normals,nx,ny,nz,dx,rho,origin_m);
    out.reconstructed_partial_cells=field.partial_cells;
    out.one_sided_axis_estimates=field.one_sided_axis_estimates;
    return out;
}
} // namespace upc
