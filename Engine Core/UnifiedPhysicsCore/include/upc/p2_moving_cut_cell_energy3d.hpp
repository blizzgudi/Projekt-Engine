#pragma once
// P2 PARTIAL: pressure-work / staggered-face kinetic proxy for a translating
// box in a CLOSED, constant-density 3D cut-cell MAC/VOF step.
// Exact discrete projection identity, NOT physical free-surface or FSI energy.
#include "upc/p2_prescribed_box_mac_vof3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2MovingCutCellEnergyLedger3D {
    double predicted_face_kinetic_j=0.0;
    double projected_face_kinetic_j=0.0;
    double projection_change_j=0.0;
    double correction_energy_j=0.0;
    double predicted_pressure_work_j=0.0;
    double projected_pressure_work_j=0.0;
    double geometric_pressure_work_j=0.0;
    double predicted_identity_residual_j=0.0;
    double moving_wall_identity_residual_j=0.0;
    double projected_work_geometry_residual_j=0.0;
    double max_pressure_gradient_residual_m_s=0.0;
    double max_continuity_residual_m3=0.0;
};

class P2MovingCutCellPressureEnergy3D {
    using Vec=std::vector<double>;
public:
    // Interior edge flux q = a*dt*dx^2*u, and
    // delta_u = dt/(rho*dx)*(p_left-p_right).
    // Discrete summation by parts for a closed moving cut-cell domain:
    //   K_after-K_pred = sum_cell p*(alpha_old-alpha_new)*dx^3
    //                    - 0.5*rho*dx^3*sum_interior a*(delta_u)^2.
    // This is a FACE kinetic proxy (aperture/time averaged), not a full
    // fluid/rigid-body energy budget. No state or inputs are modified.
    static P2MovingCutCellEnergyLedger3D audit(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const Vec& pred_u,const Vec& pred_v,const Vec& pred_w,
        const P2MovingBoxCutCellProjection3D& projected) {
        if(nx<2||ny<2||nz<2||nx>32768/ny||nx*ny>32768/nz||
           !std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0||
           !std::isfinite(rho)||rho<=0)
            throw std::invalid_argument("Invalid moving cut-cell energy grid");
        const std::size_t n=nx*ny*nz;
        const std::size_t us=(nx+1)*ny*nz,vs=nx*(ny+1)*nz,ws=nx*ny*(nz+1);
        const auto& g=projected.wall.geometry;
        const auto& old_a=g.cell_geometry.alpha_old;
        const auto& new_a=g.cell_geometry.alpha_new;
        const auto& pressure=projected.pressure_pa;
        if(old_a.size()!=n||new_a.size()!=n||pressure.size()!=n||
           pred_u.size()!=us||pred_v.size()!=vs||pred_w.size()!=ws||
           g.u_open.size()!=us||g.v_open.size()!=vs||g.w_open.size()!=ws||
           projected.u_fluid_m3.size()!=us||
           projected.v_fluid_m3.size()!=vs||
           projected.w_fluid_m3.size()!=ws)
            throw std::invalid_argument("Moving cut-cell energy field shape");
        const double vol=dx*dx*dx, area_time=dt*dx*dx;
        const double energy_scale=0.5*rho*vol;
        const double gradient_scale=dt/(rho*dx);
        if(!std::isfinite(vol)||!std::isfinite(area_time)||
           !std::isfinite(energy_scale)||!std::isfinite(gradient_scale)||
           vol<=0||area_time<=0)
            throw std::invalid_argument("Moving cut-cell energy scale overflow");
        P2MovingCutCellEnergyLedger3D out;
        std::vector<double> divergence(n,0.0);
        const auto ci=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        // Process every face exactly once, including zero-flux outer walls.
        // Reject nonzero predictors or corrected flux on closed faces.
        const auto edge=[&](double a,double predicted,double q,
                            bool interior,std::size_t left,std::size_t right) {
            if(!std::isfinite(a)||a<0||a>1.0+1e-12||
               !std::isfinite(predicted)||!std::isfinite(q))
                throw std::invalid_argument("Nonfinite/invalid cut-cell face");
            if(!interior||a<=1e-12) {
                if(predicted!=0.0||std::abs(q)>1e-13)
                    throw std::invalid_argument("Nonzero closed cut-cell face");
                return;
            }
            const double diff=pressure[left]-pressure[right];
            const double after=q/(a*area_time);
            const double delta=after-predicted;
            const double expected=gradient_scale*diff;
            const double before_q=a*area_time*predicted;
            out.max_pressure_gradient_residual_m_s=std::max(
                out.max_pressure_gradient_residual_m_s,std::abs(delta-expected));
            out.predicted_face_kinetic_j+=energy_scale*a*predicted*predicted;
            out.projected_face_kinetic_j+=energy_scale*a*after*after;
            out.correction_energy_j+=energy_scale*a*delta*delta;
            out.predicted_pressure_work_j+=before_q*diff;
            out.projected_pressure_work_j+=q*diff;
            divergence[left]+=q;
            divergence[right]-=q;
        };
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x) {
            const auto f=(z*ny+y)*(nx+1)+x;
            const bool inside=x>0&&x<nx;
            edge(g.u_open[f],pred_u[f],projected.u_fluid_m3[f],inside,
                 inside?ci(x-1,y,z):0,inside?ci(x,y,z):0);
        }
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const auto f=(z*(ny+1)+y)*nx+x;
            const bool inside=y>0&&y<ny;
            edge(g.v_open[f],pred_v[f],projected.v_fluid_m3[f],inside,
                 inside?ci(x,y-1,z):0,inside?ci(x,y,z):0);
        }
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const auto f=(z*ny+y)*nx+x;
            const bool inside=z>0&&z<nz;
            edge(g.w_open[f],pred_w[f],projected.w_fluid_m3[f],inside,
                 inside?ci(x,y,z-1):0,inside?ci(x,y,z):0);
        }
        for(std::size_t i=0;i<n;++i) {
            if(!std::isfinite(pressure[i])||!std::isfinite(old_a[i])||
               !std::isfinite(new_a[i])||old_a[i]<0||new_a[i]<0||
               old_a[i]>1.0+1e-12||new_a[i]>1.0+1e-12)
                throw std::invalid_argument("Invalid moving cut-cell cell field");
            const double expected=(old_a[i]-new_a[i])*vol;
            out.geometric_pressure_work_j+=pressure[i]*expected;
            out.max_continuity_residual_m3=std::max(
                out.max_continuity_residual_m3,std::abs(divergence[i]-expected));
        }
        out.projection_change_j=out.projected_face_kinetic_j-
                                out.predicted_face_kinetic_j;
        out.predicted_identity_residual_j=out.projection_change_j-
            (out.predicted_pressure_work_j+out.correction_energy_j);
        out.moving_wall_identity_residual_j=out.projection_change_j-
            (out.geometric_pressure_work_j-out.correction_energy_j);
        out.projected_work_geometry_residual_j=
            out.projected_pressure_work_j-out.geometric_pressure_work_j;
        const double tolerance=1e-8*std::max({1.0,
            std::abs(out.predicted_face_kinetic_j),
            std::abs(out.projected_face_kinetic_j),
            std::abs(out.geometric_pressure_work_j)});
        for(double value:{out.predicted_face_kinetic_j,
            out.projected_face_kinetic_j,out.projection_change_j,
            out.correction_energy_j,out.predicted_pressure_work_j,
            out.projected_pressure_work_j,out.geometric_pressure_work_j,
            out.predicted_identity_residual_j,
            out.moving_wall_identity_residual_j,
            out.projected_work_geometry_residual_j,
            out.max_pressure_gradient_residual_m_s,
            out.max_continuity_residual_m3})
            if(!std::isfinite(value))
                throw std::runtime_error("Nonfinite cut-cell energy audit");
        if(std::abs(out.predicted_identity_residual_j)>tolerance||
           std::abs(out.moving_wall_identity_residual_j)>tolerance||
           std::abs(out.projected_work_geometry_residual_j)>tolerance||
           out.max_pressure_gradient_residual_m_s>1e-9||
           out.max_continuity_residual_m3>1e-9*std::max(1.0,vol))
            throw std::runtime_error("Moving cut-cell pressure work audit failed");
        return out;
    }
};

