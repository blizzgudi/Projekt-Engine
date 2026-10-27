#pragma once
// P2 PARTIAL: read-only audit of a frozen 3D MAC face-wrench reaction
// before and after a SECOND pressure projection. All reported momenta
// use the rho*dx^3 staggered-face proxy; NOT a physical finite-volume
// fluid/solid momentum or total-energy conservation theorem.
#include "upc/mac3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {

struct MacFaceWrenchDelta3D {
    std::array<double,3> linear_Ns{{0,0,0}};
    std::array<double,3> angular_Nms{{0,0,0}};
    double face_energy_delta_proxy_J=0;
    std::size_t wet_wet_faces=0;
    std::size_t wet_air_faces=0;
};

struct PostProjectionWrenchAudit3D {
    MacFaceWrenchDelta3D reaction;
    MacFaceWrenchDelta3D projection;
    MacFaceWrenchDelta3D total;
    std::array<double,3> body_plus_reaction_linear_Ns{{0,0,0}};
    std::array<double,3> body_plus_reaction_angular_Nms{{0,0,0}};
    std::array<double,3> body_plus_total_linear_Ns{{0,0,0}};
    std::array<double,3> body_plus_total_angular_Nms{{0,0,0}};
    double max_divergence_after_s_inv=0;
};

class FrozenMacPostProjectionAudit3D {
    static bool finite3(const std::array<double,3>& v) {
        return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
    }
    static void add(MacFaceWrenchDelta3D& out, std::size_t axis,
                    bool both_wet, double old_v, double new_v,
                    double mass, const std::array<double,3>& lever) {
        const double impulse=mass*(new_v-old_v);
        const double energy=0.5*mass*(new_v*new_v-old_v*old_v);
        if(!std::isfinite(impulse)||!std::isfinite(energy))
            throw std::overflow_error("Nonfinite MAC face impulse/energy");
        out.linear_Ns[axis]+=impulse;
        if(axis==0) {
            out.angular_Nms[1]+=lever[2]*impulse;
            out.angular_Nms[2]-=lever[1]*impulse;
        } else if(axis==1) {
            out.angular_Nms[0]-=lever[2]*impulse;
            out.angular_Nms[2]+=lever[0]*impulse;
        } else {
            out.angular_Nms[0]+=lever[1]*impulse;
            out.angular_Nms[1]-=lever[0]*impulse;
        }
        out.face_energy_delta_proxy_J+=energy;
        if(both_wet)++out.wet_wet_faces;
        else ++out.wet_air_faces;
        if(!finite3(out.linear_Ns)||!finite3(out.angular_Nms)||
           !std::isfinite(out.face_energy_delta_proxy_J))
            throw std::overflow_error("MAC audit accumulation overflow");
    }
public:
    // Three immutable snapshots must share the same cell masks and grid.
    // Only interior faces with no adjacent solid and at least one wet cell
    // contribute. This includes wet-air faces modified by projection.
    // External boundaries and solid-normal faces are excluded deliberately.
    static PostProjectionWrenchAudit3D analyze(
        const MacProjection3D& before,
        const MacProjection3D& reacted,
        const MacProjection3D& projected,
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double rho_kg_m3,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::array<double,3>& centre_m,
        const std::array<double,3>& body_linear_Ns,
        const std::array<double,3>& body_angular_Nms) {
        if(nx<3||ny<3||nz<3||
           nx>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           ny>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nz>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           wet.size()!=nx*ny*nz||solid.size()!=nx*ny*nz||
           !std::isfinite(dx_m)||dx_m<=0||
           !std::isfinite(rho_kg_m3)||rho_kg_m3<=0||
           !finite3(centre_m)||!finite3(body_linear_Ns)||
           !finite3(body_angular_Nms))
            throw std::invalid_argument("Invalid frozen MAC audit input");
        const double mass=rho_kg_m3*dx_m*dx_m*dx_m;
        if(!std::isfinite(mass)||mass<=0)
            throw std::overflow_error("Invalid MAC audit face mass");
        for(std::size_t i=0;i<wet.size();++i)
            if(wet[i]>1||solid[i]>1||(wet[i]&&solid[i]))
                throw std::invalid_argument("Invalid wet/solid audit mask");
        const auto idx=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        PostProjectionWrenchAudit3D out;
        for(std::size_t axis=0;axis<3;++axis)
        for(std::size_t z=0;z<nz+(axis==2);++z)
        for(std::size_t y=0;y<ny+(axis==1);++y)
        for(std::size_t x=0;x<nx+(axis==0);++x) {
            if((axis==0&&(x==0||x==nx))||
               (axis==1&&(y==0||y==ny))||
               (axis==2&&(z==0||z==nz)))continue;
            const std::size_t ax=x-(axis==0),ay=y-(axis==1),az=z-(axis==2);
            const std::size_t bx=x,by=y,bz=z;
            const auto a=idx(ax,ay,az),b=idx(bx,by,bz);
            if(solid[a]||solid[b]||(!wet[a]&&!wet[b]))continue;
            const bool both_wet=wet[a]&&wet[b];
            const double v0=axis==0?before.face_velocity_x(x,y,z):
                            axis==1?before.face_velocity_y(x,y,z):
                                     before.face_velocity_z(x,y,z);
            const double v1=axis==0?reacted.face_velocity_x(x,y,z):
                            axis==1?reacted.face_velocity_y(x,y,z):
                                     reacted.face_velocity_z(x,y,z);
            const double v2=axis==0?projected.face_velocity_x(x,y,z):
                            axis==1?projected.face_velocity_y(x,y,z):
                                     projected.face_velocity_z(x,y,z);
            if(!std::isfinite(v0)||!std::isfinite(v1)||!std::isfinite(v2))
                throw std::invalid_argument("Nonfinite MAC snapshot face");
            const std::array<double,3> lever{{
                (static_cast<double>(x)+(axis!=0?0.5:0.0))*dx_m-centre_m[0],
                (static_cast<double>(y)+(axis!=1?0.5:0.0))*dx_m-centre_m[1],
                (static_cast<double>(z)+(axis!=2?0.5:0.0))*dx_m-centre_m[2]
            }};
            if(!finite3(lever))throw std::overflow_error("MAC audit lever overflow");
            add(out.reaction,axis,both_wet,v0,v1,mass,lever);
            add(out.projection,axis,both_wet,v1,v2,mass,lever);
            add(out.total,axis,both_wet,v0,v2,mass,lever);
        }
        for(std::size_t a=0;a<3;++a) {
            out.body_plus_reaction_linear_Ns[a]=body_linear_Ns[a]+out.reaction.linear_Ns[a];
            out.body_plus_reaction_angular_Nms[a]=body_angular_Nms[a]+out.reaction.angular_Nms[a];
            out.body_plus_total_linear_Ns[a]=body_linear_Ns[a]+out.total.linear_Ns[a];
            out.body_plus_total_angular_Nms[a]=body_angular_Nms[a]+out.total.angular_Nms[a];
        }
        if(!finite3(out.body_plus_reaction_linear_Ns)||
           !finite3(out.body_plus_reaction_angular_Nms)||
           !finite3(out.body_plus_total_linear_Ns)||
           !finite3(out.body_plus_total_angular_Nms))
            throw std::overflow_error("MAC audit residual overflow");
        out.max_divergence_after_s_inv=projected.max_divergence();
        if(!std::isfinite(out.max_divergence_after_s_inv))
            throw std::overflow_error("MAC audit divergence overflow");
        return out;
    }
};
} // namespace upc
