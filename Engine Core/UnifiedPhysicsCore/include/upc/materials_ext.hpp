#pragma once
// Optional experimental material modules for UnifiedPhysicsCore.
// SI units: kg, m, K, J. Not a reactive quantum/chemical simulator.
#include "upc/core.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace upc {
struct PhaseFractions {
    double ice=0,liquid=0,vapour=0,temperature_k=273.15;
};

// Calibrated to approximate pure water at ~101325 Pa only.
// Specific enthalpy reference: solid water at 273.15K = 0 J/kg.
class WaterThermalState {
    double mass_, specific_enthalpy_;
    static constexpr double melt=273.15, boil=373.15;
    static constexpr double c_ice=2100, c_liquid=4184, c_vapour=2000;
    static constexpr double latent_fusion=334000, latent_vaporization=2256000;
public:
    WaterThermalState(double mass_kg,double initial_temperature_k,double initial_liquid_fraction=1)
        :mass_(mass_kg),specific_enthalpy_(0){
        if(!std::isfinite(mass_)||mass_<=0||!std::isfinite(initial_temperature_k)||
           initial_temperature_k<0||!std::isfinite(initial_liquid_fraction)||
           initial_liquid_fraction<0||initial_liquid_fraction>1)
            throw std::invalid_argument("Invalid thermal initial state");
        // Liquid/ice mixture at melting point, or pure ice below / liquid above melting.
        if(initial_temperature_k<melt && initial_liquid_fraction>0)
            throw std::invalid_argument("Cannot initialize liquid below melting point");
        if(initial_temperature_k>melt && initial_liquid_fraction<1)
            throw std::invalid_argument("Cannot initialize ice above melting point");
        if(initial_temperature_k==melt)
            specific_enthalpy_=initial_liquid_fraction*latent_fusion;
        else if(initial_temperature_k<melt)
            specific_enthalpy_=c_ice*(initial_temperature_k-melt);
        else if(initial_temperature_k<=boil)
            specific_enthalpy_=latent_fusion+c_liquid*(initial_temperature_k-melt);
        else
            specific_enthalpy_=latent_fusion+c_liquid*(boil-melt)+latent_vaporization
                              +c_vapour*(initial_temperature_k-boil);
    }
    double mass_kg()const{return mass_;}
    double total_enthalpy_j()const{return mass_*specific_enthalpy_;}
    void add_heat_j(double energy){
        if(!std::isfinite(energy))throw std::invalid_argument("Heat must be finite");
        if(specific_enthalpy_+energy/mass_<-c_ice*melt)
            throw std::invalid_argument("Temperature cannot fall below absolute zero");
        specific_enthalpy_+=energy/mass_;
    }
    PhaseFractions state()const{
        const double h=specific_enthalpy_;
        const double boiling_start=latent_fusion+c_liquid*(boil-melt);
        if(h<0)return {1,0,0,melt+h/c_ice};
        if(h<=latent_fusion){
            const double f=h/latent_fusion;
            return {1-f,f,0,melt};
        }
        if(h<boiling_start)return {0,1,0,melt+(h-latent_fusion)/c_liquid};
        if(h<=boiling_start+latent_vaporization){
            const double f=(h-boiling_start)/latent_vaporization;
            return {0,1-f,f,boil};
        }
        return {0,0,1,boil+(h-boiling_start-latent_vaporization)/c_vapour};
    }
};

// Per-sample composition is distinct from a shared immutable material ID.
// No chemical property prediction: blending is mass-conserving bookkeeping.
class MaterialMixture {
    std::map<std::uint32_t,double> masses_;
public:
    void add(std::uint32_t id,double mass_kg,const MaterialLibrary& lib){
        if(!id||!std::isfinite(mass_kg)||mass_kg<=0)
            throw std::invalid_argument("Invalid mass");
        lib.at(id);
        masses_[id]+=mass_kg;
    }
    void combine(const MaterialMixture& other){
        for(const auto& kv:other.masses_)masses_[kv.first]+=kv.second;
    }
    double total_mass_kg()const{
        double m=0;for(const auto& kv:masses_)m+=kv.second;return m;
    }
    double mass_kg(std::uint32_t id)const{
        auto it=masses_.find(id);return it==masses_.end()?0:it->second;
    }
    double ideal_volume_m3(const MaterialLibrary& lib)const{
        double v=0;for(const auto& kv:masses_)v+=kv.second/lib.at(kv.first).density_kg_m3;
        return v;
    }
    double ideal_density_kg_m3(const MaterialLibrary& lib)const{
        const double v=ideal_volume_m3(lib);
        if(v<=0)throw std::runtime_error("Empty mixture");
        return total_mass_kg()/v;
    }
    void transform(std::uint32_t input,std::uint32_t output,double mass_kg,
                   const MaterialLibrary& lib){
        lib.at(input);lib.at(output);
        if(input==output||!std::isfinite(mass_kg)||mass_kg<=0||
           mass_kg>this->mass_kg(input))throw std::invalid_argument("Invalid reaction");
        masses_[input]-=mass_kg;
        if(masses_[input]<=1e-13)masses_.erase(input);
        masses_[output]+=mass_kg;
    }
};

