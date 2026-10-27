#pragma once
// P2 partial: cell-centred pressure traction on *static, axis-aligned* solid voxels.
// Diagnostic only. No body dynamics, pressure/viscous stress integration over
// cut cells, surface tension, or bidirectional FSI impulse transfer.
// In a fully enclosed pure-Neumann liquid, pressure is defined only up to
// a constant; a solid voxel must be wetted on all opposing faces for its
// NET force to be invariant to that arbitrary pressure gauge.
#include "upc/mac3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct StaticSolidPressureLoad3D {
    std::array<double,3> force_N{{0.0,0.0,0.0}};
    std::size_t wetted_faces=0;
    double wetted_area_m2=0.0;
};

// Static diagnostic of a caller-selected set of solid voxels. The torque is
// about an explicit world-space origin; it is NOT an applied FSI impulse.
struct StaticSolidBodyPressureLoad3D {
    std::array<double,3> force_N{{0.0,0.0,0.0}};
    std::array<double,3> torque_Nm{{0.0,0.0,0.0}};
    std::size_t solid_voxels=0;
    std::size_t wetted_faces=0;
    double wetted_area_m2=0.0;
};

class StaticSolidPressureProbe3D {
    const MacProjection3D& mac_;
    std::size_t nx_,ny_,nz_;
    double area_,dx_;
    // The caller must keep both occupancy masks alive and unchanged during
    // probe use; the masks must describe the same grid as the MAC projection.
    const std::vector<std::uint8_t>& wet_;
    const std::vector<std::uint8_t>& solid_;
    std::size_t index(std::size_t x,std::size_t y,std::size_t z) const {
        return (z*ny_+y)*nx_+x;
    }
public:
    StaticSolidPressureProbe3D(
        const MacProjection3D& mac,std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid)
      :mac_(mac),nx_(nx),ny_(ny),nz_(nz),area_(dx*dx),dx_(dx),wet_(wet),solid_(solid) {
        if(nx==0||ny==0||nz==0||
           nx>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           ny>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nz>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           !std::isfinite(dx)||dx<=0||!std::isfinite(area_)||area_<=0||
           wet.size()!=nx*ny*nz||solid.size()!=nx*ny*nz)
            throw std::invalid_argument("Invalid static pressure probe grid");
        for(std::size_t i=0;i<wet.size();++i)
            if(wet[i]>1||solid[i]>1||(wet[i]&&solid[i]))
                throw std::invalid_argument("Overlapping or invalid occupancy masks");
        // Detect a MAC grid smaller than the supplied masks up front.
        (void)mac_.pressure_pa(nx_-1,ny_-1,nz_-1);
    }

    // body_mask selects one or more static solid voxels, excluding any
    // enclosing wall voxels not part of the requested body. Each exposed
    // wet face contributes -p*n_solid_to_liquid*A at its geometric centre.
    // Caller must supply a finite torque origin in world metres.
    StaticSolidBodyPressureLoad3D load_on_body(
        const std::vector<std::uint8_t>& body_mask,
        const std::array<double,3>& torque_origin_m) const {
        if(body_mask.size()!=solid_.size() ||
           !std::isfinite(torque_origin_m[0]) ||
           !std::isfinite(torque_origin_m[1]) ||
           !std::isfinite(torque_origin_m[2]))
            throw std::invalid_argument("Invalid static body pressure probe input");
        constexpr int directions[6][3] = {
            {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}
        };
        StaticSolidBodyPressureLoad3D result;
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x) {
            const auto id=index(x,y,z);
            if(body_mask[id]>1 || (body_mask[id] && !solid_[id]))
                throw std::invalid_argument("Body mask must select only solid voxels");
            if(!body_mask[id])continue;
            ++result.solid_voxels;
            for(const auto& d:directions) {
                const int xx=static_cast<int>(x)+d[0];
                const int yy=static_cast<int>(y)+d[1];
                const int zz=static_cast<int>(z)+d[2];
                if(xx<0||yy<0||zz<0 ||
                   xx>=static_cast<int>(nx_) ||
                   yy>=static_cast<int>(ny_) ||
                   zz>=static_cast<int>(nz_))continue;
                const auto ux=static_cast<std::size_t>(xx);
                const auto uy=static_cast<std::size_t>(yy);
                const auto uz=static_cast<std::size_t>(zz);
                if(!wet_[index(ux,uy,uz)])continue;
                const double pressure=mac_.pressure_pa(ux,uy,uz);
                if(!std::isfinite(pressure))
                    throw std::runtime_error("Nonfinite static body pressure");
                const std::array<double,3> force{{
                    -d[0]*pressure*area_,-d[1]*pressure*area_,
                    -d[2]*pressure*area_
                }};
                const std::array<double,3> arm{{
                    (static_cast<double>(x)+0.5+0.5*d[0])*dx_-torque_origin_m[0],
                    (static_cast<double>(y)+0.5+0.5*d[1])*dx_-torque_origin_m[1],
                    (static_cast<double>(z)+0.5+0.5*d[2])*dx_-torque_origin_m[2]
                }};
                for(std::size_t axis=0;axis<3;++axis)
                    result.force_N[axis]+=force[axis];
                result.torque_Nm[0]+=arm[1]*force[2]-arm[2]*force[1];
                result.torque_Nm[1]+=arm[2]*force[0]-arm[0]*force[2];
                result.torque_Nm[2]+=arm[0]*force[1]-arm[1]*force[0];
                ++result.wetted_faces;
            }
        }
        if(result.solid_voxels==0)
            throw std::invalid_argument("Static body pressure probe requires solid voxels");
        result.wetted_area_m2=static_cast<double>(result.wetted_faces)*area_;
        for(std::size_t axis=0;axis<3;++axis)
            if(!std::isfinite(result.force_N[axis]) ||
               !std::isfinite(result.torque_Nm[axis]))
                throw std::overflow_error("Static body pressure load overflow");
        return result;
    }

    StaticSolidPressureLoad3D load_on_solid(
        std::size_t x,std::size_t y,std::size_t z) const {
        if(x>=nx_||y>=ny_||z>=nz_||!solid_[index(x,y,z)])
            throw std::invalid_argument("Pressure load requires a solid voxel");
        StaticSolidPressureLoad3D result;
        constexpr int directions[6][3] = {
            {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}
        };
        for(const auto& d:directions) {
            const int xx=static_cast<int>(x)+d[0];
            const int yy=static_cast<int>(y)+d[1];
            const int zz=static_cast<int>(z)+d[2];
            if(xx<0||yy<0||zz<0||
               xx>=static_cast<int>(nx_)||yy>=static_cast<int>(ny_)||
               zz>=static_cast<int>(nz_))continue;
            const auto ux=static_cast<std::size_t>(xx);
            const auto uy=static_cast<std::size_t>(yy);
            const auto uz=static_cast<std::size_t>(zz);
            if(!wet_[index(ux,uy,uz)])continue;
            const double pressure=mac_.pressure_pa(ux,uy,uz);
            if(!std::isfinite(pressure))
                throw std::runtime_error("Nonfinite static pressure load");
            // d points from solid to liquid. Pressure force ON the solid
            // points from the liquid towards the solid: -d * p * face area.
            for(std::size_t axis=0;axis<3;++axis)
                result.force_N[axis]-=static_cast<double>(d[axis])*pressure*area_;
            ++result.wetted_faces;
        }
        result.wetted_area_m2=static_cast<double>(result.wetted_faces)*area_;
        return result;
    }
};
} // namespace upc
