#pragma once
// P2 PARTIAL: closed-basin, quiescent hydrostatic equilibrium with a moving
// vertical box. Waterline and cellwise liquid inventory are reconstructed from
// displaced volume. NO MAC flow, fluid inertia or applied counterimpulse.
#include "upc/p2_moving_box_open_volume3d.hpp"
#include "upc/rigid_pressure_response3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2ClosedBasinHydrostaticResult3D {
    RigidPressureState3D after;
    P2Box3D box_after;
    std::vector<double> water_fraction_before, water_fraction_after;
    double water_level_before_m=0.0, water_level_after_m=0.0;
    double water_volume_before_m3=0.0, water_volume_after_m3=0.0;
    double mean_buoyancy_N=0.0;
    double buoyancy_reaction_unapplied_Ns=0.0;
    double gravity_impulse_Ns=0.0;
    double kinetic_change_J=0.0;
    double body_gravity_change_J=0.0;
    double water_gravity_change_J=0.0;
    double equilibrium_energy_defect_J=0.0;
};

class P2ClosedBasinHydrostatic3D {
    struct Equilibrium {
        double wet_m,level_m,water_potential_J;
    };
    static Equilibrium state(double centre_z,double height,double tank_area,
                             double box_area,double water_volume,
                             double rho,double gravity) {
        const double bottom=centre_z-0.5*height;
        const double wet=std::clamp(
            (water_volume-tank_area*bottom)/(tank_area-box_area),
            0.0,height);
        const double level=(water_volume+box_area*wet)/tank_area;
        const double submerged_top=std::min(level,bottom+height);
        const double excluded_moment=level>bottom?
            0.5*(submerged_top*submerged_top-bottom*bottom):0.0;
        const double potential=0.5*rho*gravity*tank_area*level*level-
            rho*gravity*box_area*excluded_moment;
        if(!std::isfinite(level)||!std::isfinite(potential))
            throw std::overflow_error("Closed-basin hydrostatic state overflow");
        return {wet,level,potential};
    }
    static double mean_wet(double z0,double z1,double height,
                           double tank_area,double box_area,double volume) {
        const auto wet=[&](double z) {
            return std::clamp(
                (volume-tank_area*(z-0.5*height))/(tank_area-box_area),
                0.0,height);
        };
        if(z0==z1)return wet(z0);
        const double a=std::min(z0,z1),b=std::max(z0,z1);
        const double dry=0.5*height+volume/tank_area;
        const double full=dry-height*(1.0-box_area/tank_area);
        std::array<double,4> points{{a,std::clamp(full,a,b),
                                     std::clamp(dry,a,b),b}};
        std::sort(points.begin(),points.end());
        double integral=0.0;
        for(std::size_t i=1;i<points.size();++i)
            integral+=0.5*(wet(points[i-1])+wet(points[i]))*
                      (points[i]-points[i-1]);
        return integral/(b-a);
    }
    // Fraction of the *open* voxel occupied by water. The box overlap is
    // subtracted from the submerged voxel volume, including cut cells.
    static std::pair<std::vector<double>,double> reconstruct(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        double level,const P2Box3D& box,const std::vector<double>& open) {
        std::vector<double> fraction(open.size(),0.0);
        const double cell=dx*dx*dx;
        double total=0.0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const auto i=(z*ny+y)*nx+x;
            const double zlo=z*dx, zhi=zlo+dx;
            const double wet_z=std::clamp(level-zlo,0.0,dx);
            const double overlap_x=std::max(0.0,std::min((x+1)*dx,box.max_m[0])-
                                                std::max(x*dx,box.min_m[0]));
            const double overlap_y=std::max(0.0,std::min((y+1)*dx,box.max_m[1])-
                                                std::max(y*dx,box.min_m[1]));
            const double overlap_z=std::max(0.0,
                std::min(std::min(zhi,level),box.max_m[2])-
                std::max(zlo,box.min_m[2]));
            const double liquid=dx*dx*wet_z-overlap_x*overlap_y*overlap_z;
            const double capacity=open[i]*cell;
            if(!std::isfinite(liquid)||liquid< -1e-10*cell||
               liquid>capacity+1e-10*cell)
                throw std::logic_error("Invalid closed-basin cut-cell liquid");
            const double bounded=std::clamp(liquid,0.0,capacity);
            fraction[i]=capacity>1e-13*cell?bounded/capacity:0.0;
            total+=bounded;
        }
        return {std::move(fraction),total};
    }
