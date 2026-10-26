#pragma once
// P7: conservative, deterministic sparse render LOD for the REAL P2 MAC/VOF
// water grid. This is a read-only spatial snapshot, NOT a coarse CFD solver.
// Coarse boundary cells are volume-weighted by their actual fine-cell counts.
#include "upc/coupled_water3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace upc {
class SparseWaterLod3D {
public:
    static constexpr std::size_t brick_edge=8;
    static constexpr std::size_t cells_per_brick=brick_edge*brick_edge*brick_edge;
    struct Sample {
        double water_fraction=0;
        double solid_fraction=0;
    };
private:
    struct Brick {
        std::array<Sample,cells_per_brick> cells{};
    };
    struct Level {
        std::size_t nx=0,ny=0,nz=0,scale=1;
        std::map<std::array<std::size_t,3>,Brick> bricks;
    };
    std::vector<Level> levels_;
    std::size_t nx_=0,ny_=0,nz_=0;
    double dx_=0;
    static std::size_t ceil_div(std::size_t a,std::size_t b){
        return a/b+(a%b!=0);
    }
    static std::size_t offset(std::size_t x,std::size_t y,std::size_t z){
        return ((z%brick_edge)*brick_edge+y%brick_edge)*brick_edge+x%brick_edge;
    }
    static std::array<std::size_t,3> key(std::size_t x,std::size_t y,std::size_t z){
        return {x/brick_edge,y/brick_edge,z/brick_edge};
    }
    static Sample sample(const Level& l,std::size_t x,std::size_t y,std::size_t z){
        if(x>=l.nx||y>=l.ny||z>=l.nz)
            throw std::out_of_range("Sparse LOD voxel outside grid");
        const auto it=l.bricks.find(key(x,y,z));
        if(it==l.bricks.end())return {};
        return it->second.cells[offset(x,y,z)];
    }
    static std::size_t valid_count(std::size_t i,std::size_t scale,std::size_t dim){
        const std::size_t begin=i*scale;
        return std::min(scale,dim-begin);
    }
public:
    void rebuild(const CoupledMacWater3D& source,std::size_t max_level){
        const std::size_t x=source.nx(),y=source.ny(),z=source.nz();
        const double dx=source.cell_size_m();
        if(!x||!y||!z||!std::isfinite(dx)||dx<=0)
            throw std::invalid_argument("Invalid MAC grid for LOD");
        if(max_level>sizeof(std::size_t)*8-2)
            throw std::invalid_argument("LOD shift exceeds safe integer range");
        // Reject levels that would duplicate a 1x1x1 level.
        const std::size_t max_dim=std::max({x,y,z});
        std::size_t max_valid=0;
        for(std::size_t n=max_dim;n>1;n=ceil_div(n,2))++max_valid;
        if(max_level>max_valid)
            throw std::invalid_argument("LOD exceeds useful domain resolution");
        std::vector<Level> next;
        next.reserve(max_level+1);
        for(std::size_t lod=0;lod<=max_level;++lod){
            Level l;
            l.scale=std::size_t(1)<<lod;
            l.nx=ceil_div(x,l.scale);l.ny=ceil_div(y,l.scale);
            l.nz=ceil_div(z,l.scale);
            for(std::size_t iz=0;iz<z;++iz)
            for(std::size_t iy=0;iy<y;++iy)
            for(std::size_t ix=0;ix<x;++ix){
                const double water=source.water_fraction(ix,iy,iz);
                const bool solid=source.is_solid(ix,iy,iz);
                if(!std::isfinite(water)||water<0||water>1||
                   (solid&&water>0))
                    throw std::runtime_error("Invalid source MAC/VOF cell");
                if(water==0&&!solid)continue; // no brick for empty space
                const auto cx=ix/l.scale,cy=iy/l.scale,cz=iz/l.scale;
                auto& cell=l.bricks[key(cx,cy,cz)].cells[offset(cx,cy,cz)];
                cell.water_fraction+=water;
                cell.solid_fraction+=solid?1.0:0.0;
            }
            // Coarse samples are fractions of the physical covered volume,
            // including partial edge cells; never divide by 2^(3*LOD) blindly.
            for(auto& [brick_key,brick]:l.bricks){
                for(std::size_t bz=0;bz<brick_edge;++bz)
                for(std::size_t by=0;by<brick_edge;++by)
                for(std::size_t bx=0;bx<brick_edge;++bx){
                    const std::size_t cx=brick_key[0]*brick_edge+bx;
                    const std::size_t cy=brick_key[1]*brick_edge+by;
                    const std::size_t cz=brick_key[2]*brick_edge+bz;
                    if(cx>=l.nx||cy>=l.ny||cz>=l.nz)continue;
                    auto& cell=brick.cells[offset(cx,cy,cz)];
                    const double count=static_cast<double>(
                        valid_count(cx,l.scale,x)*
                        valid_count(cy,l.scale,y)*
                        valid_count(cz,l.scale,z));
                    cell.water_fraction/=count;
                    cell.solid_fraction/=count;
                    if(cell.water_fraction<-1e-12||cell.solid_fraction<-1e-12||
                       cell.water_fraction+cell.solid_fraction>1+1e-12)
                        throw std::runtime_error("Invalid conservative LOD fraction");
                }
            }
            next.push_back(std::move(l));
        }
        levels_.swap(next);
        nx_=x;ny_=y;nz_=z;dx_=dx;
    }
    std::size_t level_count()const{return levels_.size();}
    Sample at(std::size_t lod,std::size_t x,std::size_t y,std::size_t z)const{
        return sample(levels_.at(lod),x,y,z);
    }
    std::array<std::size_t,3> dimensions(std::size_t lod)const{
        const auto& l=levels_.at(lod);
        return {l.nx,l.ny,l.nz};
    }
    std::size_t active_bricks(std::size_t lod)const{
        return levels_.at(lod).bricks.size();
    }
    std::size_t resident_bytes_model()const{
        // Lower bound: brick payload only; excludes map nodes/allocator metadata.
        std::size_t bytes=0;
        for(const auto& l:levels_)bytes+=l.bricks.size()*sizeof(Brick);
        return bytes;
    }
    std::size_t dense_bytes_model()const{
        std::size_t bytes=0;
        for(const auto& l:levels_)bytes+=l.nx*l.ny*l.nz*sizeof(Sample);
        return bytes;
    }
    double water_volume_m3(std::size_t lod)const{
        const auto& l=levels_.at(lod);
        double fine_cells=0;
        for(const auto& [brick_key,brick]:l.bricks)
        for(std::size_t bz=0;bz<brick_edge;++bz)
        for(std::size_t by=0;by<brick_edge;++by)
        for(std::size_t bx=0;bx<brick_edge;++bx){
            const auto cx=brick_key[0]*brick_edge+bx;
            const auto cy=brick_key[1]*brick_edge+by;
            const auto cz=brick_key[2]*brick_edge+bz;
            if(cx>=l.nx||cy>=l.ny||cz>=l.nz)continue;
            const auto cell=brick.cells[offset(cx,cy,cz)];
            const double covered=static_cast<double>(
                valid_count(cx,l.scale,nx_)*
                valid_count(cy,l.scale,ny_)*
                valid_count(cz,l.scale,nz_));
            fine_cells+=cell.water_fraction*covered;
        }
        return fine_cells*dx_*dx_*dx_;
    }
};
} // namespace upc
