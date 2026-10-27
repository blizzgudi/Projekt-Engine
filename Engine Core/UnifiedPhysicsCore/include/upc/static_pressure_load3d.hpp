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

class StaticSolidPressureProbe3D {
    const MacProjection3D& mac_;
    std::size_t nx_,ny_,nz_;
    double area_;
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
      :mac_(mac),nx_(nx),ny_(ny),nz_(nz),area_(dx*dx),wet_(wet),solid_(solid) {
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
