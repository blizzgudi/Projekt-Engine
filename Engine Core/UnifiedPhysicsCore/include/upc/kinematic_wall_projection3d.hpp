#pragma once
// P2 PARTIAL: prescribed rigid-wall NORMAL velocities on a FROZEN voxel mask.
// This is a kinematic MAC pressure projection, not dynamic moving geometry,
// no-slip tangential viscosity, two-way body impulse, or full FSI conservation.
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

struct KinematicWallProjectionResult3D {
    std::array<std::size_t,3> moving_wall_faces{{0,0,0}};
    double net_body_wall_flux_m3_s=0;
    double max_divergence_after_s_inv=0;
    double max_abs_pressure_pa=0;
    int pressure_iterations=0;
    // Pressure is a gauge-dependent numerical field; solid/air cells are zero.
    std::vector<double> pressure_pa;
};

class KinematicWallMacProjection3D {
    static bool finite3(const std::array<double,3>& v) {
        return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
    }
public:
    // MAC grid dimensions, dx and rho MUST match mac's construction.
    // External boundaries remain impermeable. A solid/wet face gets the
    // rigid-body velocity's normal component if exactly one solid belongs
    // to body; all other solid-normal faces are stationary. Geometry is
    // fixed throughout this call. Writes to mac happen only after solving.
    static KinematicWallProjectionResult3D project(
        MacProjection3D& mac,
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double rho_kg_m3,double dt_s,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::vector<std::uint8_t>& body,
        const std::array<double,3>& centre_m,
        const std::array<double,3>& linear_velocity_m_s,
        const std::array<double,3>& angular_velocity_rad_s,
        int max_iterations=5000) {
        if(nx<4||ny<4||nz<4||nx>128||ny>128||nz>128||
           nx*ny*nz>2000000||wet.size()!=nx*ny*nz||
           solid.size()!=nx*ny*nz||body.size()!=nx*ny*nz||
           !std::isfinite(dx_m)||dx_m<=0||
           !std::isfinite(rho_kg_m3)||rho_kg_m3<=0||
           !std::isfinite(dt_s)||dt_s<=0||
           !finite3(centre_m)||!finite3(linear_velocity_m_s)||
           !finite3(angular_velocity_rad_s)||
           max_iterations<1||max_iterations>10000)
            throw std::invalid_argument("Invalid kinematic wall MAC input");
        const double scale=dt_s/(rho_kg_m3*dx_m);
        const double rhs_scale=rho_kg_m3*dx_m/dt_s;
        if(!std::isfinite(scale)||scale<=0||
           !std::isfinite(rhs_scale)||rhs_scale<=0)
            throw std::overflow_error("Kinematic projection coefficient overflow");
        std::size_t nbody=0,nwet=0;
        for(std::size_t i=0;i<wet.size();++i) {
            if(wet[i]>1||solid[i]>1||body[i]>1||
               (wet[i]&&solid[i])||(body[i]&&!solid[i]))
                throw std::invalid_argument("Invalid solid/wet/body masks");
            nbody+=body[i]!=0;
            nwet+=wet[i]!=0;
        }
        if(!nbody||!nwet)
            throw std::invalid_argument("Kinematic projection needs body and liquid");
        const auto idx=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        const auto inside=[&](int x,int y,int z){
            return x>=0&&y>=0&&z>=0&&x<static_cast<int>(nx)&&
                   y<static_cast<int>(ny)&&z<static_cast<int>(nz);
        };
        const auto index=[&](int x,int y,int z){
            return idx(static_cast<std::size_t>(x),
                       static_cast<std::size_t>(y),
                       static_cast<std::size_t>(z));
        };
        const auto wall=[&](int x,int y,int z){
            return !inside(x,y,z)||solid[index(x,y,z)]!=0;
        };
        const auto liquid=[&](int x,int y,int z){
            return inside(x,y,z)&&wet[index(x,y,z)]!=0;
        };
        const auto p=[&](int x,int y,int z,const std::vector<double>& pressure){
            return liquid(x,y,z)?pressure[index(x,y,z)]:0.0;
        };
        const auto ux=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*(nx+1)+x;
        };
        const auto vy=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*(ny+1)+y)*nx+x;
        };
        const auto wz=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        std::vector<double> u((nx+1)*ny*nz),v(nx*(ny+1)*nz),w(nx*ny*(nz+1));
        KinematicWallProjectionResult3D result;
        // Snapshot and enforce wall-normal velocities without mutating mac.
        for(std::size_t axis=0;axis<3;++axis)
        for(std::size_t z=0;z<nz+(axis==2);++z)
        for(std::size_t y=0;y<ny+(axis==1);++y)
        for(std::size_t x=0;x<nx+(axis==0);++x) {
            const double original=axis==0?mac.face_velocity_x(x,y,z):
                axis==1?mac.face_velocity_y(x,y,z):mac.face_velocity_z(x,y,z);
            if(!std::isfinite(original))
                throw std::invalid_argument("Nonfinite MAC input velocity");
            const int ax=static_cast<int>(x)-(axis==0);
            const int ay=static_cast<int>(y)-(axis==1);
            const int az=static_cast<int>(z)-(axis==2);
            const int bx=static_cast<int>(x),by=static_cast<int>(y),bz=static_cast<int>(z);
            const bool ainside=inside(ax,ay,az),binside=inside(bx,by,bz);
            const bool asolid=ainside&&solid[index(ax,ay,az)];
            const bool bsolid=binside&&solid[index(bx,by,bz)];
            const bool a_body=asolid&&body[index(ax,ay,az)];
            const bool b_body=bsolid&&body[index(bx,by,bz)];
            double value=original;
            if(!ainside||!binside||asolid||bsolid) {
                value=0;
                if(ainside&&binside&&(a_body!=b_body)&&
                   ((a_body&&liquid(bx,by,bz))||
                    (b_body&&liquid(ax,ay,az)))) {
                    const std::array<double,3> r{{
                        (static_cast<double>(x)+(axis!=0?0.5:0))*dx_m-centre_m[0],
                        (static_cast<double>(y)+(axis!=1?0.5:0))*dx_m-centre_m[1],
                        (static_cast<double>(z)+(axis!=2?0.5:0))*dx_m-centre_m[2]
                    }};
                    const auto& o=angular_velocity_rad_s;
                    const std::array<double,3> cross{{
                        o[1]*r[2]-o[2]*r[1],
                        o[2]*r[0]-o[0]*r[2],
                        o[0]*r[1]-o[1]*r[0]
                    }};
                    value=linear_velocity_m_s[axis]+cross[axis];
                    if(!std::isfinite(value))
                        throw std::overflow_error("Nonfinite rigid-wall velocity");
                    ++result.moving_wall_faces[axis];
                    result.net_body_wall_flux_m3_s+=
                        (a_body?1.0:-1.0)*value*dx_m*dx_m;
                }
            }
            if(axis==0)u[ux(x,y,z)]=value;
            else if(axis==1)v[vy(x,y,z)]=value;
            else w[wz(x,y,z)]=value;
        }
        if(!std::isfinite(result.net_body_wall_flux_m3_s))
            throw std::overflow_error("Kinematic wall flux overflow");
        // A fully enclosed liquid region cannot accommodate a nonzero
        // total volume source with impermeable external walls.
        bool has_free_surface=false;
        constexpr int directions[6][3]={{1,0,0},{-1,0,0},
            {0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        std::vector<double> rhs(nx*ny*nz,0),pressure(nx*ny*nz,0);
        double total_divergence_volume=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(wet[idx(x,y,z)]) {
            const double divergence=(
                u[ux(x+1,y,z)]-u[ux(x,y,z)]+
                v[vy(x,y+1,z)]-v[vy(x,y,z)]+
                w[wz(x,y,z+1)]-w[wz(x,y,z)])/dx_m;
            rhs[idx(x,y,z)]=rhs_scale*divergence*dx_m;
            total_divergence_volume+=divergence*dx_m*dx_m*dx_m;
            if(!std::isfinite(rhs[idx(x,y,z)])||
               !std::isfinite(total_divergence_volume))
                throw std::overflow_error("Kinematic projection RHS overflow");
            for(const auto& d:directions) {
                const int xx=static_cast<int>(x)+d[0];
                const int yy=static_cast<int>(y)+d[1];
                const int zz=static_cast<int>(z)+d[2];
                if(!wall(xx,yy,zz)&&!liquid(xx,yy,zz))
                    has_free_surface=true;
            }
        }
        if(!has_free_surface &&
           std::abs(total_divergence_volume)>
           1e-10*(1+std::abs(result.net_body_wall_flux_m3_s)))
            throw std::invalid_argument("Incompatible closed-domain moving-wall flux");
        for(int iteration=0;iteration<max_iterations;++iteration) {
            double max_delta=0;
            for(std::size_t z=0;z<nz;++z)
            for(std::size_t y=0;y<ny;++y)
            for(std::size_t x=0;x<nx;++x)if(wet[idx(x,y,z)]) {
                double sum=0;int coefficient=0;
                for(const auto& d:directions) {
                    const int xx=static_cast<int>(x)+d[0];
                    const int yy=static_cast<int>(y)+d[1];
                    const int zz=static_cast<int>(z)+d[2];
                    if(wall(xx,yy,zz))continue;
                    if(liquid(xx,yy,zz)) {
                        ++coefficient;sum+=p(xx,yy,zz,pressure);
                    }else coefficient+=2;
                }
                if(coefficient==0)continue;
                const auto i=idx(x,y,z);
                const double next=(sum-rhs[i])/coefficient;
                if(!std::isfinite(next))
                    throw std::overflow_error("Nonfinite kinematic pressure");
                max_delta=std::max(max_delta,std::abs(next-pressure[i]));
                pressure[i]=next;
            }
            result.pressure_iterations=iteration+1;
            if(max_delta<1e-10)break;
        }
        for(std::size_t axis=0;axis<3;++axis)
        for(std::size_t z=0;z<nz+(axis==2);++z)
        for(std::size_t y=0;y<ny+(axis==1);++y)
        for(std::size_t x=0;x<nx+(axis==0);++x) {
            const int ax=static_cast<int>(x)-(axis==0);
            const int ay=static_cast<int>(y)-(axis==1);
            const int az=static_cast<int>(z)-(axis==2);
            const int bx=static_cast<int>(x),by=static_cast<int>(y),bz=static_cast<int>(z);
            if(wall(ax,ay,az)||wall(bx,by,bz))continue;
            const bool a=liquid(ax,ay,az),b=liquid(bx,by,bz);
            if(!a&&!b)continue;
            const double factor=a!=b?2.0:1.0;
            const double delta=scale*factor*(p(bx,by,bz,pressure)-p(ax,ay,az,pressure));
            double* face=axis==0?&u[ux(x,y,z)]:
                         axis==1?&v[vy(x,y,z)]:&w[wz(x,y,z)];
            *face-=delta;
            if(!std::isfinite(*face))
                throw std::overflow_error("Nonfinite projected MAC face");
        }
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(wet[idx(x,y,z)]) {
            const double div=(
                u[ux(x+1,y,z)]-u[ux(x,y,z)]+
                v[vy(x,y+1,z)]-v[vy(x,y,z)]+
                w[wz(x,y,z+1)]-w[wz(x,y,z)])/dx_m;
            result.max_divergence_after_s_inv=
                std::max(result.max_divergence_after_s_inv,std::abs(div));
        }
        for(double value:pressure)
            result.max_abs_pressure_pa=std::max(result.max_abs_pressure_pa,std::abs(value));
        if(!std::isfinite(result.max_divergence_after_s_inv)||
           !std::isfinite(result.max_abs_pressure_pa))
            throw std::overflow_error("Nonfinite kinematic projection diagnostics");
        // Atomic commit of all face arrays after successful preflight/solve.
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x)mac.set_face_velocity_x(x,y,z,u[ux(x,y,z)]);
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x)mac.set_face_velocity_y(x,y,z,v[vy(x,y,z)]);
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)mac.set_face_velocity_z(x,y,z,w[wz(x,y,z)]);
        result.pressure_pa=std::move(pressure);
        return result;
    }
};
} // namespace upc
