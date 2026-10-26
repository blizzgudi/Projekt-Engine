#pragma once
// P4 heat and mass feedback for P3 vertical conservative Darcy transfer.
// A pore is a separate water-enthalpy inventory hosted by a solid voxel.
// Gravity-potential energy lost by downward transfer is converted to heat.
// Single-pressure, local-equilibrium liquid model; no capillary vapor/ice flow.
#include "upc/material_coupling3d.hpp"
#include "upc/thermal3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>
namespace upc {
class ThermalPoreExchange3D {
    MaterialVolumeCoupler3D& soil_;
    ThermalVolume3D& heat_;
    const CoupledMacWater3D& fluid_;
    double rho_;
    std::vector<double> pore_mass_,pore_enthalpy_;
    static constexpr double fusion=334000,cp=4184,melt=273.15;
    double total_pore_enthalpy_j()const{
        double e=0;for(double v:pore_enthalpy_)e+=v;return e;
    }
public:
    ThermalPoreExchange3D(MaterialVolumeCoupler3D& soil,
                          ThermalVolume3D& heat,
                          const CoupledMacWater3D& fluid)
       :soil_(soil),heat_(heat),fluid_(fluid),rho_(fluid.density_kg_m3()),
        pore_mass_(heat.nx()*heat.ny()*heat.nz(),0),
        pore_enthalpy_(heat.nx()*heat.ny()*heat.nz(),0){
        if(heat.nx()!=fluid.nx()||heat.ny()!=fluid.ny()||heat.nz()!=fluid.nz()||
           std::abs(heat.cell_size_m()-fluid.cell_size_m())>1e-12)
            throw std::invalid_argument("P3/P4 thermal pore dimensions differ");
        for(std::size_t z=0;z<heat.nz();++z)
         for(std::size_t y=0;y<heat.ny();++y)
          for(std::size_t x=0;x<heat.nx();++x){
            const auto i=(z*heat.ny()+y)*heat.nx()+x;
            const auto& c=soil.soil(x,y,z);
            if(!c.material_id)continue;
            if(!heat.at(x,y,z).material_id ||
               heat.at(x,y,z).water)
                throw std::runtime_error("Pore water requires registered solid material");
            const double temperature=heat.temperature_k(x,y,z);
            if(temperature<273.15||temperature>373.15)
                throw std::invalid_argument("Pore phase must start as liquid");
            pore_mass_[i]=c.pore_water_m3*rho_;
            pore_enthalpy_[i]=pore_mass_[i]*(fusion+cp*(temperature-melt));
        }
        soil_.set_pore_transfer_listener([this](const PoreTransfer3D& event){
            const double m=event.volume_m3*rho_;
            if(event.source>=pore_mass_.size()||event.to>=pore_mass_.size()||
               !std::isfinite(m)||m<=0)
                throw std::runtime_error("Invalid pore heat transfer");
            double moved_heat=0;
            if(event.source_pore){
                auto& amount=pore_mass_[event.source];
                auto& energy=pore_enthalpy_[event.source];
                if(amount<m-1e-9||amount<=0)
                    throw std::runtime_error("No pore enthalpy for source liquid");
                moved_heat=energy*m/amount;
                amount-=m;energy-=moved_heat;
                if(amount<1e-12){amount=0;energy=0;}
            } else {
                moved_heat=heat_.remove_liquid_mass(event.source,m);
            }
            pore_mass_[event.to]+=m;
            // Hydrostatic potential drop is thermalized locally, rather
            // than double-counted as unspecified mechanical dissipation.
            pore_enthalpy_[event.to]+=moved_heat+
                                      event.potential_released_j;
        });
    }
    ThermalPoreExchange3D(const ThermalPoreExchange3D&)=delete;
    ThermalPoreExchange3D& operator=(const ThermalPoreExchange3D&)=delete;
    ~ThermalPoreExchange3D(){soil_.set_pore_transfer_listener({});}
    double total_pore_mass_kg()const {
        double m=0;for(double v:pore_mass_)m+=v;return m;
    }
    double total_thermal_enthalpy_j()const {
        return heat_.total_enthalpy_j()+total_pore_enthalpy_j();
    }
    double total_thermal_gravity_j()const {
        return total_thermal_enthalpy_j()+soil_.gravity_potential_j();
    }
    P3ExchangeReport exchange(double seconds){
        const double before_mass=heat_.total_water_mass_kg()+total_pore_mass_kg();
        const double before_energy=total_thermal_gravity_j();
        const auto report=soil_.exchange(seconds);
        const double after_mass=heat_.total_water_mass_kg()+total_pore_mass_kg();
        const double after_energy=total_thermal_gravity_j();
        if(std::abs(before_mass-after_mass)>1e-8 ||
           std::abs(after_mass-soil_.total_water_mass_kg())>1e-8 ||
           std::abs(before_energy-after_energy)>1e-6)
            throw std::runtime_error("P3-to-P4 porous total energy/mass balance failed");
        return report;
    }
};
} // namespace upc
