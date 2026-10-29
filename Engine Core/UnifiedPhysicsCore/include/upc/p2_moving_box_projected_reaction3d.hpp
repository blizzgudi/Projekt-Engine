#pragma once
// P2 PARTIAL: project the applied rigid-box MAC reaction onto the moving
// cut-cell continuity constraint. Pressure projection is NOT an additional
// physical body reaction: its momentum change is exposed, not hidden.
#include "upc/p2_moving_box_mac_counterimpulse3d.hpp"
#include "upc/p2_moving_box_cut_cell_projection3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MovingBoxProjectedReactionResult3D {
    using Faces=P2MovingBoxMacCounterImpulse3D::Faces;
    P2MovingBoxMacCounterImpulseResult3D unprojected;
    P2MovingBoxCutCellProjection3D projection;
    Faces projected_velocity_m_s;
    std::array<double,3> projection_momentum_change_proxy_Ns{};
    std::array<double,3> combined_impulse_residual_proxy_Ns{};
    double projection_face_energy_change_proxy_J=0.0;
};
class P2MovingBoxProjectedReaction3D {
public:
    using Faces=P2MovingBoxProjectedReactionResult3D::Faces;
    static P2MovingBoxProjectedReactionResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& box,const RigidPressureState3D& body,
        const std::vector<double>& pressure_pa,const Faces& before) {
        P2MovingBoxProjectedReactionResult3D out;
        out.unprojected=P2MovingBoxMacCounterImpulse3D::step(
            nx,ny,nz,dx,dt,rho,box,body,pressure_pa,before);
        const auto& moved=out.unprojected.body.box_after;
        const auto& raw=out.unprojected.velocity_after_m_s;
        out.projection=P2MovingBoxCutCellProjection3DBuilder::sample_with_predictor(
            nx,ny,nz,dx,dt,rho,box,moved,raw[0],raw[1],raw[2]);
        const auto dual=P2MovingMacDualVolumes3DBuilder::sample(
            nx,ny,nz,dx,dt,box,moved);
        const auto& aperture=out.projection.wall.geometry;
        const std::array<const std::vector<double>*,3> open{{
            &aperture.u_open,&aperture.v_open,&aperture.w_open}};
        const std::array<const std::vector<double>*,3> flux{{
            &out.projection.u_fluid_m3,
            &out.projection.v_fluid_m3,
            &out.projection.w_fluid_m3}};
        const double area_time=dx*dx*dt;
        const double full_mass=rho*dx*dx*dx;
        if(!std::isfinite(area_time)||area_time<=0.0||
           !std::isfinite(full_mass)||full_mass<=0.0)
            throw std::overflow_error("Projected reaction scaling overflow");
        for(int a=0;a<3;++a) {
            const auto shape=dual.dimensions[a];
            auto& velocities=out.projected_velocity_m_s[a];
            velocities.assign(raw[a].size(),0.0);
            const auto& g=dual.component[a].geometry.cell_geometry;
            for(std::size_t z=0;z<shape[2];++z)
            for(std::size_t y=0;y<shape[1];++y)
            for(std::size_t x=0;x<shape[0];++x) {
                const std::size_t di=(z*shape[1]+y)*shape[0]+x;
                auto fshape=std::array<std::size_t,3>{{nx,ny,nz}}; ++fshape[a];
                auto p=std::array<std::size_t,3>{{x,y,z}}; ++p[a];
                const std::size_t fi=(p[2]*fshape[1]+p[1])*fshape[0]+p[0];
                const double aperture_value=(*open[a])[fi];
                const double q=(*flux[a])[fi];
                if(aperture_value<=1e-12) {
                    if(std::abs(q)>1e-14)
                        throw std::runtime_error("Closed projected face has flux");
                    continue;
                }
                const double v=q/(area_time*aperture_value);
                const double mass=full_mass*0.5*(g.alpha_old[di]+g.alpha_new[di]);
                const double delta=v-raw[a][fi];
                const double dE=mass*delta*(raw[a][fi]+0.5*delta);
                if(!std::isfinite(v)||!std::isfinite(mass)||!std::isfinite(dE))
                    throw std::overflow_error("Nonfinite projected reaction");
                velocities[fi]=v;
                out.projection_momentum_change_proxy_Ns[a]+=mass*delta;
                out.projection_face_energy_change_proxy_J+=dE;
            }
            out.combined_impulse_residual_proxy_Ns[a]=
                out.unprojected.total_impulse_residual_Ns[a]+
                out.projection_momentum_change_proxy_Ns[a];
            if(!std::isfinite(out.combined_impulse_residual_proxy_Ns[a])||
               !std::isfinite(out.projection_face_energy_change_proxy_J))
                throw std::overflow_error("Projected reaction ledger overflow");
        }
        return out;
    }
};
} // namespace upc
