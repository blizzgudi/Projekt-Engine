#pragma once
// P5 bounded 3D lattice solid CPU MVP. Axial/diagonal spring connectivity
// preserves shear resistance, accumulates fracture-released elastic energy.
// Does NOT solve isotropic FEM stress-strain, bending, plasticity or full
// volumetric self-collision. Fragmentation is intact-bond connectivity.
#include "upc/core.hpp"
#include "upc/solid.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
namespace upc {
struct LatticeMaterial3D {
    std::uint32_t id=0;
    double stiffness_n_per_m=0;
    double fracture_strain=0;
};
class SpringLattice3D {
    SpringSolid3D solid_;
    std::size_t nx_,ny_,nz_;
    double dx_,rho_,stiffness_;
    std::uint32_t material_id_;
    std::size_t index(std::size_t x,std::size_t y,std::size_t z)const {
        if(x>=nx_||y>=ny_||z>=nz_)
            throw std::out_of_range("Lattice index");
        return (z*ny_+y)*nx_+x;
    }
public:
    SpringLattice3D(const MaterialLibrary& lib,
                    std::size_t nx,std::size_t ny,std::size_t nz,
                    double spacing_m,LatticeMaterial3D material,
                    Vec3 start={0,0,0})
       :nx_(nx),ny_(ny),nz_(nz),dx_(spacing_m),
        rho_(lib.at(material.id).density_kg_m3),
        stiffness_(material.stiffness_n_per_m),material_id_(material.id){
        if(lib.at(material_id_).phase!=Phase::Solid||
           !nx_||!ny_||!nz_||!std::isfinite(dx_)||dx_<=0||
           !std::isfinite(stiffness_)||stiffness_<=0||
           !std::isfinite(material.fracture_strain)||material.fracture_strain<=0||
           !std::isfinite(start.x)||!std::isfinite(start.y)||!std::isfinite(start.z)||
           nx_>std::numeric_limits<std::size_t>::max()/ny_||
           nx_*ny_>std::numeric_limits<std::size_t>::max()/nz_)
            throw std::invalid_argument("Invalid lattice dimensions or material");
        const double pointmass=rho_*dx_*dx_*dx_;
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            solid_.add_node({start+Vec3{dx_*x,dx_*y,dx_*z},
                             {0,0,0},pointmass,false});
        // All 26-neighborhood links exactly once; face/edge/corner diagonals
        // avoid an underconstrained shear-only axis lattice.
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            for(int dz=-1;dz<=1;++dz)
            for(int dy=-1;dy<=1;++dy)
            for(int ddx=-1;ddx<=1;++ddx){
                if(dz<0||(dz==0&&dy<0)||
                   (dz==0&&dy==0&&ddx<=0))continue;
                const auto xx=static_cast<long long>(x)+ddx;
                const auto yy=static_cast<long long>(y)+dy;
                const auto zz=static_cast<long long>(z)+dz;
                if(xx<0||yy<0||zz<0||
                   xx>=static_cast<long long>(nx_)||
                   yy>=static_cast<long long>(ny_)||
                   zz>=static_cast<long long>(nz_))continue;
                solid_.add_bond(index(x,y,z),
                       index(static_cast<std::size_t>(xx),
                             static_cast<std::size_t>(yy),
                             static_cast<std::size_t>(zz)),
                       stiffness_,material.fracture_strain);
            }
        }
    }
    SpringSolid3D& solid(){return solid_;}
    const SpringSolid3D& solid()const{return solid_;}
    const MassNode& node(std::size_t x,std::size_t y,std::size_t z)const{
        return solid_.node(index(x,y,z));
    }
    MassNode& node(std::size_t x,std::size_t y,std::size_t z){
        return solid_.node(index(x,y,z));
    }
    void fix_layer(std::size_t z){
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            node(x,y,z).fixed=true;
    }
    std::size_t count()const{return solid_.node_count();}
    std::size_t intact_bonds()const{return solid_.intact_bonds();}
    std::size_t bond_count()const{return solid_.bonds().size();}
    std::uint32_t material_id()const{return material_id_;}
    double lattice_mass_kg()const{
        return rho_*dx_*dx_*dx_*solid_.node_count();
    }
    double fracture_energy_j()const{return solid_.fracture_released_j();}
    // Elastic+gravity energy is not perfectly preserved by symplectic Euler.
    // Soft bound on stable dt based on fastest local mass/spring vibration.
    std::size_t step(double dt){
        if(!std::isfinite(dt)||dt<=0||dt>0.2)
            throw std::invalid_argument("Invalid lattice dt");
        const double pointmass=rho_*dx_*dx_*dx_;
        const double safe=0.12*std::sqrt(pointmass/(26*stiffness_));
        const auto steps=static_cast<std::size_t>(std::ceil(dt/safe));
        if(!steps||steps>100000)
            throw std::runtime_error("Lattice substep cap");
        for(std::size_t k=0;k<steps;++k)solid_.step(dt/steps);
        return steps;
    }
    // Export occupied material voxels and their broken-bond damage into the
    // shared Engine VoxelGrid (no synthetic pressure/stress field).
    void export_damage_to_voxels(VoxelGrid& grid,const MaterialLibrary& lib,
                                 std::size_t x0=0,std::size_t y0=0,
                                 std::size_t z0=0)const{
        if(x0>grid.nx()||y0>grid.ny()||z0>grid.nz()||
           nx_>grid.nx()-x0||ny_>grid.ny()-y0||nz_>grid.nz()-z0||
           std::abs(grid.cell_size_m()-dx_)>1e-12)
            throw std::invalid_argument("Lattice and material voxel geometry differ");
        if(lib.at(material_id_).phase!=Phase::Solid)
            throw std::invalid_argument("Lattice material ID no longer solid");
        // Preflight prevents half-exported damage on collision.
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            const auto c=grid.at(x+x0,y+y0,z+z0);
            if(c.material_id!=0&&c.material_id!=material_id_)
                throw std::invalid_argument("Damage would overwrite other material");
        }
        std::vector<std::size_t> degree(count()), intact(count());
        for(const auto& b:solid_.bonds()){
            ++degree[b.a];++degree[b.b];
            if(b.intact){++intact[b.a];++intact[b.b];}
        }
        for(std::size_t z=0;z<nz_;++z)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            const auto i=index(x,y,z);
            auto c=grid.at(x+x0,y+y0,z+z0);
            c.material_id=material_id_;
            c.fill=1;
            c.damage=degree[i]?1.0-static_cast<double>(intact[i])/degree[i]:0;
            grid.set(x+x0,y+y0,z+z0,c,lib);
        }
    }
    // Connected components through still-intact elastic bonds, including
    // isolated nodes as one-node fragments.
    std::vector<std::size_t> component_sizes()const{
        std::vector<std::size_t> parent(count());
        for(std::size_t i=0;i<count();++i)parent[i]=i;
        auto root=[&](std::size_t a){
            while(parent[a]!=a){parent[a]=parent[parent[a]];a=parent[a];}
            return a;
        };
        for(const auto& b:solid_.bonds())if(b.intact){
            const auto a=root(b.a),c=root(b.b);
            if(a!=c)parent[c]=a;
        }
        std::vector<std::size_t> counts(count(),0);
        for(std::size_t i=0;i<count();++i)++counts[root(i)];
        std::vector<std::size_t> fragments;
        for(auto v:counts)if(v)fragments.push_back(v);
        std::sort(fragments.begin(),fragments.end(),std::greater<std::size_t>());
        return fragments;
    }
};
} // namespace upc
