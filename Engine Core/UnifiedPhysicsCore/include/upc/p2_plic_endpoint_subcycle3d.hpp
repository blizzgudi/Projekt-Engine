#pragma once
// P2 PARTIAL: optional bounded 0/1 endpoint-roundoff repair between every
// geometric PLIC substep. No fallback normal for genuine mixed geometry.
// Existing adaptive PLIC API remains unchanged. No two-phase CFD claim.
#include "upc/p2_plic_adaptive_subcycle3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2PlicEndpointSubcycle3D {
    P2PlicAdaptiveSubcycle3D transport;
    std::size_t snapped_cells=0;
    double signed_snap_volume_m3=0;
    double absolute_snap_volume_m3=0;
    double transport_only_mass_residual_m3=0;
};

inline P2PlicEndpointSubcycle3D p2_plic_endpoint_subcycle3d(
    const std::vector<double>& fill,
    const std::array<std::vector<double>,3>& velocity,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
    double target=0.4,std::size_t max_substeps=256) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>2000000||fill.size()!=nx*ny*nz||
       !std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0||
       !std::isfinite(target)||target<=0||target>1||
       max_substeps==0||max_substeps>4096)
        throw std::invalid_argument("Invalid endpoint PLIC subcycling parameters");
    for(double f:fill)
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid endpoint PLIC fraction");
    for(const auto& component:velocity) {
        if(component.size()!=fill.size())
            throw std::invalid_argument("Invalid endpoint PLIC velocity size");
        for(double v:component)
            if(!std::isfinite(v))
                throw std::invalid_argument("Nonfinite endpoint PLIC velocity");
    }
    const std::array<std::size_t,3> dims{nx,ny,nz};
    const auto idx=[&](const std::array<std::size_t,3>& p) {
        return (p[2]*ny+p[1])*nx+p[0];
    };
    double maximum=0;
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const std::array<std::size_t,3> p{x,y,z};
        double sum=0;
        for(int a=0;a<3;++a) {
            auto left=p;
            left[a]=(left[a]+dims[a]-1)%dims[a];
            sum+=std::abs(velocity[a][idx(p)])+
                 std::abs(velocity[a][idx(left)]);
        }
        maximum=std::max(maximum,sum);
    }
    const double required=maximum*(dt/dx)/target;
    if(!std::isfinite(required)||required>max_substeps)
        throw std::invalid_argument("Endpoint PLIC CFL budget exceeded");
    const auto count=std::max<std::size_t>(1,static_cast<std::size_t>(std::ceil(required)));
    const double sub_dt=dt/count;
    const double cell_volume=dx*dx*dx;
    if(!std::isfinite(sub_dt)||sub_dt<=0||
       !std::isfinite(cell_volume)||cell_volume<=0)
        throw std::invalid_argument("Invalid endpoint PLIC physical units");
    P2PlicEndpointSubcycle3D out;
    out.transport.water_fill=fill;
    out.transport.substeps=count;
    long double initial_cells=0;
    for(double f:fill)initial_cells+=f;
    out.transport.initial_volume_m3=static_cast<double>(initial_cells)*cell_volume;
    long double snap_signed=0,snap_absolute=0;
    constexpr double endpoint_tol=8*std::numeric_limits<double>::epsilon();
    for(std::size_t step=0;step<count;++step) {
        const auto r=p2_plic_unsplit_mac3d(
            out.transport.water_fill,velocity,nx,ny,nz,dx,sub_dt);
        out.transport.water_fill=r.water_fill;
        out.transport.transported_faces+=r.transported_faces;
        out.transport.reconstructed_partial_cell_visits+=r.reconstructed_partial_cells;
        out.transport.max_six_face_courant_per_substep=std::max(
            out.transport.max_six_face_courant_per_substep,r.max_cell_face_courant_sum);
        out.transport.max_divergence_s_inv=std::max(
            out.transport.max_divergence_s_inv,r.max_divergence_s_inv);
        out.transport_only_mass_residual_m3+=r.mass_residual_m3;
        for(double& f:out.transport.water_fill) {
            const double old=f;
            if(f>0&&f<=endpoint_tol)f=0;
            else if(f<1&&1-f<=endpoint_tol)f=1;
            if(f!=old) {
                ++out.snapped_cells;
                const long double delta=static_cast<long double>(f)-old;
                snap_signed+=delta;
                snap_absolute+=std::abs(delta);
            }
        }
        if(snap_absolute*static_cast<long double>(cell_volume)>
             1e-10L*std::max(1.0L,
                  static_cast<long double>(out.transport.initial_volume_m3)))
            throw std::runtime_error("PLIC substep endpoint correction exceeds mass budget");
    }
    long double final_cells=0;
    for(double f:out.transport.water_fill)final_cells+=f;
    out.transport.final_volume_m3=static_cast<double>(final_cells)*cell_volume;
    out.signed_snap_volume_m3=static_cast<double>(snap_signed)*cell_volume;
    out.absolute_snap_volume_m3=static_cast<double>(snap_absolute)*cell_volume;
    out.transport.mass_residual_m3=out.transport.final_volume_m3-
                                   out.transport.initial_volume_m3;
    const double tolerance=1e-10*std::max(1.0,out.transport.initial_volume_m3);
    if(!std::isfinite(out.transport.mass_residual_m3)||
       std::abs(out.transport.mass_residual_m3-
                out.signed_snap_volume_m3)>tolerance)
        throw std::runtime_error("PLIC substep physical mass imbalance");
    return out;
}
} // namespace upc