// Opt-in transactional coupling to the existing sequential MAC/VOF path.
// Audit runs BEFORE the new water/box/MAC state is committed. Copying the
// bounded state is intentional; this is a diagnostic, not a fast solver.
struct P2AuditedPrescribedBoxStep3D {
    P2PrescribedBoxMacVofStep3D state;
    P2MovingCutCellEnergyLedger3D energy;
};
class P2AuditedPrescribedBoxMacVof3D {
    P2PrescribedBoxMacVof3D state_;
    std::size_t nx_,ny_,nz_;
    double dx_,rho_;
public:
    P2AuditedPrescribedBoxMacVof3D(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double rho,
        const P2Box3D& box,std::vector<double> water={})
        :state_(nx,ny,nz,dx,rho,box,std::move(water)),
         nx_(nx),ny_(ny),nz_(nz),dx_(dx),rho_(rho) {}
    const P2PrescribedBoxMacVof3D& state() const noexcept {return state_;}
    P2AuditedPrescribedBoxStep3D advance(
        const P2Box3D& next,double dt,
        const std::vector<double>& u,const std::vector<double>& v,
        const std::vector<double>& w,std::size_t iterations=4000) {
        auto candidate=state_;
        auto step=candidate.advance(next,dt,u,v,w,iterations);
        auto ledger=P2MovingCutCellPressureEnergy3D::audit(
            nx_,ny_,nz_,dx_,dt,rho_,u,v,w,step.projection);
        state_=std::move(candidate);
        return {std::move(step),ledger};
    }
};
} // namespace upc
