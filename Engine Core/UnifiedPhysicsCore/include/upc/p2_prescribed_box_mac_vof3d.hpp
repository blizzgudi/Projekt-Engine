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
    // Previous corrected OPEN-face MAC velocity, not aperture-weighted VOF flux.
    // Feedback is opt-in and does not supply momentum advection or FSI.
    std::vector<double> last_u_,last_v_,last_w_;
    bool has_corrected_mac_=false;
    static std::vector<double> corrected_open_face_velocity(
        const std::vector<double>& swept_m3,
        const std::vector<double>& aperture,double dt,double dx) {
        const double area_time=dt*dx*dx;
        if(!std::isfinite(area_time)||area_time<=0.0||
           swept_m3.size()!=aperture.size())
            throw std::invalid_argument("Invalid corrected MAC face geometry");
        std::vector<double> out(swept_m3.size(),0.0);
        for(std::size_t i=0;i<out.size();++i) {
            if(aperture[i]>1e-12)
                out[i]=swept_m3[i]/(area_time*aperture[i]);
            else if(swept_m3[i]!=0.0)
                throw std::logic_error("Closed face has fluid flux");
            if(!std::isfinite(out[i]))
                throw std::invalid_argument("Nonfinite corrected MAC face velocity");
        }
        return out;
    }
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
    bool has_corrected_mac() const noexcept {return has_corrected_mac_;}
    const std::vector<double>& corrected_u_m_s() const noexcept {return last_u_;}
    const std::vector<double>& corrected_v_m_s() const noexcept {return last_v_;}
    const std::vector<double>& corrected_w_m_s() const noexcept {return last_w_;}

    // Feedback-only predictor: retain previous projected open-face velocities.
    // Zero faces that became closed in the new swept geometry. This is a
    // kinematic remask, NOT conservative momentum remapping/advection.
    // First call must use advance() with a supplied predictor.
    P2PrescribedBoxMacVofStep3D advance_reusing_projected_mac(
        const P2Box3D& next_box,double dt_s,
        std::size_t max_iterations=4000) {
        if(!has_corrected_mac_)
            throw std::logic_error("No corrected MAC state for feedback");
        const auto wall=P2MovingBoxSweptWallFlux3DBuilder::sample(
            nx_,ny_,nz_,dx_,dt_s,box_,next_box);
        auto u=last_u_,v=last_v_,w=last_w_;
        const auto remask=[](std::vector<double>& velocities,
                             const std::vector<double>& open) {
            if(velocities.size()!=open.size())
                throw std::logic_error("Feedback MAC face shape mismatch");
            for(std::size_t i=0;i<velocities.size();++i)
                if(open[i]<=1e-12)velocities[i]=0.0;
        };
        remask(u,wall.geometry.u_open);
        remask(v,wall.geometry.v_open);
        remask(w,wall.geometry.w_open);
        return advance(next_box,dt_s,u,v,w,max_iterations);
    }

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
        // Extract true corrected MAC velocities on the time-averaged open
        // face area. Do not feed aperture-weighted effective VOF speeds back
        // into the pressure predictor; that would double-apply apertures.
        auto next_u=corrected_open_face_velocity(
            projection.u_fluid_m3,projection.wall.geometry.u_open,dt_s,dx_);
        auto next_v=corrected_open_face_velocity(
            projection.v_fluid_m3,projection.wall.geometry.v_open,dt_s,dx_);
        auto next_w=corrected_open_face_velocity(
            projection.w_fluid_m3,projection.wall.geometry.w_open,dt_s,dx_);
        auto next_alpha=geom.alpha_new;
        auto next_water=vof.water_cell_fraction;
        P2PrescribedBoxMacVofStep3D result{
            std::move(projection),std::move(vof),steps_+1};
        alpha_.swap(next_alpha);
        water_.swap(next_water);
        last_u_.swap(next_u);
        last_v_.swap(next_v);
        last_w_.swap(next_w);
        has_corrected_mac_=true;
        box_=next_box;
        ++steps_;
        return result;
    }
};
} // namespace upc
