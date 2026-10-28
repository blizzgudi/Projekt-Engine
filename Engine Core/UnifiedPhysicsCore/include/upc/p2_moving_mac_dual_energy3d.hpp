#pragma once
// P2 PARTIAL: exact open-dual-volume L2 stability audit for the existing
// conservative moving MAC transport. This is NUMERICAL upwind dissipation,
// not physical fluid/solid work, pressure work, or Navier-Stokes energy.
#include "upc/p2_moving_mac_shared_predictor3d.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace upc {
struct P2MovingMacDualEnergyResult3D {
    P2MovingMacDualTransportResult3D transport;
    std::array<double,3> energy_before_j{},energy_after_j{},upwind_loss_j{};
    std::array<double,3> momentum_before_kg_m_s{},momentum_after_kg_m_s{};
    double total_energy_before_j=0.0,total_energy_after_j=0.0;
    double total_upwind_loss_j=0.0;
    // No solid counter-impulse or mechanical energy is computed here.
};
class P2MovingMacDualEnergy3D {
public:
    using Bridge=P2MovingMacSharedPredictor3D;
    static P2MovingMacDualEnergyResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const Bridge::Faces& primal,
        const Bridge::Faces& component_old) {
        if(!std::isfinite(rho)||rho<=0.0)
            throw std::invalid_argument("Invalid dual energy density");
        const auto dual=P2MovingMacDualVolumes3DBuilder::sample(
            nx,ny,nz,dx,dt,old_box,new_box);
        P2MovingMacDualEnergyResult3D out;
        out.transport=Bridge::step(nx,ny,nz,dx,dt,rho,
                                   old_box,new_box,primal,component_old);
        const double volume=dx*dx*dx;
        if(!std::isfinite(volume)||volume<=0.0||
           !std::isfinite(rho*volume)||rho*volume<=0.0)
            throw std::invalid_argument("Invalid dual energy volume/density");
        for(int a=0;a<3;++a) {
            const auto& g=dual.component[a].geometry.cell_geometry;
            const auto& before=component_old[a];
            const auto& after=out.transport.component_new[a];
            if(before.size()!=g.alpha_old.size()||
               after.size()!=g.alpha_new.size())
                throw std::runtime_error("Dual energy shape mismatch");
            for(std::size_t i=0;i<before.size();++i) {
                if(!std::isfinite(before[i])||!std::isfinite(after[i]))
                    throw std::invalid_argument("Nonfinite dual energy state");
                const double m0=rho*volume*g.alpha_old[i];
                const double m1=rho*volume*g.alpha_new[i];
                out.energy_before_j[a]+=0.5*m0*before[i]*before[i];
                out.energy_after_j[a]+=0.5*m1*after[i]*after[i];
                out.momentum_before_kg_m_s[a]+=m0*before[i];
                out.momentum_after_kg_m_s[a]+=m1*after[i];
            }
            const double e0=out.energy_before_j[a];
            const double e1=out.energy_after_j[a];
            const double p0=out.momentum_before_kg_m_s[a];
            const double p1=out.momentum_after_kg_m_s[a];
            const double tolerance=1e-9*std::max(1.0,e0);
            if(!std::isfinite(e0)||!std::isfinite(e1)||
               !std::isfinite(p0)||!std::isfinite(p1)||
               e1>e0+tolerance||
               std::abs(p1-p0)>1e-9*std::max(1.0,std::abs(p0)))
                throw std::runtime_error("Dual upwind energy/momentum invariant");
            out.upwind_loss_j[a]=e0-e1;
            out.total_energy_before_j+=e0;
            out.total_energy_after_j+=e1;
            out.total_upwind_loss_j+=e0-e1;
        }
        if(!std::isfinite(out.total_energy_before_j)||
           !std::isfinite(out.total_energy_after_j)||
           !std::isfinite(out.total_upwind_loss_j))
            throw std::runtime_error("Dual energy accumulation overflow");
        return out;
    }
};
} // namespace upc
