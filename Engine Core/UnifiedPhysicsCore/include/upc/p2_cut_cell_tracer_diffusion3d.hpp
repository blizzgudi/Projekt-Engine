#pragma once
// P2 PARTIAL: conservative Fick-type tracer mixing through water-wetted
// stationary 3D cut-cell apertures AFTER an existing VOF/tracer step.
// No moving-body FSI, momentum coupling, or resolved two-phase interface law.
#include "upc/p2_static_cut_cell_vof3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2CutCellTracerDiffusionResult3D {
    P2StaticCutCellTracerResult3D advected;
    std::vector<double> concentration_after_kg_m3;
    std::vector<double> u_diffusion_flux_kg,v_diffusion_flux_kg,w_diffusion_flux_kg;
    double tracer_after_kg=0.0;
    double max_diffusion_cfl=0.0;
    std::size_t diffusion_substeps=1;
    // Discrete solute mixing functional Q=sum(wet_cell_volume*c^2)
    // [kg^2/m^3], NOT kinetic/thermal energy or thermodynamic entropy.
    // Populated only by the heterogeneous diffusion-subcycle path.
    bool diffusive_quadratic_audited=false;
    double diffusive_quadratic_before_kg2_m3=0.0;
    double diffusive_quadratic_after_kg2_m3=0.0;
    double diffusive_quadratic_loss_kg2_m3=0.0;
    // Exact forward-Euler no-flux identity, NOT physical energy:
    // Q_before-Q_after = 2*sum_faces(J*(c_a-c_b))
    //                    - sum_cells(V*(c_after-c_before)^2).
    double diffusive_quadratic_face_drive_kg2_m3=0.0;
    double diffusive_quadratic_euler_correction_kg2_m3=0.0;
    double diffusive_quadratic_identity_residual_kg2_m3=0.0;
};
class P2CutCellTracerDiffusion3D {
    // Scaled harmonic mean avoids reciprocal overflow for subnormal D
    // and multiplication overflow for large finite D.
    static double harmonic_face_diffusivity(double a,double b) {
        const double small=std::min(a,b),large=std::max(a,b);
        return small/(0.5+0.5*(small/large));
    }
public:
    // g = D*dt*dx*aperture*min(VOF_a,VOF_b) [m^3],
    // signed mass J = g*(c_a-c_b) [kg]. This wet-aperture rule is an
    // approximation; NOT a validated two-phase constitutive model.
    // Strict local sum(g)/liquid_volume <= 1 enforces explicit positivity.
    static P2CutCellTracerDiffusionResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& concentration_kg_m3,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,double diffusivity_m2_s) {
        return step_anisotropic(nx,ny,nz,dx,dt,stationary_box,fraction,
            concentration_kg_m3,u_m_s,v_m_s,w_m_s,
            {diffusivity_m2_s,diffusivity_m2_s,diffusivity_m2_s});
    }
    // Diagonal, grid-aligned diffusion tensor [D_x,D_y,D_z] in m^2/s.
    // Off-diagonal cross diffusion and physical anisotropic soil calibration
    // are NOT modeled; prescribed velocities and geometry remain unchanged.
    static P2CutCellTracerDiffusionResult3D step_anisotropic(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& concentration_kg_m3,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,
        const std::array<double,3>& diffusivity_axis_m2_s) {
        for(double d:diffusivity_axis_m2_s)
            if(!std::isfinite(d)||d<0.0)
                throw std::invalid_argument("Invalid axis tracer diffusivity");
        P2CutCellTracerDiffusionResult3D out;
        out.advected=P2StaticCutCellVof3D::step_passive_tracer(
            nx,ny,nz,dx,dt,stationary_box,fraction,concentration_kg_m3,
            u_m_s,v_m_s,w_m_s);
        out.concentration_after_kg_m3=out.advected.concentration_after_kg_m3;
        out.tracer_after_kg=out.advected.tracer_after_kg;
        out.u_diffusion_flux_kg.assign(u_m_s.size(),0.0);
        out.v_diffusion_flux_kg.assign(v_m_s.size(),0.0);
        out.w_diffusion_flux_kg.assign(w_m_s.size(),0.0);
        if(diffusivity_axis_m2_s[0]==0.0&&diffusivity_axis_m2_s[1]==0.0&&
           diffusivity_axis_m2_s[2]==0.0)return out; // Exact legacy parity.
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,stationary_box,stationary_box);
        const std::size_t n=out.concentration_after_kg_m3.size();
        const double cell=dx*dx*dx;
        const auto& f=out.advected.water.fraction_after;
        const auto& c=out.advected.concentration_after_kg_m3;
        std::vector<double> water(n),mass(n),sum_g(n,0.0);
        double max_c=0.0;
        for(std::size_t i=0;i<n;++i) {
            water[i]=geom.cell_geometry.alpha_old[i]*cell*f[i];
            mass[i]=water[i]*c[i];
            max_c=std::max(max_c,c[i]);
            if(!std::isfinite(water[i])||!std::isfinite(mass[i]))
                throw std::invalid_argument("Nonfinite diffusion inventory");
        }
        const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        const auto transfer=[&](double d,double aperture,std::size_t a,std::size_t b) {
            if(aperture<=0.0||f[a]<=0.0||f[b]<=0.0)return 0.0;
            const double g=d*dt*dx*aperture*std::min(f[a],f[b]);
            if(!std::isfinite(g)||g<0)
                throw std::invalid_argument("Nonfinite diffusive conductance");
            sum_g[a]+=g;
            sum_g[b]+=g;
            const double flux=g*(c[a]-c[b]);
            if(!std::isfinite(flux))
                throw std::invalid_argument("Nonfinite diffusive tracer flux");
            mass[a]-=flux;
            mass[b]+=flux;
            return flux;
        };
        std::size_t face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x,++face)
            if(x>0&&x<nx)
                out.u_diffusion_flux_kg[face]=transfer(
                    diffusivity_axis_m2_s[0],geom.u_open[face],id(x-1,y,z),id(x,y,z));
        face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            if(y>0&&y<ny)
                out.v_diffusion_flux_kg[face]=transfer(
                    diffusivity_axis_m2_s[1],geom.v_open[face],id(x,y-1,z),id(x,y,z));
        face=0;
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            if(z>0&&z<nz)
                out.w_diffusion_flux_kg[face]=transfer(
                    diffusivity_axis_m2_s[2],geom.w_open[face],id(x,y,z-1),id(x,y,z));
        const double tol=1e-11*std::max(1.0,out.advected.tracer_after_kg);
        out.tracer_after_kg=0.0;
        for(std::size_t i=0;i<n;++i) {
            if(water[i]==0.0) {
                if(sum_g[i]!=0.0||std::abs(mass[i])>tol)
                    throw std::logic_error("Diffusive solute in dry cell");
                continue;
            }
            const double cfl=sum_g[i]/water[i];
            if(!std::isfinite(cfl)||cfl>1.0+1e-12)
                throw std::invalid_argument("Explicit 3D tracer diffusion CFL exceeded");
            out.max_diffusion_cfl=std::max(out.max_diffusion_cfl,cfl);
            if(!std::isfinite(mass[i])||mass[i]<-tol||
               mass[i]>water[i]*max_c+tol)
                throw std::logic_error("Diffusive tracer positivity/bounds failed");
            const double next=std::max(0.0,mass[i]/water[i]);
            if(!std::isfinite(next)||next>max_c+1e-10*std::max(1.0,max_c))
                throw std::logic_error("Diffusive tracer concentration unbounded");
            out.concentration_after_kg_m3[i]=next;
            out.tracer_after_kg+=water[i]*next;
        }
        if(!std::isfinite(out.tracer_after_kg)||
           std::abs(out.tracer_after_kg-out.advected.tracer_after_kg)>tol)
            throw std::logic_error("Diffusive tracer mass not conserved");
        return out;
    }

    // Cell-wise diagonal diffusivity: harmonic face transmissivity across
    // heterogeneous material boundaries. Zero D blocks the interface.
    // This is a prescribed passive transport model, NOT calibrated soil FSI.
    static P2CutCellTracerDiffusionResult3D step_heterogeneous(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& concentration_kg_m3,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,
        const std::array<std::vector<double>,3>& diffusivity_cell_axis_m2_s) {
        const std::size_t n=fraction.size();
        for(const auto& axis:diffusivity_cell_axis_m2_s) {
            if(axis.size()!=n)throw std::invalid_argument("Heterogeneous diffusivity size");
            for(double d:axis)
                if(!std::isfinite(d)||d<0.0)
                    throw std::invalid_argument("Invalid heterogeneous diffusivity");
        }
        auto out=step_anisotropic(nx,ny,nz,dx,dt,stationary_box,fraction,
            concentration_kg_m3,u_m_s,v_m_s,w_m_s,{0.0,0.0,0.0});
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,stationary_box,stationary_box);
        const auto& f=out.advected.water.fraction_after;
        const double cell=dx*dx*dx;
        std::vector<double> water(n),mass(n),sum_g(n,0.0);
        double max_c=0.0;
        for(std::size_t i=0;i<n;++i) {
            water[i]=geom.cell_geometry.alpha_old[i]*cell*f[i];
            mass[i]=water[i]*out.concentration_after_kg_m3[i];
            max_c=std::max(max_c,out.concentration_after_kg_m3[i]);
            if(!std::isfinite(water[i])||!std::isfinite(mass[i]))
                throw std::invalid_argument("Nonfinite heterogeneous inventory");
        }
        const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        const auto transfer=[&](std::size_t axis,double aperture,
                                std::size_t a,std::size_t b) {
            const double da=diffusivity_cell_axis_m2_s[axis][a];
            const double db=diffusivity_cell_axis_m2_s[axis][b];
            if(aperture<=0.0||f[a]<=0.0||f[b]<=0.0||da==0.0||db==0.0)
                return 0.0;
            // Stable harmonic mean, avoiding overflow in da*db.
            const double d=harmonic_face_diffusivity(da,db);
            const double g=d*dt*dx*aperture*std::min(f[a],f[b]);
            if(!std::isfinite(g)||g<0.0)
                throw std::invalid_argument("Nonfinite heterogeneous conductance");
            sum_g[a]+=g;sum_g[b]+=g;
            const double flux=g*(out.advected.concentration_after_kg_m3[a]-
                                 out.advected.concentration_after_kg_m3[b]);
            if(!std::isfinite(flux))
                throw std::invalid_argument("Nonfinite heterogeneous flux");
            mass[a]-=flux;mass[b]+=flux;
            return flux;
        };
        std::size_t face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x,++face)
            if(x>0&&x<nx)
                out.u_diffusion_flux_kg[face]=transfer(
                    0,geom.u_open[face],id(x-1,y,z),id(x,y,z));
        face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            if(y>0&&y<ny)
                out.v_diffusion_flux_kg[face]=transfer(
                    1,geom.v_open[face],id(x,y-1,z),id(x,y,z));
        face=0;
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            if(z>0&&z<nz)
                out.w_diffusion_flux_kg[face]=transfer(
                    2,geom.w_open[face],id(x,y,z-1),id(x,y,z));
        const double tol=1e-11*std::max(1.0,out.advected.tracer_after_kg);
        out.tracer_after_kg=0.0;
        for(std::size_t i=0;i<n;++i) {
            if(water[i]==0.0) {
                if(sum_g[i]!=0.0||std::abs(mass[i])>tol)
                    throw std::logic_error("Heterogeneous solute in dry cell");
                continue;
            }
            const double cfl=sum_g[i]/water[i];
            if(!std::isfinite(cfl)||cfl>1.0+1e-12)
                throw std::invalid_argument("Heterogeneous diffusion CFL exceeded");
            out.max_diffusion_cfl=std::max(out.max_diffusion_cfl,cfl);
            if(!std::isfinite(mass[i])||mass[i]<-tol||
               mass[i]>water[i]*max_c+tol)
                throw std::logic_error("Heterogeneous concentration bounds");
            out.concentration_after_kg_m3[i]=std::max(0.0,mass[i]/water[i]);
            out.tracer_after_kg+=water[i]*out.concentration_after_kg_m3[i];
        }
        if(!std::isfinite(out.tracer_after_kg)||
           std::abs(out.tracer_after_kg-out.advected.tracer_after_kg)>tol)
            throw std::logic_error("Heterogeneous tracer mass not conserved");
        return out;
    }

    // Optional heterogeneous diffusion subcycling after ONE prescribed-MAC
    // advection. The exact per-cell conductance sum (not the maximum global
    // material D) determines the explicit stability budget on the wet geometry.
    // Flux ledgers accumulate each substep's updated donor concentrations.
    // Stationary geometry only; no porous-media calibration or FSI.
    static P2CutCellTracerDiffusionResult3D step_diffusion_subcycled_heterogeneous(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& concentration_kg_m3,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,
        const std::array<std::vector<double>,3>& diffusivity_cell_axis_m2_s,
        double target_diffusion_cfl=0.8,std::size_t max_substeps=1024) {
        if(!std::isfinite(target_diffusion_cfl)||
           target_diffusion_cfl<=0.0||target_diffusion_cfl>1.0||
           max_substeps==0||max_substeps>1024)
            throw std::invalid_argument("Invalid heterogeneous diffusion subcycle budget");
        const std::size_t n=fraction.size();
        bool any_diffusion=false;
        for(const auto& axis:diffusivity_cell_axis_m2_s) {
            if(axis.size()!=n)
                throw std::invalid_argument("Heterogeneous diffusivity size");
            for(double d:axis) {
                if(!std::isfinite(d)||d<0.0)
                    throw std::invalid_argument("Invalid heterogeneous diffusivity");
                any_diffusion=any_diffusion||(d>0.0);
            }
        }
        auto out=step_anisotropic(nx,ny,nz,dx,dt,stationary_box,fraction,
            concentration_kg_m3,u_m_s,v_m_s,w_m_s,{0.0,0.0,0.0});
        if(!any_diffusion)return out; // Preserve exact zero-D advection.
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,stationary_box,stationary_box);
        const auto& f=out.advected.water.fraction_after;
        const double cell=dx*dx*dx;
        // The symmetric no-flux finite-volume diffusion stencil is a
        // volume-weighted convex averaging operator at local CFL <= 1.
        // Jensen's inequality therefore requires Q_after <= Q_before.
        // This audits numerical mixing AFTER prescribed-MAC advection.
        const auto quadratic=[&](const std::vector<double>& c) {
            double q=0.0;
            for(std::size_t i=0;i<n;++i) {
                const double volume=geom.cell_geometry.alpha_old[i]*cell*f[i];
                const double term=volume*c[i]*c[i];
                if(!std::isfinite(term)||term<0.0)
                    throw std::invalid_argument("Nonfinite tracer quadratic inventory");
                q+=term;
            }
            if(!std::isfinite(q))
                throw std::invalid_argument("Nonfinite total tracer quadratic");
            return q;
        };
        out.diffusive_quadratic_before_kg2_m3=
            quadratic(out.advected.concentration_after_kg_m3);
        std::vector<double> conductance_sum(n,0.0);
        const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        const auto add_face=[&](std::size_t axis,double aperture,
                                std::size_t a,std::size_t b) {
            const double da=diffusivity_cell_axis_m2_s[axis][a];
            const double db=diffusivity_cell_axis_m2_s[axis][b];
            if(aperture<=0.0||f[a]<=0.0||f[b]<=0.0||
               da==0.0||db==0.0)return;
            const double d=harmonic_face_diffusivity(da,db);
            const double g=d*dt*dx*aperture*std::min(f[a],f[b]);
            if(!std::isfinite(g)||g<0.0)
                throw std::invalid_argument("Nonfinite heterogeneous conductance");
            conductance_sum[a]+=g;
            conductance_sum[b]+=g;
        };
        std::size_t face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x,++face)
            if(x>0&&x<nx)
                add_face(0,geom.u_open[face],id(x-1,y,z),id(x,y,z));
        face=0;
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            if(y>0&&y<ny)
                add_face(1,geom.v_open[face],id(x,y-1,z),id(x,y,z));
        face=0;
        for(std::size_t z=0;z<=nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x,++face)
            if(z>0&&z<nz)
                add_face(2,geom.w_open[face],id(x,y,z-1),id(x,y,z));
        double max_full_cfl=0.0;
        for(std::size_t i=0;i<n;++i) {
            const double water=geom.cell_geometry.alpha_old[i]*cell*f[i];
            if(water==0.0) {
                if(conductance_sum[i]!=0.0)
                    throw std::logic_error("Diffusion conductance in dry cell");
                continue;
            }
            const double local=conductance_sum[i]/water;
            if(!std::isfinite(local)||local<0.0)
                throw std::invalid_argument("Nonfinite heterogeneous diffusion CFL");
            max_full_cfl=std::max(max_full_cfl,local);
        }
        const double needed=std::ceil(max_full_cfl/target_diffusion_cfl);
        if(!std::isfinite(needed)||needed>static_cast<double>(max_substeps))
            throw std::invalid_argument("Heterogeneous diffusion subcycle budget exceeded");
        const std::size_t steps=std::max<std::size_t>(
            1,static_cast<std::size_t>(needed));
        out.diffusion_substeps=steps;
        const std::vector<double> zero_u(u_m_s.size(),0.0);
        const std::vector<double> zero_v(v_m_s.size(),0.0);
        const std::vector<double> zero_w(w_m_s.size(),0.0);
        const double sub_dt=dt/static_cast<double>(steps);
        for(std::size_t k=0;k<steps;++k) {
            const auto part=step_heterogeneous(nx,ny,nz,dx,sub_dt,
                stationary_box,f,out.concentration_after_kg_m3,
                zero_u,zero_v,zero_w,diffusivity_cell_axis_m2_s);
            // Each signed face flux is paired with its PRE-substep gradient.
            const auto& pre=out.concentration_after_kg_m3;
            std::size_t qface=0;
            for(std::size_t z=0;z<nz;++z)
            for(std::size_t y=0;y<ny;++y)
            for(std::size_t x=0;x<=nx;++x,++qface)
                if(x>0&&x<nx)
                    out.diffusive_quadratic_face_drive_kg2_m3+=
                        2.0*part.u_diffusion_flux_kg[qface]*
                        (pre[id(x-1,y,z)]-pre[id(x,y,z)]);
            qface=0;
            for(std::size_t z=0;z<nz;++z)
            for(std::size_t y=0;y<=ny;++y)
            for(std::size_t x=0;x<nx;++x,++qface)
                if(y>0&&y<ny)
                    out.diffusive_quadratic_face_drive_kg2_m3+=
                        2.0*part.v_diffusion_flux_kg[qface]*
                        (pre[id(x,y-1,z)]-pre[id(x,y,z)]);
            qface=0;
            for(std::size_t z=0;z<=nz;++z)
            for(std::size_t y=0;y<ny;++y)
            for(std::size_t x=0;x<nx;++x,++qface)
                if(z>0&&z<nz)
                    out.diffusive_quadratic_face_drive_kg2_m3+=
                        2.0*part.w_diffusion_flux_kg[qface]*
                        (pre[id(x,y,z-1)]-pre[id(x,y,z)]);
            for(std::size_t i=0;i<n;++i) {
                const double dc=part.concentration_after_kg_m3[i]-pre[i];
                const double volume=geom.cell_geometry.alpha_old[i]*cell*f[i];
                out.diffusive_quadratic_euler_correction_kg2_m3+=volume*dc*dc;
            }
            for(std::size_t i=0;i<out.u_diffusion_flux_kg.size();++i)
                out.u_diffusion_flux_kg[i]+=part.u_diffusion_flux_kg[i];
            for(std::size_t i=0;i<out.v_diffusion_flux_kg.size();++i)
                out.v_diffusion_flux_kg[i]+=part.v_diffusion_flux_kg[i];
            for(std::size_t i=0;i<out.w_diffusion_flux_kg.size();++i)
                out.w_diffusion_flux_kg[i]+=part.w_diffusion_flux_kg[i];
            out.concentration_after_kg_m3=part.concentration_after_kg_m3;
            out.tracer_after_kg=part.tracer_after_kg;
            out.max_diffusion_cfl=std::max(out.max_diffusion_cfl,
                                            part.max_diffusion_cfl);
        }
        const double tol=1e-10*std::max(1.0,out.advected.tracer_after_kg);
        if(std::abs(out.tracer_after_kg-out.advected.tracer_after_kg)>tol)
            throw std::logic_error("Subcycled heterogeneous tracer mass not conserved");
        out.diffusive_quadratic_after_kg2_m3=
            quadratic(out.concentration_after_kg_m3);
        out.diffusive_quadratic_loss_kg2_m3=
            out.diffusive_quadratic_before_kg2_m3-
            out.diffusive_quadratic_after_kg2_m3;
        out.diffusive_quadratic_identity_residual_kg2_m3=
            out.diffusive_quadratic_loss_kg2_m3-
            (out.diffusive_quadratic_face_drive_kg2_m3-
             out.diffusive_quadratic_euler_correction_kg2_m3);
        if(!std::isfinite(out.diffusive_quadratic_identity_residual_kg2_m3)||
           std::abs(out.diffusive_quadratic_identity_residual_kg2_m3)>
               1e-10*std::max(1.0,out.diffusive_quadratic_before_kg2_m3))
            throw std::logic_error("Heterogeneous diffusion quadratic identity failed");
        if(out.diffusive_quadratic_loss_kg2_m3 <
           -1e-11*std::max(1.0,out.diffusive_quadratic_before_kg2_m3))
            throw std::logic_error("Heterogeneous diffusion increased solute quadratic");
        out.diffusive_quadratic_audited=true;
        return out;
    }

    // Conservative opt-in: one prescribed-MAC advection step followed by
    // bounded explicit anisotropic diffusion on the resulting wet geometry.
    // A conservative six-neighbor bound accounts for small cut-cell volumes.
    // No moving geometry, momentum exchange or resolved interface physics.
    static P2CutCellTracerDiffusionResult3D step_diffusion_subcycled_anisotropic(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& stationary_box,const std::vector<double>& fraction,
        const std::vector<double>& concentration_kg_m3,
        const std::vector<double>& u_m_s,const std::vector<double>& v_m_s,
        const std::vector<double>& w_m_s,
        const std::array<double,3>& diffusivity_axis_m2_s,
        double target_diffusion_cfl=0.8,std::size_t max_substeps=1024) {
        if(!std::isfinite(target_diffusion_cfl)||
           target_diffusion_cfl<=0.0||target_diffusion_cfl>1.0||
           max_substeps==0||max_substeps>1024)
            throw std::invalid_argument("Invalid diffusion subcycle budget/CFL");
        for(double d:diffusivity_axis_m2_s)
            if(!std::isfinite(d)||d<0.0)
                throw std::invalid_argument("Invalid axis tracer diffusivity");
        auto out=step_anisotropic(nx,ny,nz,dx,dt,stationary_box,fraction,
            concentration_kg_m3,u_m_s,v_m_s,w_m_s,{0.0,0.0,0.0});
        if(diffusivity_axis_m2_s[0]==0.0&&diffusivity_axis_m2_s[1]==0.0&&
           diffusivity_axis_m2_s[2]==0.0)return out;
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,stationary_box,stationary_box);
        const auto& f=out.advected.water.fraction_after;
        double min_wet_alpha=1.0;
        for(std::size_t i=0;i<f.size();++i)
            if(f[i]>0.0) {
                const double alpha=geom.cell_geometry.alpha_old[i];
                if(!std::isfinite(alpha)||alpha<=0.0)
                    throw std::logic_error("Wet tracer in closed cut cell");
                min_wet_alpha=std::min(min_wet_alpha,alpha);
            }
        // Since min(f_a,f_b)<=f_a and every face aperture<=1,
        // local sum(g)/water <= 2*dt*sum(D)/(alpha*dx^2).
        const double bound=2.0*dt*(diffusivity_axis_m2_s[0]+
            diffusivity_axis_m2_s[1]+diffusivity_axis_m2_s[2])/
            (min_wet_alpha*dx*dx);
        const double needed=std::ceil(bound/target_diffusion_cfl);
        if(!std::isfinite(needed)||needed>static_cast<double>(max_substeps))
            throw std::invalid_argument("Diffusion subcycle budget exceeded");
        const std::size_t steps=std::max<std::size_t>(
            1,static_cast<std::size_t>(needed));
        out.diffusion_substeps=steps;
        const std::vector<double> zero_u(u_m_s.size(),0.0);
        const std::vector<double> zero_v(v_m_s.size(),0.0);
        const std::vector<double> zero_w(w_m_s.size(),0.0);
        const double sub_dt=dt/static_cast<double>(steps);
        for(std::size_t k=0;k<steps;++k) {
            const auto part=step_anisotropic(nx,ny,nz,dx,sub_dt,
                stationary_box,f,out.concentration_after_kg_m3,
                zero_u,zero_v,zero_w,diffusivity_axis_m2_s);
            for(std::size_t i=0;i<out.u_diffusion_flux_kg.size();++i)
                out.u_diffusion_flux_kg[i]+=part.u_diffusion_flux_kg[i];
            for(std::size_t i=0;i<out.v_diffusion_flux_kg.size();++i)
                out.v_diffusion_flux_kg[i]+=part.v_diffusion_flux_kg[i];
            for(std::size_t i=0;i<out.w_diffusion_flux_kg.size();++i)
                out.w_diffusion_flux_kg[i]+=part.w_diffusion_flux_kg[i];
            out.concentration_after_kg_m3=part.concentration_after_kg_m3;
            out.tracer_after_kg=part.tracer_after_kg;
            out.max_diffusion_cfl=std::max(out.max_diffusion_cfl,
                                            part.max_diffusion_cfl);
        }
        const double tol=1e-10*std::max(1.0,out.advected.tracer_after_kg);
        if(std::abs(out.tracer_after_kg-out.advected.tracer_after_kg)>tol)
            throw std::logic_error("Subcycled diffusion mass not conserved");
        return out;
    }
};
} // namespace upc
