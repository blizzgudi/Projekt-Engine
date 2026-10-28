#pragma once
// P2 PARTIAL: independently audit discrete pressure work on all three moving
// MAC dual grids reconstructed from one primal projected predictor.
// No common momentum tensor, rigid-body reaction, or physical energy closure.
#include "upc/p2_moving_mac_shared_predictor3d.hpp"
#include "upc/p2_moving_cut_cell_energy3d.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace upc {
struct P2MovingMacDualPressureWorkResult3D {
    std::array<P2MovingCutCellEnergyLedger3D,3> dual{};
    double primal_gcl_residual_m3=0.0;
    double sum_projection_change_j=0.0;
    double sum_geometric_pressure_work_j=0.0;
    double sum_correction_energy_j=0.0;
    double max_identity_residual_j=0.0;
};
class P2MovingMacDualPressureWork3D {
public:
    using Bridge=P2MovingMacSharedPredictor3D;
    static P2MovingMacDualPressureWorkResult3D audit(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const Bridge::Faces& primal) {
        const auto projected=Bridge::project_primal(
            nx,ny,nz,dx,dt,rho,old_box,new_box,primal);
        const auto predicted=Bridge::reconstruct(
            nx,ny,nz,dx,dt,old_box,new_box,projected.corrected);
        const auto dual=P2MovingMacDualVolumes3DBuilder::sample(
            nx,ny,nz,dx,dt,old_box,new_box);
        P2MovingMacDualPressureWorkResult3D result;
        result.primal_gcl_residual_m3=projected.max_continuity_residual_m3;
        for(int a=0;a<3;++a) {
            auto old_shift=old_box,new_shift=new_box;
            old_shift.min_m[a]-=0.5*dx;
            old_shift.max_m[a]-=0.5*dx;
            new_shift.min_m[a]-=0.5*dx;
            new_shift.max_m[a]-=0.5*dx;
            const auto& s=dual.dimensions[a];
            const auto q=P2MovingBoxCutCellProjection3DBuilder::sample_with_predictor(
                s[0],s[1],s[2],dx,dt,rho,old_shift,new_shift,
                predicted[a][0],predicted[a][1],predicted[a][2]);
            result.dual[a]=P2MovingCutCellPressureEnergy3D::audit(
                s[0],s[1],s[2],dx,dt,rho,
                predicted[a][0],predicted[a][1],predicted[a][2],q);
            const auto& e=result.dual[a];
            result.sum_projection_change_j+=e.projection_change_j;
            result.sum_geometric_pressure_work_j+=e.geometric_pressure_work_j;
            result.sum_correction_energy_j+=e.correction_energy_j;
            result.max_identity_residual_j=std::max({
                result.max_identity_residual_j,
                std::abs(e.predicted_identity_residual_j),
                std::abs(e.moving_wall_identity_residual_j),
                std::abs(e.projected_work_geometry_residual_j)});
        }
        const double closure=result.sum_projection_change_j-
            (result.sum_geometric_pressure_work_j-
             result.sum_correction_energy_j);
        const double tol=1e-8*std::max({
            1.0,std::abs(result.sum_projection_change_j),
            std::abs(result.sum_geometric_pressure_work_j)});
        if(!std::isfinite(closure)||std::abs(closure)>tol||
           !std::isfinite(result.max_identity_residual_j))
            throw std::runtime_error("Moving dual pressure work closure failed");
        return result;
    }
};
} // namespace upc
