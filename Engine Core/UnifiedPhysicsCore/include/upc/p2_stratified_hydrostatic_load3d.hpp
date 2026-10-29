#pragma once
// P2 PARTIAL: stratified VOF water level -> *face-integrated* hydrostatic
// buoyancy -> dynamic rigid translation. This is NOT a live MAC/VOF/FSI solve.
#include "upc/p2_moving_box_open_volume3d.hpp"
#include "upc/rigid_pressure_response3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2StratifiedHydrostaticLoadResult3D {
    // Cell-centred pressure is diagnostic ONLY. It is NOT used for body
    // traction: a centre sample would give wrong buoyancy for cut faces.
    std::vector<double> water_pressure_pa;
    double water_level_m=0.0;
    double displaced_water_volume_m3=0.0;
    double hydrostatic_force_n=0.0;
    RigidPressureImpulse3D rigid;
    P2Box3D box_after;
};
class P2StratifiedHydrostaticLoad3D {
public:
    // Each vertical column must contain full water cells, at most one
    // partial interface cell, then dry air. Gravity is -Z. This diagnostic
    // p is the water-volume-averaged pressure in each wet cell;
    // interface/cut-face traction still requires analytic integration
    // at the actual box surface (see step).
    static std::vector<double> pressure(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        double rho_water,double gravity_m_s2,
        const std::vector<double>& water_fraction) {
        if(nx==0||ny==0||nz==0||ny>32768/nx||
           nz>32768/(nx*ny)||water_fraction.size()!=nx*ny*nz||
           !std::isfinite(dx)||dx<=0.0||
           !std::isfinite(rho_water)||rho_water<=0.0||
           !std::isfinite(gravity_m_s2)||gravity_m_s2<0.0)
            throw std::invalid_argument("Invalid stratified hydrostatic input");
        std::vector<double> p(water_fraction.size(),0.0);
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            double height=0.0;
            bool interface_seen=false;
            for(std::size_t z=0;z<nz;++z) {
                const auto i=(z*ny+y)*nx+x;
                const double f=water_fraction[i];
                if(!std::isfinite(f)||f<0.0||f>1.0)
                    throw std::invalid_argument("Invalid hydrostatic water fraction");
                if(interface_seen&&f>0.0)
                    throw std::invalid_argument("Non-stratified water column");
                height+=f*dx;
                if(f<1.0)interface_seen=true;
            }
            for(std::size_t z=0;z<nz;++z) {
                const auto i=(z*ny+y)*nx+x;
                if(water_fraction[i]==0.0)continue;
                // A partial top cell has water only in its bottom f*dx.
                // Average pressure over that WATER volume, not the dry
                // remainder: integral_0^(f dx) rho*g*(f dx-s) ds / (f dx).
                const double f=water_fraction[i];
                const double depth=f<1.0 ? 0.5*f*dx :
                    std::max(0.0,height-(z+0.5)*dx);
                p[i]=rho_water*gravity_m_s2*depth;
                if(!std::isfinite(p[i]))
                    throw std::overflow_error("Hydrostatic pressure overflow");
            }
        }
        return p;
    }
    // Strictly quiescent HORIZONTAL water level, axis-aligned nonrotating
    // box. Exact Archimedes pressure traction: rho*g*submerged_box_volume.
    // Weight of the body and its external supports are NOT applied here.
    // The opposite fluid impulse is diagnostic and NOT applied to MAC.
    static P2StratifiedHydrostaticLoadResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho_water,double gravity_m_s2,
        const P2Box3D& box,const RigidPressureState3D& body,
        const std::vector<double>& water_fraction) {
        P2StratifiedHydrostaticLoadResult3D out;
        out.water_pressure_pa=pressure(
            nx,ny,nz,dx,rho_water,gravity_m_s2,water_fraction);
        double height=0.0;
        for(std::size_t z=0;z<nz;++z)
            height+=dx*water_fraction[z*nx*ny];
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            double column=0.0;
            for(std::size_t z=0;z<nz;++z)
                column+=dx*water_fraction[(z*ny+y)*nx+x];
            if(std::abs(column-height)>1e-12*std::max(1.0,height))
                throw std::invalid_argument(
                    "Hydrostatic rigid load requires horizontal water level");
        }
        for(int a=0;a<3;++a) {
            const double centre=0.5*(box.min_m[a]+box.max_m[a]);
            if(!std::isfinite(centre)||!std::isfinite(body.centre_m[a])||
               std::abs(centre-body.centre_m[a])>
                   1e-10*std::max(1.0,std::abs(centre))||
               !std::isfinite(body.angular_velocity_rad_s[a])||
               std::abs(body.angular_velocity_rad_s[a])>1e-14)
                throw std::invalid_argument(
                    "Hydrostatic box requires centred nonrotating body");
        }
        const double lx=box.max_m[0]-box.min_m[0];
        const double ly=box.max_m[1]-box.min_m[1];
        const double wet_depth=std::max(
            0.0,std::min(box.max_m[2],height)-box.min_m[2]);
        out.water_level_m=height;
        out.displaced_water_volume_m3=lx*ly*wet_depth;
        out.hydrostatic_force_n=
            rho_water*gravity_m_s2*out.displaced_water_volume_m3;
        if(!std::isfinite(out.displaced_water_volume_m3)||
           !std::isfinite(out.hydrostatic_force_n)||
           out.displaced_water_volume_m3<0.0)
            throw std::overflow_error("Hydrostatic force overflow");
        StaticSolidBodyPressureLoad3D load;
        load.force_N[2]=out.hydrostatic_force_n;
        out.rigid=RigidPressureResponse3D::step(body,load,dt);
        out.box_after=box;
        for(int a=0;a<3;++a) {
            const double shift=out.rigid.after.centre_m[a]-body.centre_m[a];
            out.box_after.min_m[a]+=shift;
            out.box_after.max_m[a]+=shift;
        }
        // Fail closed on invalid old/swept cut-cell geometry.
        (void)P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx,dt,box,out.box_after);
        return out;
    }
};
} // namespace upc
