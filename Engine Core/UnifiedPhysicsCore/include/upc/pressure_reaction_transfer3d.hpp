#pragma once
// P2 PARTIAL: explicit fluid-side transfer of a frozen-pressure body impulse.
// Distributes the opposite *linear* impulse to nearby wet-wet MAC face DOFs.
// This is an impulse-conserving discrete FACE-MOMENTUM proxy, not a physical
// finite-volume FSI scheme: the body mask stays fixed, angular reaction is not
// applied, and a later incompressibility projection can alter the face budget.
#include "upc/mac3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct PressureReactionTransfer3D {
    std::array<double,3> requested_Ns{{0,0,0}};
    std::array<double,3> applied_face_impulse_Ns{{0,0,0}};
    std::array<std::size_t,3> face_count{{0,0,0}};
    std::array<double,3> angular_impulse_unapplied_Nms{{0,0,0}};
    double face_kinetic_energy_delta_proxy_J=0;
};

class FrozenPressureFluidReaction3D {
    struct FaceUpdate {
        std::size_t axis,x,y,z;
        double velocity;
    };
    static bool finite3(const std::array<double,3>& v) {
        return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
    }
public:
    // Caller supplies exactly matching MAC grid dimensions, rho and dx.
    // All candidate updates are preflighted BEFORE any face is changed.
    // Only faces between TWO liquid cells may be modified. In particular,
    // no solid-normal face, exterior wall or liquid-air face is touched.
    static PressureReactionTransfer3D apply(
        MacProjection3D& mac,
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double rho_kg_m3,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::vector<std::uint8_t>& body,
        const std::array<double,3>& fluid_reaction_Ns,
        const std::array<double,3>& fluid_angular_reaction_Nms={{0,0,0}}) {
        if(nx<3||ny<3||nz<3||
           nx>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           ny>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nz>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           wet.size()!=nx*ny*nz||solid.size()!=nx*ny*nz||
           body.size()!=nx*ny*nz||
           !std::isfinite(dx_m)||dx_m<=0||
           !std::isfinite(rho_kg_m3)||rho_kg_m3<=0||
           !finite3(fluid_reaction_Ns)||
           !finite3(fluid_angular_reaction_Nms))
            throw std::invalid_argument("Invalid frozen-pressure reaction transfer input");
        const double face_mass=rho_kg_m3*dx_m*dx_m*dx_m;
        if(!std::isfinite(face_mass)||face_mass<=0)
            throw std::overflow_error("Invalid discrete MAC face mass");
        const auto index=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        std::size_t body_cells=0;
        for(std::size_t i=0;i<wet.size();++i) {
            if(wet[i]>1||solid[i]>1||body[i]>1||
               (wet[i]&&solid[i])||(body[i]&&!solid[i]))
                throw std::invalid_argument("Invalid or overlapping body/liquid masks");
            body_cells+=body[i]!=0;
        }
        if(!body_cells)throw std::invalid_argument("No selected solid body");
        // Mark wet cells sharing a six-neighbour face with the body.
        std::vector<std::uint8_t> adjacent(wet.size(),0);
        constexpr int dirs[6][3]={
            {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}
        };
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(body[index(x,y,z)]) {
            for(const auto& d:dirs) {
                const auto xx=static_cast<int>(x)+d[0];
                const auto yy=static_cast<int>(y)+d[1];
                const auto zz=static_cast<int>(z)+d[2];
                if(xx>=0&&yy>=0&&zz>=0&&
                   xx<static_cast<int>(nx)&&yy<static_cast<int>(ny)&&
                   zz<static_cast<int>(nz)) {
                    const auto i=index(static_cast<std::size_t>(xx),
                                       static_cast<std::size_t>(yy),
                                       static_cast<std::size_t>(zz));
                    if(wet[i])adjacent[i]=1;
                }
            }
        }
        std::array<std::vector<FaceUpdate>,3> selected;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(wet[index(x,y,z)]) {
            const auto a=index(x,y,z);
            if(x+1<nx) {
                const auto b=index(x+1,y,z);
                if(wet[b]&&(adjacent[a]||adjacent[b]))
                    selected[0].push_back({0,x+1,y,z,mac.face_velocity_x(x+1,y,z)});
            }
            if(y+1<ny) {
                const auto b=index(x,y+1,z);
                if(wet[b]&&(adjacent[a]||adjacent[b]))
                    selected[1].push_back({1,x,y+1,z,mac.face_velocity_y(x,y+1,z)});
            }
            if(z+1<nz) {
                const auto b=index(x,y,z+1);
                if(wet[b]&&(adjacent[a]||adjacent[b]))
                    selected[2].push_back({2,x,y,z+1,mac.face_velocity_z(x,y,z+1)});
            }
        }
        PressureReactionTransfer3D out;
        out.requested_Ns=fluid_reaction_Ns;
        out.angular_impulse_unapplied_Nms=fluid_angular_reaction_Nms;
        std::array<double,3> increments{{0,0,0}};
        for(std::size_t a=0;a<3;++a) {
            out.face_count[a]=selected[a].size();
            if(selected[a].empty()) {
                if(fluid_reaction_Ns[a]!=0)
                    throw std::invalid_argument("No valid wet-wet MAC faces for impulse component");
                continue;
            }
            increments[a]=fluid_reaction_Ns[a]/
                          (face_mass*static_cast<double>(selected[a].size()));
            if(!std::isfinite(increments[a]))
                throw std::overflow_error("Reaction face velocity increment overflow");
            for(const auto& face:selected[a]) {
                const double after=face.velocity+increments[a];
                const double delta_energy=face_mass*increments[a]*
                                          (face.velocity+0.5*increments[a]);
                if(!std::isfinite(after)||!std::isfinite(delta_energy))
                    throw std::overflow_error("Reaction face energy/velocity overflow");
                out.face_kinetic_energy_delta_proxy_J+=delta_energy;
                if(!std::isfinite(out.face_kinetic_energy_delta_proxy_J))
                    throw std::overflow_error("Reaction energy sum overflow");
            }
            out.applied_face_impulse_Ns[a]=
                face_mass*static_cast<double>(selected[a].size())*increments[a];
            if(!std::isfinite(out.applied_face_impulse_Ns[a]))
                throw std::overflow_error("Reaction face momentum overflow");
        }
        // Commit only after all dimensions, masks, counts and numerics pass.
        for(std::size_t a=0;a<3;++a)
            for(const auto& face:selected[a]) {
                const double value=face.velocity+increments[a];
                if(a==0)mac.set_face_velocity_x(face.x,face.y,face.z,value);
                else if(a==1)mac.set_face_velocity_y(face.x,face.y,face.z,value);
                else mac.set_face_velocity_z(face.x,face.y,face.z,value);
            }
        return out;
    }
};
} // namespace upc
