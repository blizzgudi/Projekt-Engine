#pragma once
// P6<->P4 conservative thermal contact between DIFFERENT, neighboring parcels.
// Do not put the same mass in both grids; they are separate material regions.
// This transfers heat only, not mass/CFD pressure or reaction chemistry.
#include "upc/reactive_mixture3d.hpp"
#include "upc/thermal3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
namespace upc {
class ReactiveThermalContact3D {
    ReactiveMixtureVolume3D& mixture_;
    ThermalVolume3D& thermal_;
public:
    ReactiveThermalContact3D(ReactiveMixtureVolume3D& mixture,
                            ThermalVolume3D& thermal)
      :mixture_(mixture),thermal_(thermal){
        if(mixture.nx()!=thermal.nx()||mixture.ny()!=thermal.ny()||
           mixture.nz()!=thermal.nz()||
           std::abs(mixture.cell_size_m()-thermal.cell_size_m())>1e-12)
            throw std::invalid_argument("Chemistry and thermal grids must align");
    }
    double transfer_face_heat(std::size_t mx,std::size_t my,std::size_t mz,
                              std::size_t tx,std::size_t ty,std::size_t tz,
                              double conductivity_w_m_k,double dt){
        if(!std::isfinite(conductivity_w_m_k)||conductivity_w_m_k<0||
           !std::isfinite(dt)||dt<=0)
            throw std::invalid_argument("Invalid chemistry-thermal heat coupling");
        const std::size_t sep=(mx>tx?mx-tx:tx-mx)+
            (my>ty?my-ty:ty-my)+(mz>tz?mz-tz:tz-mz);
        if(sep!=1||mixture_.at(mx,my,mz).species_mol.empty())
            throw std::invalid_argument("Requires adjacent occupied chemistry voxel");
        const auto& c=thermal_.at(tx,ty,tz);
        if(!c.material_id||c.water)
            throw std::invalid_argument("P6 thermal contact supports solid P4 sink/source only");
        const double heatcap=mixture_.heat_capacity_j_k(mx,my,mz);
        const double solidcap=c.mass_kg*c.specific_heat_j_kg_k;
        if(heatcap<=0||solidcap<=0)
            throw std::runtime_error("Missing positive parcel heat capacities");
        const double delta=mixture_.temperature_k(mx,my,mz)-
                           thermal_.temperature_k(tx,ty,tz);
        const double maximum=std::abs(delta)/(1/heatcap+1/solidcap);
        const double requested=conductivity_w_m_k*mixture_.cell_size_m()*delta*dt;
        const double flow=std::clamp(requested,-maximum,maximum);
        const double energy_before=mixture_.total_energy_j()+
                                   thermal_.total_enthalpy_j();
        // After bounded capacity constraint both temperatures stay >=0.
        mixture_.add_heat_j(mx,my,mz,-flow);
        thermal_.add_heat_j(tx,ty,tz,flow);
        const double residual=mixture_.total_energy_j()+
                              thermal_.total_enthalpy_j()-energy_before;
        if(std::abs(residual)>1e-8*std::max(1.0,std::abs(energy_before)))
            throw std::runtime_error("P4/P6 shared energy transfer residual");
        return flow;
    }
};
} // namespace upc
