#pragma once
// P2 PARTIAL: apply a frozen-pressure dynamic-box counterimpulse to
// fractional-open MAC dual masses. NOT a coupled pressure/VOF/FSI solve.
// Face impulse is conservative in this selected discrete proxy; no
// incompressibility projection or stress-consistent energy closure follows.
#include "upc/p2_moving_box_dynamic_pressure3d.hpp"
#include "upc/p2_moving_mac_dual_volumes3d.hpp"
#include "upc/p2_moving_box_mac_apertures3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MovingBoxMacCounterImpulseResult3D {
    using Faces=std::array<std::vector<double>,3>;
    P2MovingBoxDynamicPressureResult3D body;
    Faces velocity_after_m_s;
    std::array<std::size_t,3> updated_faces{};
    std::array<double,3> selected_dual_mass_kg{};
    std::array<double,3> face_delta_velocity_m_s{};
    std::array<double,3> applied_fluid_impulse_Ns{};
    std::array<double,3> total_impulse_residual_Ns{};
    double selected_face_energy_change_proxy_J=0.0;
    double max_geometric_continuity_residual_m3=0.0;
};
class P2MovingBoxMacCounterImpulse3D {
public:
    using Faces=P2MovingBoxMacCounterImpulseResult3D::Faces;
    // Fully flooded, constant-density, interior staggered DOFs only.
    // Pressure sampled at the old box; box then translates. MAC impulse
    // is deposited on open dual volumes within a one-cell exterior shell.
    // Caller data is never mutated, including if validation fails.
    static P2MovingBoxMacCounterImpulseResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& old_box,const RigidPressureState3D& rigid,
        const std::vector<double>& pressure_pa,const Faces& before) {
        if(nx<3||ny<3||nz<3||ny>32768/nx||nz>32768/(nx*ny)||
           !std::isfinite(rho)||rho<=0.0)
            throw std::invalid_argument("Invalid counterimpulse grid/density");
        const std::array<std::size_t,3> dims{{nx,ny,nz}};
        for(int axis=0;axis<3;++axis) {
            auto shape=dims;++shape[axis];
            if(before[axis].size()!=shape[0]*shape[1]*shape[2])
                throw std::invalid_argument("Counterimpulse MAC shape");
            for(std::size_t z=0;z<shape[2];++z)
            for(std::size_t y=0;y<shape[1];++y)
            for(std::size_t x=0;x<shape[0];++x) {
                const std::array<std::size_t,3> p{{x,y,z}};
                const double v=before[axis][(z*shape[1]+y)*shape[0]+x];
                if(!std::isfinite(v)||
                   ((p[axis]==0||p[axis]==dims[axis])&&v!=0.0))
                    throw std::invalid_argument("Invalid MAC boundary/velocity");
            }
        }
        P2MovingBoxMacCounterImpulseResult3D out;
        out.body=P2MovingBoxDynamicPressure3D::step(
            nx,ny,nz,dx,dt,old_box,rigid,pressure_pa);
        const auto dual=P2MovingMacDualVolumes3DBuilder::sample(
            nx,ny,nz,dx,dt,old_box,out.body.box_after);
        const auto apertures=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,old_box,out.body.box_after);
        const std::array<const std::vector<double>*,3> open{{
            &apertures.u_open,&apertures.v_open,&apertures.w_open}};
        out.velocity_after_m_s=before;
        const double full_mass=rho*dx*dx*dx;
        if(!std::isfinite(full_mass)||full_mass<=0.0)
            throw std::overflow_error("Counterimpulse mass overflow");
        P2Box3D mid;
        for(int a=0;a<3;++a) {
            mid.min_m[a]=0.5*(old_box.min_m[a]+out.body.box_after.min_m[a]);
            mid.max_m[a]=0.5*(old_box.max_m[a]+out.body.box_after.max_m[a]);
        }
        for(int axis=0;axis<3;++axis) {
            const auto s=dual.dimensions[axis];
            const auto& g=dual.component[axis].geometry.cell_geometry;
            std::vector<std::size_t> selected;
            std::vector<double> masses;
            for(std::size_t z=0;z<s[2];++z)
            for(std::size_t y=0;y<s[1];++y)
            for(std::size_t x=0;x<s[0];++x) {
                const std::array<std::size_t,3> p{{x,y,z}};
                std::array<double,3> centre{};
                bool in_shell=true,inside=true;
                for(int a=0;a<3;++a) {
                    centre[a]=(static_cast<double>(p[a])+
                               (a==axis?1.0:0.5))*dx;
                    if(centre[a]<mid.min_m[a]-dx||
                       centre[a]>mid.max_m[a]+dx)in_shell=false;
                    if(centre[a]>=mid.min_m[a]&&centre[a]<=mid.max_m[a]){}
                    else inside=false;
                }
                if(!in_shell||inside)continue;
                const std::size_t di=(z*s[1]+y)*s[0]+x;
                auto fp=p;++fp[axis];
                auto fs=dims;++fs[axis];
                const std::size_t fi=(fp[2]*fs[1]+fp[1])*fs[0]+fp[0];
                const double aperture=(*open[axis])[fi];
                const double fraction=0.5*(g.alpha_old[di]+g.alpha_new[di]);
                if(aperture<=1e-10||fraction<=1e-10)continue;
                const double mass=full_mass*fraction;
                if(!std::isfinite(mass)||mass<=0.0)
                    throw std::overflow_error("Nonfinite selected dual mass");
                selected.push_back(fi);
                masses.push_back(mass);
                out.selected_dual_mass_kg[axis]+=mass;
            }
            const double requested=out.body.rigid.fluid_reaction_impulse_Ns[axis];
            if(selected.empty()||!std::isfinite(out.selected_dual_mass_kg[axis])||
               out.selected_dual_mass_kg[axis]<=0.0)
                throw std::invalid_argument("No open MAC duals near moving body");
            const double dv=requested/out.selected_dual_mass_kg[axis];
            if(!std::isfinite(dv))
                throw std::overflow_error("Counterimpulse velocity overflow");
            out.face_delta_velocity_m_s[axis]=dv;
            out.updated_faces[axis]=selected.size();
            for(std::size_t k=0;k<selected.size();++k) {
                const std::size_t fi=selected[k];
                const double old=before[axis][fi];
                const double next=old+dv;
                const double dE=masses[k]*dv*(old+0.5*dv);
                if(!std::isfinite(next)||!std::isfinite(dE))
                    throw std::overflow_error("Counterimpulse update overflow");
                out.velocity_after_m_s[axis][fi]=next;
                out.selected_face_energy_change_proxy_J+=dE;
                out.applied_fluid_impulse_Ns[axis]+=masses[k]*dv;
            }
            out.total_impulse_residual_Ns[axis]=
                out.body.rigid.body_linear_impulse_Ns[axis]+
                out.applied_fluid_impulse_Ns[axis];
            if(!std::isfinite(out.selected_face_energy_change_proxy_J)||
               !std::isfinite(out.total_impulse_residual_Ns[axis]))
                throw std::overflow_error("Counterimpulse ledger overflow");
        }
        // Measure, but DO NOT silently correct, the missing moving
        // cut-cell incompressibility projection.
        const auto& alpha=apertures.cell_geometry;
        const double area_time=dx*dx*dt,cell_volume=dx*dx*dx;
        const auto ui=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*(nx+1)+x;};
        const auto vi=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*(ny+1)+y)*nx+x;};
        const auto wi=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;};
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const std::size_t ci=(z*ny+y)*nx+x;
            const auto flux=[&](int a,std::size_t i){
                return out.velocity_after_m_s[a][i]*(*open[a])[i]*area_time;};
            const double residual=(alpha.alpha_new[ci]-alpha.alpha_old[ci])*cell_volume+
                flux(0,ui(x+1,y,z))-flux(0,ui(x,y,z))+
                flux(1,vi(x,y+1,z))-flux(1,vi(x,y,z))+
                flux(2,wi(x,y,z+1))-flux(2,wi(x,y,z));
            if(!std::isfinite(residual))
                throw std::overflow_error("Counterimpulse continuity overflow");
            out.max_geometric_continuity_residual_m3=std::max(
                out.max_geometric_continuity_residual_m3,std::abs(residual));
        }
        return out;
    }
};
} // namespace upc
