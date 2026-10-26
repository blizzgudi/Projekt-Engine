#pragma once
// P5 partial: point-mass contact against one immovable infinite plane.
// Discrete unilateral projection/impulse, NOT volumetric contact, FEM or granular dynamics.
#include "upc/solid.hpp"
#include <cmath>
#include <stdexcept>

namespace upc {
inline double solid_dot(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

struct SolidPlaneContactSettings3D {
    Vec3 normal{0,0,1};           // points into permitted half-space
    double offset_m=0;            // dot(normalized_normal, position) >= offset_m
    double restitution=0;         // 0..1; applied only above impact threshold
    double friction=0;            // Coulomb impulse coefficient, >=0
    double bounce_threshold_m_s=0.05;
};

class SolidPlaneContact3D {
    Vec3 n_;
    double offset_, restitution_, friction_, threshold_;
public:
    explicit SolidPlaneContact3D(SolidPlaneContactSettings3D settings)
        : offset_(settings.offset_m), restitution_(settings.restitution),
          friction_(settings.friction), threshold_(settings.bounce_threshold_m_s) {
        const Vec3 n=settings.normal;
        if(!std::isfinite(n.x)||!std::isfinite(n.y)||!std::isfinite(n.z)||
           !std::isfinite(offset_)||!std::isfinite(restitution_)||
           !std::isfinite(friction_)||!std::isfinite(threshold_)||
           restitution_<0||restitution_>1||friction_<0||threshold_<0)
            throw std::invalid_argument("Invalid plane contact settings");
        const double length=norm(n);
        if(!std::isfinite(length)||length<=1e-12)
            throw std::invalid_argument("Degenerate plane normal");
        n_=n/length;
    }
    Vec3 normal() const {return n_;}
    double offset_m() const {return offset_;}

    // Integrates the existing spring model, then projects penetrating free nodes.
    // Normal restitution and bounded Coulomb tangential impulse cannot increase
    // relative kinetic energy when restitution <= 1 and the plane is stationary.
    // Accuracy is time-step-dependent; no swept collision detection or multi-contact solve.
    void step(SpringSolid3D& solid, double dt) const {
        solid.step(dt);
        for(std::size_t i=0;i<solid.node_count();++i) {
            auto& node=solid.node(i);
            if(node.fixed) continue;
            if(!std::isfinite(node.position.x)||!std::isfinite(node.position.y)||
               !std::isfinite(node.position.z)||!std::isfinite(node.velocity.x)||
               !std::isfinite(node.velocity.y)||!std::isfinite(node.velocity.z))
                throw std::runtime_error("Non-finite solid contact state");
            const double gap=solid_dot(n_,node.position)-offset_;
            if(gap>0) continue;
            if(gap<0) node.position=node.position-n_*gap;
            const double vn=solid_dot(n_,node.velocity);
            if(vn>=0) continue;
            const double closing=-vn;
            const double e=closing>threshold_?restitution_:0;
            const double normal_dv=(1+e)*closing;
            node.velocity=node.velocity+n_*normal_dv;
            const Vec3 tangent=node.velocity-n_*solid_dot(n_,node.velocity);
            const double tangential_speed=norm(tangent);
            if(tangential_speed>0 && friction_>0) {
                const double reduction=std::min(tangential_speed,friction_*normal_dv);
                node.velocity=node.velocity-tangent*(reduction/tangential_speed);
            }
        }
    }
};
} // namespace upc
