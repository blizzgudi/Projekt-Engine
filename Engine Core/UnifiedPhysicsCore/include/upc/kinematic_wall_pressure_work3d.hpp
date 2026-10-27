#pragma once
// P2 PARTIAL: read-only pressure traction and instantaneous wall work on a
// prescribed-velocity body with a FROZEN voxel mask. Pressure is supplied by
// KinematicWallMacProjection3D::project (NOT MacProjection3D::pressure_pa).
// This is a diagnostic, NOT an applied fluid reaction or moving-geometry FSI.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct KinematicWallPressureWork3D {
    std::array<double,3> force_N{{0,0,0}};
    std::array<double,3> torque_Nm{{0,0,0}};
    std::array<double,3> body_impulse_Ns{{0,0,0}};
    std::array<double,3> body_angular_impulse_Nms{{0,0,0}};
    double pressure_power_W=0;
    double pressure_work_J=0;
    std::size_t wetted_faces=0;
    double wetted_area_m2=0;
};

class KinematicWallPressureProbe3D {
    static bool finite3(const std::array<double,3>& a) {
        return std::isfinite(a[0])&&std::isfinite(a[1])&&std::isfinite(a[2]);
    }
public:
    // Pressure traction on body: -p * n_solid_to_water * dx^2.
    // Power is the sum of local face traction dot rigid face velocity;
    // equivalently F dot linear_velocity + torque dot angular_velocity.
    // dt converts instantaneous pressure power into frozen-step work only.
    static KinematicWallPressureWork3D measure(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,double dt_s,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::vector<std::uint8_t>& body,
        const std::vector<double>& pressure_pa,
        const std::array<double,3>& centre_m,
        const std::array<double,3>& linear_velocity_m_s,
        const std::array<double,3>& angular_velocity_rad_s) {
        if(nx<2||ny<2||nz<2||nx>128||ny>128||nz>128||
           nx*ny*nz>2000000||wet.size()!=nx*ny*nz||
           solid.size()!=nx*ny*nz||body.size()!=nx*ny*nz||
           pressure_pa.size()!=nx*ny*nz||
           !std::isfinite(dx_m)||dx_m<=0||
           !std::isfinite(dt_s)||dt_s<=0||
           !finite3(centre_m)||!finite3(linear_velocity_m_s)||
           !finite3(angular_velocity_rad_s))
            throw std::invalid_argument("Invalid kinematic pressure-work input");
        const double area=dx_m*dx_m;
        if(!std::isfinite(area)||area<=0)
            throw std::overflow_error("Kinematic pressure-work face area overflow");
        std::size_t selected=0;
        for(std::size_t i=0;i<wet.size();++i) {
            if(wet[i]>1||solid[i]>1||body[i]>1||
               (wet[i]&&solid[i])||(body[i]&&!solid[i])||
               !std::isfinite(pressure_pa[i]))
                throw std::invalid_argument("Invalid pressure-work masks or pressure");
            selected+=body[i]!=0;
        }
        if(!selected)
            throw std::invalid_argument("Kinematic pressure-work requires a body");
        const auto idx=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        constexpr int directions[6][3]={
            {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}
        };
        KinematicWallPressureWork3D result;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)if(body[idx(x,y,z)]) {
            for(const auto& d:directions) {
                const int xx=static_cast<int>(x)+d[0];
                const int yy=static_cast<int>(y)+d[1];
                const int zz=static_cast<int>(z)+d[2];
                if(xx<0||yy<0||zz<0||xx>=static_cast<int>(nx)||
                   yy>=static_cast<int>(ny)||zz>=static_cast<int>(nz))
                    continue;
                const auto ni=idx(static_cast<std::size_t>(xx),
                                  static_cast<std::size_t>(yy),
                                  static_cast<std::size_t>(zz));
                if(!wet[ni])continue;
                const double p=pressure_pa[ni];
                const std::array<double,3> force{{
                    -p*d[0]*area,-p*d[1]*area,-p*d[2]*area
                }};
                const std::array<double,3> r{{
                    (static_cast<double>(x)+0.5+0.5*d[0])*dx_m-centre_m[0],
                    (static_cast<double>(y)+0.5+0.5*d[1])*dx_m-centre_m[1],
                    (static_cast<double>(z)+0.5+0.5*d[2])*dx_m-centre_m[2]
                }};
                const std::array<double,3> moment{{
                    r[1]*force[2]-r[2]*force[1],
                    r[2]*force[0]-r[0]*force[2],
                    r[0]*force[1]-r[1]*force[0]
                }};
                for(std::size_t a=0;a<3;++a) {
                    result.force_N[a]+=force[a];
                    result.torque_Nm[a]+=moment[a];
                }
                // The face-wise pressure power is evaluated at the prescribed
                // rigid velocity, not at the updated MAC cell/face velocity.
                const std::array<double,3> wall_velocity{{
                    linear_velocity_m_s[0]+
                        angular_velocity_rad_s[1]*r[2]-angular_velocity_rad_s[2]*r[1],
                    linear_velocity_m_s[1]+
                        angular_velocity_rad_s[2]*r[0]-angular_velocity_rad_s[0]*r[2],
                    linear_velocity_m_s[2]+
                        angular_velocity_rad_s[0]*r[1]-angular_velocity_rad_s[1]*r[0]
                }};
                for(std::size_t a=0;a<3;++a)
                    result.pressure_power_W+=force[a]*wall_velocity[a];
                ++result.wetted_faces;
            }
        }
        if(!result.wetted_faces)
            throw std::invalid_argument("Kinematic pressure-work body has no wet faces");
        result.wetted_area_m2=static_cast<double>(result.wetted_faces)*area;
        result.pressure_work_J=result.pressure_power_W*dt_s;
        for(std::size_t a=0;a<3;++a) {
            result.body_impulse_Ns[a]=result.force_N[a]*dt_s;
            result.body_angular_impulse_Nms[a]=result.torque_Nm[a]*dt_s;
        }
        if(!finite3(result.force_N)||!finite3(result.torque_Nm)||
           !finite3(result.body_impulse_Ns)||
           !finite3(result.body_angular_impulse_Nms)||
           !std::isfinite(result.wetted_area_m2)||
           !std::isfinite(result.pressure_power_W)||
           !std::isfinite(result.pressure_work_J))
            throw std::overflow_error("Kinematic pressure-work diagnostic overflow");
        return result;
    }
};
} // namespace upc
