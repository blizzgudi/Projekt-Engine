#pragma once
// P2 PARTIAL: exact geometric swept SOLID volume through 3D MAC faces
// for an axis-aligned box translating linearly within a voxel grid.
// This is a moving-boundary GCL source, NOT a water velocity/pressure
// flux, momentum transfer, rotation, or a two-way FSI solution.
#include "upc/p2_moving_box_mac_apertures3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {

struct P2MovingBoxSweptWallFlux3D {
    // Signed swept solid volume, positive along the corresponding axis.
    // Face ordering is identical to P2MovingBoxMacApertures3D.
    std::vector<double> u_solid_m3, v_solid_m3, w_solid_m3;
    P2MovingBoxMacApertures3D geometry;
    double max_local_gcl_residual_m3=0.0;
    double signed_open_volume_change_m3=0.0;
    double boundary_solid_flux_m3=0.0;
};

class P2MovingBoxSweptWallFlux3DBuilder {
public:
    // Integrates (wall_velocity dot face_normal) * blocked_face_area
    // over dt. For constant rigid translation this equals
    // translation[axis] * dx^2 * (1 - time_averaged_open_aperture).
    // Per-cell geometric conservation: (alpha_new-alpha_old)*dx^3
    //   = Qx_right-Qx_left + Qy_top-Qy_bottom + Qz_far-Qz_near.
    // No input is mutated on failure.
    static P2MovingBoxSweptWallFlux3D sample(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,
        const P2Box3D& old_box,const P2Box3D& new_box) {
        P2MovingBoxSweptWallFlux3D out;
        out.geometry=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx_m,dt_s,old_box,new_box);
        const auto& shift=out.geometry.cell_geometry.translation_m;
        const double area=dx_m*dx_m;
        const double volume=area*dx_m;
        if(!std::isfinite(area)||area<=0.0||
           !std::isfinite(volume)||volume<=0.0)
            throw std::invalid_argument("Nonfinite swept-wall cell geometry");
        const auto make_flux=[&](const std::vector<double>& apertures,
                                 double displacement,
                                 std::vector<double>& result) {
            result.reserve(apertures.size());
            for(double a:apertures) {
                const double q=displacement*area*(1.0-a);
                if(!std::isfinite(q))
                    throw std::invalid_argument("Nonfinite swept-wall flux");
                result.push_back(q);
            }
        };
        make_flux(out.geometry.u_open,shift[0],out.u_solid_m3);
        make_flux(out.geometry.v_open,shift[1],out.v_solid_m3);
        make_flux(out.geometry.w_open,shift[2],out.w_solid_m3);
        const auto u=[&](std::size_t x,std::size_t y,std::size_t z) {
            return out.u_solid_m3[(z*ny+y)*(nx+1)+x];
        };
        const auto v=[&](std::size_t x,std::size_t y,std::size_t z) {
            return out.v_solid_m3[(z*(ny+1)+y)*nx+x];
        };
        const auto w=[&](std::size_t x,std::size_t y,std::size_t z) {
            return out.w_solid_m3[(z*ny+y)*nx+x];
        };
        const auto& old_alpha=out.geometry.cell_geometry.alpha_old;
        const auto& new_alpha=out.geometry.cell_geometry.alpha_new;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const std::size_t i=(z*ny+y)*nx+x;
            const double delta=(new_alpha[i]-old_alpha[i])*volume;
            const double div=u(x+1,y,z)-u(x,y,z)
                            +v(x,y+1,z)-v(x,y,z)
                            +w(x,y,z+1)-w(x,y,z);
            const double residual=std::abs(delta-div);
            if(!std::isfinite(residual))
                throw std::logic_error("Nonfinite swept-wall GCL residual");
            out.max_local_gcl_residual_m3=
                std::max(out.max_local_gcl_residual_m3,residual);
            out.signed_open_volume_change_m3+=delta;
        }
        // Sum all six outward boundary fluxes; a box contained in the
        // domain cannot advect solid volume through its outer boundary.
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
            out.boundary_solid_flux_m3+=u(nx,y,z)-u(0,y,z);
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t x=0;x<nx;++x)
            out.boundary_solid_flux_m3+=v(x,ny,z)-v(x,0,z);
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            out.boundary_solid_flux_m3+=w(x,y,nz)-w(x,y,0);
        const double tol=5e-10*volume;
        if(out.max_local_gcl_residual_m3>tol ||
           std::abs(out.boundary_solid_flux_m3)>tol ||
           std::abs(out.signed_open_volume_change_m3)>tol)
            throw std::logic_error("Moving-box swept-wall GCL failed");
        return out;
    }
};
} // namespace upc
