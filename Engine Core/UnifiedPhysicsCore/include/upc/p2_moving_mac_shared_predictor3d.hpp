#pragma once
// P2 PARTIAL: reconstruct all three moving MAC-dual predictors from ONE
// primal staggered 3D velocity field by exact half-cell interpolation.
// Each dual STILL receives its own pressure correction: this is NOT a
// globally consistent momentum tensor, free-surface solver, or FSI.
#include "upc/p2_moving_mac_dual_transport3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
class P2MovingMacSharedPredictor3D {
public:
    using Faces=std::array<std::vector<double>,3>;
    using DualFaces=std::array<Faces,3>;

    // Primal face order: x (nx+1,ny,nz), y (nx,ny+1,nz),
    // z (nx,ny,nz+1). All outer NORMAL faces must be exactly zero.
    // Dual axis a is shifted +dx/2 in a: every dual face velocity is
    // 0.5*(primal_face[i_a]+primal_face[i_a+1]).
    // Faces blocked by the time-averaged moving box are zeroed.
    static DualFaces reconstruct(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const Faces& primal) {
        if(nx<3||ny<3||nz<3||ny>32768/nx||nz>32768/(nx*ny))
            throw std::invalid_argument("Invalid shared MAC grid");
        const std::array<std::size_t,3> dims{{nx,ny,nz}};
        for(int f=0;f<3;++f) {
            auto shape=dims;
            ++shape[f];
            if(primal[f].size()!=shape[0]*shape[1]*shape[2])
                throw std::invalid_argument("Shared MAC face shape mismatch");
            for(std::size_t z=0;z<shape[2];++z)
            for(std::size_t y=0;y<shape[1];++y)
            for(std::size_t x=0;x<shape[0];++x) {
                const std::array<std::size_t,3> p{{x,y,z}};
                const double v=primal[f][(z*shape[1]+y)*shape[0]+x];
                if(!std::isfinite(v)||
                   ((p[f]==0||p[f]==dims[f])&&v!=0.0))
                    throw std::invalid_argument("Nonfinite or exterior shared MAC face");
            }
        }
        const auto dual=P2MovingMacDualVolumes3DBuilder::sample(
            nx,ny,nz,dx,dt,old_box,new_box);
        DualFaces result;
        for(int a=0;a<3;++a) {
            const auto shape=dual.dimensions[a];
            for(int f=0;f<3;++f) {
                auto fs=shape,ps=dims;
                ++fs[f];++ps[f];
                const auto& apertures=dual.component[a].geometry;
                const auto& open=f==0?apertures.u_open:
                                 f==1?apertures.v_open:apertures.w_open;
                auto& dest=result[a][f];
                dest.assign(fs[0]*fs[1]*fs[2],0.0);
                for(std::size_t z=0;z<fs[2];++z)
                for(std::size_t y=0;y<fs[1];++y)
                for(std::size_t x=0;x<fs[0];++x) {
                    const std::array<std::size_t,3> p{{x,y,z}};
                    const auto i=(z*fs[1]+y)*fs[0]+x;
                    if(p[f]==0||p[f]==shape[f]||open[i]<=1e-12)
                        continue;
                    auto next=p;
                    ++next[a];
                    const auto j=(p[2]*ps[1]+p[1])*ps[0]+p[0];
                    const auto k=(next[2]*ps[1]+next[1])*ps[0]+next[0];
                    dest[i]=0.5*(primal[f][j]+primal[f][k]);
                    if(!std::isfinite(dest[i]))
                        throw std::invalid_argument("Shared MAC interpolation overflow");
                }
            }
        }
        return result;
    }

