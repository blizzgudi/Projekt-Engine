#pragma once
// P2 PARTIAL: reconstruct a locally planar VOF interface's intersection
// with MAC wet-to-dry cell-centre segments, including oblique normals.
// The plane is volume-matched against the existing exact clipped cube.
// This is a geometrical pressure boundary, NOT a two-phase traction,
// surface-tension, contact-angle, or experimental CFD validation.
#include "upc/p2_axis_vof_pcg_geometry3d.hpp"
#include "upc/p2_oblique_plic_geometry3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PlanarVofPcgGeometry3D {
    std::vector<P2AxisVofPcgFace3D> resolved;
    std::size_t unresolved_pure_faces=0;
    std::size_t oblique_faces=0;
    std::size_t two_partial_faces=0;
};
namespace p2_planar_vof_detail {
using V=std::array<double,3>;
inline double dot(const V& a,const V& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
struct Plane {
    V normal{};
    double cut=0;
};
inline Plane fit(double fraction,V normal) {
    if(!std::isfinite(fraction)||fraction<=0||fraction>=1)
        throw std::invalid_argument("Planar VOF needs partial cell");
    const double scale=std::max({std::abs(normal[0]),
                                 std::abs(normal[1]),std::abs(normal[2])});
    if(!std::isfinite(scale)||scale<=1e-13)
        throw std::invalid_argument("Planar VOF normal is degenerate");
    for(double& a:normal)a/=scale;
    const double length=std::sqrt(dot(normal,normal));
    for(double& a:normal)a/=length;
    double lo=0.0,hi=0.0;
    for(double a:normal) {lo+=std::min(0.0,a);hi+=std::max(0.0,a);}
    for(int i=0;i<65;++i) {
        const double mid=(lo+hi)*.5;
        if(p2_oblique_detail::clip_cube(normal,mid).volume<fraction)lo=mid;
        else hi=mid;
    }
    const double cut=(lo+hi)*.5;
    const double realized=p2_oblique_detail::clip_cube(normal,cut).volume;
    if(!std::isfinite(realized)||std::abs(realized-fraction)>1e-10)
        throw std::runtime_error("Planar VOF inversion inconsistent");
    return {normal,cut};
}
inline double fraction_from_wet(
    const Plane& plane,int axis,int direction,bool partial_wet) {
    // Local coordinates in cell chosen for the volume-matched plane.
    // If wet cell is partial, its centre is (.5,.5,.5). Otherwise the
    // dry partial centre is the origin and wet centre is one cell away.
    V local{.5,.5,.5};
    if(!partial_wet)local[axis]-=static_cast<double>(direction);
    const double denom=plane.normal[axis]*static_cast<double>(direction);
    if(!std::isfinite(denom)||denom<=1e-12)
        throw std::invalid_argument(
            "Oblique VOF interface normal opposes wet-to-dry face");
    const double theta=(plane.cut-dot(plane.normal,local))/denom;
    if(!std::isfinite(theta)||theta<=1e-10||theta>1.0+1e-10)
        throw std::invalid_argument("Planar VOF cut misses wet/dry centre segment");
    return std::min(1.0,theta);
}
}
inline P2PlanarVofPcgGeometry3D p2_planar_vof_pcg_geometry3d(
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    const std::vector<std::array<double,3>>& normals,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,
    double wet_threshold=0.5) {
    if(!nx||!ny||!nz||
       nx>std::numeric_limits<std::size_t>::max()/ny||
       nx*ny>std::numeric_limits<std::size_t>::max()/nz||
       !std::isfinite(dx)||dx<=0||wet_threshold!=0.5)
        throw std::invalid_argument("Invalid planar VOF PCG grid or threshold");
    const auto n=nx*ny*nz;
    if(fill.size()!=n||solid.size()!=n||normals.size()!=n)
        throw std::invalid_argument("Planar VOF PCG array shapes");
    for(std::size_t i=0;i<n;++i){
        if(!std::isfinite(fill[i])||fill[i]<0||fill[i]>1||
           solid[i]>1||(solid[i]&&fill[i]>0))
            throw std::invalid_argument("Invalid planar VOF cell or mask");
        for(double v:normals[i])
            if(!std::isfinite(v))
                throw std::invalid_argument("Nonfinite planar VOF normal");
    }
    const std::array<std::size_t,3> dims{{nx,ny,nz}};
    const std::array<std::size_t,3> stride{{1,nx,nx*ny}};
    P2PlanarVofPcgGeometry3D result;
    for(int axis=0;axis<3;++axis)
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        std::array<std::size_t,3> pos{{x,y,z}};
        if(pos[axis]+1>=dims[axis])continue;
        const auto i=(z*ny+y)*nx+x,j=i+stride[axis];
        if(solid[i]||solid[j])continue;
        const bool left_wet=fill[i]>wet_threshold;
        const bool right_wet=fill[j]>wet_threshold;
        if(left_wet==right_wet)continue;
        const auto wet=left_wet?i:j,dry=left_wet?j:i;
        const bool partial_wet=fill[wet]<1.0;
        const bool partial_dry=fill[dry]>0.0;
        if(!partial_wet&&!partial_dry){
            ++result.unresolved_pure_faces;
            continue; // preserve the existing half-cell ghost condition
        }
        const int direction=left_wet?1:-1;
        double theta=0;
        if(partial_wet) {
            const auto plane=p2_planar_vof_detail::fit(
                fill[wet],normals[wet]);
            theta=p2_planar_vof_detail::fraction_from_wet(
                plane,axis,direction,true);
        }
        if(partial_dry) {
            const auto plane=p2_planar_vof_detail::fit(
                fill[dry],normals[dry]);
            if(partial_wet) {
                const auto wet_plane=p2_planar_vof_detail::fit(
                    fill[wet],normals[wet]);
                const double alignment=p2_planar_vof_detail::dot(
                    plane.normal,wet_plane.normal);
                if(alignment<1.0-1e-7)
                    throw std::invalid_argument(
                        "Two partial cells disagree on planar normal");
            }
            const double check=p2_planar_vof_detail::fraction_from_wet(
                plane,axis,direction,false);
            if(partial_wet&&std::abs(check-theta)>1e-8)
                throw std::invalid_argument(
                    "Inconsistent two-cell planar VOF intersection");
            theta=check;
        }
        // The derivative component on the crossed axis must point to air,
        // but a strongly oblique cross-plane is supported and audited.
        const auto& normal=normals[partial_wet?wet:dry];
        const double norm=std::sqrt(p2_planar_vof_detail::dot(normal,normal));
        if(!std::isfinite(norm)||norm<=0)
            throw std::invalid_argument("Degenerate planar VOF normal");
        const double alignment=std::abs(normal[axis])/norm;
        if(alignment<1.0-1e-9)++result.oblique_faces;
        if(partial_wet&&partial_dry)++result.two_partial_faces;
        ++pos[axis];
        result.resolved.push_back({axis,pos[0],pos[1],pos[2],theta});
    }
    return result;
}
inline void p2_apply_planar_vof_pcg_geometry3d(
    MacProjection3D& mac,const P2PlanarVofPcgGeometry3D& geometry) {
    // Transactional unlike the bare axis helper: bad face index or
    // invalid scalar cannot partially replace a previously valid frame.
    MacProjection3D trial=mac;
    P2AxisVofPcgGeometry3D copy;
    copy.resolved=geometry.resolved;
    copy.unresolved_pure_faces=geometry.unresolved_pure_faces;
    p2_apply_axis_vof_pcg_geometry3d(trial,copy);
    mac=std::move(trial);
}
} // namespace upc
