#pragma once
// P4 phase specific-volume diagnostic, fixed-pressure near 1 atm.
// Gas obeys ideal-gas law, not a full steam-table equation of state.
// This module estimates required ice/vapor occupancy; MAC does NOT simulate
// vapor pressure, thermal expansion forces or gas advection.
#include "upc/thermal_phase_bridge.hpp"
#include <cmath>
#include <stdexcept>
namespace upc {
struct WaterPhaseVolumes {
    double solid_ice_m3=0,liquid_m3=0,vapor_m3=0;
    double total_m3()const{return solid_ice_m3+liquid_m3+vapor_m3;}
};
inline WaterPhaseVolumes phase_volumes_at_pressure(
    const PhaseInventory3D& masses,double temperature_k,
    double pressure_pa=101325.0){
    if(!std::isfinite(temperature_k)||temperature_k<=0||
       !std::isfinite(pressure_pa)||pressure_pa<=0||
       !std::isfinite(masses.ice_kg)||masses.ice_kg<0||
       !std::isfinite(masses.liquid_kg)||masses.liquid_kg<0||
       !std::isfinite(masses.vapor_kg)||masses.vapor_kg<0)
        throw std::invalid_argument("Invalid thermodynamic phase state");
    // Typical reference densities, not full T/P-dependent properties.
    constexpr double ice_density=917.0,liquid_density=1000.0;
    constexpr double vapor_specific_R=461.5; // J/(kg K), water vapor
    return {masses.ice_kg/ice_density,
            masses.liquid_kg/liquid_density,
            masses.vapor_kg*vapor_specific_R*temperature_k/pressure_pa};
}
} // namespace upc
