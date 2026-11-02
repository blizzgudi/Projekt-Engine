#pragma once
// P2 PARTIAL: independent manufactured Young-Laplace equilibrium reference.
// The exact spherical curvature 2/R is supplied externally to isolate
// pressure-PCG balancing error from VOF curvature reconstruction error.
// This is NOT a production curvature model, PLIC, FSI or measured CFD PASS.
#include "upc/p2_two_phase_capillary_mac_vof3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2CapillaryLaplaceReferenceResult3D {
    P2TwoPhaseMacPressureLedger3D pressure;
    double analytic_jump_pa=0;
    double recovered_pure_phase_jump_pa=0;
    double max_pressure_field_error_pa=0;
    double max_predicted_velocity_m_s=0;
    double max_post_projection_velocity_m_s=0;
    double water_mass_kg=0,air_mass_kg=0;
    std::size_t pure_water_cells=0,pure_air_cells=0;
    bool measured_cfd_validated=false;
    bool phase_complete=false;
};
class P2CapillaryLaplaceReference3D {
public:
    static P2CapillaryLaplaceReferenceResult3D audit(
        const P2TwoPhaseMacVofState3D& state,double sigma_n_m,
        double sphere_radius_m,int max_pcg_iterations=4000) {
        const auto& m=state.mac;
        if(!(std::isfinite(sigma_n_m)&&sigma_n_m>=0)||
           !(std::isfinite(sphere_radius_m)&&sphere_radius_m>0)||
           !(std::isfinite(m.dx_m)&&m.dx_m>0)||
           !(std::isfinite(m.dt_s)&&m.dt_s>0)||
           m.nx<3||m.ny<3||m.nz<3||m.nx>128||m.ny>128||m.nz>128||
           m.nx*m.ny*m.nz>100000||
           state.water_fraction.size()!=m.nx*m.ny*m.nz||
           m.density_kg_m3.size()!=m.nx*m.ny*m.nz||
           m.u_m_s.size()!=(m.nx+1)*m.ny*m.nz||
           m.v_m_s.size()!=m.nx*(m.ny+1)*m.nz||
           m.w_m_s.size()!=m.nx*m.ny*(m.nz+1)||
           !(std::isfinite(state.water_density_kg_m3)&&
             std::isfinite(state.air_density_kg_m3)&&
             state.water_density_kg_m3>state.air_density_kg_m3&&
             state.air_density_kg_m3>0))
            throw std::invalid_argument("Invalid manufactured Laplace state");
        const double dx=m.dx_m,dt=m.dt_s;
        if(sigma_n_m>0&&dt>std::sqrt(
            state.air_density_kg_m3*dx*dx*dx/
            (2*3.14159265358979323846*sigma_n_m)))
            throw std::invalid_argument("Unstable manufactured capillary timestep");
        for(const auto* face:{&m.u_m_s,&m.v_m_s,&m.w_m_s})
            for(double v:*face)
                if(!std::isfinite(v)||v!=0)
                    throw std::invalid_argument("Laplace equilibrium must start at rest");
        const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*m.ny+y)*m.nx+x;
        };
        P2CapillaryLaplaceReferenceResult3D out;
        const double volume=dx*dx*dx;
        const double curvature=2/sphere_radius_m;
        out.analytic_jump_pa=sigma_n_m*curvature;
        if(!std::isfinite(out.analytic_jump_pa))
            throw std::invalid_argument("Unresolved Laplace pressure jump");
        double alpha_sum=0;
        for(std::size_t i=0;i<state.water_fraction.size();++i){
            const double a=state.water_fraction[i];
            const double rho=state.air_density_kg_m3+
                (state.water_density_kg_m3-state.air_density_kg_m3)*a;
            if(!(std::isfinite(a)&&a>=0&&a<=1&&
                 std::isfinite(m.density_kg_m3[i])&&
                 std::abs(m.density_kg_m3[i]-rho)<=1e-12*std::max(1.,rho)))
                throw std::invalid_argument("Inconsistent Laplace phase density");
            alpha_sum+=a;
            out.water_mass_kg+=a*state.water_density_kg_m3*volume;
            out.air_mass_kg+=(1-a)*state.air_density_kg_m3*volume;
            if(a>=.999999)++out.pure_water_cells;
            if(a<=.000001)++out.pure_air_cells;
        }
        if(!out.pure_water_cells||!out.pure_air_cells||
           !std::isfinite(out.water_mass_kg)||!std::isfinite(out.air_mass_kg))
            throw std::invalid_argument("Need resolved pure water and air cells");
        // Construct a discrete pressure-gradient force on the same MAC
        // faces and with the same arithmetic face densities as PCG.
        // This is a manufactured reference, not a guessed curvature solver.
        auto trial=m;
        const auto apply=[&](std::size_t a,std::size_t b,double& face){
            const double rho=.5*(m.density_kg_m3[a]+m.density_kg_m3[b]);
            const double dv=dt*out.analytic_jump_pa*
                (state.water_fraction[b]-state.water_fraction[a])/(dx*rho);
            if(!std::isfinite(dv))
                throw std::overflow_error("Nonfinite Laplace source velocity");
            face=dv;
            out.max_predicted_velocity_m_s=std::max(
                out.max_predicted_velocity_m_s,std::abs(dv));
        };
        for(std::size_t z=0;z<m.nz;++z)
        for(std::size_t y=0;y<m.ny;++y)
        for(std::size_t x=1;x<m.nx;++x)
            apply(ci(x-1,y,z),ci(x,y,z),
                  trial.u_m_s[(z*m.ny+y)*(m.nx+1)+x]);
        for(std::size_t z=0;z<m.nz;++z)
        for(std::size_t y=1;y<m.ny;++y)
        for(std::size_t x=0;x<m.nx;++x)
            apply(ci(x,y-1,z),ci(x,y,z),
                  trial.v_m_s[(z*(m.ny+1)+y)*m.nx+x]);
        for(std::size_t z=1;z<m.nz;++z)
        for(std::size_t y=0;y<m.ny;++y)
        for(std::size_t x=0;x<m.nx;++x)
            apply(ci(x,y,z-1),ci(x,y,z),
                  trial.w_m_s[(z*m.ny+y)*m.nx+x]);
        out.pressure=p2_two_phase_mac_pressure_step3d(
            trial,max_pcg_iterations);
        const double alpha_mean=alpha_sum/double(state.water_fraction.size());
        double water_p=0,air_p=0;
        for(std::size_t i=0;i<state.water_fraction.size();++i){
            const double a=state.water_fraction[i];
            const double expected=out.analytic_jump_pa*(a-alpha_mean);
            out.max_pressure_field_error_pa=std::max(
                out.max_pressure_field_error_pa,
                std::abs(out.pressure.pressure_pa[i]-expected));
            if(a>=.999999)water_p+=out.pressure.pressure_pa[i];
            if(a<=.000001)air_p+=out.pressure.pressure_pa[i];
        }
        out.recovered_pure_phase_jump_pa=
            water_p/double(out.pure_water_cells)-
            air_p/double(out.pure_air_cells);
        for(const auto* face:{&trial.u_m_s,&trial.v_m_s,&trial.w_m_s})
            for(double v:*face)
                out.max_post_projection_velocity_m_s=std::max(
                    out.max_post_projection_velocity_m_s,std::abs(v));
        if(!std::isfinite(out.max_post_projection_velocity_m_s)||
           !std::isfinite(out.max_pressure_field_error_pa))
            throw std::runtime_error("Invalid manufactured Laplace result");
        return out;
    }
};
} // namespace upc
