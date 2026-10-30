#pragma once
// P2 PARTIAL: reference-point transformation of discrete pressure traction.
// Equal/opposite impulses are DIAGNOSTIC ONLY, not applied to MAC fluid or body.
#include "upc/p2_moving_box_pressure_traction3d.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace upc {
struct P2PressureWrenchReferenceResult3D {
    P2MovingBoxPressureTractionResult3D traction;
    std::array<double,3> torque_about_reference_Nm{};
    std::array<double,3> body_linear_impulse_Ns{};
    std::array<double,3> body_angular_impulse_Nms{};
    std::array<double,3> fluid_linear_counterimpulse_Ns{};
    std::array<double,3> fluid_angular_counterimpulse_Nms{};
};

class P2PressureWrenchReference3D {
public:
    static P2PressureWrenchReferenceResult3D audit(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,const P2Box3D& old_box,
        const P2Box3D& new_box,const std::vector<double>& pressure_pa,
        const std::array<double,3>& reference_m) {
        for(double v:reference_m)
            if(!std::isfinite(v))
                throw std::invalid_argument("Nonfinite wrench reference point");
        P2PressureWrenchReferenceResult3D r;
        r.traction=P2MovingBoxPressureTraction3D::audit(
            nx,ny,nz,dx,dt,old_box,new_box,pressure_pa);
        std::array<double,3> arm{};
        for(int a=0;a<3;++a) {
            const double old_c=0.5*(old_box.min_m[a]+old_box.max_m[a]);
            const double new_c=0.5*(new_box.min_m[a]+new_box.max_m[a]);
            arm[a]=0.5*(old_c+new_c)-reference_m[a];
        }
        const auto& f=r.traction.solid_force_n;
        const std::array<double,3> cross{{
            arm[1]*f[2]-arm[2]*f[1],
            arm[2]*f[0]-arm[0]*f[2],
            arm[0]*f[1]-arm[1]*f[0]}};
        for(int a=0;a<3;++a) {
            r.torque_about_reference_Nm[a]=r.traction.solid_torque_nm[a]+cross[a];
            r.body_linear_impulse_Ns[a]=dt*f[a];
            r.body_angular_impulse_Nms[a]=dt*r.torque_about_reference_Nm[a];
            r.fluid_linear_counterimpulse_Ns[a]=-r.body_linear_impulse_Ns[a];
            r.fluid_angular_counterimpulse_Nms[a]=-r.body_angular_impulse_Nms[a];
            if(!std::isfinite(r.torque_about_reference_Nm[a]) ||
               !std::isfinite(r.body_linear_impulse_Ns[a]) ||
               !std::isfinite(r.body_angular_impulse_Nms[a]))
                throw std::overflow_error("Nonfinite pressure wrench impulse");
        }
        return r;
    }
};
} // namespace upc
