#pragma once
// P2 PARTIAL: apply a measured, prescribed-box pressure wrench to a frozen
// diagonal-inertia rigid-body ODE. NO MAC feedback, rotation or geometry update.
#include "upc/p2_pressure_wrench_reference3d.hpp"
#include "upc/rigid_pressure_response3d.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace upc {

struct P2FrozenWrenchRigidResult3D {
    P2PressureWrenchReferenceResult3D measured;
    StaticSolidBodyPressureLoad3D load_about_body_centre;
    RigidPressureImpulse3D response;
    double prescribed_translation_mismatch_m=0.0;
};

class P2FrozenWrenchRigidStep3D {
public:
    static P2FrozenWrenchRigidResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,const P2Box3D& old_box,const P2Box3D& prescribed_new_box,
        const std::vector<double>& pressure_pa,
        const std::array<double,3>& reference_m,
        const RigidPressureState3D& before) {
        // The pressure is sampled on the prescribed midpoint box; this does
        // not solve the coupled body trajectory or rotate its collision mask.
        if(!std::isfinite(dx)||dx<=0.0)
            throw std::invalid_argument("Invalid frozen wrench cell size");
        for(int a=0;a<3;++a) {
            const double c=0.5*(old_box.min_m[a]+old_box.max_m[a]);
            if(!std::isfinite(c)||!std::isfinite(before.centre_m[a])||
               std::abs(c-before.centre_m[a])>1e-10*dx)
                throw std::invalid_argument("Body centre must match old pressure box");
        }
        P2FrozenWrenchRigidResult3D r;
        r.measured=P2PressureWrenchReference3D::audit(
            nx,ny,nz,dx,dt,old_box,prescribed_new_box,pressure_pa,reference_m);
        const auto& f=r.measured.traction.solid_force_n;
        const auto& m=r.measured.torque_about_reference_Nm;
        r.load_about_body_centre.force_N=f;
        const std::array<double,3> arm{{
            reference_m[0]-before.centre_m[0],
            reference_m[1]-before.centre_m[1],
            reference_m[2]-before.centre_m[2]}};
        const std::array<double,3> cross{{
            arm[1]*f[2]-arm[2]*f[1],
            arm[2]*f[0]-arm[0]*f[2],
            arm[0]*f[1]-arm[1]*f[0]}};
        double error_sq=0.0;
        for(int a=0;a<3;++a) {
            r.load_about_body_centre.torque_Nm[a]=m[a]+cross[a];
            if(!std::isfinite(r.load_about_body_centre.torque_Nm[a]))
                throw std::overflow_error("Nonfinite body-centred torque");
        }
        r.response=RigidPressureResponse3D::step(
            before,r.load_about_body_centre,dt);
        for(int a=0;a<3;++a) {
            const double prescribed_c=0.5*(
                prescribed_new_box.min_m[a]+prescribed_new_box.max_m[a]);
            const double delta=r.response.after.centre_m[a]-prescribed_c;
            error_sq+=delta*delta;
        }
        r.prescribed_translation_mismatch_m=std::sqrt(error_sq);
        if(!std::isfinite(r.prescribed_translation_mismatch_m))
            throw std::overflow_error("Nonfinite prescribed translation mismatch");
        return r;
    }
};
} // namespace upc
