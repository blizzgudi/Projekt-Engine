#pragma once
// P2 PARTIAL: bounded temporal composition of the existing periodic viscous
// nonlinear MAC/projection + passive PLIC step. Not two-phase CFD or FSI.
#include "upc/p2_viscous_nonlinear_mac_plic3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
// Constant-density, full-domain periodic MAC bulk diagnostics. These
// staggered-face quadratures include air cells as modeled fluid: they are
// NOT water-only momentum/energy, moving-solid FSI or a total-energy law.
struct P2PeriodicMacBulkBudget3D {
    std::array<double,3> momentum_kg_m_s{{0,0,0}};
    double kinetic_energy_j=0;
};
inline P2PeriodicMacBulkBudget3D p2_periodic_mac_bulk_budget3d(
    const P2NonlinearMacPlicState3D& state,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double density_kg_m3) {
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>32768||!std::isfinite(dx_m)||dx_m<=0||
       !std::isfinite(density_kg_m3)||density_kg_m3<=0)
        throw std::invalid_argument("Invalid periodic MAC bulk budget grid or units");
    const double cell_mass=density_kg_m3*dx_m*dx_m*dx_m;
    if(!std::isfinite(cell_mass)||cell_mass<=0)
        throw std::invalid_argument("Invalid periodic MAC bulk cell mass");
    const std::array<std::size_t,3> sizes{{
        (nx+1)*ny*nz,nx*(ny+1)*nz,nx*ny*(nz+1)}};
    auto index=[&](int c,std::size_t i,std::size_t j,std::size_t k) {
        if(c==0)return (k*ny+j)*(nx+1)+i;
        if(c==1)return (k*(ny+1)+j)*nx+i;
        return (k*ny+j)*nx+i;
    };
    P2PeriodicMacBulkBudget3D out;
    for(int c=0;c<3;++c) {
        const auto& v=state.staggered_velocity[c];
        if(v.size()!=sizes[c])
            throw std::invalid_argument("Invalid periodic MAC bulk face dimensions");
        for(double x:v)
            if(!std::isfinite(x))
                throw std::invalid_argument("Nonfinite periodic MAC bulk velocity");
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i) {
            const double x=v[index(c,i,j,k)];
            out.momentum_kg_m_s[c]+=cell_mass*x;
            out.kinetic_energy_j+=0.5*cell_mass*x*x;
            if((c==0&&i==0&&
                std::abs(v[index(c,nx,j,k)]-x)>1e-12)||
               (c==1&&j==0&&
                std::abs(v[index(c,i,ny,k)]-x)>1e-12)||
               (c==2&&k==0&&
                std::abs(v[index(c,i,j,nz)]-x)>1e-12))
                throw std::invalid_argument("Nonperiodic MAC bulk seam");
        }
    }
    if(!std::isfinite(out.kinetic_energy_j))
        throw std::runtime_error("Nonfinite periodic MAC bulk energy");
    for(double p:out.momentum_kg_m_s)
        if(!std::isfinite(p))
            throw std::runtime_error("Nonfinite periodic MAC bulk momentum");
    return out;
}
struct P2ViscousMacPlicInterval3D {
    double dt_s=0;
    double nu_m2_s=0;
};
struct P2ViscousMacPlicIntervalReport3D {
    double dt_s=0, nu_m2_s=0;
    std::size_t viscous_substeps=0, plic_substeps=0;
    double initial_volume_m3=0, final_volume_m3=0, mass_residual_m3=0;
    double max_projected_divergence_s_inv=0;
    double viscous_face_square_sum_before=0, viscous_face_square_sum_after=0;
    std::array<double,3> viscous_unique_face_sum_delta{{0,0,0}};
    P2PeriodicMacBulkBudget3D bulk_before, bulk_after;
    std::array<double,3> bulk_momentum_delta_kg_m_s{{0,0,0}};
    double bulk_kinetic_delta_j=0;
};
struct P2ViscousMacPlicSequence3D {
    P2NonlinearMacPlicState3D state;
    std::vector<P2ViscousMacPlicIntervalReport3D> intervals;
    double elapsed_s=0, total_mass_residual_m3=0;
    // End-to-end water inventory and telescoping continuity are diagnostic
    // checks only; this is NOT a physical fluid/solid momentum-energy ledger.
    double initial_volume_m3=0, final_volume_m3=0;
    double initial_water_mass_kg=0, final_water_mass_kg=0;
    double mass_chain_residual_m3=0, mass_telescoping_residual_m3=0;
    double max_projected_divergence_s_inv=0;
    std::array<double,3> total_viscous_unique_face_sum_delta{{0,0,0}};
    std::size_t total_viscous_substeps=0, total_plic_substeps=0;
    P2PeriodicMacBulkBudget3D bulk_initial, bulk_final;
    std::array<double,3> bulk_momentum_telescoping_residual_kg_m_s{{0,0,0}};
    double bulk_energy_telescoping_residual_j=0;
};

