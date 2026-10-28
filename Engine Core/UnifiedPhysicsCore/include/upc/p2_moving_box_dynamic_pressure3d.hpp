#pragma once
// P2 PARTIAL: explicit frozen-pressure translation of a rigid box.
// Applies pressure impulse to a dynamic body, but the equal/opposite fluid
// impulse is ONLY a ledger: no MAC momentum update or two-way FSI.
#include "upc/p2_moving_box_pressure_traction3d.hpp"
#include "upc/rigid_pressure_response3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MovingBoxDynamicPressureResult3D {
    P2Box3D box_after;
    RigidPressureImpulse3D rigid;
    P2MovingBoxPressureTractionResult3D frozen_pressure;
    P2MovingBoxPressureTractionResult3D swept_pressure;
    std::array<double,3> diagnostic_total_impulse_residual_Ns{};
    std::array<double,3> force_sampling_change_N{};
    double rigid_vs_geometric_pressure_work_J=0.0;
};

class P2MovingBoxDynamicPressure3D {
public:
    // Only nonrotating, axis-aligned translation is supported. The pressure
    // field is frozen for one step, sampled at the OLD box. The body's
    // midpoint displacement is then audited against the swept-cell geometry.
    // A torque-bearing load is rejected rather than silently rotating an
    // axis-aligned collision box without updating its orientation.
    static P2MovingBoxDynamicPressureResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,const P2Box3D& old_box,
        const RigidPressureState3D& before,
        const std::vector<double>& pressure_pa) {
        for(int a=0;a<3;++a) {
            const double centre=0.5*(old_box.min_m[a]+old_box.max_m[a]);
            if(!std::isfinite(centre) ||
               !std::isfinite(before.centre_m[a]) ||
               std::abs(before.centre_m[a]-centre)>
                   1e-10*std::max(1.0,std::abs(centre)) ||
               !std::isfinite(before.angular_velocity_rad_s[a]) ||
               std::abs(before.angular_velocity_rad_s[a])>1e-14)
                throw std::invalid_argument(
                    "Dynamic pressure box requires centred nonrotating state");
        }
        P2MovingBoxDynamicPressureResult3D result;
        result.frozen_pressure=P2MovingBoxPressureTraction3D::audit(
            nx,ny,nz,dx,dt,old_box,old_box,pressure_pa);
        StaticSolidBodyPressureLoad3D load;
        load.force_N=result.frozen_pressure.solid_force_n;
        load.torque_Nm=result.frozen_pressure.solid_torque_nm;
        for(int a=0;a<3;++a) {
            const double tolerance=1e-10*std::max(
                1.0,std::abs(load.force_N[a])*dx);
            if(std::abs(load.torque_Nm[a])>tolerance)
                throw std::invalid_argument(
                    "Torque requires oriented rigid-body geometry");
        }
        result.rigid=RigidPressureResponse3D::step(before,load,dt);
        result.box_after=old_box;
        for(int a=0;a<3;++a) {
            const double displacement=
                result.rigid.after.centre_m[a]-before.centre_m[a];
            result.box_after.min_m[a]+=displacement;
            result.box_after.max_m[a]+=displacement;
        }
        // Validates domain, swept volumes and geometric CFL transactionally.
        result.swept_pressure=P2MovingBoxPressureTraction3D::audit(
            nx,ny,nz,dx,dt,old_box,result.box_after,pressure_pa);
        for(int a=0;a<3;++a) {
            result.diagnostic_total_impulse_residual_Ns[a]=
                result.rigid.body_linear_impulse_Ns[a]+
                result.rigid.fluid_reaction_impulse_Ns[a];
            result.force_sampling_change_N[a]=
                result.swept_pressure.solid_force_n[a]-
                result.frozen_pressure.solid_force_n[a];
        }
        result.rigid_vs_geometric_pressure_work_J=
            result.rigid.frozen_pressure_work_J-
            result.swept_pressure.solid_geometric_pressure_work_j;
        if(!std::isfinite(result.rigid_vs_geometric_pressure_work_J))
            throw std::overflow_error("Dynamic pressure work overflow");
        for(int a=0;a<3;++a)
            if(!std::isfinite(result.force_sampling_change_N[a])||
               !std::isfinite(result.diagnostic_total_impulse_residual_Ns[a]))
                throw std::overflow_error("Dynamic pressure impulse overflow");
        return result;
    }
};
} // namespace upc
