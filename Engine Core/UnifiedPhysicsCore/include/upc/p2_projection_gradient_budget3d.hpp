#pragma once
// P2 PARTIAL: pressure-gradient explanation of the post-projection MAC
// face-wrench change. This is a staggered-face MASS PROXY, not a physical
// fluid/rigid-body impulse or global mechanical-energy conservation law.
#include "upc/mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct ProjectionGradientBudget3D {
    std::array<double,3> observed_linear_Ns{{0,0,0}};
    std::array<double,3> gradient_linear_Ns{{0,0,0}};
    std::array<double,3> observed_angular_Nms{{0,0,0}};
    std::array<double,3> gradient_angular_Nms{{0,0,0}};
    double observed_face_energy_delta_proxy_J=0;
    double pressure_gradient_work_proxy_J=0;
    double euler_quadratic_proxy_J=0;
    double max_gradient_velocity_residual_m_s=0;
    double max_divergence_after_s_inv=0;
    std::size_t wet_wet_faces=0,wet_air_faces=0;
    // Forced-zero MAC wall/exterior faces are separate from the pressure
    // gradient. These remain staggered-face proxies, NOT physical wall forces.
    std::array<double,3> clamp_linear_Ns{{0,0,0}};
    std::array<double,3> clamp_angular_Nms{{0,0,0}};
    double clamp_face_energy_delta_proxy_J=0;
    double max_clamp_after_velocity_m_s=0;
    std::size_t clamp_solid_faces=0,clamp_exterior_faces=0;
};

