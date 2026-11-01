#pragma once
// P2 PARTIAL: exact axis-aligned VOF plane -> MAC liquid/air Dirichlet
// distance bridge. Not a general PLIC, air stress or live CFD validation.
#include "upc/mac3d.hpp"
#include "upc/p2_vof_normal_reconstruction3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2AxisVofPcgFace3D {
    int axis=0;
    std::size_t x=0,y=0,z=0;
    double theta=0.5;
};
struct P2AxisVofPcgGeometry3D {
    std::vector<P2AxisVofPcgFace3D> resolved;
    std::size_t unresolved_pure_faces=0;
};

// The caller must set the MAC wet mask using the SAME wet_threshold.
// For a known axis-aligned planar cut, a partially wet cell with f>0.5
// gives theta=f-0.5; a neighboring partially dry cell with f<0.5
// gives theta=0.5+f. Only ONE side may be partial. Both-partial or
// oblique cuts fail closed rather than silently treating VOF as distance.
// All-pure wet/dry faces are geometrically ambiguous and stay at 0.5.
inline P2AxisVofPcgGeometry3D p2_axis_vof_pcg_geometry3d(
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,
    double wet_threshold=0.5) {
    if(!std::isfinite(wet_threshold)||wet_threshold!=0.5)
        throw std::invalid_argument("Axis VOF pressure geometry requires 0.5 wet threshold");
    const auto normals=p2_reconstruct_vof_normals3d(
        fill,solid,nx,ny,nz,dx);
    P2AxisVofPcgGeometry3D out;
    const std::array<std::size_t,3> dims{nx,ny,nz};
    const std::array<std::size_t,3> strides{1,nx,nx*ny};
    for(int axis=0;axis<3;++axis)
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        std::array<std::size_t,3> a{x,y,z};
        if(a[axis]+1>=dims[axis])continue;
        const auto i=(z*ny+y)*nx+x;
        const auto j=i+strides[axis];
        if(solid[i]||solid[j])continue;
        const bool wi=fill[i]>wet_threshold;
        const bool wj=fill[j]>wet_threshold;
        if(wi==wj)continue;
        const auto wet=wi?i:j, dry=wi?j:i;
        const double wf=fill[wet],df=fill[dry];
        const bool wp=wf<1.0,dp=df>0.0;
        if(wp&&dp)
            throw std::invalid_argument("Ambiguous two-partial-cell VOF interface");
        if(!wp&&!dp){++out.unresolved_pure_faces;continue;}
        const auto partial=wp?wet:dry;
        const double expected_sign=wi?1.0:-1.0;
        for(int k=0;k<3;++k) {
            const double expected=k==axis?expected_sign:0.0;
            if(std::abs(normals.normals[partial][k]-expected)>1e-10)
                throw std::invalid_argument("Non-axis-aligned VOF interface normal");
        }
        const double theta=wp?wf-0.5:0.5+df;
        if(!(theta>0.0&&theta<=1.0))
            throw std::invalid_argument("Degenerate VOF interface distance");
        std::array<std::size_t,3> face=a;
        ++face[axis];
        out.resolved.push_back({axis,face[0],face[1],face[2],theta});
    }
    return out;
}
inline void p2_apply_axis_vof_pcg_geometry3d(
    MacProjection3D& mac,const P2AxisVofPcgGeometry3D& geometry) {
    // Validate before clearing old geometry; a new frame must never
    // inherit stale fractional faces from an earlier VOF snapshot.
    for(const auto& f:geometry.resolved)
        if(f.axis<0||f.axis>2||!std::isfinite(f.theta)||
           f.theta<=0.0||f.theta>1.0)
            throw std::invalid_argument("Invalid axis VOF pressure face");
    mac.clear_interface_distances();
    for(const auto& f:geometry.resolved) {
        if(f.axis==0)mac.set_interface_distance_x(f.x,f.y,f.z,f.theta);
        else if(f.axis==1)mac.set_interface_distance_y(f.x,f.y,f.z,f.theta);
        else if(f.axis==2)mac.set_interface_distance_z(f.x,f.y,f.z,f.theta);
        else throw std::invalid_argument("Invalid axis VOF geometry");
    }
}
} // namespace upc
