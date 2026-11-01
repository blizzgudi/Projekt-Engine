#pragma once
// P2 PARTIAL: finite circular footprint averaging of the ACTUAL liquid-side
// 3D MAC pressure on the eight MARIN box gauges. The radius is an explicit
// caller-supplied parameter: the original Test 02 transducer diameter is NOT
// established here. This is a sampling operator, NOT a wall stress solver.
#include "upc/p2_marin_pressure_sensors3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace upc {
struct P2MarinFootprintProbe3D {
    double area_mean_gauge_pressure_pa=0;
    double wet_area_fraction=0;
    unsigned wet_quadrature_points=0;
    unsigned extrapolated_points=0;
    unsigned fallback_points=0;
};
struct P2MarinFootprintSample3D {
    std::array<P2MarinFootprintProbe3D,8> probes{};
    unsigned wet_count=0;
    double radius_m=0;
    unsigned radial_rings=0, angular_points=0;
};
class P2MarinFootprintPressure3D {
    struct Axis {
        std::array<std::size_t,2> index{};
        std::array<double,2> weight{};
    };
    static Axis tangent(double q_m,double dx,std::size_t n) {
        if(n<2||!std::isfinite(q_m)||q_m<0||q_m>=dx*double(n))
            throw std::out_of_range("MARIN footprint outside domain");
        const double q=std::clamp(q_m/dx-0.5,0.0,double(n-1));
        const auto lo=std::size_t(std::floor(q));
        const auto hi=std::min(lo+1,n-1);
        const double t=q-double(lo);
        return {{lo,hi},{1.0-t,t}};
    }
public:
    template<class Pressure,class Fraction,class Solid>
    static P2MarinFootprintSample3D sample_field(
        double dx,std::size_t nx,std::size_t ny,std::size_t nz,
        double radius_m,unsigned radial_rings,unsigned angular_points,
        Pressure pressure,Fraction fraction,Solid solid) {
        // All sensor disks must stay strictly on their assigned box face.
        // P1 and P5 are 0.021m from the closest edge. 0.020m leaves
        // a nonzero geometric margin without guessing a real sensor radius.
        if(!std::isfinite(dx)||dx<=0||nx<2||ny<2||nz<2||
           !std::isfinite(radius_m)||radius_m<=0||radius_m>0.020||
           radial_rings<1||radial_rings>64||
           angular_points<4||angular_points>256||angular_points%4!=0)
            throw std::invalid_argument("Invalid MARIN footprint configuration");
        constexpr double pi=3.141592653589793238462643383279502884;
        const auto coords=P2MarinPressureSensors3D::transducers_m();
        P2MarinFootprintSample3D out{};
        out.radius_m=radius_m;
        out.radial_rings=radial_rings;
        out.angular_points=angular_points;
        const double weight=1.0/double(radial_rings*angular_points);
        for(std::size_t i=0;i<8;++i) {
            const bool front=i<4;
            const double wall=coords[i][front?0:2];
            const auto normal=std::size_t(std::floor((wall+0.5*dx)/dx));
            const auto n=front?nx:nz;
            if(wall<0||wall>=double(n)*dx||normal>=n)
                throw std::out_of_range("MARIN footprint normal outside domain");
            const double normal_offset=wall-(double(normal)+0.5)*dx;
            auto& probe=out.probes[i];
            for(unsigned ring=0;ring<radial_rings;++ring) {
                // Equal-area annuli; each angular sample represents the
                // same area. This avoids centre-biased polar quadrature.
                const double r=radius_m*std::sqrt((double(ring)+0.5)/double(radial_rings));
                for(unsigned k=0;k<angular_points;++k) {
                    const double phi=2.0*pi*(double(k)+0.5)/double(angular_points);
                    const double u=r*std::cos(phi),v=r*std::sin(phi);
                    const double a_m=(front?coords[i][1]:coords[i][0])+u;
                    const double b_m=(front?coords[i][2]:coords[i][1])+v;
                    const Axis a=tangent(a_m,dx,front?ny:nx);
                    const Axis b=tangent(b_m,dx,front?nz:ny);
                    const auto plane=[&](std::size_t normal_index,double& wet_weight) {
                        double sum=0;
                        for(std::size_t j=0;j<2;++j)
                        for(std::size_t l=0;l<2;++l) {
                            const double w=a.weight[j]*b.weight[l];
                            if(w==0)continue;
                            const auto x=front?normal_index:a.index[j];
                            const auto y=front?a.index[j]:b.index[l];
                            const auto z=front?b.index[l]:normal_index;
                            if(solid(x,y,z))continue;
                            const double f=fraction(x,y,z);
                            if(!std::isfinite(f)||f<0||f>1)
                                throw std::invalid_argument("Invalid footprint liquid fraction");
                            if(f<=1e-5)continue;
                            const double p=pressure(x,y,z);
                            if(!std::isfinite(p))
                                throw std::runtime_error("Nonfinite footprint pressure");
                            wet_weight+=w*f;
                            sum+=w*f*p;
                        }
                        return wet_weight>1e-5?sum/wet_weight:0.0;
                    };
                    double first_w=0,second_w=0;
                    const double first=plane(normal,first_w);
                    if(first_w<=1e-5)continue; // dry disk patch = zero gauge pressure
                    double reconstructed=first;
                    if(normal+1<n) {
                        const double second=plane(normal+1,second_w);
                        if(second_w>1e-5) {
                            reconstructed+=normal_offset/dx*(second-first);
                            ++probe.extrapolated_points;
                        } else ++probe.fallback_points;
                    } else ++probe.fallback_points;
                    if(!std::isfinite(reconstructed))
                        throw std::runtime_error("Nonfinite footprint extrapolation");
                    // Fractional wet patches contribute proportionally to
                    // total disk area, not normalized to the remaining wet area.
                    probe.area_mean_gauge_pressure_pa+=weight*first_w*reconstructed;
                    probe.wet_area_fraction+=weight*first_w;
                    ++probe.wet_quadrature_points;
                }
            }
            if(probe.wet_area_fraction>1e-5)++out.wet_count;
            if(!std::isfinite(probe.area_mean_gauge_pressure_pa)||
               !std::isfinite(probe.wet_area_fraction)||
               probe.wet_area_fraction<0||probe.wet_area_fraction>1+1e-10)
                throw std::runtime_error("Invalid footprint area integral");
        }
        return out;
    }
    static P2MarinFootprintSample3D sample(
        const CoupledMacWater3D& w,double radius_m,
        unsigned radial_rings=8,unsigned angular_points=32) {
        return sample_field(
            w.cell_size_m(),w.nx(),w.ny(),w.nz(),
            radius_m,radial_rings,angular_points,
            [&w](std::size_t x,std::size_t y,std::size_t z){return w.pressure_pa(x,y,z);},
            [&w](std::size_t x,std::size_t y,std::size_t z){return w.water_fraction(x,y,z);},
            [&w](std::size_t x,std::size_t y,std::size_t z){return w.is_solid(x,y,z);});
    }
};
} // namespace upc
