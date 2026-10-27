#pragma once
// P2 partial: frozen-pressure rigid-body impulse response.
// This integrates a pressure wrench already measured on a static voxel body.
// It does NOT update the fluid, move the occupancy mask, rotate the body frame,
// or implement bidirectional FSI. The opposite fluid impulse is a diagnostic
// bookkeeping obligation, NOT an impulse applied to the MAC velocity field.
#include "upc/static_pressure_load3d.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace upc {

struct RigidPressureState3D {
    double mass_kg=1.0;
    std::array<double,3> inertia_world_kg_m2{{1.0,1.0,1.0}};
    std::array<double,3> centre_m{{0.0,0.0,0.0}};
    std::array<double,3> velocity_m_s{{0.0,0.0,0.0}};
    std::array<double,3> angular_velocity_rad_s{{0.0,0.0,0.0}};
};

struct RigidPressureImpulse3D {
    RigidPressureState3D after;
    std::array<double,3> body_linear_impulse_Ns{{0.0,0.0,0.0}};
    std::array<double,3> body_angular_impulse_Nms{{0.0,0.0,0.0}};
    std::array<double,3> fluid_reaction_impulse_Ns{{0.0,0.0,0.0}};
    std::array<double,3> fluid_reaction_angular_impulse_Nms{{0.0,0.0,0.0}};
    double kinetic_energy_change_J=0.0;
    double frozen_pressure_work_J=0.0;
    double energy_residual_J=0.0;
};

class RigidPressureResponse3D {
    static bool finite3(const std::array<double,3>& v) {
        return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);
    }
public:
    // The pressure torque MUST be measured about before.centre_m.
    // Inertia is a fixed diagonal WORLD-frame tensor; rotation/orientation
    // is deliberately not integrated. One constant-wrench interval only.
    static RigidPressureImpulse3D step(
        const RigidPressureState3D& before,
        const StaticSolidBodyPressureLoad3D& pressure,
        double dt_s) {
        if(!std::isfinite(dt_s)||dt_s<=0.0||
           !std::isfinite(before.mass_kg)||before.mass_kg<=0.0||
           !finite3(before.inertia_world_kg_m2)||
           !finite3(before.centre_m)||!finite3(before.velocity_m_s)||
           !finite3(before.angular_velocity_rad_s)||
           !finite3(pressure.force_N)||!finite3(pressure.torque_Nm)||
           before.inertia_world_kg_m2[0]<=0.0||
           before.inertia_world_kg_m2[1]<=0.0||
           before.inertia_world_kg_m2[2]<=0.0)
            throw std::invalid_argument("Invalid frozen-pressure rigid-body input");
        RigidPressureImpulse3D result;
        result.after=before;
        for(std::size_t a=0;a<3;++a) {
            const double impulse=pressure.force_N[a]*dt_s;
            const double angular_impulse=pressure.torque_Nm[a]*dt_s;
            const double new_v=before.velocity_m_s[a]+impulse/before.mass_kg;
            const double new_omega=before.angular_velocity_rad_s[a]+
                angular_impulse/before.inertia_world_kg_m2[a];
            const double displacement=0.5*dt_s*
                (before.velocity_m_s[a]+new_v);
            const double angular_displacement=0.5*dt_s*
                (before.angular_velocity_rad_s[a]+new_omega);
            result.after.velocity_m_s[a]=new_v;
            result.after.angular_velocity_rad_s[a]=new_omega;
            result.after.centre_m[a]=before.centre_m[a]+displacement;
            result.body_linear_impulse_Ns[a]=impulse;
            result.body_angular_impulse_Nms[a]=angular_impulse;
            result.fluid_reaction_impulse_Ns[a]=-impulse;
            result.fluid_reaction_angular_impulse_Nms[a]=-angular_impulse;
            result.frozen_pressure_work_J+=pressure.force_N[a]*displacement+
                pressure.torque_Nm[a]*angular_displacement;
            result.kinetic_energy_change_J+=
                0.5*before.mass_kg*(new_v*new_v-
                                    before.velocity_m_s[a]*before.velocity_m_s[a])+
                0.5*before.inertia_world_kg_m2[a]*
                    (new_omega*new_omega-
                     before.angular_velocity_rad_s[a]*before.angular_velocity_rad_s[a]);
        }
        result.energy_residual_J=result.kinetic_energy_change_J-
            result.frozen_pressure_work_J;
        if(!finite3(result.after.centre_m)||
           !finite3(result.after.velocity_m_s)||
           !finite3(result.after.angular_velocity_rad_s)||
           !finite3(result.body_linear_impulse_Ns)||
           !finite3(result.body_angular_impulse_Nms)||
           !std::isfinite(result.kinetic_energy_change_J)||
           !std::isfinite(result.frozen_pressure_work_J)||
           !std::isfinite(result.energy_residual_J))
            throw std::overflow_error("Frozen-pressure rigid-body response overflow");
        return result;
    }
};
} // namespace upc
