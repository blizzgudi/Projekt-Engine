#pragma once
// P2 PARTIAL: fixed-level, quiescent hydrostatic *body-only* discrete-gradient
// integration. No MAC/VOF reaction is applied, and no global FSI claim is made.
#include "upc/p2_hydrostatic_gravity3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace upc {
struct P2HydrostaticDiscreteGradientResult3D {
    P2HydrostaticGravityResult3D frozen_reference;
    RigidPressureState3D after;
    P2Box3D box_after;
    double average_buoyancy_force_N=0.0;
    double buoyancy_reaction_unapplied_Ns=0.0;
    double gravity_impulse_Ns=0.0;
    double kinetic_energy_change_J=0.0;
    double gravity_potential_change_J=0.0;
    double buoyancy_potential_change_J=0.0;
    double mechanical_defect_J=0.0;
    std::size_t bisection_iterations=0;
};

class P2HydrostaticDiscreteGradient3D {
    // Exact integral of the piecewise-linear submerged height along a
    // vertical displacement. Splitting at both wet/dry breakpoints avoids
    // subtracting nearly equal large potentials for tiny translations.
    static double mean_wet_height(double z0,double z1,double height,
                                  double level) {
        const auto wet=[&](double z) {
            return std::clamp(level-(z-0.5*height),0.0,height);
        };
        if(z0==z1)return wet(z0);
        const double a=std::min(z0,z1),b=std::max(z0,z1);
        std::array<double,4> points{{
            a,std::clamp(level-0.5*height,a,b),
            std::clamp(level+0.5*height,a,b),b}};
        std::sort(points.begin(),points.end());
        double integral=0.0;
        for(std::size_t k=1;k<points.size();++k)
            integral+=0.5*(wet(points[k-1])+wet(points[k]))*
                      (points[k]-points[k-1]);
        return integral/(b-a);
    }
public:
    // Midpoint kinematics plus an exact potential secant. For a stationary
    // horizontal bath this closes the body's discrete mechanical ledger,
    // even while the box crosses the waterline. It does NOT close fluid energy.
    static P2HydrostaticDiscreteGradientResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,double gravity,
        const P2Box3D& box,const RigidPressureState3D& body,
        const std::vector<double>& fill) {
        P2HydrostaticDiscreteGradientResult3D out;
        // All geometry, stratification, body and CFL input validation is
        // inherited. This is deliberately an opt-in alternative.
        out.frozen_reference=P2HydrostaticGravity3D::step(
            nx,ny,nz,dx,dt,rho,gravity,box,body,fill);
        const double level=out.frozen_reference.water_level_m;
        const double height=box.max_m[2]-box.min_m[2];
        const double area=(box.max_m[0]-box.min_m[0])*
                          (box.max_m[1]-box.min_m[1]);
        const double z0=body.centre_m[2];
        const double v0=body.velocity_m_s[2];
        const double m=body.mass_kg;
        const auto mean_buoyancy=[&](double dz) {
            return rho*gravity*area*
                mean_wet_height(z0,z0+dz,height,level);
        };
        const auto slope=[&](double dz) {
            return m*gravity-mean_buoyancy(dz);
        };
        const auto residual=[&](double dz) {
            return dz-dt*v0+0.5*dt*dt/m*slope(dz);
        };
        // Convex piecewise-quadratic hydrostatic potential -> monotone
        // secant slope. Bracket and bisect rather than use a fragile
        // fixed-point iteration across the wet/dry transitions.
        double lo=-dx,hi=dx;
        for(int k=0;k<30 && (residual(lo)>0.0||residual(hi)<0.0);++k) {
            lo*=2.0;hi*=2.0;
        }
        if(!std::isfinite(residual(lo))||!std::isfinite(residual(hi))||
           residual(lo)>0.0||residual(hi)<0.0)
            throw std::runtime_error("Hydrostatic discrete-gradient bracket failed");
        for(std::size_t k=0;k<100;++k) {
            const double mid=lo+0.5*(hi-lo);
            if(residual(mid)>0.0)hi=mid;else lo=mid;
            out.bisection_iterations=k+1;
        }
        const double dz=lo+0.5*(hi-lo);
        const double v1=2.0*dz/dt-v0;
        out.average_buoyancy_force_N=mean_buoyancy(dz);
        out.buoyancy_reaction_unapplied_Ns=
            -out.average_buoyancy_force_N*dt;
        out.gravity_impulse_Ns=-m*gravity*dt;
        out.after=out.frozen_reference.net_rigid.after;
        out.after.centre_m[2]=z0+dz;
        out.after.velocity_m_s[2]=v1;
        out.box_after=out.frozen_reference.box_after;
        out.box_after.min_m[2]=box.min_m[2]+dz;
        out.box_after.max_m[2]=box.max_m[2]+dz;
        (void)P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx,dt,box,out.box_after);
        out.kinetic_energy_change_J=0.5*m*(v1*v1-v0*v0);
        out.gravity_potential_change_J=m*gravity*dz;
        out.buoyancy_potential_change_J=-out.average_buoyancy_force_N*dz;
        out.mechanical_defect_J=out.kinetic_energy_change_J+
            out.gravity_potential_change_J+out.buoyancy_potential_change_J;
        if(!std::isfinite(dz)||!std::isfinite(v1)||
           !std::isfinite(out.average_buoyancy_force_N)||
           !std::isfinite(out.mechanical_defect_J))
            throw std::overflow_error("Hydrostatic discrete-gradient overflow");
        return out;
    }
};
} // namespace upc
