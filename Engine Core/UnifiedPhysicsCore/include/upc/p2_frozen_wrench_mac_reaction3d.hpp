#pragma once
// P2 PARTIAL: apply a frozen, pressure-derived 6-DOF rigid-body wrench to
// stationary-mask wet/wet MAC faces. Constrains BOTH discrete linear and
// angular face impulse, unlike the older linear-only reaction adapter.
// Face masses rho*dx^3 are a staggered-grid PROXY, not physical FSI control
// volumes. No moving/rotating geometry, incompressibility re-projection,
// free-surface stress, or globally conserved fluid+solid energy is claimed.
#include "upc/p2_frozen_wrench_rigid_step3d.hpp"
#include "upc/mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {

struct P2FrozenWrenchMacReactionResult3D {
    P2FrozenWrenchRigidResult3D rigid;
    std::array<std::size_t,3> selected_faces{};
    std::array<double,3> applied_fluid_linear_Ns{};
    std::array<double,3> applied_fluid_angular_Nms{};
    std::array<double,3> body_plus_fluid_linear_residual_Ns{};
    std::array<double,3> body_plus_fluid_angular_residual_Nms{};
    double fluid_face_energy_delta_proxy_J=0.0;
    double max_abs_face_delta_m_s=0.0;
    double max_divergence_after_s_inv=0.0;
};

class P2FrozenWrenchMacReaction3D {
    using V3=std::array<double,3>;
    using V6=std::array<double,6>;
    using M6=std::array<V6,6>;
    struct Face {
        std::size_t axis,x,y,z;
        double velocity;
        V6 column;
        double updated_velocity=0.0;
    };
    static bool finite3(const V3& v) {
        return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
    }
    // Solve the 6x6 minimum-kinetic-norm normal equations, normalized by
    // each diagonal to avoid mixing linear and angular units in pivot tests.
    // A zero/near-zero pivot is a genuine missing wrench DOF: reject rather
    // than silently dropping an angular counterimpulse.
    static V6 solve(const M6& gram,const V6& rhs) {
        V6 scales{};
        M6 h{};
        V6 y{};
        for(std::size_t i=0;i<6;++i) {
            if(!std::isfinite(gram[i][i])||gram[i][i]<=0.0)
                throw std::invalid_argument("No independent MAC wrench DOFs");
            scales[i]=std::sqrt(gram[i][i]);
        }
        for(std::size_t i=0;i<6;++i) {
            y[i]=rhs[i]/scales[i];
            for(std::size_t j=0;j<6;++j) {
                h[i][j]=gram[i][j]/scales[i]/scales[j];
                if(!std::isfinite(h[i][j]))
                    throw std::overflow_error("Nonfinite MAC wrench matrix");
            }
        }
        for(std::size_t k=0;k<6;++k) {
            std::size_t pivot=k;
            for(std::size_t i=k+1;i<6;++i)
                if(std::abs(h[i][k])>std::abs(h[pivot][k]))pivot=i;
            if(std::abs(h[pivot][k])<1e-11)
                throw std::invalid_argument("Rank-deficient MAC wrench transfer");
            if(pivot!=k) {std::swap(h[k],h[pivot]);std::swap(y[k],y[pivot]);}
            for(std::size_t i=k+1;i<6;++i) {
                const double ratio=h[i][k]/h[k][k];
                for(std::size_t j=k;j<6;++j)h[i][j]-=ratio*h[k][j];
                y[i]-=ratio*y[k];
            }
        }
        V6 solution{};
        for(int i=5;i>=0;--i) {
            double value=y[static_cast<std::size_t>(i)];
            for(int j=i+1;j<6;++j)
                value-=h[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]*
                       solution[static_cast<std::size_t>(j)];
            solution[static_cast<std::size_t>(i)]=
                value/h[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)];
        }
        for(std::size_t i=0;i<6;++i) {
            solution[i]/=scales[i];
            if(!std::isfinite(solution[i]))
                throw std::overflow_error("Nonfinite MAC wrench multiplier");
        }
        return solution;
    }
