#pragma once
// P2 PARTIAL: closed-box, variable-density two-phase 3D MAC pressure
// projection. Both water and air carry velocity; no VOF transport, moving
// walls, capillary jumps or stress-consistent full Navier-Stokes coupling.
// Transactional PCG with independent divergence, energy and wall-impulse ledgers.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2TwoPhaseMacPressureGrid3D {
    std::size_t nx=0,ny=0,nz=0;
    double dx_m=0,dt_s=0;
    std::vector<double> density_kg_m3; // water/air density at every cell
    std::vector<double> u_m_s,v_m_s,w_m_s; // standard staggered MAC layout
};
struct P2TwoPhaseMacPressureLedger3D {
    std::vector<double> pressure_pa;
    int pcg_iterations=0;
    double final_pressure_residual_s_inv=0;
    double divergence_before_s_inv=0,divergence_after_s_inv=0;
    double total_mass_kg=0;
    double kinetic_before_j=0,kinetic_after_j=0;
    double projection_dissipation_j=0,energy_residual_j=0;
    std::array<double,3> momentum_before_kg_m_s{},momentum_after_kg_m_s{};
    std::array<double,3> wall_pressure_impulse_n_s{},momentum_residual_n_s{};
};
inline P2TwoPhaseMacPressureLedger3D p2_two_phase_mac_pressure_step3d(
    P2TwoPhaseMacPressureGrid3D& grid,int max_iterations=4000) {
    const auto nx=grid.nx,ny=grid.ny,nz=grid.nz;
    if(nx<2||ny<2||nz<2||nx>128||ny>128||nz>128||
       nx*ny*nz>100000||max_iterations<1||max_iterations>20000||
       !(grid.dx_m>0&&grid.dt_s>0)||
       !std::isfinite(grid.dx_m)||!std::isfinite(grid.dt_s))
        throw std::invalid_argument("Invalid two-phase MAC projection geometry");
    const std::size_t n=nx*ny*nz;
    if(grid.density_kg_m3.size()!=n||
       grid.u_m_s.size()!=(nx+1)*ny*nz||
       grid.v_m_s.size()!=nx*(ny+1)*nz||
       grid.w_m_s.size()!=nx*ny*(nz+1))
        throw std::invalid_argument("Invalid two-phase MAC field dimensions");
    const double dx=grid.dx_m,dt=grid.dt_s;
    const double volume=dx*dx*dx,area=dx*dx,scale=1/(dx*dx);
    if(!(std::isfinite(volume)&&volume>0&&std::isfinite(scale)&&scale>0))
        throw std::invalid_argument("Invalid MAC cell volume");
    auto ci=[&](std::size_t x,std::size_t y,std::size_t z) {
        return (z*ny+y)*nx+x;
    };
    auto ui=[&](std::size_t x,std::size_t y,std::size_t z) {
        return (z*ny+y)*(nx+1)+x;
    };
    auto vi=[&](std::size_t x,std::size_t y,std::size_t z) {
        return (z*(ny+1)+y)*nx+x;
    };
    auto wi=[&](std::size_t x,std::size_t y,std::size_t z) {
        return (z*ny+y)*nx+x;
    };
    P2TwoPhaseMacPressureLedger3D out;
    for(double rho:grid.density_kg_m3) {
        if(!(rho>0&&std::isfinite(rho)&&std::isfinite(rho*volume)))
            throw std::invalid_argument("Invalid two-phase cell density");
        out.total_mass_kg+=rho*volume;
    }
    if(!std::isfinite(out.total_mass_kg))
        throw std::invalid_argument("Unresolved total two-phase mass");
    for(const auto* f:{&grid.u_m_s,&grid.v_m_s,&grid.w_m_s})
        for(double v:*f)if(!std::isfinite(v))
            throw std::invalid_argument("Nonfinite two-phase MAC velocity");
    // Impermeable box: reject rather than silently discard prescribed flux.
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        if(grid.u_m_s[ui(0,y,z)]!=0||grid.u_m_s[ui(nx,y,z)]!=0)
            throw std::invalid_argument("Nonzero x-wall normal velocity");
    for(std::size_t z=0;z<nz;++z)for(std::size_t x=0;x<nx;++x)
        if(grid.v_m_s[vi(x,0,z)]!=0||grid.v_m_s[vi(x,ny,z)]!=0)
            throw std::invalid_argument("Nonzero y-wall normal velocity");
    for(std::size_t y=0;y<ny;++y)for(std::size_t x=0;x<nx;++x)
        if(grid.w_m_s[wi(x,y,0)]!=0||grid.w_m_s[wi(x,y,nz)]!=0)
            throw std::invalid_argument("Nonzero z-wall normal velocity");
    struct Face {std::size_t a,b,index;int axis;double rho,beta;};
    std::vector<Face> faces;
    faces.reserve((nx-1)*ny*nz+nx*(ny-1)*nz+nx*ny*(nz-1));
    auto add=[&](std::size_t a,std::size_t b,std::size_t index,int axis){
        const double rho=.5*(grid.density_kg_m3[a]+grid.density_kg_m3[b]);
        const double beta=1/rho;
        if(!(rho>0&&std::isfinite(rho)&&beta>0&&std::isfinite(beta)))
            throw std::invalid_argument("Unresolved two-phase face density");
        faces.push_back({a,b,index,axis,rho,beta});
    };
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=1;x<nx;++x)
        add(ci(x-1,y,z),ci(x,y,z),ui(x,y,z),0);
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        add(ci(x,y-1,z),ci(x,y,z),vi(x,y,z),1);
    for(std::size_t z=1;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        add(ci(x,y,z-1),ci(x,y,z),wi(x,y,z),2);
    auto face_value=[&](const Face& f)->double {
        if(f.axis==0)return grid.u_m_s[f.index];
        if(f.axis==1)return grid.v_m_s[f.index];
        return grid.w_m_s[f.index];
    };
    std::vector<double> divergence(n,0),rhs(n,0),diag(n,0);
    for(const auto& f:faces){
        const double u=face_value(f);
        divergence[f.a]+=u/dx;divergence[f.b]-=u/dx;
        const double a=f.beta*scale;
        diag[f.a]+=a;diag[f.b]+=a;
        const double m=f.rho*volume;
        out.kinetic_before_j+=.5*m*u*u;
        out.momentum_before_kg_m_s[f.axis]+=m*u;
    }
    for(std::size_t i=0;i<n;++i){
        out.divergence_before_s_inv=std::max(
            out.divergence_before_s_inv,std::abs(divergence[i]));
        rhs[i]=-divergence[i]/dt;
    }
    // The closed-box Laplacian has a constant null mode; rhs is compatible
    // because each interior face contributes opposite flux to its two cells.
    const double mean=std::accumulate(rhs.begin(),rhs.end(),0.0)/double(n);
    for(double& v:rhs)v-=mean;
    std::vector<double> pressure(n,0),r=rhs,z(n),direction(n),ap(n);
    auto apply=[&](const std::vector<double>& p,std::vector<double>& result) {
        std::fill(result.begin(),result.end(),0);
        for(const auto& f:faces) {
            const double a=f.beta*scale*(p[f.a]-p[f.b]);
            result[f.a]+=a;result[f.b]-=a;
        }
    };
    double rz=0,bnorm2=0;
    for(std::size_t i=0;i<n;++i){
        z[i]=r[i]/diag[i];direction[i]=z[i];
        rz+=r[i]*z[i];bnorm2+=rhs[i]*rhs[i];
    }
    const double tol=1e-12*std::max(1.0,std::sqrt(bnorm2));
    double res2=0;
    for(double v:r)res2+=v*v;
    for(int it=0;res2>tol*tol&&it<max_iterations;++it){
        apply(direction,ap);
        double pap=0;
        for(std::size_t i=0;i<n;++i)pap+=direction[i]*ap[i];
        if(!(pap>0&&std::isfinite(pap)&&std::isfinite(rz)))
            throw std::runtime_error("Two-phase MAC PCG breakdown");
        const double alpha=rz/pap;
        double rznew=0;res2=0;
        for(std::size_t i=0;i<n;++i){
            pressure[i]+=alpha*direction[i];
            r[i]-=alpha*ap[i];
            z[i]=r[i]/diag[i];
            rznew+=r[i]*z[i];res2+=r[i]*r[i];
        }
        ++out.pcg_iterations;
        if(!std::isfinite(rznew)||!std::isfinite(res2))
            throw std::runtime_error("Nonfinite two-phase PCG iteration");
        if(res2<=tol*tol)break;
        const double beta=rznew/rz;
        for(std::size_t i=0;i<n;++i)direction[i]=z[i]+beta*direction[i];
        rz=rznew;
    }
    apply(pressure,ap);
    double residual2=0;
    for(std::size_t i=0;i<n;++i)
        residual2+=(rhs[i]-ap[i])*(rhs[i]-ap[i]);
    out.final_pressure_residual_s_inv=std::sqrt(residual2);
    if(!std::isfinite(residual2)||out.final_pressure_residual_s_inv>5*tol)
        throw std::runtime_error("Two-phase MAC PCG did not converge");
    // Work with temporary arrays. No input mutation until the full audit passes.
    auto u=grid.u_m_s,v=grid.v_m_s,w=grid.w_m_s;
    std::fill(divergence.begin(),divergence.end(),0);
    for(const auto& f:faces){
        const double old=face_value(f);
        const double change=-dt*f.beta*(pressure[f.b]-pressure[f.a])/dx;
        const double updated=old+change;
        if(!std::isfinite(updated))throw std::runtime_error("Nonfinite projected face");
        if(f.axis==0)u[f.index]=updated;
        else if(f.axis==1)v[f.index]=updated;
        else w[f.index]=updated;
        divergence[f.a]+=updated/dx;divergence[f.b]-=updated/dx;
        const double m=f.rho*volume;
        out.kinetic_after_j+=.5*m*updated*updated;
        out.projection_dissipation_j+=.5*m*change*change;
        out.momentum_after_kg_m_s[f.axis]+=m*updated;
    }
    for(double d:divergence)
        out.divergence_after_s_inv=std::max(out.divergence_after_s_inv,std::abs(d));
    // Discrete integration by parts: all pressure impulse on the fluid
    // comes from the six impermeable outer walls.
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        out.wall_pressure_impulse_n_s[0]+=dt*area*
            (pressure[ci(0,y,z)]-pressure[ci(nx-1,y,z)]);
    for(std::size_t z=0;z<nz;++z)for(std::size_t x=0;x<nx;++x)
        out.wall_pressure_impulse_n_s[1]+=dt*area*
            (pressure[ci(x,0,z)]-pressure[ci(x,ny-1,z)]);
    for(std::size_t y=0;y<ny;++y)for(std::size_t x=0;x<nx;++x)
        out.wall_pressure_impulse_n_s[2]+=dt*area*
            (pressure[ci(x,y,0)]-pressure[ci(x,y,nz-1)]);
    for(int k=0;k<3;++k)
        out.momentum_residual_n_s[k]=out.momentum_after_kg_m_s[k]-
            out.momentum_before_kg_m_s[k]-out.wall_pressure_impulse_n_s[k];
    out.energy_residual_j=out.kinetic_before_j-out.kinetic_after_j-
        out.projection_dissipation_j;
    const double pmean=std::accumulate(pressure.begin(),pressure.end(),0.0)/double(n);
    for(double& p:pressure)p-=pmean;
    out.pressure_pa=std::move(pressure);
    const double max_impulse=std::max({std::abs(out.momentum_residual_n_s[0]),
        std::abs(out.momentum_residual_n_s[1]),std::abs(out.momentum_residual_n_s[2])});
    const double impulse_scale=1+std::abs(out.momentum_before_kg_m_s[0])+
        std::abs(out.momentum_before_kg_m_s[1])+std::abs(out.momentum_before_kg_m_s[2]);
    const double energy_scale=1+std::abs(out.kinetic_before_j);
    if(out.divergence_after_s_inv>1e-9+10*dt*out.final_pressure_residual_s_inv||
       std::abs(out.energy_residual_j)>1e-9*energy_scale||
       max_impulse>1e-9*impulse_scale)
        throw std::runtime_error("Two-phase MAC physical ledger did not close");
    grid.u_m_s.swap(u);grid.v_m_s.swap(v);grid.w_m_s.swap(w);
    return out;
}
} // namespace upc
