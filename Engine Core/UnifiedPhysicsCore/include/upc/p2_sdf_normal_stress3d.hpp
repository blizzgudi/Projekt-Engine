#pragma once
// P2 PARTIAL: prescribed liquid/air velocity gradients contribute to the
// normal viscous traction jump at a 3D SDF free surface. No dynamic air
// solver, tangential stress continuity or live VOF-to-SDF reconstruction.
#include "upc/p2_sdf_capillary3d.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
using P2GradientTensor3D = std::array<double,9>; // row-major d(u_i)/d(x_j), 1/s
struct P2SdfNormalStressFace3D {
    P2SdfCapillaryFace3D face;
    std::array<double,3> outward_normal;
    double viscous_normal_jump_pa;
};
struct P2SdfNormalStressResult3D {
    std::vector<P2SdfNormalStressFace3D> faces;
};
inline P2SdfNormalStressResult3D p2_sdf_normal_stress_faces3d(
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,
    const std::vector<double>& phi,double sigma,double p_air,
    double mu_water,double mu_air,
    const P2GradientTensor3D& grad_water,
    const P2GradientTensor3D& grad_air){
    if(!std::isfinite(mu_water)||!std::isfinite(mu_air)||
       mu_water<0||mu_air<0)
        throw std::invalid_argument("Invalid liquid/air dynamic viscosity");
    for(double a:grad_water)if(!std::isfinite(a))
        throw std::invalid_argument("Nonfinite liquid velocity gradient");
    for(double a:grad_air)if(!std::isfinite(a))
        throw std::invalid_argument("Nonfinite air velocity gradient");
    const auto base=p2_sdf_capillary_faces3d(nx,ny,nz,dx,phi,sigma,p_air);
    const auto at=[&](std::size_t x,std::size_t y,std::size_t z){
        return phi[(z*ny+y)*nx+x];
    };
    const auto gradient=[&](std::size_t x,std::size_t y,std::size_t z){
        std::array<double,3> g{};
        const std::size_t v[3]={x,y,z},limit[3]={nx,ny,nz};
        for(int i=0;i<3;++i){
            std::size_t lo[3]={x,y,z},hi[3]={x,y,z};
            lo[i]=v[i]?v[i]-1:v[i];
            hi[i]=v[i]+1<limit[i]?v[i]+1:v[i];
            // One-sided at the boundary; SDF curvature already rejects
            // any nonconstant boundary normal used by a cut face.
            g[i]=(at(hi[0],hi[1],hi[2])-at(lo[0],lo[1],lo[2]))/
                 ((hi[i]-lo[i])*dx);
        }
        return g;
    };
    const auto normal_strain=[&](const P2GradientTensor3D& a,
                                  const std::array<double,3>& n){
        double value=0;
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)
            value+=n[i]*0.5*(a[3*i+j]+a[3*j+i])*n[j];
        return value;
    };
    P2SdfNormalStressResult3D out;
    out.faces.reserve(base.faces.size());
    for(const auto& f:base.faces){
        std::size_t left[3]={f.x,f.y,f.z},right[3]={f.x,f.y,f.z};
        --left[f.axis];
        const auto ga=gradient(left[0],left[1],left[2]);
        const auto gb=gradient(right[0],right[1],right[2]);
        const bool left_wet=at(left[0],left[1],left[2])<0;
        const double wa=left_wet?1-f.theta:f.theta;
        std::array<double,3> n{};
        double norm2=0;
        for(int i=0;i<3;++i){
            n[i]=wa*ga[i]+(1-wa)*gb[i];
            norm2+=n[i]*n[i];
        }
        if(!std::isfinite(norm2)||norm2<1e-12)
            throw std::invalid_argument("Degenerate interface normal");
        for(double& v:n)v/=std::sqrt(norm2);
        const double jump=2*(mu_water*normal_strain(grad_water,n)-
                             mu_air*normal_strain(grad_air,n));
        const double pressure=f.pressure_pa+jump;
        if(!std::isfinite(jump)||!std::isfinite(pressure))
            throw std::invalid_argument("Normal stress pressure overflow");
        auto corrected=f;
        corrected.pressure_pa=pressure;
        out.faces.push_back({corrected,n,jump});
    }
    return out;
}
inline P2SdfNormalStressResult3D p2_apply_sdf_normal_stress3d(
    MacProjection3D& mac,std::size_t nx,std::size_t ny,std::size_t nz,
    double dx,const std::vector<double>& phi,double sigma,double p_air,
    double mu_water,double mu_air,
    const P2GradientTensor3D& grad_water,
    const P2GradientTensor3D& grad_air){
    if(mac.nx()!=nx||mac.ny()!=ny||mac.nz()!=nz||mac.cell_size_m()!=dx)
        throw std::invalid_argument("MAC/SDF shape mismatch");
    auto result=p2_sdf_normal_stress_faces3d(nx,ny,nz,dx,phi,sigma,p_air,
                                             mu_water,mu_air,grad_water,grad_air);
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        if(mac.solid_cell(x,y,z)||
           mac.fluid_cell(x,y,z)!=(phi[(z*ny+y)*nx+x]<0))
            throw std::invalid_argument("MAC/SDF wet or solid mask mismatch");
    MacProjection3D work=mac;
    work.clear_interface_distances();
    work.clear_interface_pressures();
    for(const auto& e:result.faces){
        const auto& f=e.face;
        if(f.axis==0){
            work.set_interface_distance_x(f.x,f.y,f.z,f.theta);
            work.set_interface_pressure_x(f.x,f.y,f.z,f.pressure_pa);
        }else if(f.axis==1){
            work.set_interface_distance_y(f.x,f.y,f.z,f.theta);
            work.set_interface_pressure_y(f.x,f.y,f.z,f.pressure_pa);
        }else{
            work.set_interface_distance_z(f.x,f.y,f.z,f.theta);
            work.set_interface_pressure_z(f.x,f.y,f.z,f.pressure_pa);
        }
    }
    mac=std::move(work);
    return result;
}
} // namespace upc