// The input is never mutated. A failed later interval throws instead of
// returning a misleading partially advanced state. The global budgets count
// actual substeps, not merely intervals. Per-step projection is recomputed.
inline P2ViscousMacPlicSequence3D p2_viscous_mac_plic_sequence3d(
    const P2NonlinearMacPlicState3D& initial,
    const std::vector<P2ViscousMacPlicInterval3D>& schedule,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double density_kg_m3,
    double target_six_face_courant=0.4,
    std::size_t max_total_plic_substeps=256,
    std::size_t max_total_viscous_substeps=256) {
    if(schedule.empty()||schedule.size()>128||
       max_total_plic_substeps==0||max_total_viscous_substeps==0||
       max_total_plic_substeps>4096||max_total_viscous_substeps>4096||
       !std::isfinite(target_six_face_courant)||
       target_six_face_courant<=0||target_six_face_courant>1)
        throw std::invalid_argument("Invalid MAC/PLIC temporal schedule or budgets");
    double duration=0;
    for(const auto& interval:schedule) {
        if(!std::isfinite(interval.dt_s)||interval.dt_s<=0||
           !std::isfinite(interval.nu_m2_s)||interval.nu_m2_s<0||
           !std::isfinite(duration+interval.dt_s))
            throw std::invalid_argument("Invalid MAC/PLIC interval");
        duration+=interval.dt_s;
    }
    P2ViscousMacPlicSequence3D out;
    out.state=initial;
    out.intervals.reserve(schedule.size());
    for(const auto& interval:schedule) {
        const auto bulk_before=p2_periodic_mac_bulk_budget3d(
            out.state,nx,ny,nz,dx_m,density_kg_m3);
        const auto plic_left=max_total_plic_substeps-out.total_plic_substeps;
        const auto viscous_left=max_total_viscous_substeps-out.total_viscous_substeps;
        if(plic_left==0||(interval.nu_m2_s>0&&viscous_left==0))
            throw std::invalid_argument("MAC/PLIC cumulative substep budget exhausted");
        const auto step=p2_viscous_nonlinear_mac_plic_step3d(
            out.state,nx,ny,nz,dx_m,interval.dt_s,density_kg_m3,
            interval.nu_m2_s,target_six_face_courant,plic_left,
            viscous_left==0?1:viscous_left);
        const auto plic_count=step.coupled.transport.substeps;
        if(plic_count==0||plic_count>plic_left||
           step.viscous_substeps>viscous_left)
            throw std::runtime_error("Invalid MAC/PLIC step budget accounting");
        P2ViscousMacPlicIntervalReport3D report;
        report.bulk_before=bulk_before;
        report.bulk_after=p2_periodic_mac_bulk_budget3d(
            step.coupled.state,nx,ny,nz,dx_m,density_kg_m3);
        report.bulk_kinetic_delta_j=report.bulk_after.kinetic_energy_j-
                                    report.bulk_before.kinetic_energy_j;
        for(int c=0;c<3;++c)
            report.bulk_momentum_delta_kg_m_s[c]=
                report.bulk_after.momentum_kg_m_s[c]-
                report.bulk_before.momentum_kg_m_s[c];
        report.dt_s=interval.dt_s;
        report.nu_m2_s=interval.nu_m2_s;
        report.viscous_substeps=step.viscous_substeps;
        report.plic_substeps=plic_count;
        report.initial_volume_m3=step.coupled.transport.initial_volume_m3;
        report.final_volume_m3=step.coupled.transport.final_volume_m3;
        report.mass_residual_m3=step.coupled.transport.mass_residual_m3;
        report.max_projected_divergence_s_inv=
            step.coupled.projection.max_divergence_s_inv;
        report.viscous_face_square_sum_before=step.face_square_sum_before;
        report.viscous_face_square_sum_after=step.face_square_sum_after;
        report.viscous_unique_face_sum_delta=step.unique_face_sum_delta;
        if(out.intervals.empty()) {
            out.initial_volume_m3=report.initial_volume_m3;
            out.bulk_initial=report.bulk_before;
        } else {
            const double discontinuity=report.initial_volume_m3-out.final_volume_m3;
            out.mass_chain_residual_m3+=discontinuity;
            if(!std::isfinite(discontinuity)||
               std::abs(discontinuity)>1e-10*std::max(1.0,out.initial_volume_m3))
                throw std::runtime_error("MAC/PLIC sequence water inventory discontinuity");
        }
        out.final_volume_m3=report.final_volume_m3;
        out.bulk_final=report.bulk_after;
        out.bulk_energy_telescoping_residual_j+=report.bulk_kinetic_delta_j;
        for(int c=0;c<3;++c)
            out.bulk_momentum_telescoping_residual_kg_m_s[c]+=
                report.bulk_momentum_delta_kg_m_s[c];
        out.max_projected_divergence_s_inv=std::max(
            out.max_projected_divergence_s_inv,
            report.max_projected_divergence_s_inv);
        for(int c=0;c<3;++c)
            out.total_viscous_unique_face_sum_delta[c]+=
                report.viscous_unique_face_sum_delta[c];
        out.total_viscous_substeps+=step.viscous_substeps;
        out.total_plic_substeps+=plic_count;
        out.total_mass_residual_m3+=report.mass_residual_m3;
        out.state=step.coupled.state;
        out.intervals.push_back(report);
        out.elapsed_s+=interval.dt_s;
    }
    out.initial_water_mass_kg=out.initial_volume_m3*density_kg_m3;
    out.final_water_mass_kg=out.final_volume_m3*density_kg_m3;
    out.mass_telescoping_residual_m3=out.final_volume_m3-
        out.initial_volume_m3-out.total_mass_residual_m3-
        out.mass_chain_residual_m3;
    out.bulk_energy_telescoping_residual_j-=
        out.bulk_final.kinetic_energy_j-out.bulk_initial.kinetic_energy_j;
    for(int c=0;c<3;++c)
        out.bulk_momentum_telescoping_residual_kg_m_s[c]-=
            out.bulk_final.momentum_kg_m_s[c]-
            out.bulk_initial.momentum_kg_m_s[c];
    const double energy_tol=1e-10*std::max(1.0,
        std::abs(out.bulk_initial.kinetic_energy_j));
    if(!std::isfinite(out.bulk_energy_telescoping_residual_j)||
       std::abs(out.bulk_energy_telescoping_residual_j)>energy_tol)
        throw std::runtime_error("MAC/PLIC bulk kinetic ledger invalid");
    for(int c=0;c<3;++c)
        if(!std::isfinite(out.bulk_momentum_telescoping_residual_kg_m_s[c])||
           std::abs(out.bulk_momentum_telescoping_residual_kg_m_s[c])>
           1e-10*std::max(1.0,std::abs(out.bulk_initial.momentum_kg_m_s[c])))
            throw std::runtime_error("MAC/PLIC bulk momentum ledger invalid");
    if(!std::isfinite(out.initial_water_mass_kg)||
       !std::isfinite(out.final_water_mass_kg)||
       !std::isfinite(out.mass_telescoping_residual_m3)||
       std::abs(out.mass_telescoping_residual_m3)>
           1e-10*std::max(1.0,out.initial_volume_m3))
        throw std::runtime_error("MAC/PLIC sequence mass ledger invalid");
    return out;
}
} // namespace upc
