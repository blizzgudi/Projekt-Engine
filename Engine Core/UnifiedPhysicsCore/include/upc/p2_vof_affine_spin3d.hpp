#pragma once
// P2 partial: read-only affine subcell spin diagnostic for MAC/VOF water.
// Uniform fractional density in each cubic cell, linear velocity field
// reconstructed from neighbouring MAC cell-centred velocities. Not a PLIC
// free surface, conservative advection, or coupled FSI angular-momentum law.
// Dry neighbours may supply velocity samples: accuracy requires a meaningful
// velocity extension into the stencil, especially near free surfaces.
#include "upc/p2_vof_uniform_cell_inertia3d.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace upc {
struct VofAffineSpinInventory3D {
    VofAngularInventory3D inventory;
    std::array<double,3> intrinsic_spin_kg_m2_s{};
    std::array<double,3> angular_momentum_about_com_kg_m2_s{};
};

// The exact intrinsic angular momentum of an affine field in a uniform
// cube is m*dx^2/12 * curl(v). In particular curl(omega x r)=2*omega,
// giving I_cube*omega with I_cube=m*dx^2/6.
// Transverse derivatives use central differences internally and one-sided
// differences on the boundary. A singleton axis has zero derivative.
// The existing point-mass inventory is preserved and augmented, not changed.
inline VofAffineSpinInventory3D vof_affine_spin_inventory3d(
    const MacProjection3D& mac,
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double rho,
    const std::array<double,3>& origin_m={0,0,0}) {
    VofAffineSpinInventory3D out;
    out.inventory=vof_uniform_cell_inertia3d(
        mac,fill,solid,nx,ny,nz,dx,rho,origin_m);
    const std::array<std::size_t,3> sizes{nx,ny,nz};
    auto sample=[&](int component,const std::array<std::size_t,3>& q) {
        const auto x=q[0],y=q[1],z=q[2];
        double v=0;
        if(component==0)
            v=0.5*(mac.face_velocity_x(x,y,z)+mac.face_velocity_x(x+1,y,z));
        else if(component==1)
            v=0.5*(mac.face_velocity_y(x,y,z)+mac.face_velocity_y(x,y+1,z));
        else
            v=0.5*(mac.face_velocity_z(x,y,z)+mac.face_velocity_z(x,y,z+1));
        if(!std::isfinite(v))
            throw std::runtime_error("Nonfinite MAC affine velocity sample");
        return v;
    };
    auto derivative=[&](int component,int direction,
                        const std::array<std::size_t,3>& q) {
        if(sizes[direction]==1)return 0.0;
        auto a=q,b=q;
        a[direction]=q[direction]==0?0:q[direction]-1;
        b[direction]=q[direction]+1<sizes[direction]?
            q[direction]+1:sizes[direction]-1;
        const double grad=(sample(component,b)-sample(component,a))/
                          ((static_cast<double>(b[direction]-a[direction]))*dx);
        if(!std::isfinite(grad))
            throw std::runtime_error("Nonfinite MAC affine velocity gradient");
        return grad;
    };
    const double cell_mass=rho*dx*dx*dx;
    const double spin_scale=cell_mass*dx*dx/12.0;
    if(!std::isfinite(spin_scale))
        throw std::runtime_error("Nonfinite MAC affine spin scale");
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto i=(z*ny+y)*nx+x;
        if(fill[i]==0)continue;
        const std::array<std::size_t,3> q{x,y,z};
        const std::array<double,3> curl{
            derivative(2,1,q)-derivative(1,2,q),
            derivative(0,2,q)-derivative(2,0,q),
            derivative(1,0,q)-derivative(0,1,q)};
        for(int k=0;k<3;++k) {
            const double increment=fill[i]*spin_scale*curl[k];
            out.intrinsic_spin_kg_m2_s[k]+=increment;
            if(!std::isfinite(out.intrinsic_spin_kg_m2_s[k]))
                throw std::runtime_error("Nonfinite MAC affine spin");
        }
    }
    const std::array<double,3> r{
        out.inventory.centre_of_mass_m[0]-origin_m[0],
        out.inventory.centre_of_mass_m[1]-origin_m[1],
        out.inventory.centre_of_mass_m[2]-origin_m[2]};
    for(int k=0;k<3;++k) {
        out.inventory.angular_momentum_kg_m2_s[k]+=
            out.intrinsic_spin_kg_m2_s[k];
        out.angular_momentum_about_com_kg_m2_s[k]=
            out.inventory.angular_momentum_kg_m2_s[k]-
            (r[(k+1)%3]*out.inventory.linear_momentum_kg_m_s[(k+2)%3]-
             r[(k+2)%3]*out.inventory.linear_momentum_kg_m_s[(k+1)%3]);
        if(!std::isfinite(out.inventory.angular_momentum_kg_m2_s[k])||
           !std::isfinite(out.angular_momentum_about_com_kg_m2_s[k]))
            throw std::runtime_error("Nonfinite MAC affine angular momentum");
    }
    return out;
}
} // namespace upc
