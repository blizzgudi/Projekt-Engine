#pragma once
// P2 PARTIAL: exact axis-aligned 3D box/voxel overlap at two times.
// Produces geometric open fractions, NOT a fluid flux, pressure or FSI solver.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {

struct P2Box3D {
    std::array<double,3> min_m, max_m;
};
struct P2BoxOpenVolumes3D {
    std::vector<double> alpha_old, alpha_new;
    double old_open_m3=0.0, new_open_m3=0.0;
    double solid_volume_m3=0.0, solid_volume_residual_m3=0.0;
    double changed_open_cell_volumes_l1=0.0;
    std::array<double,3> translation_m{};
};

class P2MovingBoxOpenVolumes3D {
public:
    // Origin is (0,0,0); cells are axis-aligned cubes of edge dx_m.
    // The same rigid box must remain fully within the grid at both times.
    // Translation is limited to one cell per axis per step (geometry CFL).
    // No MAC face velocities are produced: a pressure/continuity solve is
    // required to determine the actual fluid displacement around the box.
    static P2BoxOpenVolumes3D sample(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,double dt_s,
        const P2Box3D& old_box,const P2Box3D& new_box) {
        const auto lim=std::numeric_limits<std::size_t>::max();
        if(!nx||!ny||!nz||nx>lim/ny||nx*ny>lim/nz||
           nx*ny*nz>2000000||
           !std::isfinite(dx_m)||dx_m<=0.0||
           !std::isfinite(dt_s)||dt_s<=0.0)
            throw std::invalid_argument("Invalid box geometry grid/time");
        const double vol=dx_m*dx_m*dx_m;
        if(!std::isfinite(vol)||vol<=0.0)
            throw std::invalid_argument("Invalid cell volume");
        P2BoxOpenVolumes3D out;
        double physical_solid=1.0;
        for(int a=0;a<3;++a) {
            const double bound=static_cast<double>(
                a==0?nx:(a==1?ny:nz))*dx_m;
            const double lo=old_box.min_m[a], hi=old_box.max_m[a];
            const double nlo=new_box.min_m[a], nhi=new_box.max_m[a];
            if(!std::isfinite(bound)||!std::isfinite(lo)||
               !std::isfinite(hi)||!std::isfinite(nlo)||
               !std::isfinite(nhi)||lo<0.0||nlo<0.0||
               hi>bound||nhi>bound||hi<=lo||nhi<=nlo)
                throw std::invalid_argument("Box outside domain or degenerate");
            const double old_width=hi-lo, new_width=nhi-nlo;
            const double tolerance=1e-11*std::max(1.0,old_width);
            if(std::abs(new_width-old_width)>tolerance ||
               std::abs((nhi-hi)-(nlo-lo))>tolerance)
                throw std::invalid_argument("Not a rigid translation");
            out.translation_m[a]=nlo-lo;
            if(std::abs(out.translation_m[a])>dx_m*(1.0+1e-12))
                throw std::invalid_argument("Box translation exceeds cell CFL");
            physical_solid*=old_width;
        }
        if(!std::isfinite(physical_solid)||physical_solid<=0.0)
            throw std::invalid_argument("Invalid box solid volume");
        const std::size_t n=nx*ny*nz;
        out.alpha_old.resize(n);
        out.alpha_new.resize(n);
        // Evaluate actual intersection volume with each voxel (not a
        // center-sample or binary occupancy approximation).
        const auto overlap=[&](const P2Box3D& b,
                               std::size_t x,std::size_t y,std::size_t z) {
            const std::array<std::size_t,3> ijk{{x,y,z}};
            double fraction=1.0;
            for(int a=0;a<3;++a) {
                const double left=static_cast<double>(ijk[a])*dx_m;
                const double right=left+dx_m;
                const double length=std::max(0.0,
                    std::min(right,b.max_m[a])-std::max(left,b.min_m[a]));
                fraction*=length/dx_m;
            }
            return fraction;
        };
        double old_solid_cells=0.0,new_solid_cells=0.0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const std::size_t i=(z*ny+y)*nx+x;
            const double so=overlap(old_box,x,y,z);
            const double sn=overlap(new_box,x,y,z);
            if(!std::isfinite(so)||!std::isfinite(sn)||
               so<0.0||so>1.0+1e-12||sn<0.0||sn>1.0+1e-12)
                throw std::logic_error("Invalid box voxel intersection");
            out.alpha_old[i]=1.0-std::min(1.0,so);
            out.alpha_new[i]=1.0-std::min(1.0,sn);
            old_solid_cells+=so;
            new_solid_cells+=sn;
            out.changed_open_cell_volumes_l1+=
                std::abs(out.alpha_new[i]-out.alpha_old[i]);
        }
        out.solid_volume_m3=physical_solid;
        out.old_open_m3=(static_cast<double>(n)-old_solid_cells)*vol;
        out.new_open_m3=(static_cast<double>(n)-new_solid_cells)*vol;
        out.solid_volume_residual_m3=(new_solid_cells-old_solid_cells)*vol;
        const double tol=1e-10*std::max(vol,physical_solid);
        if(std::abs(old_solid_cells*vol-physical_solid)>tol ||
           std::abs(new_solid_cells*vol-physical_solid)>tol ||
           std::abs(out.solid_volume_residual_m3)>tol)
            throw std::logic_error("Box voxel volume conservation failed");
        return out;
    }
};
} // namespace upc
