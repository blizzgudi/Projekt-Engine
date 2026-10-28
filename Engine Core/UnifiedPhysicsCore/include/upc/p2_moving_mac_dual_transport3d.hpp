#pragma once
// P2 PARTIAL: conservative transport of signed values stored on exact
// moving staggered MAC dual volumes. Each dual uses an independently
// projected CLOSED-DOMAIN volume flux. This is a numerical transport
// precursor, NOT Navier-Stokes momentum self-advection, traction or FSI.
#include "upc/p2_moving_mac_dual_volumes3d.hpp"
#include "upc/p2_moving_box_cut_cell_projection3d.hpp"
#include "upc/p2_moving_cut_cell_scalar_transport3d.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MovingMacDualTransportResult3D {
    std::array<std::vector<double>,3> component_new;
    std::array<double,3> integral_before{},integral_after{},integral_residual{};
    std::array<std::size_t,3> projection_iterations{},active_faces{};
    double max_gcl_residual=0.0,max_donor_courant=0.0;
};
class P2MovingMacDualTransport3D {
public:
    // Three signed values per unit open dual volume (u/v/w).
    // For each shifted staggered grid, project a zero predictor onto
    // the geometric continuity constraint, then conservatively advect
    // its stored component with that projected flux. No state is mutated
    // if a projection or transport fails. Independent dual projections
    // are NOT guaranteed to be a single consistent MAC momentum flux.
    static P2MovingMacDualTransportResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,double density_kg_m3,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const std::array<std::vector<double>,3>& component_old) {
        if(!std::isfinite(density_kg_m3)||density_kg_m3<=0.0)
            throw std::invalid_argument("Invalid dual transport density");
        const auto dual=P2MovingMacDualVolumes3DBuilder::sample(
            nx,ny,nz,dx_m,dt_s,old_box,new_box);
        P2MovingMacDualTransportResult3D result;
        for(int axis=0;axis<3;++axis) {
            const auto& s=dual.dimensions[axis];
            auto a=old_box,b=new_box;
            a.min_m[axis]-=0.5*dx_m;
            a.max_m[axis]-=0.5*dx_m;
            b.min_m[axis]-=0.5*dx_m;
            b.max_m[axis]-=0.5*dx_m;
            const auto projected=P2MovingBoxCutCellProjection3DBuilder::sample(
                s[0],s[1],s[2],dx_m,dt_s,density_kg_m3,a,b);
            const auto transport=P2MovingCutCellScalarTransport3D::step(
                s[0],s[1],s[2],dx_m,component_old[axis],projected);
            result.component_new[axis]=transport.value_new;
            result.integral_before[axis]=transport.integral_before_m3;
            result.integral_after[axis]=transport.integral_after_m3;
            result.integral_residual[axis]=transport.integral_residual_m3;
            result.projection_iterations[axis]=projected.cg_iterations;
            result.active_faces[axis]=transport.active_faces;
            result.max_gcl_residual=std::max(
                result.max_gcl_residual,transport.max_gcl_residual);
            result.max_donor_courant=std::max(
                result.max_donor_courant,transport.max_donor_courant);
        }
        return result;
    }
};
} // namespace upc
