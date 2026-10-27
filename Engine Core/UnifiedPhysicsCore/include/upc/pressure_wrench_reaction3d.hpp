#pragma once
// P2 PARTIAL: constrained, minimum-face-impulse reaction for a frozen solid.
// Both linear and angular pressure impulses are APPLIED to wet-wet MAC faces.
// This is a discrete momentum proxy BEFORE re-projection. The body mask does
// not move; this is NOT moving-wall FSI or a globally conservative fluid solve.
#include "upc/mac3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {

struct PressureWrenchReaction3D {
    std::array<double,3> applied_linear_Ns{{0,0,0}};
    std::array<double,3> applied_angular_Nms{{0,0,0}};
    std::array<std::size_t,3> selected_face_count{{0,0,0}};
    double face_energy_delta_proxy_J=0;
};

class FrozenPressureFluidWrench3D {
    struct Face {
        std::size_t axis,x,y,z;
        double old_velocity;
        std::array<double,6> column;
        double new_velocity=0;
    };
    static bool finite3(const std::array<double,3>& v) {
        return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
    }
public:
    // Each eligible wet-wet face has discrete mass rho*dx^3. The six
    // constraints are net face impulse and net torque about centre_m.
    // Torque rows are scaled by dx to avoid ill-conditioned SI units.
    // The minimum L2 face impulse is found from a 6x6 Gram solve.
    // No face is written until all checks and the solve succeed.
    static PressureWrenchReaction3D apply(
        MacProjection3D& mac,
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double rho_kg_m3,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::vector<std::uint8_t>& body,
        const std::array<double,3>& centre_m,
        const std::array<double,3>& reaction_linear_Ns,
        const std::array<double,3>& reaction_angular_Nms) {
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
           !finite3(centre_m)||!finite3(reaction_linear_Ns)||
           !finite3(reaction_angular_Nms))
            throw std::invalid_argument("Invalid pressure wrench reaction input");
        const double mass=rho_kg_m3*dx_m*dx_m*dx_m;
        if(!std::isfinite(mass)||mass<=0)
            throw std::overflow_error("Invalid face mass");
        const auto idx=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        std::size_t body_cells=0;
        for(std::size_t i=0;i<wet.size();++i) {
            if(wet[i]>1||solid[i]>1||body[i]>1||
               (wet[i]&&solid[i])||(body[i]&&!solid[i]))
                throw std::invalid_argument("Overlapping/invalid body and liquid masks");
            body_cells+=body[i]!=0;
        }
        if(!body_cells)throw std::invalid_argument("No selected solid body");
        constexpr int dirs[6][3]={{-1,0,0},{1,0,0},{0,-1,0},
                                   {0,1,0},{0,0,-1},{0,0,1}};
        std::vector<std::uint8_t> adjacent(wet.size(),0);
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(body[idx(x,y,z)]) {
            for(const auto& d:dirs) {
                const int xx=static_cast<int>(x)+d[0];
                const int yy=static_cast<int>(y)+d[1];
                const int zz=static_cast<int>(z)+d[2];
                if(xx>=0&&yy>=0&&zz>=0&&
                   xx<static_cast<int>(nx)&&yy<static_cast<int>(ny)&&
                   zz<static_cast<int>(nz)) {
                    const auto j=idx(static_cast<std::size_t>(xx),
                                     static_cast<std::size_t>(yy),
                                     static_cast<std::size_t>(zz));
                    if(wet[j])adjacent[j]=1;
                }
            }
        }
        std::vector<Face> faces;
        PressureWrenchReaction3D out;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(wet[idx(x,y,z)]) {
            for(std::size_t axis=0;axis<3;++axis) {
                const auto xx=x+(axis==0),yy=y+(axis==1),zz=z+(axis==2);
                if(xx>=nx||yy>=ny||zz>=nz||!wet[idx(xx,yy,zz)]||
                   !(adjacent[idx(x,y,z)]||adjacent[idx(xx,yy,zz)]))continue;
                const std::size_t fx=x+(axis==0),fy=y+(axis==1),fz=z+(axis==2);
                const double velocity=axis==0?mac.face_velocity_x(fx,fy,fz):
                    axis==1?mac.face_velocity_y(fx,fy,fz):
                            mac.face_velocity_z(fx,fy,fz);
                if(!std::isfinite(velocity))
                    throw std::invalid_argument("Nonfinite selected face velocity");
                const std::array<double,3> r{{
                    ((static_cast<double>(fx)+(axis!=0?0.5:0.0))*dx_m-centre_m[0])/dx_m,
                    ((static_cast<double>(fy)+(axis!=1?0.5:0.0))*dx_m-centre_m[1])/dx_m,
                    ((static_cast<double>(fz)+(axis!=2?0.5:0.0))*dx_m-centre_m[2])/dx_m
                }};
                if(!finite3(r))throw std::overflow_error("Face lever arm overflow");
                std::array<double,6> c{{0,0,0,0,0,0}};
                c[axis]=1;
                if(axis==0){c[4]=r[2];c[5]=-r[1];}
                if(axis==1){c[3]=-r[2];c[5]=r[0];}
                if(axis==2){c[3]=r[1];c[4]=-r[0];}
                faces.push_back({axis,fx,fy,fz,velocity,c,velocity});
                ++out.selected_face_count[axis];
            }
        }
        const std::array<double,6> rhs{{
            reaction_linear_Ns[0],reaction_linear_Ns[1],reaction_linear_Ns[2],
            reaction_angular_Nms[0]/dx_m,
            reaction_angular_Nms[1]/dx_m,
            reaction_angular_Nms[2]/dx_m
        }};
        for(double v:rhs)if(!std::isfinite(v))
            throw std::overflow_error("Scaled reaction wrench overflow");
        bool nonzero=false;
        for(double v:rhs)nonzero|=v!=0;
        if(!nonzero)return out;
        double aug[6][7]={{0}};
        for(const auto& face:faces)
            for(int i=0;i<6;++i)for(int j=0;j<6;++j)
                aug[i][j]+=face.column[i]*face.column[j];
        double max_diag=0;
        for(int i=0;i<6;++i) {
            aug[i][6]=rhs[i];
            if(!std::isfinite(aug[i][i]))
                throw std::overflow_error("Reaction Gram matrix overflow");
            if(aug[i][i]>max_diag)max_diag=aug[i][i];
        }
        if(max_diag<=0)throw std::invalid_argument("No eligible reaction faces");
        // Partial-pivot Gaussian elimination, rejecting underconstrained
        // geometries rather than inventing an angular impulse solution.
        for(int col=0;col<6;++col) {
            int pivot=col;
            for(int row=col+1;row<6;++row)
                if(std::abs(aug[row][col])>std::abs(aug[pivot][col]))pivot=row;
            if(!std::isfinite(aug[pivot][col])||
               std::abs(aug[pivot][col])<1e-11*max_diag)
                throw std::invalid_argument("Rank-deficient reaction wrench geometry");
            if(pivot!=col)for(int j=col;j<7;++j) {
                const double tmp=aug[col][j];aug[col][j]=aug[pivot][j];aug[pivot][j]=tmp;
            }
            for(int row=col+1;row<6;++row) {
                const double factor=aug[row][col]/aug[col][col];
                for(int j=col;j<7;++j)aug[row][j]-=factor*aug[col][j];
            }
        }
        double lambda[6]={0};
        for(int i=5;i>=0;--i) {
            double value=aug[i][6];
            for(int j=i+1;j<6;++j)value-=aug[i][j]*lambda[j];
            lambda[i]=value/aug[i][i];
            if(!std::isfinite(lambda[i]))
                throw std::overflow_error("Reaction wrench solve overflow");
        }
        std::array<double,6> applied{{0,0,0,0,0,0}};
        for(auto& face:faces) {
            double dp=0;
            for(int i=0;i<6;++i)dp+=face.column[i]*lambda[i];
            const double dv=dp/mass;
            face.new_velocity=face.old_velocity+dv;
            const double energy=0.5*mass*
                (face.new_velocity*face.new_velocity-face.old_velocity*face.old_velocity);
            if(!std::isfinite(dp)||!std::isfinite(face.new_velocity)||
               !std::isfinite(energy))
                throw std::overflow_error("Reaction face update overflow");
            for(int i=0;i<6;++i)applied[i]+=face.column[i]*dp;
            out.face_energy_delta_proxy_J+=energy;
            if(!std::isfinite(out.face_energy_delta_proxy_J))
                throw std::overflow_error("Reaction energy accumulation overflow");
        }
        for(int i=0;i<6;++i)
            if(!std::isfinite(applied[i])||
               std::abs(applied[i]-rhs[i])>1e-9*(1+std::abs(rhs[i])))
                throw std::overflow_error("Reaction wrench constraint did not close");
        for(int i=0;i<3;++i) {
            out.applied_linear_Ns[i]=applied[i];
            out.applied_angular_Nms[i]=applied[i+3]*dx_m;
        }
        // Commit after the full six-constraint preflight.
        for(const auto& face:faces) {
            if(face.axis==0)mac.set_face_velocity_x(face.x,face.y,face.z,face.new_velocity);
            else if(face.axis==1)mac.set_face_velocity_y(face.x,face.y,face.z,face.new_velocity);
            else mac.set_face_velocity_z(face.x,face.y,face.z,face.new_velocity);
        }
        return out;
    }
};
} // namespace upc
