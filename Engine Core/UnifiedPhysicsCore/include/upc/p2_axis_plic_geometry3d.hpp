#pragma once
// P2 PARTIAL: exact moments for axis-aligned planar VOF cuts of cubic cells.
// The supplied axis/sign describes which end of each partially filled cell
// contains liquid. This is NOT general oblique PLIC, interface reconstruction,
// advection, surface tension, stress continuity, or two-way fluid-solid FSI.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2AxisPlicGeometry3D {
    double mass_kg=0.0;
    std::array<double,3> centre_of_mass_m{};
    std::array<std::array<double,3>,3> inertia_about_origin_kg_m2{};
    std::array<std::array<double,3>,3> inertia_about_com_kg_m2{};
    std::size_t occupied_cells=0;
    std::size_t partial_cells=0;
};

// For each cell, axis=0,1,2 selects the cut direction; sign=+1 fills
// from the low face, sign=-1 fills from the high face. Empty/full cells
// are orientation-independent. All moments are relative to origin_m.
// The liquid in a partially filled cell is a rectangular prism, so its
// centroid and intrinsic inertia are exact under this limited geometry.
inline P2AxisPlicGeometry3D p2_axis_plic_geometry3d(
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    const std::vector<std::uint8_t>& axis,
    const std::vector<std::int8_t>& sign,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx,double rho,
    const std::array<double,3>& origin_m={0,0,0}) {
    if(nx==0||ny==0||nz==0||
       nx>std::numeric_limits<std::size_t>::max()/ny||
       nx*ny>std::numeric_limits<std::size_t>::max()/nz||
       !std::isfinite(dx)||dx<=0.0||
       !std::isfinite(rho)||rho<=0.0)
        throw std::invalid_argument("Invalid axis-PLIC grid or material");
    const auto n=nx*ny*nz;
    if(fill.size()!=n||solid.size()!=n||axis.size()!=n||sign.size()!=n)
        throw std::invalid_argument("Axis-PLIC array size mismatch");
    for(double v:origin_m)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite axis-PLIC origin");
    const double base_mass=rho*dx*dx*dx;
    if(!std::isfinite(base_mass)||base_mass<=0.0)
        throw std::overflow_error("Nonfinite axis-PLIC cell mass");
    P2AxisPlicGeometry3D out;
    std::array<double,3> first{};
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto i=(z*ny+y)*nx+x;
        const double f=fill[i];
        if(!std::isfinite(f)||f<0.0||f>1.0||
           solid[i]>1||(solid[i]&&f>0.0)||
           axis[i]>2||(sign[i]!=1&&sign[i]!=-1))
            throw std::invalid_argument("Invalid axis-PLIC cell state");
        if(f==0.0)continue;
        std::array<double,3> low{
            static_cast<double>(x)*dx,
            static_cast<double>(y)*dx,
            static_cast<double>(z)*dx};
        std::array<double,3> high{low[0]+dx,low[1]+dx,low[2]+dx};
        if(f<1.0) {
            const auto a=axis[i];
            if(sign[i]==1)high[a]=low[a]+f*dx;
            else low[a]=high[a]-f*dx;
            ++out.partial_cells;
        }
        const std::array<double,3> w{
            high[0]-low[0],high[1]-low[1],high[2]-low[2]};
        const std::array<double,3> r{
            (low[0]+high[0])*0.5-origin_m[0],
            (low[1]+high[1])*0.5-origin_m[1],
            (low[2]+high[2])*0.5-origin_m[2]};
        const double m=base_mass*f;
        const double r2=r[0]*r[0]+r[1]*r[1]+r[2]*r[2];
        out.mass_kg+=m;
        for(int a=0;a<3;++a) {
            first[a]+=m*r[a];
            for(int b=0;b<3;++b) {
                const double intrinsic=a==b?
                    m*(w[(a+1)%3]*w[(a+1)%3]+
                       w[(a+2)%3]*w[(a+2)%3])/12.0:0.0;
                out.inertia_about_origin_kg_m2[a][b]+=
                    intrinsic+m*((a==b?r2:0.0)-r[a]*r[b]);
            }
        }
        ++out.occupied_cells;
    }
    if(out.mass_kg>0.0) {
        for(int a=0;a<3;++a)
            out.centre_of_mass_m[a]=origin_m[a]+first[a]/out.mass_kg;
    } else out.centre_of_mass_m=origin_m;
    const std::array<double,3> rc{
        out.centre_of_mass_m[0]-origin_m[0],
        out.centre_of_mass_m[1]-origin_m[1],
        out.centre_of_mass_m[2]-origin_m[2]};
    const double rc2=rc[0]*rc[0]+rc[1]*rc[1]+rc[2]*rc[2];
    for(int a=0;a<3;++a)for(int b=0;b<3;++b) {
        out.inertia_about_com_kg_m2[a][b]=
            out.inertia_about_origin_kg_m2[a][b]-
            out.mass_kg*((a==b?rc2:0.0)-rc[a]*rc[b]);
        if(!std::isfinite(out.inertia_about_origin_kg_m2[a][b])||
           !std::isfinite(out.inertia_about_com_kg_m2[a][b]))
            throw std::overflow_error("Nonfinite axis-PLIC inertia");
    }
    if(!std::isfinite(out.mass_kg))
        throw std::overflow_error("Nonfinite axis-PLIC total mass");
    for(double c:out.centre_of_mass_m)
        if(!std::isfinite(c))
            throw std::overflow_error("Nonfinite axis-PLIC centroid");
    return out;
}
} // namespace upc