    // P2 PARTIAL: project ONE primal MAC predictor against the actual
    // moving-box cell GCL before interpolating it to the three duals.
    // The subsequent dual pressure solves are still independent.
    struct PrimalProjection {
        Faces corrected;
        double max_continuity_residual_m3=0.0;
        std::size_t iterations=0;
    };
    static PrimalProjection project_primal(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const Faces& primal) {
        if(!std::isfinite(rho)||rho<=0.0)
            throw std::invalid_argument("Invalid primal projection density");
        const auto geometry=P2MovingBoxSweptWallFlux3DBuilder::sample(
            nx,ny,nz,dx,dt,old_box,new_box);
        const auto& g=geometry.geometry;
        const std::array<const std::vector<double>*,3> apertures{{
            &g.u_open,&g.v_open,&g.w_open}};
        for(int f=0;f<3;++f) {
            if(primal[f].size()!=apertures[f]->size())
                throw std::invalid_argument("Primal projection face shape");
            for(std::size_t i=0;i<primal[f].size();++i)
                if(!std::isfinite(primal[f][i])||
                   ((*apertures[f])[i]<=1e-12&&primal[f][i]!=0.0))
                    throw std::invalid_argument("Primal blocked/nonfinite face");
        }
        const auto projected=
            P2MovingBoxCutCellProjection3DBuilder::sample_with_predictor(
                nx,ny,nz,dx,dt,rho,old_box,new_box,
                primal[0],primal[1],primal[2]);
        const std::array<const std::vector<double>*,3> flux{{
            &projected.u_fluid_m3,&projected.v_fluid_m3,
            &projected.w_fluid_m3}};
        const double area_time=dt*dx*dx;
        if(!std::isfinite(area_time)||area_time<=0.0)
            throw std::invalid_argument("Primal area-time overflow");
        PrimalProjection result;
        result.max_continuity_residual_m3=
            projected.max_continuity_residual_m3;
        result.iterations=projected.cg_iterations;
        for(int f=0;f<3;++f) {
            result.corrected[f].assign(primal[f].size(),0.0);
            for(std::size_t i=0;i<primal[f].size();++i) {
                const double aperture=(*apertures[f])[i];
                if(aperture<=1e-12)continue;
                const double value=(*flux[f])[i]/(area_time*aperture);
                if(!std::isfinite(value))
                    throw std::runtime_error("Nonfinite corrected primal face");
                result.corrected[f][i]=value;
            }
        }
        return result;
    }
    static P2MovingMacDualTransportResult3D step_projected_primal(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const Faces& primal,const Faces& component_old) {
        const auto projected=project_primal(
            nx,ny,nz,dx,dt,rho,old_box,new_box,primal);
        return step(nx,ny,nz,dx,dt,rho,old_box,new_box,
                    projected.corrected,component_old);
    }

    // Conservatively advect signed dual component values using the
    // reconstructed common primal predictor and separately projected
    // moving-box dual fluxes. No caller-owned input is mutated.
    static P2MovingMacDualTransportResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const Faces& primal,const Faces& component_old) {
        if(!std::isfinite(rho)||rho<=0.0)
            throw std::invalid_argument("Invalid shared MAC density");
        const auto predicted=reconstruct(
            nx,ny,nz,dx,dt,old_box,new_box,primal);
        const auto dual=P2MovingMacDualVolumes3DBuilder::sample(
            nx,ny,nz,dx,dt,old_box,new_box);
        P2MovingMacDualTransportResult3D result;
        for(int a=0;a<3;++a) {
            const auto& s=dual.dimensions[a];
            auto old_shift=old_box,new_shift=new_box;
            old_shift.min_m[a]-=0.5*dx;
            old_shift.max_m[a]-=0.5*dx;
            new_shift.min_m[a]-=0.5*dx;
            new_shift.max_m[a]-=0.5*dx;
            const auto projected=
                P2MovingBoxCutCellProjection3DBuilder::sample_with_predictor(
                    s[0],s[1],s[2],dx,dt,rho,old_shift,new_shift,
                    predicted[a][0],predicted[a][1],predicted[a][2]);
            const auto advected=P2MovingCutCellScalarTransport3D::step(
                s[0],s[1],s[2],dx,component_old[a],projected);
            result.component_new[a]=advected.value_new;
            result.integral_before[a]=advected.integral_before_m3;
            result.integral_after[a]=advected.integral_after_m3;
            result.integral_residual[a]=advected.integral_residual_m3;
            result.projection_iterations[a]=projected.cg_iterations;
            result.active_faces[a]=advected.active_faces;
            result.max_gcl_residual=std::max(
                result.max_gcl_residual,advected.max_gcl_residual);
            result.max_donor_courant=std::max(
                result.max_donor_courant,advected.max_donor_courant);
        }
        return result;
    }
};
} // namespace upc
