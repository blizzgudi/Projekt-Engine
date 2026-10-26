#pragma once
// P4 conservative enthalpy advection with the existing P2 MAC/VOF fluxes.
// A single listener follows each finite-volume water transfer in all 3 axes.
// Only fully-liquid cells: ice/vapor phases handled by ThermalPhaseMacBridge3D.
#include "upc/thermal_phase_bridge.hpp"
#include <cmath>
#include <stdexcept>
namespace upc {
class ThermalMacAdvection3D {
    CoupledMacWater3D& water_;
    ThermalVolume3D& heat_;
    ThermalPhaseMacBridge3D& phase_;
    double density_;
    double last_energy_residual_j_=0;
    double last_mass_residual_kg_=0;
public:
    ThermalMacAdvection3D(CoupledMacWater3D& fluid,ThermalVolume3D& thermal,
                          ThermalPhaseMacBridge3D& phase)
        :water_(fluid),heat_(thermal),phase_(phase),
         density_(fluid.density_kg_m3()){
        // The adapter owns the transport listener for the lifetime of
        // this object. No copies: the callback captures its owner.
        water_.set_transport_listener([this](const TransportFlux3D& flux){
            heat_.transport_liquid(flux.from,flux.to,
                                   density_*flux.volume_m3);
        });
    }
    ThermalMacAdvection3D(const ThermalMacAdvection3D&)=delete;
    ThermalMacAdvection3D& operator=(const ThermalMacAdvection3D&)=delete;
    ~ThermalMacAdvection3D(){water_.set_transport_listener({});}
    void advect(double dt,int projection_iterations=250){
        if(!std::isfinite(dt)||dt<=0||dt>2)
            throw std::invalid_argument("Invalid thermal advection step");
        phase_.verify_before_step();
        const double initial_mass=heat_.total_mass_kg();
        const double initial_energy=heat_.total_enthalpy_j();
        water_.step(dt,projection_iterations);
        phase_.synchronize_after_mac_advection();
        last_mass_residual_kg_=heat_.total_mass_kg()-initial_mass;
        last_energy_residual_j_=heat_.total_enthalpy_j()-initial_energy;
        if(std::abs(last_mass_residual_kg_)>1e-7 ||
           std::abs(last_energy_residual_j_)>
             1e-7*std::max(1.0,std::abs(initial_energy)))
            throw std::runtime_error("Thermal fluid advection mass or energy loss");
    }
    double energy_residual_j()const{return last_energy_residual_j_;}
    double mass_residual_kg()const{return last_mass_residual_kg_;}
};
} // namespace upc
