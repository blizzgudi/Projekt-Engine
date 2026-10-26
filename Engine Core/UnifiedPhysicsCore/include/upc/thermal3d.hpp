#pragma once
// P4 experimental 3D thermal finite-volume CPU module.
// Conservative, insulated six-neighbour Fourier heat exchange.
// Water phase changes via enthalpy at 1 atm; no phase-dependent MAC transport.
// Explicit substeps with stability limit based on minimum sensible heat capacity.
#include "upc/fluid3d_module.hpp"
#include "upc/materials_ext.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {

struct ThermalVoxelState {
    std::uint32_t material_id=0;
    bool water=false;
    double mass_kg=0;
    double enthalpy_j=0;
    double conductivity_w_m_k=0;
    double specific_heat_j_kg_k=0; // solids only; water uses phase-dependent cp
};
struct ThermalExchangeAudit {
    double initial_energy_j=0;
    double final_energy_j=0;
    double supplied_heat_j=0;
    double residual_j=0;
    std::size_t substeps=0;
};
class ThermalVolume3D {
    std::size_t nx_,ny_,nz_;
    double dx_;
    std::vector<ThermalVoxelState> cells_;
    double injected_heat_j_=0;
    std::size_t substeps_=0;
    static constexpr double melting_=273.15, boiling_=373.15;
    static constexpr double ice_cp_=2100, liquid_cp_=4184, vapour_cp_=2000;
    static constexpr double fusion_=334000, evaporation_=2256000;
    std::size_t idx(std::size_t x,std::size_t y,std::size_t z)const{
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("Thermal voxel index");
        return (z*ny_+y)*nx_+x;
    }
    static double water_temperature(double specific_h){
        const double at_boil=fusion_+liquid_cp_*(boiling_-melting_);
        if(specific_h<0)return melting_+specific_h/ice_cp_;
        if(specific_h<=fusion_)return melting_;
        if(specific_h<at_boil)
            return melting_+(specific_h-fusion_)/liquid_cp_;
        if(specific_h<=at_boil+evaporation_)return boiling_;
        return boiling_+(specific_h-at_boil-evaporation_)/vapour_cp_;
    }
    static double min_cp(const ThermalVoxelState& c){
        return c.water?2000:c.specific_heat_j_kg_k;
    }
    static double harmonic(double a,double b){
        return (a<=0||b<=0)?0:2*a*b/(a+b);
    }
    static void valid_positive(double v,const char* name){
        if(!std::isfinite(v)||v<=0)throw std::invalid_argument(name);
    }
    static double temp(const ThermalVoxelState& c){
        if(!c.material_id)return 0;
        return c.water?water_temperature(c.enthalpy_j/c.mass_kg):
                       melting_+c.enthalpy_j/(c.mass_kg*c.specific_heat_j_kg_k);
    }
public:
    ThermalVolume3D(std::size_t nx,std::size_t ny,std::size_t nz,double dx)
       :nx_(nx),ny_(ny),nz_(nz),dx_(dx){
        if(!nx||!ny||!nz||!std::isfinite(dx)||dx<=0||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz)
            throw std::invalid_argument("Invalid thermal domain");
        cells_.resize(nx*ny*nz);
    }
    std::size_t nx()const{return nx_;}
    std::size_t ny()const{return ny_;}
    std::size_t nz()const{return nz_;}
    double cell_size_m()const{return dx_;}
    void set_water(std::size_t x,std::size_t y,std::size_t z,
                   std::uint32_t material_id,const MaterialLibrary& lib,
                   double mass_kg,double temperature_k,double conductivity,
                   double initial_liquid_fraction=1){
        auto& cell=cells_.at(idx(x,y,z));
        if(cell.material_id)throw std::invalid_argument("Thermal cell already occupied");
        if(lib.at(material_id).phase!=Phase::Liquid)
            throw std::invalid_argument("Water material must be liquid in library");
        valid_positive(mass_kg,"Water mass");
        valid_positive(conductivity,"Water thermal conductivity");
        const WaterThermalState water(mass_kg,temperature_k,initial_liquid_fraction);
        cell={material_id,true,mass_kg,water.total_enthalpy_j(),conductivity,0};
    }
    void set_solid(std::size_t x,std::size_t y,std::size_t z,
                   std::uint32_t material_id,const MaterialLibrary& lib,
                   double mass_kg,double temperature_k,
                   double conductivity,double specific_heat){
        auto& cell=cells_.at(idx(x,y,z));
        if(cell.material_id)throw std::invalid_argument("Thermal cell already occupied");
        if(lib.at(material_id).phase!=Phase::Solid)
            throw std::invalid_argument("Thermal solid requires solid material");
        valid_positive(mass_kg,"Solid mass");
        valid_positive(conductivity,"Solid thermal conductivity");
        valid_positive(specific_heat,"Solid specific heat");
        if(!std::isfinite(temperature_k)||temperature_k<0)
            throw std::invalid_argument("Invalid solid temperature");
        cell={material_id,false,mass_kg,
              mass_kg*specific_heat*(temperature_k-melting_),
              conductivity,specific_heat};
    }
    const ThermalVoxelState& at(std::size_t x,std::size_t y,std::size_t z)const{
        return cells_.at(idx(x,y,z));
    }
    double temperature_k(std::size_t x,std::size_t y,std::size_t z)const{
        const auto& cell=at(x,y,z);
        if(!cell.material_id)throw std::invalid_argument("Empty thermal cell");
        return temp(cell);
    }
    PhaseFractions water_phase(std::size_t x,std::size_t y,std::size_t z)const{
        const auto& cell=at(x,y,z);
        if(!cell.water)throw std::invalid_argument("Thermal voxel is not water");
        const double h=cell.enthalpy_j/cell.mass_kg;
        const double at_boil=fusion_+liquid_cp_*(boiling_-melting_);
        if(h<0)return {1,0,0,water_temperature(h)};
        if(h<=fusion_){const double f=h/fusion_;return {1-f,f,0,melting_};}
        if(h<at_boil)return {0,1,0,water_temperature(h)};
        if(h<=at_boil+evaporation_){
            const double f=(h-at_boil)/evaporation_;
            return {0,1-f,f,boiling_};
        }
        return {0,0,1,water_temperature(h)};
    }
    double total_mass_kg()const{
        double m=0;for(const auto& c:cells_)m+=c.mass_kg;return m;
    }
    double total_enthalpy_j()const{
        double e=0;for(const auto& c:cells_)e+=c.enthalpy_j;return e;
    }
    double supplied_heat_j()const{return injected_heat_j_;}
    std::size_t last_substeps()const{return substeps_;}
    void add_heat_j(std::size_t x,std::size_t y,std::size_t z,double energy){
        auto& cell=cells_.at(idx(x,y,z));
        if(!cell.material_id||!std::isfinite(energy))
            throw std::invalid_argument("Invalid thermal energy source");
        const double new_h=cell.enthalpy_j+energy;
        const double absolute_zero=cell.water?
            cell.mass_kg*(-ice_cp_*melting_):
            cell.mass_kg*(-cell.specific_heat_j_kg_k*melting_);
        if(!std::isfinite(new_h)||new_h<absolute_zero)
            throw std::invalid_argument("Thermal energy below 0 K");
        cell.enthalpy_j=new_h;
        injected_heat_j_+=energy;
    }
    // Synchronous six-face finite-volume conduction. No-flux boundaries.
    // Harmonic k across material interfaces; pairwise anti-symmetric flux.
    // dt adaptive to min cp so all sensible temperatures remain nonnegative.
    ThermalExchangeAudit step(double dt){
        if(!std::isfinite(dt)||dt<=0||dt>2)
            throw std::invalid_argument("Invalid thermal time step");
        ThermalExchangeAudit audit;
        audit.initial_energy_j=total_enthalpy_j();
        const double area_over_distance=dx_;
        std::vector<double> conductance_sum(cells_.size(),0);
        struct Edge{std::size_t a,b;double conductance;};
        std::vector<Edge> edges;
        edges.reserve(cells_.size()*3);
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            const auto a=idx(x,y,z);
            if(!cells_[a].material_id)continue;
            auto connect=[&](std::size_t b){
                if(!cells_[b].material_id)return;
                const double G=harmonic(cells_[a].conductivity_w_m_k,
                                        cells_[b].conductivity_w_m_k)*area_over_distance;
                if(G<=0)return;
                edges.push_back({a,b,G});
                conductance_sum[a]+=G;conductance_sum[b]+=G;
            };
            if(x+1<nx_)connect(idx(x+1,y,z));
            if(y+1<ny_)connect(idx(x,y+1,z));
            if(z+1<nz_)connect(idx(x,y,z+1));
        }
        double safe_dt=dt;
        for(std::size_t i=0;i<cells_.size();++i){
            if(conductance_sum[i]<=0)continue;
            const double capacity=cells_[i].mass_kg*min_cp(cells_[i]);
            safe_dt=std::min(safe_dt,0.4*capacity/conductance_sum[i]);
        }
        if(!std::isfinite(safe_dt)||safe_dt<=0)
            throw std::runtime_error("Thermal timestep stability failure");
        const std::size_t count=static_cast<std::size_t>(std::ceil(dt/safe_dt));
        if(count>100000)throw std::runtime_error("Thermal substep cap");
        const double h=dt/static_cast<double>(count);
        std::vector<double> delta(cells_.size());
        for(std::size_t s=0;s<count;++s){
            std::fill(delta.begin(),delta.end(),0.0);
            for(const auto& edge:edges){
                const double q=h*edge.conductance*
                    (temp(cells_[edge.a])-temp(cells_[edge.b]));
                delta[edge.a]-=q;
                delta[edge.b]+=q;
            }
            for(std::size_t i=0;i<cells_.size();++i){
                auto& c=cells_[i];
                if(!c.material_id)continue;
                const double next=c.enthalpy_j+delta[i];
                const double zero=c.mass_kg*(c.water?-ice_cp_*melting_:
                    -c.specific_heat_j_kg_k*melting_);
                if(!std::isfinite(next)||next<zero-1e-7)
                    throw std::runtime_error("Thermal positivity/energy failure");
                c.enthalpy_j=std::max(next,zero);
            }
        }
        substeps_=count;
        audit.final_energy_j=total_enthalpy_j();
        audit.residual_j=audit.final_energy_j-audit.initial_energy_j;
        audit.substeps=count;
        const double tolerance=1e-8*std::max(1.0,std::abs(audit.initial_energy_j));
        if(std::abs(audit.residual_j)>tolerance)
            throw std::runtime_error("Thermal conduction energy balance failure");
        return audit;
    }
    // Snapshot-based bridge: reads current MAC liquid mass and solid VoxelGrid
    // temperature into a thermal grid. Does NOT transfer phase changes back
    // into MAC/VOF and must not be mistaken for advective thermal coupling.
    void import_runtime_snapshot(const UnifiedPhysicsRuntime& rt,
                                 const CoupledMacWater3D& water,
                                 std::uint32_t water_id,double water_temperature_k,
                                 double water_conductivity,
                                 double solid_conductivity,
                                 double solid_specific_heat){
        if(nx_!=water.nx()||ny_!=water.ny()||nz_!=water.nz()||
           nx_!=rt.core().voxels().nx()||ny_!=rt.core().voxels().ny()||
           nz_!=rt.core().voxels().nz()||
           std::abs(dx_-water.cell_size_m())>1e-12||
           std::abs(dx_-rt.core().voxels().cell_size_m())>1e-12)
            throw std::invalid_argument("Thermal runtime grids not aligned");
        valid_positive(water_conductivity,"Invalid liquid thermal conductivity");
        valid_positive(solid_conductivity,"Invalid solid thermal conductivity");
        valid_positive(solid_specific_heat,"Invalid solid heat capacity");
        if(!std::isfinite(water_temperature_k)||water_temperature_k<melting_||
           water_temperature_k>boiling_)
            throw std::invalid_argument("Snapshot liquid temperature out of range");
        for(const auto& c:cells_)if(c.material_id)
            throw std::invalid_argument("Thermal import requires empty grid");
        const auto& lib=rt.core().materials();
        if(lib.at(water_id).phase!=Phase::Liquid)
            throw std::invalid_argument("Snapshot water ID not liquid");
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            const auto v=rt.core().voxels().at(x,y,z);
            const double fraction=water.water_fraction(x,y,z);
            if(v.material_id&&fraction>0)
                throw std::invalid_argument("Snapshot overlapping solid and water");
            if(v.material_id){
                const auto& mat=lib.at(v.material_id);
                set_solid(x,y,z,v.material_id,lib,
                          mat.density_kg_m3*dx_*dx_*dx_*v.fill,
                          v.temperature_k,solid_conductivity,solid_specific_heat);
            }else if(fraction>0){
                set_water(x,y,z,water_id,lib,
                          fraction*dx_*dx_*dx_*water.density_kg_m3(),
                          water_temperature_k,water_conductivity);
            }
        }
    }
    void write_solid_temperatures(VoxelGrid& grid,const MaterialLibrary& lib)const{
        if(nx_!=grid.nx()||ny_!=grid.ny()||nz_!=grid.nz()||
           std::abs(dx_-grid.cell_size_m())>1e-12)
            throw std::invalid_argument("Thermal export grids not aligned");
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            const auto& c=at(x,y,z);
            if(!c.material_id||c.water)continue;
            auto v=grid.at(x,y,z);
            if(v.material_id!=c.material_id)
                throw std::invalid_argument("Thermal solid voxel ID changed");
            v.temperature_k=temp(c);
            grid.set(x,y,z,v,lib);
        }
    }
};
} // namespace upc
