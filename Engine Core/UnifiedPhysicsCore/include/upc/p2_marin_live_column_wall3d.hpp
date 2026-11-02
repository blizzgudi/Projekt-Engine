#pragma once
// P2 PARTIAL: connect the real 3D MAC pressure and actual VOF inventory to
// MARIN's geometrically clipped five-face box-wall traction diagnostic.
// This is deliberately restricted to a single bottom-connected water graph
// per vertical column. Reject spray, cavities and overhangs rather than
// pretending the column sum is an exact interface or physical wall stress.
// Binary solid support and extrapolated pressure remain model limitations.
#include "upc/p2_marin_box_wall_traction3d.hpp"
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MarinLiveColumnWallResult3D {
    P2MarinBoxWallTractionResult3D traction{};
    std::size_t wet_columns=0;
    std::size_t partial_surface_columns=0;
    double max_waterline_m=0;
};

class P2MarinLiveColumnWall3D {
public:
    template<class Pressure,class Fraction,class Solid>
    static P2MarinLiveColumnWallResult3D audit_field(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        const P2Box3D& box,Pressure pressure,Fraction fraction,Solid solid) {
        if(nx<2||ny<2||nz<2||!std::isfinite(dx)||dx<=0||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz)
            throw std::invalid_argument("Invalid MARIN live VOF grid");
        const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        std::vector<double> p(nx*ny*nz,0.);
        std::vector<double> heights(nx*ny,0.);
        P2MarinLiveColumnWallResult3D result{};
        constexpr double eps=1e-10;
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            double h=0;
            bool solid_prefix=true,seen_air=false,seen_partial=false;
            bool has_liquid=false;
            for(std::size_t z=0;z<nz;++z) {
                const auto index=id(x,y,z);
                const double value=pressure(x,y,z);
                if(!std::isfinite(value))
                    throw std::invalid_argument("Nonfinite live MARIN pressure");
                p[index]=value;
                if(solid(x,y,z)) {
                    if(!solid_prefix)
                        throw std::invalid_argument(
                            "Non-floor-connected solid in MARIN VOF column");
                    const double f=fraction(x,y,z);
                    if(!std::isfinite(f)||std::abs(f)>eps)
                        throw std::invalid_argument(
                            "Liquid overlaps MARIN solid support");
                    h+=dx;
                    continue;
                }
                solid_prefix=false;
                const double f=fraction(x,y,z);
                if(!std::isfinite(f)||f<0||f>1)
                    throw std::invalid_argument("Invalid live MARIN VOF fraction");
                if(f<=eps) {
                    seen_air=true;
                    continue;
                }
                if(seen_air||seen_partial)
                    throw std::invalid_argument(
                        "MARIN column is not a bottom-connected single interface");
                has_liquid=true;
                h+=dx*f;
                if(f<1-eps) {
                    seen_partial=true;
                    ++result.partial_surface_columns;
                }
            }
            // A dry solid box is NOT an artificial water column: roof load
            // requires actual liquid above the floor-connected solid prefix.
            if(!has_liquid)h=0.;
            else ++result.wet_columns;
            heights[y*nx+x]=h;
            if(h>result.max_waterline_m)result.max_waterline_m=h;
        }
        result.traction=P2MarinBoxWallTraction3D::audit_heightfield(
            nx,ny,nz,dx,box,p,heights);
        return result;
    }

    static P2MarinLiveColumnWallResult3D audit_live(
        const CoupledMacWater3D& w,const P2Box3D& box) {
        return audit_field(w.nx(),w.ny(),w.nz(),w.cell_size_m(),box,
            [&w](std::size_t x,std::size_t y,std::size_t z) {
                return w.pressure_pa(x,y,z);
            },
            [&w](std::size_t x,std::size_t y,std::size_t z) {
                return w.water_fraction(x,y,z);
            },
            [&w](std::size_t x,std::size_t y,std::size_t z) {
                return w.is_solid(x,y,z);
            });
    }
};
} // namespace upc
