#pragma once
// P2 PARTIAL: numerical audit of the mismatch between ONE projected primal
// MAC predictor and THREE separately projected moving dual-grid fluxes.
// This quantifies a consistency gap; it is NOT a shared momentum tensor or FSI.
#include "upc/p2_moving_mac_shared_predictor3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace upc {
struct P2MovingMacFluxConsistencyAudit3D {
    double primal_gcl_residual_m3=0.0;
    double max_dual_gcl_residual_m3=0.0;
    double max_flux_correction_m3=0.0;
    double sum_abs_flux_correction_m3=0.0;
    double sum_abs_predicted_flux_m3=0.0;
    std::array<double,3> component_abs_correction_m3{};
    std::array<std::size_t,3> active_dual_faces{};
    // No physical closure is inferred even when correction is zero.
};
class P2MovingMacFluxConsistency3D {
public:
    using Bridge=P2MovingMacSharedPredictor3D;
    static P2MovingMacFluxConsistencyAudit3D audit(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const Bridge::Faces& primal) {
        const auto p=Bridge::project_primal(
            nx,ny,nz,dx,dt,rho,old_box,new_box,primal);
        const auto reconstructed=Bridge::reconstruct(
            nx,ny,nz,dx,dt,old_box,new_box,p.corrected);
        const auto dual=P2MovingMacDualVolumes3DBuilder::sample(
            nx,ny,nz,dx,dt,old_box,new_box);
        const double scale=dt*dx*dx;
        if(!std::isfinite(scale)||scale<=0.0)
            throw std::invalid_argument("Invalid flux audit area-time");
        P2MovingMacFluxConsistencyAudit3D out;
        out.primal_gcl_residual_m3=p.max_continuity_residual_m3;
        for(int a=0;a<3;++a) {
            auto old_shift=old_box,new_shift=new_box;
            old_shift.min_m[a]-=0.5*dx;
            old_shift.max_m[a]-=0.5*dx;
            new_shift.min_m[a]-=0.5*dx;
            new_shift.max_m[a]-=0.5*dx;
            const auto& s=dual.dimensions[a];
            const auto q=P2MovingBoxCutCellProjection3DBuilder::sample_with_predictor(
                s[0],s[1],s[2],dx,dt,rho,old_shift,new_shift,
                reconstructed[a][0],reconstructed[a][1],reconstructed[a][2]);
            out.max_dual_gcl_residual_m3=std::max(
                out.max_dual_gcl_residual_m3,q.max_continuity_residual_m3);
            const auto& g=q.wall.geometry;
            const std::array<const std::vector<double>*,3> apertures{{
                &g.u_open,&g.v_open,&g.w_open}};
            const std::array<const std::vector<double>*,3> fluxes{{
                &q.u_fluid_m3,&q.v_fluid_m3,&q.w_fluid_m3}};
            for(int f=0;f<3;++f) {
                if(fluxes[f]->size()!=apertures[f]->size()||
                   fluxes[f]->size()!=reconstructed[a][f].size())
                    throw std::runtime_error("Inconsistent dual flux shapes");
                for(std::size_t i=0;i<fluxes[f]->size();++i) {
                    const double predicted=scale*(*apertures[f])[i]*
                        reconstructed[a][f][i];
                    const double corrected=(*fluxes[f])[i];
                    if(!std::isfinite(predicted)||!std::isfinite(corrected))
                        throw std::runtime_error("Nonfinite dual flux audit");
                    const double difference=std::abs(corrected-predicted);
                    out.max_flux_correction_m3=std::max(
                        out.max_flux_correction_m3,difference);
                    out.sum_abs_flux_correction_m3+=difference;
                    out.sum_abs_predicted_flux_m3+=std::abs(predicted);
                    out.component_abs_correction_m3[a]+=difference;
                    if((*apertures[f])[i]>1e-12)
                        ++out.active_dual_faces[a];
                }
            }
        }
        if(!std::isfinite(out.sum_abs_flux_correction_m3)||
           !std::isfinite(out.sum_abs_predicted_flux_m3))
            throw std::runtime_error("Dual flux audit accumulation overflow");
        return out;
    }
};
} // namespace upc
