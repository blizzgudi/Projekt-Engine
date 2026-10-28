#pragma once
// P2 PARTIAL: exact time-averaged open MAC face area for a linearly
// translating, axis-aligned 3D box. Geometry only: NOT a velocity,
// pressure, GCL flux, wall traction or two-way FSI solver.
#include "upc/p2_moving_box_open_volume3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {

struct P2MovingBoxMacApertures3D {
    // Fractions of full dx^2 face area, averaged over the whole step.
    std::vector<double> u_open, v_open, w_open;
    P2BoxOpenVolumes3D cell_geometry;
    double min_open_fraction=1.0;
    double max_blocked_fraction=0.0;
};

class P2MovingBoxFaceApertures3D {
    static double blocked_at(
        int normal, const std::array<std::size_t,3>& face,
        double dx, const P2Box3D& old_box,
        const std::array<double,3>& shift, double t) {
        const double plane=static_cast<double>(face[normal])*dx;
        const double low=old_box.min_m[normal]+t*shift[normal];
        const double high=old_box.max_m[normal]+t*shift[normal];
        // A face coincident with a stationary solid surface is closed.
        // For a crossing edge, isolated endpoint contact has zero time measure.
        if(low>plane || plane>high) return 0.0;
        double blocked=1.0;
        for(int a=0;a<3;++a) {
            if(a==normal) continue;
            const double cell_low=static_cast<double>(face[a])*dx;
            const double cell_high=cell_low+dx;
            const double box_low=old_box.min_m[a]+t*shift[a];
            const double box_high=old_box.max_m[a]+t*shift[a];
            const double overlap=std::max(0.0,
                std::min(cell_high,box_high)-std::max(cell_low,box_low));
            blocked*=overlap/dx;
        }
        return blocked;
    }
    static double average_open(
        int normal, const std::array<std::size_t,3>& face,
        double dx, const P2Box3D& old_box,
        const std::array<double,3>& shift) {
        // Piecewise quadratic blocked area. Split at every time a box
        // edge crosses the face plane or a transverse cell edge.
        // Two-point Gauss integrates each polynomial piece exactly;
        // its interior nodes avoid endpoint discontinuities.
        std::vector<double> knots{0.0,1.0};
        for(int a=0;a<3;++a) {
            if(shift[a]==0.0) continue;
            const double lo=static_cast<double>(face[a])*dx;
            const double hi=lo+dx;
            const std::array<double,2> edges{{
                old_box.min_m[a],old_box.max_m[a]}};
            for(double edge:edges) {
                const auto add=[&](double boundary) {
                    const double t=(boundary-edge)/shift[a];
                    if(std::isfinite(t)&&t>0.0&&t<1.0)
                        knots.push_back(t);
                };
                add(lo);
                if(a!=normal) add(hi);
            }
        }
        std::sort(knots.begin(),knots.end());
        double blocked_average=0.0;
        constexpr double inv_sqrt3=0.577350269189625764509148780501957;
        for(std::size_t k=1;k<knots.size();++k) {
            const double half=0.5*(knots[k]-knots[k-1]);
            if(half<=0.0)continue;
            const double mid=0.5*(knots[k]+knots[k-1]);
            blocked_average+=half*(
                blocked_at(normal,face,dx,old_box,shift,mid-half*inv_sqrt3)+
                blocked_at(normal,face,dx,old_box,shift,mid+half*inv_sqrt3));
        }
        if(!std::isfinite(blocked_average)||
           blocked_average < -1e-12 || blocked_average > 1.0+1e-12)
            throw std::logic_error("Invalid moving-box face aperture");
        return std::max(0.0,std::min(1.0,1.0-blocked_average));
    }
public:
    // Reuses exact cut-cell volume/rigid-translation/domain/CFL checks.
    // Faces may be partially occluded for part of a step. This is a
    // TIME AVERAGE of geometric aperture, not a swept fluid volume.
    // No input is changed, including on exceptions.
    static P2MovingBoxMacApertures3D sample(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,double dt_s,
        const P2Box3D& old_box,const P2Box3D& new_box) {
        P2MovingBoxMacApertures3D result;
        result.cell_geometry=P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx_m,dt_s,old_box,new_box);
        const auto& shift=result.cell_geometry.translation_m;
        result.u_open.reserve((nx+1)*ny*nz);
        result.v_open.reserve(nx*(ny+1)*nz);
        result.w_open.reserve(nx*ny*(nz+1));
        const auto append=[&](std::vector<double>& dest,int axis,
                              std::size_t x,std::size_t y,std::size_t z) {
            const double aperture=average_open(
                axis,{{x,y,z}},dx_m,old_box,shift);
            dest.push_back(aperture);
            result.min_open_fraction=
                std::min(result.min_open_fraction,aperture);
            result.max_blocked_fraction=
                std::max(result.max_blocked_fraction,1.0-aperture);
        };
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x)
            append(result.u_open,0,x,y,z);
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x)
            append(result.v_open,1,x,y,z);
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            append(result.w_open,2,x,y,z);
        return result;
    }
};
} // namespace upc