// Sparse 16^3-cell chunk registry: CPU storage prototype, not an LOD renderer.
struct ChunkPosition {
    int x,y,z;
    bool operator<(const ChunkPosition& b)const{
        if(z!=b.z)return z<b.z;
        if(y!=b.y)return y<b.y;
        return x<b.x;
    }
};
class SparseVoxelChunks {
    static constexpr int side=16;
    struct Chunk{std::vector<std::uint32_t> cells=std::vector<std::uint32_t>(4096,0);
                 std::size_t active=0;};
    std::map<ChunkPosition,Chunk> chunks_;
    static int division(int a){return a>=0?a/side:-((-a-1)/side)-1;}
    static int local(int a){return a-division(a)*side;}
public:
    void set(int x,int y,int z,std::uint32_t material_id) {
        ChunkPosition p{division(x),division(y),division(z)};
        if(!material_id&&chunks_.find(p)==chunks_.end())return;
        auto& c=chunks_[p];
        const std::size_t i=(local(z)*side+local(y))*side+local(x);
        if(c.cells[i]==0&&material_id!=0)++c.active;
        if(c.cells[i]!=0&&material_id==0)--c.active;
        c.cells[i]=material_id;
        if(c.active==0)chunks_.erase(p);
    }
    std::uint32_t get(int x,int y,int z)const{
        ChunkPosition p{division(x),division(y),division(z)};
        auto it=chunks_.find(p);
        if(it==chunks_.end())return 0;
        return it->second.cells[(local(z)*side+local(y))*side+local(x)];
    }
    std::size_t allocated_chunks()const{return chunks_.size();}
    std::size_t allocated_data_bytes()const{return chunks_.size()*4096*sizeof(std::uint32_t);}
    std::size_t active_voxels()const{
        std::size_t n=0;for(const auto& p:chunks_)n+=p.second.active;return n;
    }
};

struct PorousSoilCell {
    double pore_capacity_m3=0;
    double stored_water_m3=0;
    double infiltration_m3_per_second=0;
};
// Local exchange with water above porous soil, conservation without momentum coupling.
inline double infiltrate_water(ShallowWater2D& fluid,std::size_t x,std::size_t y,
                               PorousSoilCell& soil,double cell_size_m,double dt_seconds){
    if(!std::isfinite(cell_size_m)||cell_size_m<=0||!std::isfinite(dt_seconds)||dt_seconds<=0||
       !std::isfinite(soil.pore_capacity_m3)||!std::isfinite(soil.stored_water_m3)||
       !std::isfinite(soil.infiltration_m3_per_second)||soil.pore_capacity_m3<0||
       soil.stored_water_m3<0||soil.stored_water_m3>soil.pore_capacity_m3||
       soil.infiltration_m3_per_second<0)
        throw std::invalid_argument("Invalid porous setup");
    auto state=fluid.at(x,y);
    if(state.solid)throw std::invalid_argument("Cannot infiltrate rigid obstacle");
    double area=cell_size_m*cell_size_m;
    double captured=std::min({state.h*area,soil.pore_capacity_m3-soil.stored_water_m3,
                               soil.infiltration_m3_per_second*dt_seconds});
    if(captured<0)captured=0;
    const double vx=state.h>0?state.hx/state.h:0.0;
    const double vy=state.h>0?state.hy/state.h:0.0;
    fluid.set_depth(x,y,std::max(0.0,state.h-captured/area),vx,vy);
    soil.stored_water_m3+=captured;
    return captured;
}
} // namespace upc
