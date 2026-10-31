#pragma once
// P2 partial: exact uniform-density moments of a cube clipped by one
// prescribed oblique plane per partially wet cell. Read-only geometry:
// NOT normal reconstruction, VOF advection, surface tension or FSI.
#include "upc/p2_axis_plic_geometry3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
namespace p2_oblique_detail {
using V = std::array<double,3>;
using M = std::array<V,3>;
inline V add(V a,V b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
inline V sub(V a,V b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline V mul(V a,double s){return {a[0]*s,a[1]*s,a[2]*s};}
inline double dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline V cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
struct Moments {double volume=0;V first{};M second{};};
inline Moments clip_cube(V normal,double cut) {
    // Outward oriented faces of [0,1]^3.
    const std::array<V,8> v{{
        V{0,0,0},V{1,0,0},V{1,1,0},V{0,1,0},
        V{0,0,1},V{1,0,1},V{1,1,1},V{0,1,1}
    }};
    const int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},
                           {3,7,6,2},{0,4,7,3},{1,2,6,5}};
    const int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},
                            {6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    std::vector<std::vector<V>> polygons;
    for(const auto& face:faces) {
        std::vector<V> poly;
        for(int k=0;k<4;++k) {
            V a=v[face[k]],b=v[face[(k+1)%4]];
            double da=dot(normal,a)-cut,db=dot(normal,b)-cut;
            if(da<=0)poly.push_back(a);
            if((da<0&&db>0)||(da>0&&db<0)) {
                double t=da/(da-db);
                poly.push_back(add(a,mul(sub(b,a),t)));
            } else if(da>0&&db==0)poly.push_back(b);
        }
        if(poly.size()>=3)polygons.push_back(std::move(poly));
    }
    // New planar cap has outward normal +normal.
    std::vector<V> cap;
    auto insert_unique=[&](V p) {
        for(V q:cap)if(dot(sub(p,q),sub(p,q))<1e-24)return;
        cap.push_back(p);
    };
    for(const auto& e:edges) {
        V a=v[e[0]],b=v[e[1]];
        double da=dot(normal,a)-cut,db=dot(normal,b)-cut;
        if(std::abs(da)<1e-14)insert_unique(a);
        if(std::abs(db)<1e-14)insert_unique(b);
        if((da<0&&db>0)||(da>0&&db<0))
            insert_unique(add(a,mul(sub(b,a),da/(da-db))));
    }
    if(cap.size()>=3) {
        V center{};
        for(V p:cap)center=add(center,p);
        center=mul(center,1.0/static_cast<double>(cap.size()));
        V basis=std::abs(normal[2])<0.9?V{0,0,1}:V{0,1,0};
        V u=cross(basis,normal);
        u=mul(u,1.0/std::sqrt(dot(u,u)));
        V w=cross(normal,u);
        std::sort(cap.begin(),cap.end(),[&](V a,V b){
            V aa=sub(a,center),bb=sub(b,center);
            return std::atan2(dot(aa,w),dot(aa,u))<
                   std::atan2(dot(bb,w),dot(bb,u));
        });
        polygons.push_back(std::move(cap));
    }
    Moments out;
    for(const auto& poly:polygons)for(std::size_t j=1;j+1<poly.size();++j) {
        V a=poly[0],b=poly[j],c=poly[j+1];
        double vol=dot(a,cross(b,c))/6.0;
        out.volume+=vol;
        V sum=add(add(a,b),c);
        for(int i=0;i<3;++i) {
            out.first[i]+=vol*sum[i]/4.0;
            for(int k=0;k<3;++k)
                out.second[i][k]+=vol*(sum[i]*sum[k]+
                    a[i]*a[k]+b[i]*b[k]+c[i]*c[k])/20.0;
        }
    }
    return out;
}
} // namespace p2_oblique_detail

