#pragma once
// P2 PARTIAL: approximate fixed-surface experimental MARIN box pressure
// sampling from the ACTUAL 3D MAC pressure field. Not a calibrated physical
// transducer model and not a continuum wall-traction convergence claim.
// Sensor layout: SPHERIC/ERCOFTAC Test 02, Fig. 7, four on box front,
// four on roof. 0.021, 0.04, 0.04, 0.04 m offsets; nominal box 0.161m.
#include "upc/coupled_water3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace upc {
struct P2MarinPressureProbe3D {
    std::size_t grid_x=0,grid_y=0,grid_z=0;
    std::array<double,3> transducer_m{};
    std::array<double,3> sampled_cell_centre_m{};
    double pressure_pa=0;
    bool water_wet=false;
};
struct P2MarinPressureSample3D {
    std::array<P2MarinPressureProbe3D,8> probes{};
    unsigned wet_count=0;
};
class P2MarinPressureSensors3D {
public:
    // Coordinates anchored to the existing fixed-obstacle approximation:
    // x=[0.6635,0.8245], y=[0.295,0.698], z=[0,0.161] m.
    // The laboratory figure places front probes 0.021/0.061/0.101/0.141 m
    // from floor and top probes the same distances from the front edge.
    // P1-P4 are sampled just ahead of the x-max wall, P5-P8 just above
    // the roof. Real transducer spatial footprint is unresolved.
    static std::array<std::array<double,3>,8> transducers_m() {
        constexpr double front_x=.8245, box_top=.161, centre_y=.295+.176;
        constexpr std::array<double,4> offsets{{.021,.061,.101,.141}};
        std::array<std::array<double,3>,8> result{};
        for(std::size_t i=0;i<4;++i) {
            result[i]={front_x,centre_y,offsets[i]};
            result[i+4]={front_x-offsets[i],centre_y,box_top};
        }
        return result;
    }
    static P2MarinPressureSample3D sample(const CoupledMacWater3D& water) {
        const auto coords=transducers_m();
        const double dx=water.cell_size_m();
        if(!std::isfinite(dx)||dx<=0)
            throw std::invalid_argument("Invalid pressure gauge cell size");
        P2MarinPressureSample3D result;
        for(std::size_t i=0;i<coords.size();++i) {
            const auto& original=coords[i];
            auto sample=original;
            // Sample one half-cell into the water side, never *inside*
            // the binary box; this is a grid pressure proxy, not a
            // sub-voxel wall stress reconstruction.
            sample[i<4?0:2]+=0.5*dx;
            for(int a=0;a<3;++a)
                if(!std::isfinite(sample[a])||sample[a]<0)
                    throw std::invalid_argument("Invalid pressure gauge coordinates");
            const auto ix=static_cast<std::size_t>(sample[0]/dx);
            const auto iy=static_cast<std::size_t>(sample[1]/dx);
            const auto iz=static_cast<std::size_t>(sample[2]/dx);
            if(ix>=water.nx()||iy>=water.ny()||iz>=water.nz())
                throw std::out_of_range("Pressure sensor outside 3D domain");
            if(water.is_solid(ix,iy,iz))
                throw std::invalid_argument("Pressure sensor samples a solid cell");
            auto& p=result.probes[i];
            p.grid_x=ix;p.grid_y=iy;p.grid_z=iz;
            p.transducer_m=original;
            p.sampled_cell_centre_m={
                (ix+0.5)*dx,(iy+0.5)*dx,(iz+0.5)*dx};
            p.pressure_pa=water.pressure_pa(ix,iy,iz);
            p.water_wet=water.water_fraction(ix,iy,iz)>1e-5;
            if(!std::isfinite(p.pressure_pa))
                throw std::runtime_error("Nonfinite native MAC gauge pressure");
            if(p.water_wet)++result.wet_count;
        }
        return result;
    }
};
} // namespace upc
