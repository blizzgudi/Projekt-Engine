#pragma once
// P2 PARTIAL: local, volume-matched PLIC plane-normal fit from VOF fractions.
// Strictly rejects patches inconsistent with one plane. This is a geometric
// preflight, not a physical stress boundary, curvature model or CFD validation.
#include "upc/p2_planar_vof_pcg_geometry3d.hpp"
#include "upc/p2_vof_normal_reconstruction3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2LocalPlanarNormals3D {
    std::vector<std::array<double,3>> normals;
    std::size_t fitted_cells=0;
    std::size_t iterations=0;
    double worst_patch_residual=0;
    double worst_rms_residual=0;
};
namespace p2_local_planar_detail {
using V=std::array<double,3>;
inline double dot(V a,V b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline V normalize(V a) {
    const double scale=std::max({std::abs(a[0]),std::abs(a[1]),std::abs(a[2])});
    if(!std::isfinite(scale)||scale<1e-14)
        throw std::invalid_argument("Degenerate fitted PLIC normal");
    for(double& v:a)v/=scale;
    const double len=std::sqrt(dot(a,a));
    for(double& v:a)v/=len;
    return a;
}
inline V tangent(V n) {
    const V ref=std::abs(n[0])<.8?V{1,0,0}:V{0,1,0};
    const double s=dot(ref,n);
    return normalize({ref[0]-s*n[0],ref[1]-s*n[1],ref[2]-s*n[2]});
}
inline V cross(V a,V b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
struct Sample {V delta; double fill;};
struct Eval {std::vector<double> residual;double cost=0,max_abs=0;};
inline Eval evaluate(double center_fill,V normal,const std::vector<Sample>& patch) {
    const auto plane=p2_planar_vof_detail::fit(center_fill,normal);
    Eval out;
    out.residual.reserve(patch.size());
    for(const auto& s:patch) {
        const double cut=plane.cut-dot(plane.normal,s.delta);
        const double predicted=std::clamp(
            p2_oblique_detail::clip_cube(plane.normal,cut).volume,0.,1.);
        const double r=predicted-s.fill;
        if(!std::isfinite(r))throw std::runtime_error("Nonfinite fitted VOF");
        out.residual.push_back(r);
        out.cost+=r*r;
        out.max_abs=std::max(out.max_abs,std::abs(r));
    }
    return out;
}
inline V perturb(V n,V t1,V t2,double a,double b) {
    return normalize({n[0]+a*t1[0]+b*t2[0],
                      n[1]+a*t1[1]+b*t2[1],
                      n[2]+a*t1[2]+b*t2[2]});
}
}
// For a genuinely planar local 3x3x3 VOF patch, solve the two orientation
// degrees of freedom using a volume-matched central cell and neighboring
// exact clipped-cube fractions. No physical normal is invented if the patch
// is curved, noisy, solid-obstructed, or locally rank-deficient.
// max_patch_error is an absolute liquid volume fraction tolerance.
inline P2LocalPlanarNormals3D p2_fit_local_planar_vof_normals3d(
    const std::vector<double>& fill,const std::vector<std::uint8_t>& solid,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,
    double max_patch_error=1e-7) {
    if(!std::isfinite(max_patch_error)||max_patch_error<=0||
       max_patch_error>=.5)
        throw std::invalid_argument("Invalid planar normal fit tolerance");
    const auto initial=p2_reconstruct_vof_normals3d(fill,solid,nx,ny,nz,dx);
    P2LocalPlanarNormals3D out;
    out.normals=initial.normals;
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto i=(z*ny+y)*nx+x;
        if(fill[i]<=0||fill[i]>=1)continue;
        std::vector<p2_local_planar_detail::Sample> patch;
        for(int oz=-1;oz<=1;++oz)
        for(int oy=-1;oy<=1;++oy)
        for(int ox=-1;ox<=1;++ox) {
            const auto xx=static_cast<long long>(x)+ox;
            const auto yy=static_cast<long long>(y)+oy;
            const auto zz=static_cast<long long>(z)+oz;
            if(xx<0||yy<0||zz<0||
               xx>=static_cast<long long>(nx)||
               yy>=static_cast<long long>(ny)||
               zz>=static_cast<long long>(nz))continue;
            const auto j=(static_cast<std::size_t>(zz)*ny+
                          static_cast<std::size_t>(yy))*nx+
                          static_cast<std::size_t>(xx);
            if(solid[j])continue;
            patch.push_back({{double(ox),double(oy),double(oz)},fill[j]});
        }
        if(patch.size()<6)
            throw std::invalid_argument("Insufficient planar VOF neighbors");
        using namespace p2_local_planar_detail;
        V n=normalize(initial.normals[i]);
        auto current=evaluate(fill[i],n,patch);
        for(int iter=0;iter<25&&current.max_abs>max_patch_error*.2;++iter) {
            const V t1=tangent(n),t2=cross(n,t1);
            const double h=1e-4;
            const auto e1=evaluate(fill[i],perturb(n,t1,t2,h,0),patch);
            const auto e2=evaluate(fill[i],perturb(n,t1,t2,0,h),patch);
            double aa=0,bb=0,ab=0,ar=0,br=0;
            for(std::size_t k=0;k<patch.size();++k) {
                const double a=(e1.residual[k]-current.residual[k])/h;
                const double b=(e2.residual[k]-current.residual[k])/h;
                aa+=a*a;bb+=b*b;ab+=a*b;
                ar+=a*current.residual[k];br+=b*current.residual[k];
            }
            const double det=aa*bb-ab*ab;
            if(det<=1e-18||!std::isfinite(det))break;
            const double a=(-bb*ar+ab*br)/det;
            const double b=(ab*ar-aa*br)/det;
            if(!std::isfinite(a)||!std::isfinite(b))break;
            bool improved=false;
            for(double step=1;step>=1./128;step*=.5) {
                const V candidate=perturb(n,t1,t2,a*step,b*step);
                const auto result=evaluate(fill[i],candidate,patch);
                if(result.cost<current.cost) {
                    n=candidate;current=result;improved=true;break;
                }
            }
            ++out.iterations;
            if(!improved)break;
        }
        out.worst_patch_residual=std::max(out.worst_patch_residual,current.max_abs);
        out.worst_rms_residual=std::max(out.worst_rms_residual,
            std::sqrt(current.cost/static_cast<double>(patch.size())));
        if(current.max_abs>max_patch_error)
            throw std::invalid_argument("VOF patch not consistent with a planar interface");
        out.normals[i]=n;
        ++out.fitted_cells;
    }
    return out;
}
} // namespace upc
