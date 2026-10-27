#pragma once
// P2 PARTIAL: lagged pressure -> rigid-body predictor using the REAL
// frozen-mask kinematic 3D MAC projection. The body state changes, but the
// solid occupancy mask and fluid impulse do NOT. NOT bidirectional FSI.
#include "upc/kinematic_wall_projection3d.hpp"
#include "upc/kinematic_wall_pressure_work3d.hpp"
#include "upc/rigid_pressure_response3d.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace upc {

struct LaggedKinematicPressureBodyResult3D {
    KinematicWallProjectionResult3D projection;
    KinematicWallPressureWork3D wall;
    RigidPressureImpulse3D body;
    // Midpoint rigid kinetic work minus instantaneous OLD-velocity wall work.
    // This positive impulse-squared term is NOT a fluid energy correction.
    double lagged_work_gap_J=0.0;
};

class LaggedKinematicPressureBody3D {
public:
    // Project prescribed OLD rigid-wall velocities, sample pressure traction,
    // integrate ONE constant-pressure rigid-body interval. A trial MAC copy
    // prevents publication of a projected fluid field if later body integration
    // fails. The returned body state is a predictor only: the voxel mask stays
    // fixed and no opposite impulse is written to fluid faces.
    static LaggedKinematicPressureBodyResult3D step(
        MacProjection3D& mac,
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double rho_kg_m3,double dt_s,
        const std::vector<std::uint8_t>& wet,
        const std::vector<std::uint8_t>& solid,
        const std::vector<std::uint8_t>& body_mask,
        const RigidPressureState3D& before,
        int max_iterations=5000) {
        if(!std::isfinite(before.mass_kg)||before.mass_kg<=0 ||
           !std::isfinite(dt_s)||dt_s<=0)
            throw std::invalid_argument("Invalid lagged rigid-body mass or dt");
        for(std::size_t a=0;a<3;++a) {
            if(!std::isfinite(before.inertia_world_kg_m2[a])||
               before.inertia_world_kg_m2[a]<=0||
               !std::isfinite(before.centre_m[a])||
               !std::isfinite(before.velocity_m_s[a])||
               !std::isfinite(before.angular_velocity_rad_s[a]))
                throw std::invalid_argument("Invalid lagged rigid-body state");
        }
        MacProjection3D trial=mac;
        LaggedKinematicPressureBodyResult3D result;
        result.projection=KinematicWallMacProjection3D::project(
            trial,nx,ny,nz,dx_m,rho_kg_m3,dt_s,
            wet,solid,body_mask,before.centre_m,
            before.velocity_m_s,before.angular_velocity_rad_s,max_iterations);
        result.wall=KinematicWallPressureProbe3D::measure(
            nx,ny,nz,dx_m,dt_s,wet,solid,body_mask,
            result.projection.pressure_pa,before.centre_m,
            before.velocity_m_s,before.angular_velocity_rad_s);
        StaticSolidBodyPressureLoad3D load;
        load.force_N=result.wall.force_N;
        load.torque_Nm=result.wall.torque_Nm;
        load.wetted_faces=result.wall.wetted_faces;
        load.wetted_area_m2=result.wall.wetted_area_m2;
        result.body=RigidPressureResponse3D::step(before,load,dt_s);
        result.lagged_work_gap_J=result.body.kinetic_energy_change_J-
            result.wall.pressure_work_J;
        if(!std::isfinite(result.lagged_work_gap_J))
            throw std::overflow_error("Nonfinite lagged pressure work gap");
        // All potentially throwing solver and integration work is complete.
        mac=std::move(trial);
        return result;
    }
};
} // namespace upc
