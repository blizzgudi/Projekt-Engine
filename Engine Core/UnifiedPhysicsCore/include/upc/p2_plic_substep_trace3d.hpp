#pragma once
// P2 PARTIAL: opt-in, read-only diagnostic of individual periodic PLIC
// substeps. No fabricated normals, no mass correction and no physics claims.
#include "upc/p2_plic_preflight3d.hpp"
#include "upc/p2_plic_adaptive_subcycle3d.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace upc {
struct P2PlicTraceFrame3D {
    std::size_t completed_substeps=0;
    double water_volume_m3=0;
    P2PeriodicPlicPreflight3D preflight;
};
struct P2PlicSubstepTrace3D {
    bool completed=false;
    std::size_t planned_substeps=0, completed_substeps=0;
    std::size_t failed_substep=0;
    std::string failure;
    std::vector<P2PlicTraceFrame3D> frames;
    std::size_t snapped_cells=0;
    double signed_snap_volume_m3=0, absolute_snap_volume_m3=0;
    double initial_volume_m3=0, last_volume_m3=0;
};

// Replay the exact existing unsplit kernel with identical six-face-CFL
// substep selection. A failure is explicitly recorded, never converted into
// a PASS. The last state is copied into the diagnostic only, not the caller.
inline P2PlicSubstepTrace3D p2_trace_periodic_plic_substeps3d(
    const std::vector<double>& fill,
    const std::array<std::vector<double>,3>& velocity,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
    double target=0.4,std::size_t max_substeps=256,
    bool sanitize_roundoff_between_substeps=false) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>2000000||fill.size()!=nx*ny*nz||
       !std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0||
       !std::isfinite(target)||target<=0||target>1||
       max_substeps==0||max_substeps>4096)
        throw std::invalid_argument("Invalid PLIC trace parameters");
    for(double f:fill)
        if(!std::isfinite(f)||f<0||f>1)
            throw std::invalid_argument("Invalid PLIC trace fraction");
    for(const auto& component:velocity) {
        if(component.size()!=fill.size())
            throw std::invalid_argument("Invalid PLIC trace velocity size");
        for(double v:component)
            if(!std::isfinite(v))
                throw std::invalid_argument("Invalid PLIC trace velocity");
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
        for(int axis=0;axis<3;++axis) {
            auto left=p;
            left[axis]=(left[axis]+dims[axis]-1)%dims[axis];
            sum+=std::abs(velocity[axis][idx(p)])+
                 std::abs(velocity[axis][idx(left)]);
        }
        maximum=std::max(maximum,sum);
    }
    const double required=maximum*(dt/dx)/target;
    if(!std::isfinite(required)||required>max_substeps)
        throw std::invalid_argument("PLIC trace CFL budget exceeded");
    P2PlicSubstepTrace3D out;
    out.planned_substeps=std::max<std::size_t>(1,static_cast<std::size_t>(std::ceil(required)));
    const double sub_dt=dt/out.planned_substeps;
    if(!std::isfinite(sub_dt)||sub_dt<=0)
        throw std::invalid_argument("Invalid trace substep duration");
    std::vector<double> state=fill;
    const double volume=dx*dx*dx;
    if(!std::isfinite(volume)||volume<=0)
        throw std::invalid_argument("Invalid trace cell volume");
    auto capture=[&](std::size_t completed) {
        P2PlicTraceFrame3D frame;
        frame.completed_substeps=completed;
        long double cells=0;
        for(double f:state)cells+=f;
        frame.water_volume_m3=static_cast<double>(cells)*volume;
        frame.preflight=p2_periodic_plic_preflight3d(state,nx,ny,nz);
        out.frames.push_back(std::move(frame));
        out.last_volume_m3=out.frames.back().water_volume_m3;
    };
    capture(0);
    out.initial_volume_m3=out.last_volume_m3;
    for(std::size_t step=0;step<out.planned_substeps;++step) {
        try {
            auto r=p2_plic_unsplit_mac3d(state,velocity,nx,ny,nz,dx,sub_dt);
            state=std::move(r.water_fill);
            if(sanitize_roundoff_between_substeps) {
                constexpr double tolerance=8*std::numeric_limits<double>::epsilon();
                long double signed_cells=0,absolute_cells=0;
                for(double& f:state) {
                    const double before=f;
                    if(f>0&&f<=tolerance)f=0;
                    else if(f<1&&1-f<=tolerance)f=1;
                    signed_cells+=static_cast<long double>(f)-before;
                    absolute_cells+=std::abs(static_cast<long double>(f)-before);
                    if(f!=before)++out.snapped_cells;
                }
                out.signed_snap_volume_m3+=static_cast<double>(signed_cells)*volume;
                out.absolute_snap_volume_m3+=static_cast<double>(absolute_cells)*volume;
                if(out.absolute_snap_volume_m3>1e-10*
                    std::max(1.0,out.initial_volume_m3))
                    throw std::runtime_error("Trace endpoint snap budget exceeded");
            }
            ++out.completed_substeps;
            capture(out.completed_substeps);
        } catch(const std::exception& e) {
            out.failed_substep=step+1;
            out.failure=e.what();
            return out;
        }
    }
    out.completed=true;
    return out;
}
} // namespace upc
