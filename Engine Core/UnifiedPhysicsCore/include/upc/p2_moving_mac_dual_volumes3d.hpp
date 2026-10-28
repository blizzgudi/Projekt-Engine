#pragma once
// P2 PARTIAL: exact 3D staggered MAC DUAL control-volume geometry for a
// translating axis-aligned rigid box. This supplies geometric GCL and
// swept solid volumes, NOT momentum advection, stress or two-way FSI.
#include "upc/p2_moving_box_swept_wall_flux3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace upc {
struct P2MovingMacDualVolumes3D {
    // u: (nx-1,ny,nz) centered at x=i*dx for i=1..nx-1;
    // v: (nx,ny-1,nz); w: (nx,ny,nz-1). Only interior duals:
    // physical boundary half-duals are excluded, not silently periodic.
    std::array<P2MovingBoxSweptWallFlux3D,3> component;
    std::array<std::array<std::size_t,3>,3> dimensions{};
    double max_local_gcl_residual_m3=0.0;
};

class P2MovingMacDualVolumes3DBuilder {
public:
    static P2MovingMacDualVolumes3D sample(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,const P2Box3D& old_box,
        const P2Box3D& new_box) {
        if(nx<3||ny<3||nz<3||!std::isfinite(dx_m)||dx_m<=0.0||
           !std::isfinite(dt_s)||dt_s<=0.0)
            throw std::invalid_argument("Invalid staggered dual grid/time");
        // Each dual grid is shifted +dx/2 in its own staggered axis.
        // The box must remain inside all three shifted dual domains,
        // hence >=dx/2 clearance from the six exterior domain faces.
        P2MovingMacDualVolumes3D out;
        const std::array<std::size_t,3> dims{{nx,ny,nz}};
        for(int axis=0;axis<3;++axis) {
            auto shape=dims;
            --shape[axis];
            auto a=old_box,b=new_box;
            a.min_m[axis]-=0.5*dx_m;
            a.max_m[axis]-=0.5*dx_m;
            b.min_m[axis]-=0.5*dx_m;
            b.max_m[axis]-=0.5*dx_m;
            out.component[axis]=P2MovingBoxSweptWallFlux3DBuilder::sample(
                shape[0],shape[1],shape[2],dx_m,dt_s,a,b);
            out.dimensions[axis]=shape;
            out.max_local_gcl_residual_m3=std::max(
                out.max_local_gcl_residual_m3,
                out.component[axis].max_local_gcl_residual_m3);
        }
        return out;
    }
};
} // namespace upc