public:
    // MacProjection3D must have the supplied dimensions/dx/rho and masks.
    // All mutations are delayed until every face update passes validation.
    // Angular impulses are taken about the OLD rigid-body centre.
    static P2FrozenWrenchMacReactionResult3D step(
        MacProjection3D& mac,
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& old_box,const P2Box3D& prescribed_box,
        const std::vector<double>& pressure_pa,const V3& reference_m,
        const RigidPressureState3D& before,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::vector<std::uint8_t>& body) {
        if(nx<3||ny<3||nz<3||
           nx>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           ny>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nz>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           !std::isfinite(dx)||dx<=0||!std::isfinite(rho)||rho<=0||
           wet.size()!=nx*ny*nz||solid.size()!=nx*ny*nz||
           body.size()!=nx*ny*nz||!finite3(before.centre_m))
            throw std::invalid_argument("Invalid frozen wrench MAC grid");
        const double face_mass=rho*dx*dx*dx;
        if(!std::isfinite(face_mass)||face_mass<=0)
            throw std::overflow_error("Invalid MAC face mass");
        std::size_t body_count=0;
        for(std::size_t i=0;i<wet.size();++i) {
            if(wet[i]>1||solid[i]>1||body[i]>1||
               (wet[i]&&solid[i])||(body[i]&&!solid[i]))
                throw std::invalid_argument("Invalid frozen wrench MAC masks");
            body_count+=body[i]!=0;
        }
        if(body_count==0)throw std::invalid_argument("Missing solid body mask");
        P2FrozenWrenchMacReactionResult3D out;
        out.rigid=P2FrozenWrenchRigidStep3D::step(
            nx,ny,nz,dx,dt,old_box,prescribed_box,pressure_pa,
            reference_m,before);
        V6 target{};
        for(std::size_t a=0;a<3;++a) {
            target[a]=out.rigid.response.fluid_reaction_impulse_Ns[a];
            target[a+3]=out.rigid.response.fluid_reaction_angular_impulse_Nms[a];
        }
        const auto index=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        std::vector<std::uint8_t> adjacent(wet.size(),0);
        constexpr int dirs[6][3]={{-1,0,0},{1,0,0},{0,-1,0},
                                   {0,1,0},{0,0,-1},{0,0,1}};
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(body[index(x,y,z)]) {
            for(const auto& d:dirs) {
                const int xx=static_cast<int>(x)+d[0];
                const int yy=static_cast<int>(y)+d[1];
                const int zz=static_cast<int>(z)+d[2];
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
        std::vector<Face> faces;
        M6 gram{};
        const auto append=[&](std::size_t axis,std::size_t x,
                              std::size_t y,std::size_t z,double v) {
            if(!std::isfinite(v))
                throw std::invalid_argument("Nonfinite MAC face velocity");
            V3 r{{(static_cast<double>(x)+(axis!=0?0.5:0.0))*dx-before.centre_m[0],
                  (static_cast<double>(y)+(axis!=1?0.5:0.0))*dx-before.centre_m[1],
                  (static_cast<double>(z)+(axis!=2?0.5:0.0))*dx-before.centre_m[2]}};
            if(!finite3(r))throw std::overflow_error("Nonfinite MAC moment arm");
            V6 col{};
            col[axis]=1.0;
            // r cross e_axis
            col[3+(axis+1)%3]=r[(axis+2)%3];
            col[3+(axis+2)%3]=-r[(axis+1)%3];
            faces.push_back({axis,x,y,z,v,col,0.0});
            ++out.selected_faces[axis];
            for(std::size_t i=0;i<6;++i)
                for(std::size_t j=0;j<6;++j)
                    gram[i][j]+=face_mass*col[i]*col[j];
        };
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(wet[index(x,y,z)]) {
            const auto a=index(x,y,z);
            if(x+1<nx) {
                const auto b=index(x+1,y,z);
                if(wet[b]&&(adjacent[a]||adjacent[b]))
                    append(0,x+1,y,z,mac.face_velocity_x(x+1,y,z));
            }
            if(y+1<ny) {
                const auto b=index(x,y+1,z);
                if(wet[b]&&(adjacent[a]||adjacent[b]))
                    append(1,x,y+1,z,mac.face_velocity_y(x,y+1,z));
            }
            if(z+1<nz) {
                const auto b=index(x,y,z+1);
                if(wet[b]&&(adjacent[a]||adjacent[b]))
                    append(2,x,y,z+1,mac.face_velocity_z(x,y,z+1));
            }
        }
        V6 multiplier{};
        if(std::any_of(target.begin(),target.end(),
                       [](double x){return x!=0.0;}))
            multiplier=solve(gram,target);
        V6 applied{};
        for(auto& f:faces) {
            double dv=0;
            for(std::size_t i=0;i<6;++i)
                dv+=f.column[i]*multiplier[i];
            f.updated_velocity=f.velocity+dv;
            const double impulse=face_mass*dv;
            const double dE=face_mass*dv*(f.velocity+0.5*dv);
            if(!std::isfinite(f.updated_velocity)||!std::isfinite(dE))
                throw std::overflow_error("Nonfinite MAC wrench face update");
            for(std::size_t i=0;i<6;++i)applied[i]+=f.column[i]*impulse;
            out.fluid_face_energy_delta_proxy_J+=dE;
            out.max_abs_face_delta_m_s=std::max(out.max_abs_face_delta_m_s,
                                               std::abs(dv));
        }
        for(std::size_t i=0;i<6;++i)
            if(!std::isfinite(applied[i])||
               !std::isfinite(out.fluid_face_energy_delta_proxy_J)||
               !std::isfinite(out.max_abs_face_delta_m_s))
                throw std::overflow_error("Nonfinite MAC wrench audit");
        // Transactional commit: stage even the divergence audit on a copy.
        // A late nonfinite-divergence failure cannot leave a partial MAC write.
        MacProjection3D candidate=mac;
        for(const auto& f:faces) {
            if(f.axis==0)candidate.set_face_velocity_x(f.x,f.y,f.z,f.updated_velocity);
            else if(f.axis==1)candidate.set_face_velocity_y(f.x,f.y,f.z,f.updated_velocity);
            else candidate.set_face_velocity_z(f.x,f.y,f.z,f.updated_velocity);
        }
        out.max_divergence_after_s_inv=candidate.max_divergence();
        if(!std::isfinite(out.max_divergence_after_s_inv))
            throw std::overflow_error("Nonfinite post-transfer divergence");
        for(std::size_t a=0;a<3;++a) {
            out.applied_fluid_linear_Ns[a]=applied[a];
            out.applied_fluid_angular_Nms[a]=applied[a+3];
            out.body_plus_fluid_linear_residual_Ns[a]=
                out.rigid.response.body_linear_impulse_Ns[a]+applied[a];
            out.body_plus_fluid_angular_residual_Nms[a]=
                out.rigid.response.body_angular_impulse_Nms[a]+applied[a+3];
        }
        mac=std::move(candidate);
        return out;
    }
};
} // namespace upc
