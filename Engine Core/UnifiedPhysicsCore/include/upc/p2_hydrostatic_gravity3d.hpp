#pragma once
// P2 PARTIAL: quiescent stratified hydrostatics with rigid-body weight and
// an exact *fixed-level* buoyancy-potential audit. No fluid reaction is applied.
#include "upc/p2_stratified_hydrostatic_load3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2HydrostaticGravityResult3D {
    double water_level_m=0.0;
    double buoyancy_force_N=0.0;
    double gravity_force_N=0.0;
    double net_vertical_force_N=0.0;
    double buoyancy_reaction_unapplied_Ns=0.0;
    double gravity_impulse_Ns=0.0;
    double buoyancy_potential_change_J=0.0;
    double gravity_potential_change_J=0.0;
    double kinetic_energy_change_J=0.0;
    // Nonzero when frozen initial buoyancy crosses the free surface.
    // This is a time-discretization defect, NOT a fluid energy closure.
    double fixed_level_mechanical_defect_J=0.0;
    RigidPressureImpulse3D net_rigid;
    P2Box3D box_after;
};
class P2HydrostaticGravity3D {
    static double buoyancy_potential(double centre_z,double height,
                                      double area,double water_level,
                                      double rho,double gravity) {
        const double depth=water_level-(centre_z-0.5*height);
        const double wet=std::clamp(depth,0.0,height);
        // U=0 for a dry box; -dU/dz = rho*g*submerged volume.
        const double volume_integral=0.5*wet*wet+
            height*std::max(0.0,depth-height);
        return rho*gravity*area*volume_integral;
    }
public:
    static P2HydrostaticGravityResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,double gravity,
        const P2Box3D& box,const RigidPressureState3D& body,
        const std::vector<double>& fill) {
        // Reuse strict stratified-column input checks, including finite
        // fractions, one partial top cell, and positive rho/dx.
        (void)P2StratifiedHydrostaticLoad3D::pressure(
            nx,ny,nz,dx,rho,gravity,fill);
        if(!std::isfinite(dt)||dt<=0.0||
           !std::isfinite(body.mass_kg)||body.mass_kg<=0.0)
            throw std::invalid_argument("Invalid hydrostatic gravity state");
        double level=0.0;
        for(std::size_t z=0;z<nz;++z)
            level+=dx*fill[z*nx*ny];
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            double column=0.0;
            for(std::size_t z=0;z<nz;++z)
                column+=dx*fill[(z*ny+y)*nx+x];
            if(std::abs(column-level)>1e-12*std::max(1.0,level))
                throw std::invalid_argument("Nonhorizontal hydrostatic surface");
        }
        for(int a=0;a<3;++a) {
            const double centre=0.5*(box.min_m[a]+box.max_m[a]);
            if(!std::isfinite(centre)||!std::isfinite(body.centre_m[a])||
               std::abs(centre-body.centre_m[a])>
                   1e-10*std::max(1.0,std::abs(centre))||
               !std::isfinite(body.angular_velocity_rad_s[a])||
               std::abs(body.angular_velocity_rad_s[a])>1e-14)
                throw std::invalid_argument("Noncentred or rotating box");
        }
        // Validate geometry even when net acceleration vanishes.
        (void)P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx,dt,box,box);
        const double height=box.max_m[2]-box.min_m[2];
        const double area=(box.max_m[0]-box.min_m[0])*
                          (box.max_m[1]-box.min_m[1]);
        const double submerged=std::clamp(level-box.min_m[2],0.0,height);
        P2HydrostaticGravityResult3D out;
        out.water_level_m=level;
        out.buoyancy_force_N=rho*gravity*area*submerged;
        out.gravity_force_N=-body.mass_kg*gravity;
        out.net_vertical_force_N=out.buoyancy_force_N+out.gravity_force_N;
        StaticSolidBodyPressureLoad3D net;
        net.force_N[2]=out.net_vertical_force_N;
        out.net_rigid=RigidPressureResponse3D::step(body,net,dt);
        out.box_after=box;
        for(int a=0;a<3;++a) {
            const double shift=out.net_rigid.after.centre_m[a]-
                               body.centre_m[a];
            out.box_after.min_m[a]+=shift;
            out.box_after.max_m[a]+=shift;
        }
        (void)P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx,dt,box,out.box_after);
        out.buoyancy_reaction_unapplied_Ns=-out.buoyancy_force_N*dt;
        out.gravity_impulse_Ns=out.gravity_force_N*dt;
        // The generic rigid integrator negates its *total* input load.
        // Correct its diagnostic reaction: body weight is an external
        // gravity force, NOT a force exerted on the fluid.
        out.net_rigid.fluid_reaction_impulse_Ns[2]=
            out.buoyancy_reaction_unapplied_Ns;
        const double dz=out.net_rigid.after.centre_m[2]-body.centre_m[2];
        out.gravity_potential_change_J=body.mass_kg*gravity*dz;
        out.buoyancy_potential_change_J=
            buoyancy_potential(out.net_rigid.after.centre_m[2],
                               height,area,level,rho,gravity)-
            buoyancy_potential(body.centre_m[2],
                               height,area,level,rho,gravity);
        out.kinetic_energy_change_J=out.net_rigid.kinetic_energy_change_J;
        out.fixed_level_mechanical_defect_J=
            out.kinetic_energy_change_J+
            out.gravity_potential_change_J+
            out.buoyancy_potential_change_J;
        if(!std::isfinite(out.buoyancy_force_N)||
           !std::isfinite(out.gravity_force_N)||
           !std::isfinite(out.net_vertical_force_N)||
           !std::isfinite(out.buoyancy_reaction_unapplied_Ns)||
           !std::isfinite(out.gravity_impulse_Ns)||
           !std::isfinite(out.gravity_potential_change_J)||
           !std::isfinite(out.buoyancy_potential_change_J)||
           !std::isfinite(out.fixed_level_mechanical_defect_J))
            throw std::overflow_error("Hydrostatic gravity energy overflow");
        return out;
    }
};
} // namespace upc