// Plane normal points toward the empty half-space. Its magnitude does not
// matter. Offset is found by bisection to match the supplied VOF fraction;
// there is no assumption that the interface normal was inferred from VOF.
// Empty/full cells may have a zero normal. Output shares axis-PLIC struct.
inline P2AxisPlicGeometry3D p2_oblique_plic_geometry3d(
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    const std::vector<std::array<double,3>>& normals,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx,double rho,const std::array<double,3>& origin_m={0,0,0}) {
    using namespace p2_oblique_detail;
    if(nx==0||ny==0||nz==0||
       nx>std::numeric_limits<std::size_t>::max()/ny||
       nx*ny>std::numeric_limits<std::size_t>::max()/nz||
       !std::isfinite(dx)||dx<=0||!std::isfinite(rho)||rho<=0)
        throw std::invalid_argument("Invalid oblique-PLIC grid/material");
    const std::size_t n=nx*ny*nz;
    if(fill.size()!=n||solid.size()!=n||normals.size()!=n)
        throw std::invalid_argument("Oblique-PLIC array sizes");
    for(double v:origin_m)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite oblique-PLIC origin");
    const double cell_mass=rho*dx*dx*dx;
    if(!std::isfinite(cell_mass)||cell_mass<=0)
        throw std::overflow_error("Oblique-PLIC cell mass overflow");
    P2AxisPlicGeometry3D out;
    V first{};
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto i=(z*ny+y)*nx+x;
        const double f=fill[i];
        V normal=normals[i];
        if(!std::isfinite(f)||f<0||f>1||solid[i]>1||
           (solid[i]&&f>0))
            throw std::invalid_argument("Invalid oblique-PLIC cell");
        for(double t:normal)if(!std::isfinite(t))
            throw std::invalid_argument("Nonfinite oblique-PLIC normal");
        if(f==0)continue;
        V mean{.5,.5,.5};
        M cov{};
        for(int k=0;k<3;++k)cov[k][k]=1.0/12.0;
        if(f<1) {
            const double norm=std::sqrt(dot(normal,normal));
            if(!std::isfinite(norm)||norm<=0)
                throw std::invalid_argument("Zero/nonfinite partial-cell normal");
            normal=mul(normal,1.0/norm);
            double lo=0,hi=0;
            for(double a:normal) {lo+=std::min(0.0,a);hi+=std::max(0.0,a);}
            for(int k=0;k<64;++k) {
                const double mid=(lo+hi)*.5;
                if(clip_cube(normal,mid).volume<f)lo=mid;
                else hi=mid;
            }
            const auto moments=clip_cube(normal,(lo+hi)*.5);
            if(!(moments.volume>0)||!std::isfinite(moments.volume)||
               std::abs(moments.volume-f)>1e-10)
                throw std::runtime_error("Oblique-PLIC volume solve failed");
            for(int a=0;a<3;++a)mean[a]=moments.first[a]/moments.volume;
            for(int a=0;a<3;++a)for(int b=0;b<3;++b)
                cov[a][b]=moments.second[a][b]/moments.volume-mean[a]*mean[b];
            ++out.partial_cells;
        }
        const double mass=cell_mass*f;
        V arm{(static_cast<double>(x)+mean[0])*dx-origin_m[0],
              (static_cast<double>(y)+mean[1])*dx-origin_m[1],
              (static_cast<double>(z)+mean[2])*dx-origin_m[2]};
        const double arm2=dot(arm,arm);
        const double tr=cov[0][0]+cov[1][1]+cov[2][2];
        out.mass_kg+=mass;
        for(int a=0;a<3;++a) {
            first[a]+=mass*arm[a];
            for(int b=0;b<3;++b)
                out.inertia_about_origin_kg_m2[a][b]+=
                    mass*dx*dx*((a==b?tr:0)-cov[a][b])+
                    mass*((a==b?arm2:0)-arm[a]*arm[b]);
        }
        ++out.occupied_cells;
    }
    if(out.mass_kg>0) {
        for(int a=0;a<3;++a)
            out.centre_of_mass_m[a]=origin_m[a]+first[a]/out.mass_kg;
    } else out.centre_of_mass_m=origin_m;
    V r=sub(out.centre_of_mass_m,origin_m);
    double r2=dot(r,r);
    for(int a=0;a<3;++a)for(int b=0;b<3;++b) {
        out.inertia_about_com_kg_m2[a][b]=
            out.inertia_about_origin_kg_m2[a][b]-
            out.mass_kg*((a==b?r2:0)-r[a]*r[b]);
        if(!std::isfinite(out.inertia_about_origin_kg_m2[a][b])||
           !std::isfinite(out.inertia_about_com_kg_m2[a][b]))
            throw std::overflow_error("Oblique-PLIC inertia overflow");
    }
    if(!std::isfinite(out.mass_kg))
        throw std::overflow_error("Oblique-PLIC mass overflow");
    for(double a:out.centre_of_mass_m)if(!std::isfinite(a))
        throw std::overflow_error("Oblique-PLIC centroid overflow");
    return out;
}
} // namespace upc
