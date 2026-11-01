#pragma once
// P2 PARTIAL: first-order wall-normal reconstruction of ACTUAL 3D MAC
// liquid-cell pressure, combined with wall-tangential bilinear interpolation.
// Two successive water-side planes are needed. A dry/solid second plane
// triggers an explicit nearest-plane fallback, not invented wall traction.
// Not a cut-cell solver, stress-consistent interface or measured CFD PASS.
#include "upc/p2_marin_pressure_sensors3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace upc {
struct P2MarinWallNormalProbe3D {
    double pressure_pa=0;
    double first_plane_pressure_pa=0;
    double second_plane_pressure_pa=0;
    double first_plane_wet_weight=0;
    double second_plane_wet_weight=0;
    double wall_to_first_centre_m=0;
    bool water_wet=false;
    bool extrapolated=false;
    bool fallback=false;
};
struct P2MarinWallNormalSample3D {
    std::array<P2MarinWallNormalProbe3D,8> probes{};
    unsigned wet_count=0,extrapolated_count=0,fallback_count=0;
};
class P2MarinWallNormalPressure3D {
    struct Axis {
        std::array<std::size_t,2> index{};
        std::array<double,2> weight{};
    };
    static Axis tangent(double coordinate,double dx,std::size_t count){
        if(count<2||!std::isfinite(coordinate)||coordinate<0||
           coordinate>=dx*static_cast<double>(count))
            throw std::out_of_range("MARIN tangential sensor outside domain");
        const double q=std::clamp(coordinate/dx-.5,0.0,double(count-1));
        const auto lo=std::size_t(std::floor(q));
        const auto hi=std::min(lo+1,count-1);
        const double t=q-double(lo);
        return {{lo,hi},{1-t,t}};
    }
public:
    template<class Pressure,class Wet,class Solid>
    static P2MarinWallNormalSample3D sample_field(
        double dx,std::size_t nx,std::size_t ny,std::size_t nz,
        Pressure pressure,Wet fraction,Solid solid){
        if(!std::isfinite(dx)||dx<=0||nx<2||ny<2||nz<2)
            throw std::invalid_argument("Invalid MARIN wall-normal grid");
        P2MarinWallNormalSample3D result{};
        const auto coords=P2MarinPressureSensors3D::transducers_m();
        for(std::size_t i=0;i<8;++i){
            auto& out=result.probes[i];
            const bool front=i<4;
            const double wall=coords[i][front?0:2];
            const std::size_t n=front?nx:nz;
            if(!std::isfinite(wall)||wall<0||wall>=n*dx)
                throw std::out_of_range("MARIN wall outside domain");
            // Keep the established first liquid-side plane. This is
            // grid-aligned geometry, not a cut-cell boundary treatment.
            const auto first=std::size_t(std::floor((wall+.5*dx)/dx));
            if(first>=n)
                throw std::out_of_range("MARIN first liquid plane missing");
            out.wall_to_first_centre_m=wall-(first+.5)*dx;
            const auto a=front?tangent(coords[i][1],dx,ny):
                               tangent(coords[i][0],dx,nx);
            const auto b=front?tangent(coords[i][2],dx,nz):
                               tangent(coords[i][1],dx,ny);
            const auto plane=[&](std::size_t normal,double& wet_weight){
                double total=0;
                for(std::size_t j=0;j<2;++j)
                for(std::size_t k=0;k<2;++k){
                    const double weight=a.weight[j]*b.weight[k];
                    if(weight==0)continue;
                    const auto x=front?normal:a.index[j];
                    const auto y=front?a.index[j]:b.index[k];
                    const auto z=front?b.index[k]:normal;
                    if(solid(x,y,z))continue;
                    const double f=fraction(x,y,z);
                    if(!std::isfinite(f)||f<0||f>1)
                        throw std::invalid_argument("Invalid MARIN liquid fraction");
                    if(f<=1e-5)continue;
                    const double p=pressure(x,y,z);
                    if(!std::isfinite(p))
                        throw std::runtime_error("Nonfinite MARIN liquid pressure");
                    const double w=weight*f;
                    wet_weight+=w;
                    total+=w*p;
                }
                return wet_weight>1e-5?total/wet_weight:0.0;
            };
            out.first_plane_pressure_pa=plane(first,out.first_plane_wet_weight);
            if(out.first_plane_wet_weight<=1e-5)continue;
            out.water_wet=true;
            ++result.wet_count;
            out.pressure_pa=out.first_plane_pressure_pa;
            if(first+1<n){
                out.second_plane_pressure_pa=plane(first+1,out.second_plane_wet_weight);
                if(out.second_plane_wet_weight>1e-5){
                    out.pressure_pa=out.first_plane_pressure_pa+
                        (out.wall_to_first_centre_m/dx)*
                        (out.second_plane_pressure_pa-out.first_plane_pressure_pa);
                    out.extrapolated=true;
                    ++result.extrapolated_count;
                }
            }
            if(!out.extrapolated){out.fallback=true;++result.fallback_count;}
            if(!std::isfinite(out.pressure_pa))
                throw std::runtime_error("Nonfinite extrapolated MARIN pressure");
        }
        return result;
    }
    static P2MarinWallNormalSample3D sample(const CoupledMacWater3D& w){
        return sample_field(w.cell_size_m(),w.nx(),w.ny(),w.nz(),
            [&w](std::size_t x,std::size_t y,std::size_t z){return w.pressure_pa(x,y,z);},
            [&w](std::size_t x,std::size_t y,std::size_t z){return w.water_fraction(x,y,z);},
            [&w](std::size_t x,std::size_t y,std::size_t z){return w.is_solid(x,y,z);});
    }
};
} // namespace upc
