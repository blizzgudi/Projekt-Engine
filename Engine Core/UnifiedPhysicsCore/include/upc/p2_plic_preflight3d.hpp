#pragma once
// P2 PARTIAL: read-only, fail-closed diagnostic for periodic 3D PLIC normals.
// This preflight NEVER repairs a VOF field or invents interface geometry.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PeriodicPlicPreflight3D {
    std::size_t partial_cells=0, resolved_cells=0, unresolved_cells=0;
    std::size_t unresolved_endpoint_roundoff_candidates=0;
    std::size_t unresolved_geometric_cells=0;
    std::array<std::size_t,3> first_unresolved_xyz{};
    double first_unresolved_fill=0;
    std::array<double,3> first_unresolved_gradient{};
    double first_unresolved_gradient_scale=0;
    double min_resolved_gradient_scale=0;
    bool has_unresolved() const noexcept { return unresolved_cells!=0; }
};

// Uses precisely the same centered periodic gradient and 1e-12 cutoff as
// p2_periodic_plic_normals3d. "Endpoint candidate" is diagnostic only:
// no correction is applied and no ambiguous geometry is declared valid.
inline P2PeriodicPlicPreflight3D p2_periodic_plic_preflight3d(
    const std::vector<double>& fill,
    std::size_t nx,std::size_t ny,std::size_t nz) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<2||ny<2||nz<2||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>2000000||fill.size()!=nx*ny*nz)
        throw std::invalid_argument("Invalid periodic PLIC preflight grid");
    const std::array<std::size_t,3> dims{nx,ny,nz};
    const auto index=[&](const std::array<std::size_t,3>& p) {
        return (p[2]*ny+p[1])*nx+p[0];
    };
    for(double f:fill)
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid periodic PLIC preflight fill");
    P2PeriodicPlicPreflight3D out;
    constexpr double threshold=1e-12;
    constexpr double endpoint_tol=64*std::numeric_limits<double>::epsilon();
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const std::array<std::size_t,3> p{x,y,z};
        const double f=fill[index(p)];
        if(f==0||f==1)continue;
        ++out.partial_cells;
        std::array<double,3> gradient{};
        for(int a=0;a<3;++a) {
            auto left=p,right=p;
            left[a]=(p[a]+dims[a]-1)%dims[a];
            right[a]=(p[a]+1)%dims[a];
            gradient[a]=0.5*(fill[index(right)]-fill[index(left)]);
        }
        const double scale=std::max({std::abs(gradient[0]),
                                     std::abs(gradient[1]),
                                     std::abs(gradient[2])});
        if(scale<=threshold) {
            if(out.unresolved_cells==0) {
                out.first_unresolved_xyz=p;
                out.first_unresolved_fill=f;
                out.first_unresolved_gradient=gradient;
                out.first_unresolved_gradient_scale=scale;
            }
            ++out.unresolved_cells;
            if(f<=endpoint_tol||1-f<=endpoint_tol)
                ++out.unresolved_endpoint_roundoff_candidates;
            else
                ++out.unresolved_geometric_cells;
        } else {
            ++out.resolved_cells;
            if(out.min_resolved_gradient_scale==0||
               scale<out.min_resolved_gradient_scale)
                out.min_resolved_gradient_scale=scale;
        }
    }
    return out;
}
} // namespace upc
