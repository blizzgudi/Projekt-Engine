#pragma once
// P2 partial: VOF-weighted cell-centre angular-momentum / inertia diagnostics.
// Point masses at cell centres; no sub-cell spin or moving-body FSI conservation.
#include "upc/mac3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct VofAngularInventory3D {
    double mass_kg=0;
    std::array<double,3> linear_momentum_kg_m_s{};
    std::array<double,3> angular_momentum_kg_m2_s{};
    std::array<double,3> centre_of_mass_m{};
    std::array<std::array<double,3>,3> inertia_about_origin_kg_m2{};
    std::array<std::array<double,3>,3> inertia_about_com_kg_m2{};
    std::size_t occupied_cells=0;
};

// origin_m is expressed in the same world coordinates as cell centres.
// Inertia is the POINT-MASS approximation consistent with the sampled
// angular momentum; intrinsic cubical-cell inertia is deliberately excluded.
inline VofAngularInventory3D vof_angular_inventory3d(
    const MacProjection3D& mac,
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double rho,
    const std::array<double,3>& origin_m={0,0,0}) {
    if(!nx||!ny||!nz||
       nx>std::numeric_limits<std::size_t>::max()/ny||
       nx*ny>std::numeric_limits<std::size_t>::max()/nz||
       fill.size()!=nx*ny*nz||solid.size()!=fill.size()||
       !std::isfinite(dx)||dx<=0||!std::isfinite(rho)||rho<=0)
        throw std::invalid_argument("Invalid angular VOF domain");
    for(double v:origin_m)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite angular VOF origin");
    const double cell_mass=rho*dx*dx*dx;
    if(!std::isfinite(cell_mass)||cell_mass<=0)
        throw std::invalid_argument("Invalid angular VOF cell mass");
    VofAngularInventory3D out;
    std::array<double,3> first_moment{};
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const std::size_t i=(z*ny+y)*nx+x;
        const double f=fill[i];
        if(!std::isfinite(f)||f<0||f>1||(solid[i]&&f>0))
            throw std::invalid_argument("Invalid angular VOF occupancy");
        if(f==0)continue;
        const std::array<double,3> r{
            (static_cast<double>(x)+0.5)*dx-origin_m[0],
            (static_cast<double>(y)+0.5)*dx-origin_m[1],
            (static_cast<double>(z)+0.5)*dx-origin_m[2]};
        const std::array<double,3> v{
            .5*(mac.face_velocity_x(x,y,z)+mac.face_velocity_x(x+1,y,z)),
            .5*(mac.face_velocity_y(x,y,z)+mac.face_velocity_y(x,y+1,z)),
            .5*(mac.face_velocity_z(x,y,z)+mac.face_velocity_z(x,y,z+1))};
        for(int k=0;k<3;++k)if(!std::isfinite(r[k])||!std::isfinite(v[k]))
            throw std::runtime_error("Nonfinite angular VOF position or velocity");
        const double m=f*cell_mass;
        out.mass_kg+=m;
        for(int k=0;k<3;++k) {
            first_moment[k]+=m*r[k];
            out.linear_momentum_kg_m_s[k]+=m*v[k];
            out.angular_momentum_kg_m2_s[k]+=
                m*(r[(k+1)%3]*v[(k+2)%3]-r[(k+2)%3]*v[(k+1)%3]);
            for(int j=0;j<3;++j)
                out.inertia_about_origin_kg_m2[k][j]+=
                    m*((k==j?r[0]*r[0]+r[1]*r[1]+r[2]*r[2]:0)-r[k]*r[j]);
        }
        ++out.occupied_cells;
    }
    if(out.mass_kg>0)
        for(int k=0;k<3;++k)
            out.centre_of_mass_m[k]=first_moment[k]/out.mass_kg+origin_m[k];
    else out.centre_of_mass_m=origin_m; // Explicit empty-domain convention.
    for(int k=0;k<3;++k)for(int j=0;j<3;++j) {
        const double a=first_moment[k]/(out.mass_kg>0?out.mass_kg:1);
        const double b=first_moment[j]/(out.mass_kg>0?out.mass_kg:1);
        const double norm2=out.mass_kg>0?
            (first_moment[0]*first_moment[0]+
             first_moment[1]*first_moment[1]+
             first_moment[2]*first_moment[2])/(out.mass_kg*out.mass_kg):0;
        out.inertia_about_com_kg_m2[k][j]=
            out.inertia_about_origin_kg_m2[k][j]-
            out.mass_kg*((k==j?norm2:0)-a*b);
    }
    if(!std::isfinite(out.mass_kg)||out.mass_kg<0)
        throw std::runtime_error("Nonfinite angular VOF mass");
    for(int k=0;k<3;++k) {
        if(!std::isfinite(out.linear_momentum_kg_m_s[k])||
           !std::isfinite(out.angular_momentum_kg_m2_s[k])||
           !std::isfinite(out.centre_of_mass_m[k]))
            throw std::runtime_error("Nonfinite angular VOF moment");
        for(int j=0;j<3;++j)
            if(!std::isfinite(out.inertia_about_origin_kg_m2[k][j])||
               !std::isfinite(out.inertia_about_com_kg_m2[k][j]))
                throw std::runtime_error("Nonfinite angular VOF inertia");
    }
    return out;
}

inline std::array<double,3> vof_angular_delta3d(
    const VofAngularInventory3D& before,
    const VofAngularInventory3D& after) {
    std::array<double,3> delta{};
    for(int k=0;k<3;++k) {
        if(!std::isfinite(before.angular_momentum_kg_m2_s[k])||
           !std::isfinite(after.angular_momentum_kg_m2_s[k]))
            throw std::invalid_argument("Nonfinite angular VOF delta input");
        delta[k]=after.angular_momentum_kg_m2_s[k]-
                 before.angular_momentum_kg_m2_s[k];
        if(!std::isfinite(delta[k]))
            throw std::runtime_error("Nonfinite angular VOF delta");
    }
    return delta;
}
} // namespace upc
