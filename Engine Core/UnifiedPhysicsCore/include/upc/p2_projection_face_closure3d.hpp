#pragma once
// P2 PARTIAL: independent all-face accounting for the MAC pressure projection.
// All impulses/energies are staggered-face MASS PROXIES, not physical FSI.
#include "upc/p2_projection_gradient_budget3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct ProjectionFaceClosure3D {
    ProjectionGradientBudget3D components;
    std::array<double,3> all_active_linear_Ns{{0,0,0}};
    std::array<double,3> all_active_angular_Nms{{0,0,0}};
    std::array<double,3> decomposition_linear_residual_Ns{{0,0,0}};
    std::array<double,3> decomposition_angular_residual_Nms{{0,0,0}};
    double all_active_energy_delta_proxy_J=0;
    double decomposition_energy_residual_proxy_J=0;
    double max_dry_dry_delta_m_s=0;
    std::size_t changed_dry_dry_faces=0;
    std::size_t dry_dry_faces=0;
    std::size_t pressure_faces=0;
    std::size_t clamped_faces=0;
    std::size_t nonzero_clamped_after_faces=0;
};

class ProjectionFaceClosureAudit3D {
public:
    static ProjectionFaceClosure3D analyze(
        const MacProjection3D& before,const MacProjection3D& after,
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double rho_kg_m3,double dt_s,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::array<double,3>& centre_m) {
        ProjectionFaceClosure3D out;
        // Reuse the independently established gradient/clamp partition and
        // its input validation, then enumerate EVERY staggered face separately.
        out.components=ProjectionGradientBudgetAudit3D::analyze(
            before,after,nx,ny,nz,dx_m,rho_kg_m3,dt_s,wet,solid,centre_m);
        const double mass=rho_kg_m3*dx_m*dx_m*dx_m;
        const auto cell=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        for(std::size_t axis=0;axis<3;++axis)
        for(std::size_t z=0;z<nz+(axis==2);++z)
        for(std::size_t y=0;y<ny+(axis==1);++y)
        for(std::size_t x=0;x<nx+(axis==0);++x) {
            const bool exterior=(axis==0&&(x==0||x==nx))||
                                (axis==1&&(y==0||y==ny))||
                                (axis==2&&(z==0||z==nz));
            bool solid_face=false,pressure_face=false;
            if(!exterior) {
                const auto a=cell(x-(axis==0),y-(axis==1),z-(axis==2));
                const auto b=cell(x,y,z);
                solid_face=solid[a]||solid[b];
                pressure_face=!solid_face&&(wet[a]||wet[b]);
            }
            const double v0=axis==0?before.face_velocity_x(x,y,z):
                            axis==1?before.face_velocity_y(x,y,z):
                                     before.face_velocity_z(x,y,z);
            const double v1=axis==0?after.face_velocity_x(x,y,z):
                            axis==1?after.face_velocity_y(x,y,z):
                                     after.face_velocity_z(x,y,z);
            if(!std::isfinite(v0)||!std::isfinite(v1))
                throw std::invalid_argument("Nonfinite all-face MAC velocity");
            const double delta=v1-v0;
            if(!std::isfinite(delta))
                throw std::overflow_error("All-face velocity delta overflow");
            if(!exterior&&!solid_face&&!pressure_face) {
                ++out.dry_dry_faces;
                if(delta!=0)++out.changed_dry_dry_faces;
                out.max_dry_dry_delta_m_s=std::max(
                    out.max_dry_dry_delta_m_s,std::abs(delta));
                continue;
            }
            if(pressure_face)++out.pressure_faces;
            else {
                ++out.clamped_faces;
                if(v1!=0)++out.nonzero_clamped_after_faces;
            }
            const double dp=mass*delta;
            const double de=0.5*mass*(v1*v1-v0*v0);
            const std::array<double,3> r{{
                (static_cast<double>(x)+(axis!=0?0.5:0.0))*dx_m-centre_m[0],
                (static_cast<double>(y)+(axis!=1?0.5:0.0))*dx_m-centre_m[1],
                (static_cast<double>(z)+(axis!=2?0.5:0.0))*dx_m-centre_m[2]
            }};
            if(!std::isfinite(dp)||!std::isfinite(de)||
               !std::isfinite(r[0])||!std::isfinite(r[1])||!std::isfinite(r[2]))
                throw std::overflow_error("All-face budget overflow");
            out.all_active_linear_Ns[axis]+=dp;
            out.all_active_angular_Nms[(axis+1)%3]+=r[(axis+2)%3]*dp;
            out.all_active_angular_Nms[(axis+2)%3]-=r[(axis+1)%3]*dp;
            out.all_active_energy_delta_proxy_J+=de;
        }
        for(std::size_t axis=0;axis<3;++axis) {
            out.decomposition_linear_residual_Ns[axis]=
                out.all_active_linear_Ns[axis]-
                (out.components.observed_linear_Ns[axis]+
                 out.components.clamp_linear_Ns[axis]);
            out.decomposition_angular_residual_Nms[axis]=
                out.all_active_angular_Nms[axis]-
                (out.components.observed_angular_Nms[axis]+
                 out.components.clamp_angular_Nms[axis]);
            if(!std::isfinite(out.all_active_linear_Ns[axis])||
               !std::isfinite(out.all_active_angular_Nms[axis])||
               !std::isfinite(out.decomposition_linear_residual_Ns[axis])||
               !std::isfinite(out.decomposition_angular_residual_Nms[axis]))
                throw std::overflow_error("All-face impulse accumulation overflow");
        }
        out.decomposition_energy_residual_proxy_J=
            out.all_active_energy_delta_proxy_J-
            (out.components.observed_face_energy_delta_proxy_J+
             out.components.clamp_face_energy_delta_proxy_J);
        if(!std::isfinite(out.all_active_energy_delta_proxy_J)||
           !std::isfinite(out.decomposition_energy_residual_proxy_J))
            throw std::overflow_error("All-face energy accumulation overflow");
        return out;
    }
};
} // namespace upc
