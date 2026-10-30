#pragma once
// P2 PARTIAL: first-order conservative 3D donor-cell VOF transport through
// stationary box cut-cell apertures using prescribed solenoidal MAC velocities.
// NOT a Navier-Stokes step, free-surface stress, or two-way moving-body FSI.
#include "upc/p2_moving_box_mac_apertures3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2StaticCutCellVofResult3D {
    std::vector<double> fraction_after;
    double water_before_m3=0.0;
    double water_after_m3=0.0;
    double max_cell_volume_divergence_m3=0.0;
    double max_donor_cfl=0.0;
    std::size_t substeps_used=1;
    // Signed water volumes crossing positive U/V/W MAC faces [m^3].
    // Time-sequence/subcycling results contain cumulative face fluxes.
    // Diagnostic finite-volume transport only, NOT fluid momentum/FSI.
    std::vector<double> u_water_flux_m3,v_water_flux_m3,w_water_flux_m3;
};
// One-step passive dissolved tracer carried by the *actual* upwind water
// face volumes. This is a conservative scalar coupling, not diffusion,
// chemical reactions, a fluid momentum equation, or FSI.
struct P2StaticCutCellTracerResult3D {
    P2StaticCutCellVofResult3D water;
    std::vector<double> concentration_after_kg_m3;
    double tracer_before_kg=0.0;
    double tracer_after_kg=0.0;
    // Signed solute mass through positive U/V/W faces [kg].
    // In adaptive runs these are sums of actual evolving-donor substeps.
    std::vector<double> u_tracer_flux_kg,v_tracer_flux_kg,w_tracer_flux_kg;
};
class P2StaticCutCellVof3D {
    // Accumulate signed water volumes across every face, not only total mass.
    // This ledger telescopes over any prescribed-velocity time sequence.
    static void add_water_fluxes(P2StaticCutCellVofResult3D& total,
                                 const P2StaticCutCellVofResult3D& next) {
        const auto add=[](std::vector<double>& dst,
                          const std::vector<double>& src) {
            if(dst.empty())dst.assign(src.size(),0.0);
            if(dst.size()!=src.size())
                throw std::logic_error("Incompatible VOF face flux ledger");
            for(std::size_t i=0;i<src.size();++i) {
                dst[i]+=src[i];
                if(!std::isfinite(dst[i]))
                    throw std::logic_error("Nonfinite cumulative VOF flux");
            }
        };
        add(total.u_water_flux_m3,next.u_water_flux_m3);
        add(total.v_water_flux_m3,next.v_water_flux_m3);
        add(total.w_water_flux_m3,next.w_water_flux_m3);
    }
    static void add_tracer_fluxes(P2StaticCutCellTracerResult3D& total,
                                  const P2StaticCutCellTracerResult3D& next) {
        const auto add=[](std::vector<double>& dst,
                          const std::vector<double>& src) {
            if(dst.empty())dst.assign(src.size(),0.0);
            if(dst.size()!=src.size())
                throw std::logic_error("Incompatible tracer face flux ledger");
            for(std::size_t i=0;i<src.size();++i) {
                dst[i]+=src[i];
                if(!std::isfinite(dst[i]))
                    throw std::logic_error("Nonfinite cumulative tracer flux");
            }
        };
        add(total.u_tracer_flux_kg,next.u_tracer_flux_kg);
        add(total.v_tracer_flux_kg,next.v_tracer_flux_kg);
        add(total.w_tracer_flux_kg,next.w_tracer_flux_kg);
    }
public:
    static P2StaticCutCellVofResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s) {
        if(!std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0)
            throw std::invalid_argument("Invalid static VOF dx/dt");
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,stationary_box,stationary_box);
        const std::size_t n=nx*ny*nz;
        if(fraction.size()!=n||u_m_s.size()!=(nx+1)*ny*nz||
           v_m_s.size()!=nx*(ny+1)*nz||w_m_s.size()!=nx*ny*(nz+1))
            throw std::invalid_argument("Static VOF array size mismatch");
        const double cell=dx*dx*dx;
        const double tol=1e-11*cell;
        std::vector<double> volume(n),change(n,0.0),divergence(n,0.0),
                            outgoing(n,0.0);
        P2StaticCutCellVofResult3D out;
        out.fraction_after.assign(n,0.0);
        out.u_water_flux_m3.assign(u_m_s.size(),0.0);
        out.v_water_flux_m3.assign(v_m_s.size(),0.0);
        out.w_water_flux_m3.assign(w_m_s.size(),0.0);
        for(std::size_t i=0;i<n;++i) {
            if(!std::isfinite(fraction[i])||fraction[i]<0||fraction[i]>1)
                throw std::invalid_argument("Invalid VOF fraction");
            volume[i]=geom.cell_geometry.alpha_old[i]*cell;
            if(volume[i]<=tol&&fraction[i]!=0)
                throw std::invalid_argument("Water fraction inside solid");
            out.water_before_m3+=volume[i]*fraction[i];
        }
        const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        const auto apply=[&](double velocity,double aperture,bool boundary,
                             std::size_t a,std::size_t b) {
            if(!std::isfinite(velocity))
                throw std::invalid_argument("Nonfinite MAC velocity");
            if(boundary||aperture<=1e-14) {
                if(std::abs(velocity)>1e-12)
                    throw std::invalid_argument("Flow through sealed face");
                return 0.0;
            }
            const double q=velocity*aperture*dx*dx*dt;
            if(!std::isfinite(q))
                throw std::invalid_argument("Nonfinite integrated volume flux");
            const double water=q*(q>=0?fraction[a]:fraction[b]);
            change[a]-=water;
            change[b]+=water;
            divergence[a]+=q;
            divergence[b]-=q;
            if(q>=0)outgoing[a]+=q;
            else outgoing[b]-=q;
            return water;
        };
        std::size_t face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x,++face)
            out.u_water_flux_m3[face]=apply(
                  u_m_s[face],geom.u_open[face],x==0||x==nx,
                  x? id(x-1,y,z):0,x<nx?id(x,y,z):0);
        face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            out.v_water_flux_m3[face]=apply(
                  v_m_s[face],geom.v_open[face],y==0||y==ny,
                  y?id(x,y-1,z):0,y<ny?id(x,y,z):0);
        face=0;
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            out.w_water_flux_m3[face]=apply(
                  w_m_s[face],geom.w_open[face],z==0||z==nz,
                  z?id(x,y,z-1):0,z<nz?id(x,y,z):0);
        for(std::size_t i=0;i<n;++i) {
            out.max_cell_volume_divergence_m3=std::max(
                out.max_cell_volume_divergence_m3,std::abs(divergence[i]));
            if(std::abs(divergence[i])>tol)
                throw std::invalid_argument("Non-solenoidal MAC volume flux");
            if(outgoing[i]>volume[i]+tol)
                throw std::invalid_argument("Static VOF donor CFL exceeded");
            if(volume[i]>tol)
                out.max_donor_cfl=std::max(out.max_donor_cfl,
                                          outgoing[i]/volume[i]);
            const double next=volume[i]*fraction[i]+change[i];
            if(!std::isfinite(next)||next < -tol||next>volume[i]+tol)
                throw std::invalid_argument("Static VOF fraction not bounded");
            if(volume[i]>tol)
                out.fraction_after[i]=std::clamp(next/volume[i],0.0,1.0);
            else if(std::abs(next)>tol)
                throw std::invalid_argument("Water inside closed cell");
            out.water_after_m3+=volume[i]*out.fraction_after[i];
        }
        if(std::abs(out.water_after_m3-out.water_before_m3)>
           1e-9*std::max(1.0,out.water_before_m3))
            throw std::logic_error("Static VOF water mass changed");
        return out;
    }
    // Advect a nonnegative passive solute [kg/m^3 of liquid water] with
    // exactly the signed MAC water fluxes produced by one VOF step.
    // Dry cells must have zero concentration. No solute crosses solid walls.
    // Only a single step is supported: cumulative fluxes from subcycling
    // cannot be multiplied by one initial concentration field.
    static P2StaticCutCellTracerResult3D step_passive_tracer(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& concentration_kg_m3,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s) {
        P2StaticCutCellTracerResult3D out;
        out.water=step(nx,ny,nz,dx,dt,stationary_box,fraction,u_m_s,v_m_s,w_m_s);
        const std::size_t n=out.water.fraction_after.size();
        if(concentration_kg_m3.size()!=n)
            throw std::invalid_argument("Tracer concentration array size mismatch");
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,stationary_box,stationary_box);
        const double cell=dx*dx*dx;
        std::vector<double> inventory(n,0.0);
        double max_concentration=0.0;
        for(std::size_t i=0;i<n;++i) {
            const double c=concentration_kg_m3[i];
            if(!std::isfinite(c)||c<0)
                throw std::invalid_argument("Invalid passive tracer concentration");
            const double water=geom.cell_geometry.alpha_old[i]*cell*fraction[i];
            if(water==0.0&&c!=0.0)
                throw std::invalid_argument("Tracer concentration in dry cell");
            inventory[i]=water*c;
            if(!std::isfinite(inventory[i]))
                throw std::invalid_argument("Nonfinite tracer inventory");
            out.tracer_before_kg+=inventory[i];
            max_concentration=std::max(max_concentration,c);
        }
        if(!std::isfinite(out.tracer_before_kg))
            throw std::invalid_argument("Nonfinite total tracer inventory");
        const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        out.u_tracer_flux_kg.assign(out.water.u_water_flux_m3.size(),0.0);
        out.v_tracer_flux_kg.assign(out.water.v_water_flux_m3.size(),0.0);
        out.w_tracer_flux_kg.assign(out.water.w_water_flux_m3.size(),0.0);
        const auto transfer=[&](double water,std::size_t a,std::size_t b) {
            if(water==0.0)return 0.0;
            const double solute=water*(water>=0?
                                      concentration_kg_m3[a]:
                                      concentration_kg_m3[b]);
            if(!std::isfinite(solute))
                throw std::invalid_argument("Nonfinite transported tracer");
            inventory[a]-=solute;
            inventory[b]+=solute;
            return solute;
        };
        std::size_t face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x,++face)
            if(x>0&&x<nx)
                out.u_tracer_flux_kg[face]=transfer(out.water.u_water_flux_m3[face],
                         id(x-1,y,z),id(x,y,z));
        face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            if(y>0&&y<ny)
                out.v_tracer_flux_kg[face]=transfer(out.water.v_water_flux_m3[face],
                         id(x,y-1,z),id(x,y,z));
        face=0;
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            if(z>0&&z<nz)
                out.w_tracer_flux_kg[face]=transfer(out.water.w_water_flux_m3[face],
                         id(x,y,z-1),id(x,y,z));
        out.concentration_after_kg_m3.assign(n,0.0);
        const double tolerance=1e-11*std::max(1.0,out.tracer_before_kg);
        for(std::size_t i=0;i<n;++i) {
            const double water=geom.cell_geometry.alpha_old[i]*cell*
                               out.water.fraction_after[i];
            const double mass=inventory[i];
            if(!std::isfinite(mass)||mass < -tolerance||
               mass > water*max_concentration+tolerance)
                throw std::logic_error("Passive tracer positivity/bounds failed");
            if(water==0.0) {
                if(std::abs(mass)>tolerance)
                    throw std::logic_error("Passive tracer stranded in dry cell");
            } else {
                const double c=std::max(0.0,mass/water);
                if(!std::isfinite(c)||c>max_concentration+
                    1e-10*std::max(1.0,max_concentration))
                    throw std::logic_error("Passive tracer concentration unbounded");
                out.concentration_after_kg_m3[i]=c;
            }
            out.tracer_after_kg+=water*out.concentration_after_kg_m3[i];
        }
        if(!std::isfinite(out.tracer_after_kg)||
           std::abs(out.tracer_after_kg-out.tracer_before_kg)>tolerance)
            throw std::logic_error("Passive tracer mass not conserved");
        return out;
    }
    // Carry passive dissolved tracer through every actual adaptive VOF
    // substep. A cumulative water-flux ledger cannot be used with one
    // initial concentration: donor concentration evolves after each step.
    // All velocities are prescribed and the cut-cell geometry is stationary.
    // This is NOT reactive chemistry, Navier-Stokes, or two-way FSI.
    static P2StaticCutCellTracerResult3D step_passive_tracer_sequence_adaptive(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& concentration_kg_m3,
        const std::vector<double>& durations_s,
        const std::vector<std::vector<double>>& u_steps,
        const std::vector<std::vector<double>>& v_steps,
        const std::vector<std::vector<double>>& w_steps,
        double target_donor_cfl=0.8,std::size_t max_total_substeps=1024) {
        const auto slices=durations_s.size();
        if(slices==0||slices>1024||u_steps.size()!=slices||
           v_steps.size()!=slices||w_steps.size()!=slices||
           max_total_substeps==0||max_total_substeps>1024||
           slices>max_total_substeps||
           !std::isfinite(target_donor_cfl)||target_donor_cfl<=0||
           target_donor_cfl>1)
            throw std::invalid_argument("Invalid adaptive tracer sequence controls");
        auto state=fraction;
        auto concentration=concentration_kg_m3;
        P2StaticCutCellTracerResult3D total;
        total.water.substeps_used=0;
        for(std::size_t k=0;k<slices;++k) {
            const double dt=durations_s[k];
            if(!std::isfinite(dt)||dt<=0)
                throw std::invalid_argument("Invalid adaptive tracer slice duration");
            const auto remaining=max_total_substeps-total.water.substeps_used;
            const double pilot_dt=dt/static_cast<double>(remaining);
            if(!std::isfinite(pilot_dt)||pilot_dt<=0)
                throw std::invalid_argument("Invalid adaptive tracer pilot duration");
            const auto pilot=step(nx,ny,nz,dx,pilot_dt,stationary_box,
                                  state,u_steps[k],v_steps[k],w_steps[k]);
            const double ratio=pilot.max_donor_cfl*
                               static_cast<double>(remaining)/target_donor_cfl;
            if(!std::isfinite(ratio)||ratio>static_cast<double>(remaining))
                throw std::invalid_argument("Adaptive tracer CFL budget exceeded");
            const auto count=std::max<std::size_t>(
                1,static_cast<std::size_t>(std::ceil(ratio)));
            const double h=dt/static_cast<double>(count);
            if(!std::isfinite(h)||h<=0)
                throw std::invalid_argument("Invalid adaptive tracer substep");
            for(std::size_t j=0;j<count;++j) {
                auto next=step_passive_tracer(
                    nx,ny,nz,dx,h,stationary_box,state,concentration,
                    u_steps[k],v_steps[k],w_steps[k]);
                if(k==0&&j==0) {
                    total.water.water_before_m3=next.water.water_before_m3;
                    total.tracer_before_kg=next.tracer_before_kg;
                }
                total.water.water_after_m3=next.water.water_after_m3;
                total.tracer_after_kg=next.tracer_after_kg;
                add_water_fluxes(total.water,next.water);
                add_tracer_fluxes(total,next);
                total.water.max_donor_cfl=std::max(
                    total.water.max_donor_cfl,next.water.max_donor_cfl);
                total.water.max_cell_volume_divergence_m3=std::max(
                    total.water.max_cell_volume_divergence_m3,
                    next.water.max_cell_volume_divergence_m3);
                state=std::move(next.water.fraction_after);
                concentration=std::move(next.concentration_after_kg_m3);
            }
            total.water.substeps_used+=count;
        }
        total.water.fraction_after=std::move(state);
        total.concentration_after_kg_m3=std::move(concentration);
        if(std::abs(total.tracer_after_kg-total.tracer_before_kg)>
           1e-9*std::max(1.0,total.tracer_before_kg))
            throw std::logic_error("Adaptive tracer mass drift");
        return total;
    }
    // Explicit temporal subcycling; prescribed MAC velocity and fixed geometry.
    // This is not a dynamic Navier-Stokes or two-way FSI integration.
    static P2StaticCutCellVofResult3D step_subcycled(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,std::size_t substeps) {
        if(substeps==0||substeps>1024)
            throw std::invalid_argument("Invalid VOF substep count");
        const double h=dt/static_cast<double>(substeps);
        if(!std::isfinite(dt)||dt<=0||!std::isfinite(h)||h<=0)
            throw std::invalid_argument("Invalid VOF subcycle time");
        auto state=fraction;
        P2StaticCutCellVofResult3D total;
        for(std::size_t k=0;k<substeps;++k) {
            auto next=step(nx,ny,nz,dx,h,stationary_box,state,u_m_s,v_m_s,w_m_s);
            if(k==0)total.water_before_m3=next.water_before_m3;
            total.water_after_m3=next.water_after_m3;
            add_water_fluxes(total,next);
            total.max_donor_cfl=std::max(total.max_donor_cfl,next.max_donor_cfl);
            total.max_cell_volume_divergence_m3=std::max(
                total.max_cell_volume_divergence_m3,
                next.max_cell_volume_divergence_m3);
            state=std::move(next.fraction_after);
        }
        total.fraction_after=std::move(state);
        total.substeps_used=substeps;
        return total;
    }
    // Time-varying prescribed MAC velocities on stationary cut-cells.
    // Passive VOF only: no dynamic Navier-Stokes or two-way FSI.
    static P2StaticCutCellVofResult3D step_velocity_sequence(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<std::vector<double>>& u_steps,
        const std::vector<std::vector<double>>& v_steps,
        const std::vector<std::vector<double>>& w_steps) {
        const auto count=u_steps.size();
        if(count==0||count>1024||v_steps.size()!=count||
           w_steps.size()!=count||!std::isfinite(dt)||dt<=0)
            throw std::invalid_argument("Invalid prescribed VOF velocity sequence");
        const double h=dt/static_cast<double>(count);
        if(!std::isfinite(h)||h<=0)
            throw std::invalid_argument("Invalid prescribed VOF sequence dt");
        auto state=fraction;
        P2StaticCutCellVofResult3D total;
        for(std::size_t k=0;k<count;++k) {
            auto next=step(nx,ny,nz,dx,h,stationary_box,state,
                           u_steps[k],v_steps[k],w_steps[k]);
            if(k==0)total.water_before_m3=next.water_before_m3;
            total.water_after_m3=next.water_after_m3;
            add_water_fluxes(total,next);
            total.max_donor_cfl=std::max(total.max_donor_cfl,next.max_donor_cfl);
            total.max_cell_volume_divergence_m3=std::max(
                total.max_cell_volume_divergence_m3,
                next.max_cell_volume_divergence_m3);
            state=std::move(next.fraction_after);
        }
        total.fraction_after=std::move(state);
        total.substeps_used=count;
        return total;
    }
    // Unequal time slices of prescribed MAC fields, with a shared CFL budget.
    // Passive transport only; not a Navier-Stokes or two-way FSI solver.
    static P2StaticCutCellVofResult3D step_velocity_sequence_adaptive(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& durations_s,
        const std::vector<std::vector<double>>& u_steps,
        const std::vector<std::vector<double>>& v_steps,
        const std::vector<std::vector<double>>& w_steps,
        double target_donor_cfl=0.8,std::size_t max_total_substeps=1024) {
        const auto count=durations_s.size();
        if(count==0||count>1024||u_steps.size()!=count||
           v_steps.size()!=count||w_steps.size()!=count||
           max_total_substeps==0||max_total_substeps>1024||
           count>max_total_substeps||
           !std::isfinite(target_donor_cfl)||target_donor_cfl<=0||
           target_donor_cfl>1)
            throw std::invalid_argument("Invalid adaptive VOF sequence controls");
        auto state=fraction;
        P2StaticCutCellVofResult3D total;
        total.substeps_used=0;
        for(std::size_t k=0;k<count;++k) {
            if(!std::isfinite(durations_s[k])||durations_s[k]<=0)
                throw std::invalid_argument("Invalid adaptive VOF slice duration");
            const auto remaining=max_total_substeps-total.substeps_used;
            auto next=step_auto_subcycled(
                nx,ny,nz,dx,durations_s[k],stationary_box,state,
                u_steps[k],v_steps[k],w_steps[k],target_donor_cfl,remaining);
            if(k==0)total.water_before_m3=next.water_before_m3;
            total.water_after_m3=next.water_after_m3;
            add_water_fluxes(total,next);
            total.max_donor_cfl=std::max(total.max_donor_cfl,next.max_donor_cfl);
            total.max_cell_volume_divergence_m3=std::max(
                total.max_cell_volume_divergence_m3,
                next.max_cell_volume_divergence_m3);
            total.substeps_used+=next.substeps_used;
            state=std::move(next.fraction_after);
        }
        total.fraction_after=std::move(state);
        return total;
    }
    // Select a safe number of substeps from the *geometric* outgoing
    // donor-CFL, independent of the evolving water fraction. A pilot
    // step at dt/max_substeps validates the same sealed-face, cut-cell,
    // and solenoidal constraints as the production step. No solver CFL
    // or pressure dynamics are inferred from this passive VOF bound.
    static P2StaticCutCellVofResult3D step_auto_subcycled(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,double target_donor_cfl=0.8,
        std::size_t max_substeps=1024) {
        if(!std::isfinite(target_donor_cfl)||target_donor_cfl<=0||
           target_donor_cfl>1||max_substeps==0||max_substeps>1024)
            throw std::invalid_argument("Invalid automatic VOF CFL controls");
        if(!std::isfinite(dt)||dt<=0)
            throw std::invalid_argument("Invalid automatic VOF dt");
        const double pilot_dt=dt/static_cast<double>(max_substeps);
        if(!std::isfinite(pilot_dt)||pilot_dt<=0)
            throw std::invalid_argument("Invalid automatic VOF pilot dt");
        const auto pilot=step(nx,ny,nz,dx,pilot_dt,stationary_box,
                              fraction,u_m_s,v_m_s,w_m_s);
        const double full_cfl=pilot.max_donor_cfl*
                              static_cast<double>(max_substeps);
        const double ratio=full_cfl/target_donor_cfl;
        if(!std::isfinite(ratio)||ratio>static_cast<double>(max_substeps))
            throw std::invalid_argument("Automatic VOF CFL budget exceeded");
        const auto count=std::max<std::size_t>(
            1,static_cast<std::size_t>(std::ceil(ratio)));
        return step_subcycled(nx,ny,nz,dx,dt,stationary_box,fraction,
                              u_m_s,v_m_s,w_m_s,count);
    }
};
} // namespace upc
