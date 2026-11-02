#pragma once
// P2 PARTIAL: globally implicit, simultaneous finite-volume tangential
// water/air shear on a FIXED, sharp, cell-centered 3D interface.
// A mass-weighted SPD solve removes the sequential-face ordering bias.
// No live MAC/VOF, pressure, interface motion, normal traction or FSI.
#include "upc/p2_two_phase_grid_shear3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
// Optional cell-resolved properties, useful for nonuniform water/air material fields.
// Empty arrays preserve the original constant-property operator exactly.
struct P2GlobalShearCellProperties3D {
    std::vector<double> density_kg_m3;
    std::vector<double> dynamic_viscosity_pa_s;
};
struct P2GlobalShearLedger3D {
    std::size_t interface_faces=0;
    std::array<std::size_t,3> axis_faces{};
    std::array<int,3> pcg_iterations{};
    std::array<double,3> pcg_residual_kg_m_s{};
    double water_mass_kg=0,air_mass_kg=0;
    P2ShearVec3 momentum_before_kg_m_s{},momentum_after_kg_m_s{};
    P2ShearVec3 surface_impulse_n_s{},momentum_residual_n_s{};
    double kinetic_before_j=0,kinetic_after_j=0,surface_work_j=0;
    double viscous_dissipation_j=0,implicit_dissipation_j=0,energy_residual_j=0;
};
inline P2GlobalShearLedger3D p2_two_phase_global_shear_step3d(
    P2TwoPhaseGridShear3D& grid,
    const P2GlobalShearCellProperties3D& properties={}) {
    if(grid.nx<2||grid.ny<2||grid.nz<2||
       grid.nx>128||grid.ny>128||grid.nz>128)
        throw std::invalid_argument("Invalid global shear grid dimensions");
    const std::size_t n=std::size_t(grid.nx)*grid.ny*grid.nz;
    if(n>1000000||grid.phase.size()!=n||grid.velocity_m_s.size()!=n)
        throw std::invalid_argument("Global shear grid size mismatch/limit");
    const double params[]={grid.dx_m,grid.dt_s,grid.water_density_kg_m3,
        grid.air_density_kg_m3,grid.water_viscosity_pa_s,grid.air_viscosity_pa_s};
    for(double v:params)if(!(v>0)||!std::isfinite(v))
        throw std::invalid_argument("Invalid global shear physical parameter");
    for(double v:grid.marangoni_gradient_pa)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite Marangoni gradient");
    const double vol=grid.dx_m*grid.dx_m*grid.dx_m;
    const double area=grid.dx_m*grid.dx_m;
    if((!properties.density_kg_m3.empty()&&properties.density_kg_m3.size()!=n)||
       (!properties.dynamic_viscosity_pa_s.empty()&&
        properties.dynamic_viscosity_pa_s.size()!=n))
        throw std::invalid_argument("Nonuniform global shear property size mismatch");
    if(!(std::isfinite(vol)&&vol>0&&std::isfinite(area)&&area>0))
        throw std::invalid_argument("Unresolved global shear cell volume/area");
    struct Edge {std::size_t water,air;int axis;double b,water_source,air_source;};
    std::vector<Edge> edges;
    std::vector<double> mass(n),viscosity(n);
    P2GlobalShearLedger3D out;
    for(std::size_t i=0;i<n;++i) {
        if(grid.phase[i]>1)throw std::invalid_argument("Partial/solid phase unsupported");
        const double density=properties.density_kg_m3.empty()?
            (grid.phase[i]?grid.water_density_kg_m3:grid.air_density_kg_m3):
            properties.density_kg_m3[i];
        viscosity[i]=properties.dynamic_viscosity_pa_s.empty()?
            (grid.phase[i]?grid.water_viscosity_pa_s:grid.air_viscosity_pa_s):
            properties.dynamic_viscosity_pa_s[i];
        mass[i]=density*vol;
        if(!(std::isfinite(mass[i])&&mass[i]>0&&
             std::isfinite(viscosity[i])&&viscosity[i]>0))
            throw std::invalid_argument("Invalid local shear density/viscosity");
        if(grid.phase[i])out.water_mass_kg+=mass[i];else out.air_mass_kg+=mass[i];
        for(double v:grid.velocity_m_s[i])if(!std::isfinite(v))
            throw std::invalid_argument("Nonfinite global shear velocity");
        for(int k=0;k<3;++k)
            out.momentum_before_kg_m_s[k]+=mass[i]*grid.velocity_m_s[i][k];
        out.kinetic_before_j+=.5*mass[i]*
            p2_shear_dot(grid.velocity_m_s[i],grid.velocity_m_s[i]);
    }
    auto index=[&](int x,int y,int z)->std::size_t {
        return (std::size_t(z)*grid.ny+y)*grid.nx+x;
    };
    for(int z=0;z<grid.nz;++z)for(int y=0;y<grid.ny;++y)
    for(int x=0;x<grid.nx;++x) {
        const auto i=index(x,y,z);
        for(int axis=0;axis<3;++axis) {
            const int xx=x+(axis==0),yy=y+(axis==1),zz=z+(axis==2);
            if(xx>=grid.nx||yy>=grid.ny||zz>=grid.nz)continue;
            const auto j=index(xx,yy,zz);
            if(grid.phase[i]==grid.phase[j])continue;
            if(std::abs(grid.marangoni_gradient_pa[axis])>
               1e-10*std::max(1.0,std::sqrt(p2_shear_dot(
                   grid.marangoni_gradient_pa,grid.marangoni_gradient_pa))))
                throw std::invalid_argument("Marangoni source normal to an interface");
            const auto wi=grid.phase[i]?i:j,ai=grid.phase[i]?j:i;
            const double rw=(grid.dx_m/2)/viscosity[wi];
            const double ra=(grid.dx_m/2)/viscosity[ai];
            const double resistance=rw+ra;
            const double b=grid.dt_s*area/resistance;
            if(!(std::isfinite(rw)&&std::isfinite(ra)&&
                 std::isfinite(b)&&b>0&&std::isfinite(resistance)&&resistance>0))
                throw std::invalid_argument("Unresolved local interface resistance");
            edges.push_back({wi,ai,axis,b,ra/resistance,rw/resistance});
            ++out.axis_faces[axis];
        }
    }
    out.interface_faces=edges.size();
    auto next=grid.velocity_m_s; // transactional until ALL 3 PCG solves succeed
    for(int k=0;k<3;++k) {
        std::vector<double> rhs(n),diag=mass,x(n),r(n),z(n),p(n),ap(n);
        for(std::size_t i=0;i<n;++i) {
            x[i]=grid.velocity_m_s[i][k];
            rhs[i]=mass[i]*x[i];
        }
        for(const auto& e:edges) {
            if(e.axis==k)continue; // only tangential traction
            diag[e.water]+=e.b;diag[e.air]+=e.b;
            const double source=grid.dt_s*area*grid.marangoni_gradient_pa[k];
            rhs[e.water]+=source*e.water_source;
            rhs[e.air]+=source*e.air_source;
        }
        auto apply=[&](const std::vector<double>& v,std::vector<double>& av){
            for(std::size_t i=0;i<n;++i)av[i]=mass[i]*v[i];
            for(const auto& e:edges) {
                if(e.axis==k)continue;
                const double flux=e.b*(v[e.water]-v[e.air]);
                av[e.water]+=flux;av[e.air]-=flux;
            }
        };
        apply(x,ap);
        double rz=0,rhs2=0,res2=0;
        for(std::size_t i=0;i<n;++i) {
            r[i]=rhs[i]-ap[i];z[i]=r[i]/diag[i];p[i]=z[i];
            rz+=r[i]*z[i];rhs2+=rhs[i]*rhs[i];res2+=r[i]*r[i];
        }
        const double tol=1e-13*std::max(1.0,std::sqrt(rhs2));
        if(!std::isfinite(rz)||!std::isfinite(res2))
            throw std::invalid_argument("Nonfinite global shear PCG initialization");
        const std::size_t limit=std::min<std::size_t>(8*n+32,20000);
        for(std::size_t it=0;res2>tol*tol&&it<limit;++it) {
            apply(p,ap);
            double pap=0;
            for(std::size_t i=0;i<n;++i)pap+=p[i]*ap[i];
            if(!(pap>0)||!std::isfinite(pap))
                throw std::invalid_argument("Global shear PCG non-SPD");
            const double alpha=rz/pap;
            double rz_new=0;
            res2=0;
            for(std::size_t i=0;i<n;++i) {
                x[i]+=alpha*p[i];r[i]-=alpha*ap[i];
                z[i]=r[i]/diag[i];
                rz_new+=r[i]*z[i];res2+=r[i]*r[i];
            }
            ++out.pcg_iterations[k];
            if(!std::isfinite(rz_new)||!std::isfinite(res2))
                throw std::invalid_argument("Nonfinite global shear PCG iteration");
            if(res2<=tol*tol)break;
            const double beta=rz_new/rz;
            for(std::size_t i=0;i<n;++i)p[i]=z[i]+beta*p[i];
            rz=rz_new;
        }
        // Independent residual from the FINAL x, not just recursive PCG r.
        apply(x,ap);
        res2=0;
        for(std::size_t i=0;i<n;++i)res2+=(rhs[i]-ap[i])*(rhs[i]-ap[i]);
        out.pcg_residual_kg_m_s[k]=std::sqrt(res2);
        if(!std::isfinite(res2)||out.pcg_residual_kg_m_s[k]>5*tol)
            throw std::invalid_argument("Global shear PCG failed to converge");
        for(std::size_t i=0;i<n;++i)next[i][k]=x[i];
    }
    for(std::size_t i=0;i<n;++i) {
        for(int k=0;k<3;++k) {
            out.momentum_after_kg_m_s[k]+=mass[i]*next[i][k];
            const double delta=next[i][k]-grid.velocity_m_s[i][k];
            out.implicit_dissipation_j+=.5*mass[i]*delta*delta;
        }
        out.kinetic_after_j+=.5*mass[i]*p2_shear_dot(next[i],next[i]);
    }
    for(const auto& e:edges)for(int k=0;k<3;++k) {
        if(e.axis==k)continue;
        const double d=next[e.air][k]-next[e.water][k];
        out.viscous_dissipation_j+=e.b*d*d;
        out.surface_impulse_n_s[k]+=grid.dt_s*area*grid.marangoni_gradient_pa[k];
        out.surface_work_j+=grid.dt_s*area*grid.marangoni_gradient_pa[k]*
            (e.water_source*next[e.water][k]+e.air_source*next[e.air][k]);
    }
    for(int k=0;k<3;++k)
        out.momentum_residual_n_s[k]=out.momentum_after_kg_m_s[k]-
            out.momentum_before_kg_m_s[k]-out.surface_impulse_n_s[k];
    out.energy_residual_j=out.kinetic_after_j-out.kinetic_before_j-
        out.surface_work_j+out.viscous_dissipation_j+out.implicit_dissipation_j;
    const double scalars[]={out.kinetic_before_j,out.kinetic_after_j,
        out.viscous_dissipation_j,out.implicit_dissipation_j,
        out.surface_work_j,out.energy_residual_j};
    for(double v:scalars)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite global shear energy");
    for(double v:out.momentum_residual_n_s)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite global shear momentum");
    grid.velocity_m_s.swap(next);
    return out;
}
} // namespace upc
