#pragma once
// P2 PARTIAL: periodic constant-density nonlinear MAC momentum predictor +
// pressure projection + conservative geometric PLIC transport of a passive
// water fraction. This is NOT a two-phase/free-surface Navier-Stokes solver,
// stress-consistent interface model or dynamic fluid-solid interaction.
#include "upc/p2_periodic_mac_projection3d.hpp"
#include "upc/p2_plic_adaptive_subcycle3d.hpp"
#include "upc/p2_plic_endpoint_subcycle3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2NonlinearMacPlicState3D {
    std::array<std::vector<double>,3> staggered_velocity;
    std::vector<double> water_fill;
};
struct P2NonlinearMacPlicStep3D {
    P2NonlinearMacPlicState3D state;
    P2PeriodicMacProjectionResult3D projection;
    P2PlicAdaptiveSubcycle3D transport;
    double momentum_predictor_courant=0.0;
    std::size_t endpoint_snapped_cells=0;
    double endpoint_signed_snap_volume_m3=0;
    double endpoint_absolute_snap_volume_m3=0;
};

// A single atomic integration step. Unlike a prescribed-velocity schedule,
// the momentum field is advected and pressure-projected before it transports
// the geometric PLIC interface. The caller may feed the returned state into
// the next step. All input arrays remain unchanged if either stage fails.
inline P2NonlinearMacPlicStep3D p2_nonlinear_mac_plic_step3d(
    const P2NonlinearMacPlicState3D& initial,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double dt_s,double density_kg_m3,
    double target_six_face_courant=0.4,
    std::size_t max_plic_substeps=256,
    bool endpoint_cleanup_per_substep=false) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>32768||initial.water_fill.size()!=nx*ny*nz||
       !std::isfinite(dx_m)||dx_m<=0||
       !std::isfinite(dt_s)||dt_s<=0||
       !std::isfinite(density_kg_m3)||density_kg_m3<=0)
        throw std::invalid_argument("Invalid nonlinear MAC/PLIC grid or units");
    for(double f:initial.water_fill)
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid nonlinear MAC/PLIC fill");
    const auto& vel=initial.staggered_velocity;
    auto predictor=P2PeriodicMacProjection3D::self_advect_project(
        nx,ny,nz,dx_m,dt_s,density_kg_m3,
        vel[0],vel[1],vel[2]);
    P2NonlinearMacPlicStep3D out;
    out.momentum_predictor_courant=predictor.max_outgoing_courant;
    out.projection=std::move(predictor.projected);
    out.state.staggered_velocity={
        out.projection.u_m_s,out.projection.v_m_s,out.projection.w_m_s};
    std::array<std::vector<double>,3> positive{
        std::vector<double>(nx*ny*nz),
        std::vector<double>(nx*ny*nz),
        std::vector<double>(nx*ny*nz)};
    for(std::size_t k=0;k<nz;++k)
    for(std::size_t j=0;j<ny;++j)
    for(std::size_t i=0;i<nx;++i) {
        const auto id=(k*ny+j)*nx+i;
        positive[0][id]=out.projection.u_m_s[(k*ny+j)*(nx+1)+i+1];
        positive[1][id]=out.projection.v_m_s[(k*(ny+1)+j+1)*nx+i];
        positive[2][id]=out.projection.w_m_s[((k+1)*ny+j)*nx+i];
    }
    if(endpoint_cleanup_per_substep) {
        auto fixed=p2_plic_endpoint_subcycle3d(
            initial.water_fill,positive,nx,ny,nz,dx_m,dt_s,
            target_six_face_courant,max_plic_substeps);
        out.transport=std::move(fixed.transport);
        out.endpoint_snapped_cells=fixed.snapped_cells;
        out.endpoint_signed_snap_volume_m3=fixed.signed_snap_volume_m3;
        out.endpoint_absolute_snap_volume_m3=fixed.absolute_snap_volume_m3;
    } else {
        out.transport=p2_plic_adaptive_subcycle3d(
            initial.water_fill,positive,nx,ny,nz,dx_m,dt_s,
            target_six_face_courant,max_plic_substeps);
    }
    out.state.water_fill=out.transport.water_fill;
    return out;
}
} // namespace upc
