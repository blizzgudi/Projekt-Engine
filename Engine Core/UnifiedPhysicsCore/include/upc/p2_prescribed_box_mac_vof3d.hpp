#pragma once
// P2 PARTIAL: opt-in prescribed rigid-box multi-step MAC projection + VOF.
// One closed domain, constant density; NO two-way FSI, free surface,
// independent measured CFD validation or live CoupledMacWater3D integration.
#include "upc/p2_moving_box_cut_cell_projection3d.hpp"
#include "upc/p2_fractional_boundary_vof_flux3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2PrescribedBoxMacVofStep3D {
    P2MovingBoxCutCellProjection3D projection;
    FractionalBoundaryVofResult3D vof;
    std::size_t step_index=0;
};

class P2PrescribedBoxMacVof3D {
    std::size_t nx_,ny_,nz_,steps_=0;
    double dx_,rho_;
    P2Box3D box_;
    std::vector<double> alpha_,water_;
    static std::vector<double> effective_face_velocity(
        const std::vector<double>& swept_m3,double dt,double dx) {
        const double denom=dt*dx*dx;
        if(!std::isfinite(denom)||denom<=0.0)
            throw std::invalid_argument("Invalid effective MAC swept-volume scale");
        std::vector<double> v;
        v.reserve(swept_m3.size());
        for(double q:swept_m3) {
            const double speed=q/denom; // aperture-weighted; VOF expects swept volume
            if(!std::isfinite(speed))
                throw std::invalid_argument("Nonfinite effective MAC swept velocity");
            v.push_back(speed);
        }
        return v;
    }
public:
    P2PrescribedBoxMacVof3D(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,
        double density_kg_m3,const P2Box3D& initial_box,
        std::vector<double> initial_water_fraction={})
        :nx_(nx),ny_(ny),nz_(nz),dx_(dx_m),rho_(density_kg_m3),
         box_(initial_box) {
        if(!nx||!ny||!nz||nx>32768/ny||nx*ny>32768/nz||
           !std::isfinite(rho_)||rho_<=0.0)
            throw std::invalid_argument("Invalid prescribed box MAC/VOF grid or density");
        auto geometry=P2MovingBoxOpenVolumes3D::sample(
            nx_,ny_,nz_,dx_,1.0,box_,box_);
        alpha_=std::move(geometry.alpha_old);
        if(initial_water_fraction.empty())initial_water_fraction=alpha_;
        if(initial_water_fraction.size()!=alpha_.size())
            throw std::invalid_argument("Initial water shape mismatch");
        for(std::size_t i=0;i<alpha_.size();++i)
            if(!std::isfinite(initial_water_fraction[i])||
               initial_water_fraction[i]<0.0||
               initial_water_fraction[i]>alpha_[i])
                throw std::invalid_argument("Initial water exceeds open volume");
        water_=std::move(initial_water_fraction);
    }
    const P2Box3D& box() const noexcept {return box_;}
    const std::vector<double>& water_fraction() const noexcept {return water_;}
    const std::vector<double>& open_fraction() const noexcept {return alpha_;}
    std::size_t step_count() const noexcept {return steps_;}

    // Predictor is externally supplied every step (not inferred from the
    // previous step). All state changes are transactional: failures in
    // projection, donor CFL, or VOF conservation leave the state unchanged.
    P2PrescribedBoxMacVofStep3D advance(
        const P2Box3D& next_box,double dt_s,
        const std::vector<double>& u_pred_m_s,
        const std::vector<double>& v_pred_m_s,
        const std::vector<double>& w_pred_m_s,
        std::size_t max_iterations=4000) {
        auto projection=P2MovingBoxCutCellProjection3DBuilder::sample_with_predictor(
            nx_,ny_,nz_,dx_,dt_s,rho_,box_,next_box,
            u_pred_m_s,v_pred_m_s,w_pred_m_s,max_iterations);
        const auto& geom=projection.wall.geometry.cell_geometry;
        for(std::size_t i=0;i<alpha_.size();++i)
            if(std::abs(geom.alpha_old[i]-alpha_[i])>1e-12)
                throw std::logic_error("Previous moving box geometry mismatch");
        auto u=effective_face_velocity(projection.u_fluid_m3,dt_s,dx_);
        auto v=effective_face_velocity(projection.v_fluid_m3,dt_s,dx_);
        auto w=effective_face_velocity(projection.w_fluid_m3,dt_s,dx_);
        auto vof=FractionalBoundaryVofFlux3D::step(
            nx_,ny_,nz_,dx_,dt_s,alpha_,geom.alpha_new,water_,u,v,w);
        auto next_alpha=geom.alpha_new;
        auto next_water=vof.water_cell_fraction;
        P2PrescribedBoxMacVofStep3D result{
            std::move(projection),std::move(vof),steps_+1};
        alpha_.swap(next_alpha);
        water_.swap(next_water);
        box_=next_box;
        ++steps_;
        return result;
    }
};
} // namespace upc
