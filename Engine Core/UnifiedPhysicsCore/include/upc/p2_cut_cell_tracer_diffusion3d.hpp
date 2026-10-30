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
};
class P2CutCellTracerDiffusion3D {
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
};
} // namespace upc