public:
    static P2ClosedBasinHydrostaticResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        double rho,double gravity,double water_volume,
        const P2Box3D& box,const RigidPressureState3D& body) {
        if(!std::isfinite(dt)||dt<=0.0||!std::isfinite(rho)||rho<=0.0||
           !std::isfinite(gravity)||gravity<0.0||
           !std::isfinite(water_volume)||water_volume<0.0||
           !std::isfinite(body.mass_kg)||body.mass_kg<=0.0)
            throw std::invalid_argument("Invalid closed-basin input");
        for(int a=0;a<3;++a) {
            const double centre=0.5*(box.min_m[a]+box.max_m[a]);
            if(!std::isfinite(body.centre_m[a])||
               !std::isfinite(body.velocity_m_s[a])||
               !std::isfinite(body.angular_velocity_rad_s[a])||
               !std::isfinite(centre)||
               std::abs(centre-body.centre_m[a])>1e-10||
               std::abs(body.angular_velocity_rad_s[a])>1e-14||
               (a<2&&std::abs(body.velocity_m_s[a])>1e-14))
                throw std::invalid_argument("Only centred vertical translation supported");
        }
        const auto old_geom=P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx,dt,box,box);
        const double tank_area=static_cast<double>(nx)*ny*dx*dx;
        const double box_area=(box.max_m[0]-box.min_m[0])*
                              (box.max_m[1]-box.min_m[1]);
        const double height=box.max_m[2]-box.min_m[2];
        if(!std::isfinite(tank_area)||!std::isfinite(box_area)||
           box_area<=0.0||box_area>=tank_area||
           water_volume>old_geom.old_open_m3+1e-10)
            throw std::invalid_argument("Invalid basin capacity");
        const double z0=body.centre_m[2],v0=body.velocity_m_s[2];
        const auto initial=state(z0,height,tank_area,box_area,
                                 water_volume,rho,gravity);
        const auto mean_force=[&](double dz) {
            return rho*gravity*box_area*mean_wet(
                z0,z0+dz,height,tank_area,box_area,water_volume);
        };
        const auto residual=[&](double dz) {
            return dz-dt*v0+0.5*dt*dt*
                (gravity-mean_force(dz)/body.mass_kg);
        };
        double lo=-dx,hi=dx;
        for(int k=0;k<30&&(residual(lo)>0.0||residual(hi)<0.0);++k) {
            lo*=2.0;hi*=2.0;
        }
        if(!std::isfinite(residual(lo))||!std::isfinite(residual(hi))||
           residual(lo)>0.0||residual(hi)<0.0)
            throw std::runtime_error("Closed-basin displacement not bracketed");
        for(int k=0;k<100;++k) {
            const double mid=lo+0.5*(hi-lo);
            if(residual(mid)>0.0)hi=mid;else lo=mid;
        }
        const double dz=lo+0.5*(hi-lo);
        const double v1=2.0*dz/dt-v0;
        P2ClosedBasinHydrostaticResult3D out;
        out.after=body;
        out.after.centre_m[2]=z0+dz;
        out.after.velocity_m_s[2]=v1;
        out.box_after=box;
        out.box_after.min_m[2]+=dz;
        out.box_after.max_m[2]+=dz;
        const auto geometry=P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx,dt,box,out.box_after);
        const auto final=state(z0+dz,height,tank_area,box_area,
                               water_volume,rho,gravity);
        const double domain_height=nz*dx;
        if(initial.level_m<0.0||final.level_m<0.0||
           initial.level_m>domain_height+1e-10||
           final.level_m>domain_height+1e-10)
            throw std::invalid_argument("Closed basin water surface exceeds domain");
        auto before=reconstruct(nx,ny,nz,dx,initial.level_m,
                                 box,geometry.alpha_old);
        auto after=reconstruct(nx,ny,nz,dx,final.level_m,
                                out.box_after,geometry.alpha_new);
        out.water_fraction_before=std::move(before.first);
        out.water_fraction_after=std::move(after.first);
        out.water_volume_before_m3=before.second;
        out.water_volume_after_m3=after.second;
        out.water_level_before_m=initial.level_m;
        out.water_level_after_m=final.level_m;
        out.mean_buoyancy_N=mean_force(dz);
        out.buoyancy_reaction_unapplied_Ns=-dt*out.mean_buoyancy_N;
        out.gravity_impulse_Ns=-dt*body.mass_kg*gravity;
        out.kinetic_change_J=0.5*body.mass_kg*(v1*v1-v0*v0);
        out.body_gravity_change_J=body.mass_kg*gravity*dz;
        out.water_gravity_change_J=final.water_potential_J-
                                   initial.water_potential_J;
        out.equilibrium_energy_defect_J=out.kinetic_change_J+
            out.body_gravity_change_J+out.water_gravity_change_J;
        const double tol=1e-9*std::max(1.0,water_volume);
        if(!std::isfinite(out.equilibrium_energy_defect_J)||
           !std::isfinite(out.mean_buoyancy_N)||
           !std::isfinite(v1)||
           std::abs(before.second-water_volume)>tol||
           std::abs(after.second-water_volume)>tol)
            throw std::runtime_error("Closed-basin mass/energy nonfinite or mass mismatch");
        return out;
    }
};
} // namespace upc
