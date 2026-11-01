#pragma once
// P2 PARTIAL: sub-cell bilinear x/y sampling of the existing 3D MAC/VOF
// vertical liquid-volume column at MARIN H1-H4 positions. This is a
// diagnostic probe reconstruction, NOT a resolved free-surface height
// in overturning flow, a physical wave-probe footprint, or CFD validation.
#include "upc/coupled_water3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace upc {
struct P2MarinHeightProbe3D {
    double height_m=0.0;
    double interpolation_weight=0.0;
    std::size_t columns=0;
};
class P2MarinHeightInterpolation3D {
    struct Axis {
        std::array<std::size_t,2> index{};
        std::array<double,2> weight{};
    };
    static Axis axis(double pos,double dx,std::size_t n) {
        if(!std::isfinite(pos)||pos<0||pos>=dx*static_cast<double>(n))
            throw std::out_of_range("MARIN height gauge outside 3D domain");
        const double q=std::clamp(pos/dx-0.5,0.0,static_cast<double>(n-1));
        const auto lo=static_cast<std::size_t>(std::floor(q));
        const auto hi=std::min(lo+1,n-1);
        const double t=q-static_cast<double>(lo);
        return {{lo,hi},{1.0-t,t}};
    }
public:
    template<class Fraction>
    static P2MarinHeightProbe3D sample_field(
        double dx,std::size_t nx,std::size_t ny,std::size_t nz,
        double x_m,double y_m,Fraction fraction) {
        if(!std::isfinite(dx)||dx<=0||nx<2||ny<2||nz<1)
            throw std::invalid_argument("Invalid MARIN height grid");
        const Axis ax=axis(x_m,dx,nx),ay=axis(y_m,dx,ny);
        P2MarinHeightProbe3D result{};
        for(std::size_t a=0;a<2;++a)
        for(std::size_t b=0;b<2;++b) {
            const double weight=ax.weight[a]*ay.weight[b];
            if(weight==0.0)continue;
            double column=0.0;
            for(std::size_t z=0;z<nz;++z) {
                const double f=fraction(ax.index[a],ay.index[b],z);
                if(!std::isfinite(f)||f<0.0||f>1.0)
                    throw std::invalid_argument("Invalid MARIN VOF column fraction");
                column+=f*dx;
            }
            result.height_m+=weight*column;
            result.interpolation_weight+=weight;
            ++result.columns;
        }
        if(std::abs(result.interpolation_weight-1.0)>1e-12||
           !std::isfinite(result.height_m))
            throw std::runtime_error("Invalid MARIN height interpolation");
        return result;
    }
    static P2MarinHeightProbe3D sample(
        const CoupledMacWater3D& water,double x_m,double y_m) {
        return sample_field(water.cell_size_m(),water.nx(),water.ny(),water.nz(),
            x_m,y_m,[&water](std::size_t x,std::size_t y,std::size_t z) {
                return water.water_fraction(x,y,z);
            });
    }
};
} // namespace upc