class ProjectionGradientBudgetAudit3D {
public:
    static ProjectionGradientBudget3D analyze(
        const MacProjection3D& before,const MacProjection3D& after,
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double rho_kg_m3,double dt_s,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::array<double,3>& centre_m) {
        const auto finite3=[](const std::array<double,3>& v) {
            return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
        };
        if(nx<3||ny<3||nz<3||
           nx>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           ny>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nz>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           wet.size()!=nx*ny*nz||solid.size()!=nx*ny*nz||
           !std::isfinite(dx_m)||dx_m<=0||
           !std::isfinite(rho_kg_m3)||rho_kg_m3<=0||
           !std::isfinite(dt_s)||dt_s<=0||!finite3(centre_m))
            throw std::invalid_argument("Invalid projection-gradient budget input");
        const double mass=rho_kg_m3*dx_m*dx_m*dx_m;
        const double scale=dt_s/(rho_kg_m3*dx_m);
        if(!std::isfinite(mass)||mass<=0||!std::isfinite(scale)||scale<=0)
            throw std::overflow_error("Projection-gradient budget mass/scale overflow");
        for(std::size_t i=0;i<wet.size();++i)
            if(wet[i]>1||solid[i]>1||(wet[i]&&solid[i]))
                throw std::invalid_argument("Invalid projection-gradient masks");
        const auto idx=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        ProjectionGradientBudget3D out;
        for(std::size_t axis=0;axis<3;++axis)
        for(std::size_t z=0;z<nz+(axis==2);++z)
        for(std::size_t y=0;y<ny+(axis==1);++y)
        for(std::size_t x=0;x<nx+(axis==0);++x) {
            if((axis==0&&(x==0||x==nx))||
               (axis==1&&(y==0||y==ny))||
               (axis==2&&(z==0||z==nz)))continue;
            const auto a=idx(x-(axis==0),y-(axis==1),z-(axis==2));
            const auto b=idx(x,y,z);
            if(solid[a]||solid[b]||(!wet[a]&&!wet[b]))continue;
            const double pa=wet[a]?after.pressure_pa(x-(axis==0),y-(axis==1),z-(axis==2)):0.0;
            const double pb=wet[b]?after.pressure_pa(x,y,z):0.0;
            const double v0=axis==0?before.face_velocity_x(x,y,z):
                            axis==1?before.face_velocity_y(x,y,z):
                                     before.face_velocity_z(x,y,z);
            const double v1=axis==0?after.face_velocity_x(x,y,z):
                            axis==1?after.face_velocity_y(x,y,z):
                                     after.face_velocity_z(x,y,z);
            if(!std::isfinite(pa)||!std::isfinite(pb)||
               !std::isfinite(v0)||!std::isfinite(v1))
                throw std::invalid_argument("Nonfinite projection-gradient state");
            const double ghost=(wet[a]!=wet[b])?2.0:1.0;
            const double predicted=-scale*ghost*(pb-pa);
            const double observed=v1-v0;
            const double dp_pred=mass*predicted,dp_obs=mass*observed;
            const double energy_obs=0.5*mass*(v1*v1-v0*v0);
            const double work=mass*v0*predicted;
            const double quadratic=0.5*mass*predicted*predicted;
            const std::array<double,3> r{{
                (static_cast<double>(x)+(axis!=0?0.5:0.0))*dx_m-centre_m[0],
                (static_cast<double>(y)+(axis!=1?0.5:0.0))*dx_m-centre_m[1],
                (static_cast<double>(z)+(axis!=2?0.5:0.0))*dx_m-centre_m[2]
            }};
            if(!finite3(r)||!std::isfinite(predicted)||!std::isfinite(observed)||
               !std::isfinite(energy_obs)||!std::isfinite(work)||
               !std::isfinite(quadratic))
                throw std::overflow_error("Projection-gradient face overflow");
            out.max_gradient_velocity_residual_m_s=std::max(
                out.max_gradient_velocity_residual_m_s,std::abs(observed-predicted));
            out.observed_linear_Ns[axis]+=dp_obs;
            out.gradient_linear_Ns[axis]+=dp_pred;
            // r x (dp * e_axis)
            if(axis==0) {
                out.observed_angular_Nms[1]+=r[2]*dp_obs;
                out.observed_angular_Nms[2]-=r[1]*dp_obs;
                out.gradient_angular_Nms[1]+=r[2]*dp_pred;
                out.gradient_angular_Nms[2]-=r[1]*dp_pred;
            } else if(axis==1) {
                out.observed_angular_Nms[0]-=r[2]*dp_obs;
                out.observed_angular_Nms[2]+=r[0]*dp_obs;
                out.gradient_angular_Nms[0]-=r[2]*dp_pred;
                out.gradient_angular_Nms[2]+=r[0]*dp_pred;
            } else {
                out.observed_angular_Nms[0]+=r[1]*dp_obs;
                out.observed_angular_Nms[1]-=r[0]*dp_obs;
                out.gradient_angular_Nms[0]+=r[1]*dp_pred;
                out.gradient_angular_Nms[1]-=r[0]*dp_pred;
            }
            out.observed_face_energy_delta_proxy_J+=energy_obs;
            out.pressure_gradient_work_proxy_J+=work;
            out.euler_quadratic_proxy_J+=quadratic;
            if(wet[a]&&wet[b])++out.wet_wet_faces;
            else ++out.wet_air_faces;
        }
        // project() first zeros external and solid-adjacent faces. These
        // changes are not pressure gradients and were previously omitted.
        // Count each face exactly once, including solid-air and exterior-air.
        for(std::size_t axis=0;axis<3;++axis)
        for(std::size_t z=0;z<nz+(axis==2);++z)
        for(std::size_t y=0;y<ny+(axis==1);++y)
        for(std::size_t x=0;x<nx+(axis==0);++x) {
            const bool exterior=(axis==0&&(x==0||x==nx))||
                                (axis==1&&(y==0||y==ny))||
                                (axis==2&&(z==0||z==nz));
            bool solid_face=false;
            if(!exterior) {
                const auto a=idx(x-(axis==0),y-(axis==1),z-(axis==2));
                const auto b=idx(x,y,z);
                solid_face=solid[a]||solid[b];
            }
            if(!exterior&&!solid_face)continue;
            const double v0=axis==0?before.face_velocity_x(x,y,z):
                            axis==1?before.face_velocity_y(x,y,z):
                                     before.face_velocity_z(x,y,z);
            const double v1=axis==0?after.face_velocity_x(x,y,z):
                            axis==1?after.face_velocity_y(x,y,z):
                                     after.face_velocity_z(x,y,z);
            if(!std::isfinite(v0)||!std::isfinite(v1))
                throw std::invalid_argument("Nonfinite MAC clamp face");
            const double impulse=mass*(v1-v0);
            const double energy=0.5*mass*(v1*v1-v0*v0);
            const std::array<double,3> r{{
                (static_cast<double>(x)+(axis!=0?0.5:0.0))*dx_m-centre_m[0],
                (static_cast<double>(y)+(axis!=1?0.5:0.0))*dx_m-centre_m[1],
                (static_cast<double>(z)+(axis!=2?0.5:0.0))*dx_m-centre_m[2]
            }};
            if(!finite3(r)||!std::isfinite(impulse)||!std::isfinite(energy))
                throw std::overflow_error("MAC clamp budget overflow");
            out.clamp_linear_Ns[axis]+=impulse;
            out.clamp_angular_Nms[(axis+1)%3]+=r[(axis+2)%3]*impulse;
            out.clamp_angular_Nms[(axis+2)%3]-=r[(axis+1)%3]*impulse;
            out.clamp_face_energy_delta_proxy_J+=energy;
            out.max_clamp_after_velocity_m_s=std::max(
                out.max_clamp_after_velocity_m_s,std::abs(v1));
            if(exterior)++out.clamp_exterior_faces;
            else ++out.clamp_solid_faces;
        }
        if(!finite3(out.clamp_linear_Ns)||!finite3(out.clamp_angular_Nms)||
           !std::isfinite(out.clamp_face_energy_delta_proxy_J))
            throw std::overflow_error("MAC clamp budget accumulation overflow");
        if(!finite3(out.observed_linear_Ns)||!finite3(out.gradient_linear_Ns)||
           !finite3(out.observed_angular_Nms)||!finite3(out.gradient_angular_Nms)||
           !std::isfinite(out.observed_face_energy_delta_proxy_J)||
           !std::isfinite(out.pressure_gradient_work_proxy_J)||
           !std::isfinite(out.euler_quadratic_proxy_J))
            throw std::overflow_error("Projection-gradient budget accumulation overflow");
        out.max_divergence_after_s_inv=after.max_divergence();
        if(!std::isfinite(out.max_divergence_after_s_inv))
            throw std::overflow_error("Projection-gradient budget divergence overflow");
        return out;
    }
};
} // namespace upc
