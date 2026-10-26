#pragma once
// Unified entry point for legacy 2D domains and P2 3D water.
// It selects material density/phase from the shared material library.
// Future P3 will couple VoxelGrid and 3D fluid occupation physically.
#include "upc/core.hpp"
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace upc {
class UnifiedPhysicsRuntime {
    UnifiedPhysicsCore legacy_;
    std::vector<std::unique_ptr<CoupledMacWater3D>> fluids3d_;
public:
    UnifiedPhysicsRuntime(std::size_t nx,std::size_t ny,std::size_t nz,double dx)
        :legacy_(nx,ny,nz,dx) {}
    UnifiedPhysicsCore& core() noexcept {return legacy_;}
    const UnifiedPhysicsCore& core()const noexcept{return legacy_;}
    CoupledMacWater3D& add_water3d(std::size_t nx,std::size_t ny,std::size_t nz,
                                  double dx,std::uint32_t material_id){
        const auto& mat=legacy_.materials().at(material_id);
        if(mat.phase!=Phase::Liquid)throw std::invalid_argument("Registered 3D water material must be liquid");
        fluids3d_.push_back(std::make_unique<CoupledMacWater3D>(
            nx,ny,nz,dx,mat.density_kg_m3));
        return *fluids3d_.back();
    }
    std::size_t fluid3d_count()const noexcept{return fluids3d_.size();}
    CoupledMacWater3D& fluid3d(std::size_t i){return *fluids3d_.at(i);}
    void step(double dt_seconds,int iterations=250){
        if(!std::isfinite(dt_seconds)||dt_seconds<=0||dt_seconds>2)
            throw std::invalid_argument("Invalid unified runtime dt");
        legacy_.step(dt_seconds);
        for(auto& w:fluids3d_)w->step(dt_seconds,iterations);
    }
};
} // namespace upc
