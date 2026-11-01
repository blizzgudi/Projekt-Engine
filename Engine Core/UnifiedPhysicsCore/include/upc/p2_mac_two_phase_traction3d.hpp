#pragma once
// P2 PARTIAL: read-only reconstruction of BOTH one-sided phase velocity
// gradients from actual 3D staggered MAC face velocities at SDF cut faces.
// Strict phase stencils; no air solve, ghost-velocity update or FSI closure.
#include "upc/p2_sdf_normal_stress3d.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MacTwoPhaseTractionFace3D {
    P2SdfNormalStressFace3D interface;
    P2GradientTensor3D water_gradient{}, air_gradient{};
    std::array<double,3> traction_jump_pa{}, tangential_residual_pa{};
    double normal_viscous_jump_pa=0, tangential_residual_norm_pa=0;
    double required_liquid_pressure_pa=0;
};
struct P2MacTwoPhaseTractionResult3D {
    std::vector<P2MacTwoPhaseTractionFace3D> faces;
};
inline P2MacTwoPhaseTractionResult3D p2_mac_two_phase_traction3d(
    const MacProjection3D& mac,const std::vector<double>& phi,
    double sigma,double p_air,double mu_water,double mu_air,
    const std::array<double,3>& surface_tension_gradient_n_m2) {
    const auto nx=mac.nx(),ny=mac.ny(),nz=mac.nz();
    const double dx=mac.cell_size_m();
    for(double g:surface_tension_gradient_n_m2)
        if(!std::isfinite(g))throw std::invalid_argument("Nonfinite surface gradient");
    const P2GradientTensor3D zero{};
    const auto base=p2_sdf_normal_stress_faces3d(
        nx,ny,nz,dx,phi,sigma,p_air,mu_water,mu_air,zero,zero);
    const auto index=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        if(mac.solid_cell(x,y,z)||mac.fluid_cell(x,y,z)!=(phi[index(x,y,z)]<0))
            throw std::invalid_argument("MAC/SDF phase or solid mismatch");
    }
    const auto centre=[&](std::size_t x,std::size_t y,std::size_t z,int c){
        if(c==0)return .5*(mac.face_velocity_x(x,y,z)+mac.face_velocity_x(x+1,y,z));
        if(c==1)return .5*(mac.face_velocity_y(x,y,z)+mac.face_velocity_y(x,y+1,z));
        return .5*(mac.face_velocity_z(x,y,z)+mac.face_velocity_z(x,y,z+1));
    };
    const auto gradient=[&](std::array<std::size_t,3> p,bool liquid){
        P2GradientTensor3D out{};
        const std::array<std::size_t,3> limit{nx,ny,nz};
        for(int d=0;d<3;++d){
            auto low=p,high=p;
            bool has_low=false,has_high=false;
            if(p[d]>0){--low[d];has_low=(phi[index(low[0],low[1],low[2])]<0)==liquid;}
            if(p[d]+1<limit[d]){++high[d];has_high=(phi[index(high[0],high[1],high[2])]<0)==liquid;}
            if(!has_low&&!has_high)
                throw std::invalid_argument("Unresolved same-phase MAC derivative stencil");
            for(int c=0;c<3;++c){
                const double here=centre(p[0],p[1],p[2],c);
                const double a=has_low?centre(low[0],low[1],low[2],c):here;
                const double b=has_high?centre(high[0],high[1],high[2],c):here;
                out[3*c+d]=(b-a)/(dx*((has_low&&has_high)?2.0:1.0));
                if(!std::isfinite(out[3*c+d]))
                    throw std::invalid_argument("Nonfinite MAC phase gradient");
            }
        }
        return out;
    };
    P2MacTwoPhaseTractionResult3D result;
    result.faces.reserve(base.faces.size());
    for(const auto& f:base.faces){
        std::array<std::size_t,3> a{f.face.x,f.face.y,f.face.z},b=a;
        --a[f.face.axis];
        const bool a_wet=phi[index(a[0],a[1],a[2])]<0;
        const auto w=gradient(a_wet?a:b,true);
        const auto air=gradient(a_wet?b:a,false);
        const auto& n=f.outward_normal;
        std::array<double,3> traction{},residual{};
        double normal=0;
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)
            traction[i]+=(mu_water*(w[3*i+j]+w[3*j+i])-
                          mu_air*(air[3*i+j]+air[3*j+i]))*n[j];
        for(int i=0;i<3;++i)normal+=traction[i]*n[i];
        double residual2=0,normal_sigma=0;
        for(int i=0;i<3;++i)normal_sigma+=surface_tension_gradient_n_m2[i]*n[i];
        for(int i=0;i<3;++i){
            residual[i]=traction[i]-normal*n[i]-
                (surface_tension_gradient_n_m2[i]-normal_sigma*n[i]);
            residual2+=residual[i]*residual[i];
        }
        const double pressure=f.face.pressure_pa+normal;
        if(!std::isfinite(pressure)||!std::isfinite(residual2))
            throw std::invalid_argument("Nonfinite reconstructed stress");
        result.faces.push_back({f,w,air,traction,residual,normal,
                                std::sqrt(residual2),pressure});
    }
    return result;
}
} // namespace upc
