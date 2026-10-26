#pragma once
// P4 thermal <-> 3D MAC/voxel phase bridge. Cell-based CPU demonstrator.
// MAC tracks liquid fraction only; ice blocks cells when fully frozen,
// vapor retains mass as a nonadvecting gas inventory (no pressure solver).
// Does NOT implement velocity/thermal advection or moving solid interface.
#include "upc/thermal3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>
namespace upc {
struct PhaseInventory3D {
    double ice_kg=0,liquid_kg=0,vapor_kg=0;
};
class ThermalPhaseMacBridge3D {
    ThermalVolume3D& thermal_;
    CoupledMacWater3D& fluid_;
    VoxelGrid& solid_voxels_;
    const MaterialLibrary& materials_;
    std::size_t nx_,ny_,nz_;
    double cell_volume_,liquid_density_;
    std::vector<unsigned char> owned_ice_mask_;
    std::vector<unsigned char> permanent_solid_;
    std::vector<double> last_synced_liquid_;
    std::size_t index(std::size_t x,std::size_t y,std::size_t z)const{
        return (z*ny_+y)*nx_+x;
    }
public:
    ThermalPhaseMacBridge3D(ThermalVolume3D& thermal,CoupledMacWater3D& fluid,
                            VoxelGrid& solid_voxels,const MaterialLibrary& materials)
     :thermal_(thermal),fluid_(fluid),solid_voxels_(solid_voxels),
      materials_(materials),nx_(thermal.nx()),ny_(thermal.ny()),nz_(thermal.nz()),
      cell_volume_(thermal.cell_size_m()*thermal.cell_size_m()*thermal.cell_size_m()),
      liquid_density_(fluid.density_kg_m3()),
      owned_ice_mask_(thermal.nx()*thermal.ny()*thermal.nz(),0),
      permanent_solid_(thermal.nx()*thermal.ny()*thermal.nz(),0),
      last_synced_liquid_(thermal.nx()*thermal.ny()*thermal.nz(),0){
        if(nx_!=fluid.nx()||ny_!=fluid.ny()||nz_!=fluid.nz()||
           nx_!=solid_voxels.nx()||ny_!=solid_voxels.ny()||
           nz_!=solid_voxels.nz()||
           std::abs(thermal.cell_size_m()-fluid.cell_size_m())>1e-12 ||
           std::abs(thermal.cell_size_m()-solid_voxels.cell_size_m())>1e-12)
            throw std::invalid_argument("Thermal bridge grids must align");
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            const std::size_t i=index(x,y,z);
            permanent_solid_[i]=fluid.is_solid(x,y,z);
            last_synced_liquid_[i]=fluid.water_fraction(x,y,z);
            const auto& cell=thermal.at(x,y,z);
            if(cell.water){
                if(fluid.is_solid(x,y,z) || solid_voxels.at(x,y,z).material_id)
                    throw std::invalid_argument("Water thermal cell overlaps rock");
                const auto phase=thermal.water_phase(x,y,z);
                const double expected=cell.mass_kg*phase.liquid/
                                      (liquid_density_*cell_volume_);
                if(expected>1+1e-10 || std::abs(expected-fluid.water_fraction(x,y,z))>1e-8)
                    throw std::invalid_argument("Thermal water inventory is not synchronized to MAC");
            }
        }
    }
    PhaseInventory3D inventory()const {
        PhaseInventory3D out;
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x)if(thermal_.at(x,y,z).water){
            const auto p=thermal_.water_phase(x,y,z);
            const double mass=thermal_.at(x,y,z).mass_kg;
            out.ice_kg+=mass*p.ice;
            out.liquid_kg+=mass*p.liquid;
            out.vapor_kg+=mass*p.vapour;
          }
        return out;
    }
    double represented_water_mass_kg()const {
        double liquid=0,ice=0,gas=0;
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x)if(thermal_.at(x,y,z).water){
            const double m=thermal_.at(x,y,z).mass_kg;
            const auto p=thermal_.water_phase(x,y,z);
            liquid+=fluid_.water_fraction(x,y,z)*cell_volume_*liquid_density_;
            ice+=m*p.ice;gas+=m*p.vapour;
          }
        return liquid+ice+gas;
    }
    // Called after MAC transported water using the conservative thermal
    // flux listener. Adopt new fluid fractions only when thermal mass matches.
    void synchronize_after_mac_advection(){
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            const auto i=index(x,y,z);
            const auto& c=thermal_.at(x,y,z);
            if(permanent_solid_[i]||owned_ice_mask_[i])continue;
            if(c.water){
                const auto phase=thermal_.water_phase(x,y,z);
                if(phase.liquid<1-1e-8)
                    throw std::runtime_error("Cannot advect partially frozen water");
                const double expected=c.mass_kg*phase.liquid/
                    (cell_volume_*liquid_density_);
                if(std::abs(expected-fluid_.water_fraction(x,y,z))>1e-7)
                    throw std::runtime_error("Thermal advection mass does not match MAC");
            }else if(fluid_.water_fraction(x,y,z)>1e-9 &&
                     !solid_voxels_.at(x,y,z).material_id)
                throw std::runtime_error("MAC received water without thermal enthalpy");
            last_synced_liquid_[i]=fluid_.water_fraction(x,y,z);
          }
    }
    void verify_before_step()const {
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            const std::size_t i=index(x,y,z);
            if(!thermal_.at(x,y,z).water)continue;
            // Compare to the last exported fluid amount. External heating
            // may change water phase before the next thermal step.
            if(std::abs(last_synced_liquid_[i]-fluid_.water_fraction(x,y,z))>1e-8 ||
               (fluid_.is_solid(x,y,z)!=bool(owned_ice_mask_[i])))
                throw std::runtime_error("MAC was modified without heat advection synchronization");
        }
    }
    ThermalExchangeAudit step(double dt){
        verify_before_step();
        const double m0=thermal_.total_mass_kg();
        const double e0=thermal_.total_enthalpy_j();
        const auto audit=thermal_.step(dt);
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            const auto& c=thermal_.at(x,y,z);
            const std::size_t i=index(x,y,z);
            if(!c.material_id)continue;
            if(c.water){
                if(permanent_solid_[i])throw std::runtime_error("Thermal phase rock collision");
                if(owned_ice_mask_[i]){
                    fluid_.set_solid(x,y,z,false);
                    owned_ice_mask_[i]=0;
                }
                const auto p=thermal_.water_phase(x,y,z);
                const double fraction=std::clamp(
                    c.mass_kg*p.liquid/(cell_volume_*liquid_density_),0.0,1.0);
                fluid_.set_water(x,y,z,fraction);
                if(p.ice>=1-1e-10 && c.mass_kg>0){
                    fluid_.set_water(x,y,z,0);
                    fluid_.set_solid(x,y,z,true);
                    owned_ice_mask_[i]=1;
                }
                last_synced_liquid_[i]=fluid_.water_fraction(x,y,z);
            }else{
                auto voxel=solid_voxels_.at(x,y,z);
                if(voxel.material_id!=c.material_id)
                    throw std::runtime_error("Solid voxel material mismatched");
                voxel.temperature_k=thermal_.temperature_k(x,y,z);
                solid_voxels_.set(x,y,z,voxel,materials_);
            }
          }
        const auto m1=thermal_.total_mass_kg();
        if(std::abs(m1-m0)>1e-9 ||
           std::abs(thermal_.total_enthalpy_j()-e0)>1e-7*std::max(1.0,std::abs(e0)) ||
           std::abs(represented_water_mass_kg()-thermal_.total_water_mass_kg())>1e-8)
            throw std::runtime_error("Thermal-MAC phase transfer mass/energy failure");
        return audit;
    }
};
} // namespace upc
