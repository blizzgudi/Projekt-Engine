#pragma once
// P2 PARTIAL: wall-tangential bilinear reconstruction of the existing
// native 3D MAC cell pressure at SPHERIC/MARIN box probe coordinates.
// NOT a sub-cell pressure solver, not wall traction, not validated CFD.
// The normal coordinate remains the first available liquid-side cell:
// no normal derivative, pressure boundary condition, or wall impulse is
// inferred. The stencil rejects solid/dry cells by weight normalization.
#include "upc/p2_marin_pressure_sensors3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace upc {
struct P2MarinInterpolatedProbe3D {
    std::array<double,3> transducer_m{};
    std::array<double,3> normal_cell_centre_m{};
    double pressure_pa=0;
    double wet_stencil_weight=0;
    bool water_wet=false;
    std::size_t wet_stencil_cells=0;
};
struct P2MarinInterpolatedSample3D {
    std::array<P2MarinInterpolatedProbe3D,8> probes{};
    unsigned wet_count=0;
};
class P2MarinWallPressureInterpolation3D {
    struct AxisStencil {
        std::array<std::size_t,2> cell{};
        std::array<double,2> weight{};
    };
    static AxisStencil axis(double coordinate,double dx,std::size_t n) {
        if(n<2||!std::isfinite(coordinate)||coordinate<0||
           coordinate>=dx*static_cast<double>(n))
            throw std::out_of_range("MARIN wall-tangential coordinate outside grid");
        const double q=std::clamp(coordinate/dx-.5,0.0,static_cast<double>(n-1));
        const auto lo=static_cast<std::size_t>(std::floor(q));
        const auto hi=std::min(lo+1,n-1);
        const double t=q-static_cast<double>(lo);
        return {{lo,hi},{1.0-t,t}};
    }
public:
    // The three callbacks are cell pressure in Pa, wet water fraction,
    // and solid mask, indexed (x,y,z). This enables manufactured-field
    // tests without mutating or inventing MAC solver pressure values.
    template<class Pressure,class Wet,class Solid>
    static P2MarinInterpolatedSample3D sample_field(
        double dx,std::size_t nx,std::size_t ny,std::size_t nz,
        Pressure pressure,Wet water_fraction,Solid is_solid) {
        if(!std::isfinite(dx)||dx<=0||nx<2||ny<2||nz<2)
            throw std::invalid_argument("Invalid MARIN wall pressure grid");
        const auto coords=P2MarinPressureSensors3D::transducers_m();
        P2MarinInterpolatedSample3D result{};
        for(std::size_t i=0;i<coords.size();++i) {
            auto& probe=result.probes[i];
            probe.transducer_m=coords[i];
            const bool front=i<4;
            const auto normal_coord=coords[i][front?0:2]+.5*dx;
            const auto normal=static_cast<std::size_t>(std::floor(normal_coord/dx));
            if(normal>=(front?nx:nz))
                throw std::out_of_range("MARIN wall-normal pressure outside grid");
            probe.normal_cell_centre_m=coords[i];
            probe.normal_cell_centre_m[front?0:2]=(normal+.5)*dx;
            const AxisStencil first=front?axis(coords[i][1],dx,ny):
                                           axis(coords[i][0],dx,nx);
            const AxisStencil second=front?axis(coords[i][2],dx,nz):
                                            axis(coords[i][1],dx,ny);
            double weighted_pressure=0;
            for(std::size_t a=0;a<2;++a)
            for(std::size_t b=0;b<2;++b) {
                const double w=first.weight[a]*second.weight[b];
                if(w==0)continue;
                const auto x=front?normal:first.cell[a];
                const auto y=front?first.cell[a]:second.cell[b];
                const auto z=front?second.cell[b]:normal;
                if(is_solid(x,y,z))continue;
                const double f=water_fraction(x,y,z);
                if(!std::isfinite(f)||f<0||f>1)
                    throw std::invalid_argument("Invalid MARIN stencil water fraction");
                if(f<=1e-5)continue;
                const double p=pressure(x,y,z);
                if(!std::isfinite(p))
                    throw std::runtime_error("Nonfinite native MARIN pressure stencil");
                // Weight by liquid occupancy, so dry cells cannot silently
                // be interpreted as zero-pressure water. A partial stencil
                // is a diagnostic approximation, not a reconstructed interface.
                const double contribution=w*f;
                weighted_pressure+=contribution*p;
                probe.wet_stencil_weight+=contribution;
                ++probe.wet_stencil_cells;
            }
            if(probe.wet_stencil_weight>1e-5) {
                probe.water_wet=true;
                probe.pressure_pa=weighted_pressure/probe.wet_stencil_weight;
                if(!std::isfinite(probe.pressure_pa))
                    throw std::runtime_error("Nonfinite interpolated MARIN pressure");
                ++result.wet_count;
            }
        }
        return result;
    }
    static P2MarinInterpolatedSample3D sample(const CoupledMacWater3D& water) {
        return sample_field(
            water.cell_size_m(),water.nx(),water.ny(),water.nz(),
            [&water](std::size_t x,std::size_t y,std::size_t z){
                return water.pressure_pa(x,y,z);
            },
            [&water](std::size_t x,std::size_t y,std::size_t z){
                return water.water_fraction(x,y,z);
            },
            [&water](std::size_t x,std::size_t y,std::size_t z){
                return water.is_solid(x,y,z);
            });
    }
};
} // namespace upc
