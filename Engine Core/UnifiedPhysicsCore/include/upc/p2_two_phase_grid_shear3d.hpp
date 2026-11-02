#pragma once
// P2 PARTIAL: explicit 3D two-phase CELL-CENTERED finite-volume shear step.
// Separate water/air velocities on a fixed, sharp voxel interface; each
// neighboring water/air pair exchanges implicit tangential impulse.
// NOT staggered MAC, VOF advection, pressure projection or moving-body FSI.
#include "upc/p2_two_phase_shear_exchange3d.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2TwoPhaseGridShear3D {
    int nx=0,ny=0,nz=0;
    double dx_m=0,dt_s=0;
    double water_density_kg_m3=0,air_density_kg_m3=0;
    double water_viscosity_pa_s=0,air_viscosity_pa_s=0;
    std::vector<std::uint8_t> phase; // 1 water, 0 air; no partial cells
    std::vector<P2ShearVec3> velocity_m_s;
    P2ShearVec3 marangoni_gradient_pa{}; // uniform, MUST be tangent to EVERY cut face
};
struct P2TwoPhaseGridShearLedger3D {
    std::size_t interface_faces=0;
    std::array<std::size_t,3> axis_faces{};
    double water_mass_kg=0,air_mass_kg=0;
    P2ShearVec3 momentum_before_kg_m_s{},momentum_after_kg_m_s{};
    P2ShearVec3 surface_impulse_n_s{},momentum_residual_n_s{};
    double kinetic_before_j=0,kinetic_after_j=0;
    double surface_work_j=0,viscous_dissipation_j=0,implicit_dissipation_j=0;
    double energy_residual_j=0;
};
inline P2TwoPhaseGridShearLedger3D p2_two_phase_grid_shear_step3d(
    P2TwoPhaseGridShear3D& grid) {
    if(grid.nx<2||grid.ny<2||grid.nz<2||
       grid.nx>256||grid.ny>256||grid.nz>256)
        throw std::invalid_argument("Invalid 3D grid extent");
    const std::size_t n=std::size_t(grid.nx)*std::size_t(grid.ny)*std::size_t(grid.nz);
    if(grid.phase.size()!=n||grid.velocity_m_s.size()!=n)
        throw std::invalid_argument("Grid/phase/velocity sizes differ");
    const double pos[]={grid.dx_m,grid.dt_s,grid.water_density_kg_m3,
        grid.air_density_kg_m3,grid.water_viscosity_pa_s,grid.air_viscosity_pa_s};
    for(double v:pos)if(!(v>0)||!std::isfinite(v))
        throw std::invalid_argument("Invalid grid physical parameter");
    const double cell_volume=grid.dx_m*grid.dx_m*grid.dx_m;
    const double face_area=grid.dx_m*grid.dx_m;
    const double water_mass=grid.water_density_kg_m3*cell_volume;
    const double air_mass=grid.air_density_kg_m3*cell_volume;
    if(!(std::isfinite(water_mass)&&water_mass>0&&
         std::isfinite(air_mass)&&air_mass>0&&std::isfinite(face_area)))
        throw std::invalid_argument("Invalid finite-volume mass/area");
    for(double v:grid.marangoni_gradient_pa)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite surface gradient");
    P2TwoPhaseGridShearLedger3D out;
    for(std::size_t i=0;i<n;++i) {
        if(grid.phase[i]>1)throw std::invalid_argument("Mixed/solid cell unsupported");
        const double mass=grid.phase[i]?water_mass:air_mass;
        if(grid.phase[i])out.water_mass_kg+=mass;else out.air_mass_kg+=mass;
        for(int a=0;a<3;++a) {
            if(!std::isfinite(grid.velocity_m_s[i][a]))
                throw std::invalid_argument("Nonfinite phase velocity");
            out.momentum_before_kg_m_s[a]+=mass*grid.velocity_m_s[i][a];
        }
        out.kinetic_before_j+=.5*mass*p2_shear_dot(grid.velocity_m_s[i],grid.velocity_m_s[i]);
    }
    if(!std::isfinite(out.kinetic_before_j))
        throw std::invalid_argument("Nonfinite grid energy");
    // Copy-on-success: no partial state changes when any interface fails.
    auto next=grid.velocity_m_s;
    auto index=[&](int x,int y,int z)->std::size_t {
        return (std::size_t(z)*grid.ny+y)*grid.nx+x;
    };
    for(int z=0;z<grid.nz;++z)for(int y=0;y<grid.ny;++y)for(int x=0;x<grid.nx;++x) {
        const std::size_t a=index(x,y,z);
        for(int axis=0;axis<3;++axis) {
            const int xx=x+(axis==0), yy=y+(axis==1), zz=z+(axis==2);
            if(xx>=grid.nx||yy>=grid.ny||zz>=grid.nz)continue;
            const std::size_t b=index(xx,yy,zz);
            if(grid.phase[a]==grid.phase[b])continue;
            const bool a_water=grid.phase[a]==1;
            const std::size_t wi=a_water?a:b, ai=a_water?b:a;
            P2TwoPhaseShearPatch3D p;
            p.normal[axis]=a_water?1.0:-1.0;
            p.water_velocity_m_s=next[wi];
            p.air_velocity_m_s=next[ai];
            p.marangoni_gradient_pa=grid.marangoni_gradient_pa;
            p.water_mass_kg=water_mass;p.air_mass_kg=air_mass;
            p.water_viscosity_pa_s=grid.water_viscosity_pa_s;
            p.air_viscosity_pa_s=grid.air_viscosity_pa_s;
            p.water_center_distance_m=grid.dx_m/2;
            p.air_center_distance_m=grid.dx_m/2;
            p.interface_area_m2=face_area;p.dt_s=grid.dt_s;
            const auto s=p2_two_phase_shear_exchange3d(p);
            next[wi]=s.water_velocity_m_s;next[ai]=s.air_velocity_m_s;
            ++out.interface_faces;++out.axis_faces[axis];
            for(int k=0;k<3;++k)out.surface_impulse_n_s[k]+=s.surface_impulse_n_s[k];
            out.surface_work_j+=s.surface_work_j;
            out.viscous_dissipation_j+=s.viscous_dissipation_j;
            out.implicit_dissipation_j+=s.implicit_dissipation_j;
        }
    }
    for(std::size_t i=0;i<n;++i) {
        const double mass=grid.phase[i]?water_mass:air_mass;
        for(int a=0;a<3;++a)out.momentum_after_kg_m_s[a]+=mass*next[i][a];
        out.kinetic_after_j+=.5*mass*p2_shear_dot(next[i],next[i]);
    }
    for(int a=0;a<3;++a)
        out.momentum_residual_n_s[a]=out.momentum_after_kg_m_s[a]-
            out.momentum_before_kg_m_s[a]-out.surface_impulse_n_s[a];
    out.energy_residual_j=out.kinetic_after_j-out.kinetic_before_j-
        out.surface_work_j+out.viscous_dissipation_j+out.implicit_dissipation_j;
    const double scalars[]={out.kinetic_after_j,out.surface_work_j,
        out.viscous_dissipation_j,out.implicit_dissipation_j,out.energy_residual_j};
    for(double v:scalars)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite global two-phase energy ledger");
    for(double v:out.momentum_residual_n_s)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite global two-phase impulse ledger");
    grid.velocity_m_s.swap(next);
    return out;
}
} // namespace upc
