#pragma once
// P2 PARTIAL: opt-in passive water-fraction remap using the actual projected
// dynamic-box MAC fluxes. This is NOT a free-surface stress solve, nor a
// globally energy/momentum-conservative two-way fluid/solid solver.
#include "upc/p2_moving_box_projected_reaction3d.hpp"
#include "upc/p2_moving_cut_cell_scalar_transport3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MovingBoxProjectedVofResult3D {
    P2MovingBoxProjectedReactionResult3D reaction;
    P2MovingCutCellScalarResult3D transport;
    double water_mass_before_kg=0.0;
    double water_mass_after_kg=0.0;
    double water_mass_residual_kg=0.0;
};

class P2MovingBoxProjectedVof3D {
public:
    using Faces=P2MovingBoxProjectedReaction3D::Faces;
    // Fill is water volume / OPEN cell volume, in [0,1]; a fully
    // blocked old cell must contain zero water. The same projected flux
    // is used for the MAC velocity reconstruction and conservative
    // scalar finite-volume transport. The existing donor CFL/GCL and
    // boundedness checks are intentionally fail-closed, not bypassed.
    static P2MovingBoxProjectedVofResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& box,const RigidPressureState3D& body,
        const std::vector<double>& pressure_pa,const Faces& before,
        const std::vector<double>& old_water_fraction) {
        if(nx==0||ny==0||nz==0||nx>32768/ny||
           nx*ny>32768/nz||old_water_fraction.size()!=nx*ny*nz)
            throw std::invalid_argument("Invalid projected VOF shape");
        for(const double f:old_water_fraction)
            if(!std::isfinite(f)||f<0.0||f>1.0)
                throw std::invalid_argument("Invalid projected VOF fraction");
        P2MovingBoxProjectedVofResult3D result;
        result.reaction=P2MovingBoxProjectedReaction3D::step(
            nx,ny,nz,dx,dt,rho,box,body,pressure_pa,before);
        result.transport=P2MovingCutCellScalarTransport3D::step(
            nx,ny,nz,dx,old_water_fraction,result.reaction.projection);
        result.water_mass_before_kg=rho*result.transport.integral_before_m3;
        result.water_mass_after_kg=rho*result.transport.integral_after_m3;
        result.water_mass_residual_kg=
            result.water_mass_after_kg-result.water_mass_before_kg;
        if(!std::isfinite(result.water_mass_residual_kg)||
           std::abs(result.water_mass_residual_kg)>
           1e-10*std::max(1.0,result.water_mass_before_kg))
            throw std::runtime_error("Projected VOF water mass not conserved");
        return result;
    }
};
} // namespace upc
