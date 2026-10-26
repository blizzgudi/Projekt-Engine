#pragma once
// UnifiedPhysicsCore v0.1: portable C++17, CPU-only.
// Shared materials/voxel state + conservative 2D shallow-water simulation.
// Future solid, gas, thermal and 3D fluid solvers must use same core interface.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace upc {

enum class Phase { Solid, Liquid, Gas };
struct Material {
    std::uint32_t id;
    Phase phase;
    double density_kg_m3;
    double viscosity_pa_s;  // reference metadata: not yet used by solver
};
class MaterialLibrary {
    std::unordered_map<std::uint32_t,Material> items_;
public:
    void add(Material m) {
        if(!m.id || !std::isfinite(m.density_kg_m3) || m.density_kg_m3<=0 ||
           !std::isfinite(m.viscosity_pa_s) || m.viscosity_pa_s<0 ||
           !items_.emplace(m.id,m).second)
            throw std::invalid_argument("Invalid or duplicate material");
    }
    const Material& at(std::uint32_t id) const {
        auto it=items_.find(id);
        if(it==items_.end()) throw std::out_of_range("Unknown material");
        return it->second;
    }
    std::size_t size() const {return items_.size();}
};
struct Voxel {
    std::uint32_t material_id=0; // 0 = empty
    double fill=0;               // 0..1
    double temperature_k=293.15; // stored, not yet simulated
    double damage=0;             // stored, not yet simulated
};
class VoxelGrid {
    std::size_t nx_,ny_,nz_;
    double dx_;
    std::vector<Voxel> cells_;
    std::size_t idx(std::size_t x,std::size_t y,std::size_t z) const {
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("Voxel position");
        return (z*ny_+y)*nx_+x;
    }
public:
    VoxelGrid(std::size_t nx,std::size_t ny,std::size_t nz,double dx)
      :nx_(nx),ny_(ny),nz_(nz),dx_(dx){
        if(!nx||!ny||!nz||!std::isfinite(dx)||dx<=0||
           nx>std::numeric_limits<std::size_t>::max()/ny ||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz)
            throw std::invalid_argument("Invalid grid");
        cells_.resize(nx*ny*nz);
    }
    void set(std::size_t x,std::size_t y,std::size_t z,Voxel v,
             const MaterialLibrary& lib){
        if(!std::isfinite(v.fill)||v.fill<0||v.fill>1||
           !std::isfinite(v.temperature_k)||v.temperature_k<0||
           !std::isfinite(v.damage)||v.damage<0||v.damage>1||
           (v.material_id==0 && v.fill!=0))
            throw std::invalid_argument("Invalid voxel properties");
        if(v.material_id)lib.at(v.material_id);
        cells_[idx(x,y,z)]=v;
    }
    Voxel at(std::size_t x,std::size_t y,std::size_t z)const{return cells_[idx(x,y,z)];}
    std::size_t size()const{return cells_.size();}
    std::size_t nx()const{return nx_;}
    std::size_t ny()const{return ny_;}
    std::size_t nz()const{return nz_;}
    double cell_size_m()const{return dx_;}

    double mass_kg(const MaterialLibrary& lib)const{
        const double volume=dx_*dx_*dx_;
        double mass=0;
        for(const auto& c:cells_)if(c.material_id)
            mass+=volume*c.fill*lib.at(c.material_id).density_kg_m3;
        return mass;
    }
};

// Depth-averaged free-surface water. NOT a volumetric 3D fluid solver.
// First-order finite volume + HLL Riemann flux + reflective wall/obstacle boundaries.
struct WaterCell {
    double h=0;             // surface height/depth m
    double hx=0;            // h * vx, m2/s
    double hy=0;            // h * vy, m2/s
    bool solid=false;
};
class ShallowWater2D {
    struct F{double h,hx,hy;};
    std::size_t nx_,ny_;
    double dx_;
    std::uint32_t mat_;
    std::vector<WaterCell> a_;
    static constexpr double g=9.81;
    static constexpr double dry=1e-12;
    std::size_t idx(std::size_t x,std::size_t y)const{
        if(x>=nx_||y>=ny_)throw std::out_of_range("Water position");
        return y*nx_+x;
    }
    static F state(const WaterCell& c){return {c.h,c.hx,c.hy};}
    static F mirror(F q,bool x){if(x)q.hx=-q.hx;else q.hy=-q.hy;return q;}
    static F flux(F q,bool x) {
        const double p=0.5*g*q.h*q.h;
        if(q.h<=dry)return x?F{0,p,0}:F{0,0,p};
        if(x)return {q.hx,q.hx*q.hx/q.h+p,q.hx*q.hy/q.h};
        return {q.hy,q.hx*q.hy/q.h,q.hy*q.hy/q.h+p};
    }
    static double wavespeed(F q,bool x){
        if(q.h<=dry)return 0;
        return std::abs((x?q.hx:q.hy)/q.h)+std::sqrt(g*q.h);
    }
    // HLL approximate Riemann flux. In a wet/dry interface use the exact
    // rarefaction fan bounds (u +/- 2c), rather than the local wet
    // eigenvalues only. This avoids artificially slow dry-bed expansion.
    static F hll_flux(F l,F r,bool x){
        if(l.h<0||r.h<0)throw std::runtime_error("Negative Riemann state");
        if(l.h<=dry&&r.h<=dry)return {0,0,0};
        const double cl=std::sqrt(g*l.h),cr=std::sqrt(g*r.h);
        const double ul=l.h>dry?(x?l.hx:l.hy)/l.h:0;
        const double ur=r.h>dry?(x?r.hx:r.hy)/r.h:0;
        double sl=0,sr=0;
        if(l.h<=dry){sl=ur-2*cr;sr=ur+cr;}
        else if(r.h<=dry){sl=ul-cl;sr=ul+2*cl;}
        else {sl=std::min(ul-cl,ur-cr);sr=std::max(ul+cl,ur+cr);}
        const F a=flux(l,x),b=flux(r,x);
        if(sl>=0)return a;
        if(sr<=0)return b;
        const double inv=1.0/(sr-sl);
        return {(sr*a.h-sl*b.h+sl*sr*(r.h-l.h))*inv,
                (sr*a.hx-sl*b.hx+sl*sr*(r.hx-l.hx))*inv,
                (sr*a.hy-sl*b.hy+sl*sr*(r.hy-l.hy))*inv};
    }
    double max_speed()const{
        double s=0;
        for(const auto& c:a_)if(!c.solid&&c.h>dry){
            const auto q=state(c);
            s=std::max({s,wavespeed(q,true),wavespeed(q,false)});
        }
        return s;
    }
    void substep(double dt){
        std::vector<F> fx((nx_+1)*ny_),fy(nx_*(ny_+1));
        for(std::size_t y=0;y<ny_;++y)for(std::size_t x=0;x<=nx_;++x){
            const bool left=x>0&&!a_[idx(x-1,y)].solid;
            const bool right=x<nx_&&!a_[idx(x,y)].solid;
            F l{0,0,0},r{0,0,0};
            if(left)l=state(a_[idx(x-1,y)]);
            if(right)r=state(a_[idx(x,y)]);
            if(left&&right)fx[y*(nx_+1)+x]=hll_flux(l,r,true);
            else if(left)fx[y*(nx_+1)+x]=hll_flux(l,mirror(l,true),true);
            else if(right)fx[y*(nx_+1)+x]=hll_flux(mirror(r,true),r,true);
        }
        for(std::size_t y=0;y<=ny_;++y)for(std::size_t x=0;x<nx_;++x){
            const bool bottom=y>0&&!a_[idx(x,y-1)].solid;
            const bool top=y<ny_&&!a_[idx(x,y)].solid;
            F b{0,0,0},t{0,0,0};
            if(bottom)b=state(a_[idx(x,y-1)]);
            if(top)t=state(a_[idx(x,y)]);
            if(bottom&&top)fy[y*nx_+x]=hll_flux(b,t,false);
            else if(bottom)fy[y*nx_+x]=hll_flux(b,mirror(b,false),false);
            else if(top)fy[y*nx_+x]=hll_flux(mirror(t,false),t,false);
        }
        auto next=a_;
        for(std::size_t y=0;y<ny_;++y)for(std::size_t x=0;x<nx_;++x){
            const auto i=idx(x,y);
            if(a_[i].solid)continue;
            const F l=fx[y*(nx_+1)+x],r=fx[y*(nx_+1)+x+1];
            const F b=fy[y*nx_+x],t=fy[(y+1)*nx_+x];
            F q=state(a_[i]);
            const double f=dt/dx_;
            q.h-=f*(r.h-l.h+t.h-b.h);
            q.hx-=f*(r.hx-l.hx+t.hx-b.hx);
            q.hy-=f*(r.hy-l.hy+t.hy-b.hy);
            if(!std::isfinite(q.h)||!std::isfinite(q.hx)||!std::isfinite(q.hy)||
               q.h < -1e-10)
                throw std::runtime_error("Fluid stability violation");
            if(q.h<dry){q.h=0;q.hx=0;q.hy=0;}
            next[i].h=q.h;next[i].hx=q.hx;next[i].hy=q.hy;
        }
        a_.swap(next);
    }
public:
    ShallowWater2D(std::size_t nx,std::size_t ny,double dx,std::uint32_t material)
      :nx_(nx),ny_(ny),dx_(dx),mat_(material){
        if(!nx||!ny||!material||!std::isfinite(dx)||dx<=0||
           nx>std::numeric_limits<std::size_t>::max()/ny)
            throw std::invalid_argument("Invalid fluid grid");
        a_.resize(nx*ny);
    }
    void set_depth(std::size_t x,std::size_t y,double h,double vx=0,double vy=0){
        if(!std::isfinite(h)||h<0||!std::isfinite(vx)||!std::isfinite(vy)||
           !std::isfinite(h*vx)||!std::isfinite(h*vy))
            throw std::invalid_argument("Invalid water state");
        auto& c=a_[idx(x,y)];
        if(c.solid&&h!=0)throw std::invalid_argument("Water inside obstacle");
        c.h=h;c.hx=h*vx;c.hy=h*vy;
    }
    void set_solid(std::size_t x,std::size_t y,bool solid=true){
        auto& c=a_[idx(x,y)];
        if(solid&&c.h>0)throw std::invalid_argument("Solid overlaps water");
        c.solid=solid;
        if(solid){c.h=0;c.hx=0;c.hy=0;}
    }
    WaterCell at(std::size_t x,std::size_t y)const{return a_[idx(x,y)];}
    std::size_t nx()const{return nx_;}
    std::size_t ny()const{return ny_;}
    void step(double seconds){
        if(!std::isfinite(seconds)||seconds<=0||seconds>10)
            throw std::invalid_argument("Bad time step");
        double left=seconds;
        for(int k=0;left>1e-13;++k){
            if(k>=200000)throw std::runtime_error("Substep limit reached");
            const double v=max_speed();
            const double dt=std::min(left,v>0?0.22*dx_/v:left);
            substep(dt);
            left-=dt;
        }
    }
    double volume_m3()const{
        double v=0;for(const auto& c:a_)v+=c.h*dx_*dx_;return v;
    }
    double mass_kg(const MaterialLibrary& materials)const{
        const auto& m=materials.at(mat_);
        if(m.phase!=Phase::Liquid)throw std::invalid_argument("Not liquid");
        return volume_m3()*m.density_kg_m3;
    }
    double pressure_pa(std::size_t x,std::size_t y,const MaterialLibrary& materials)const{
        const auto& m=materials.at(mat_);
        if(m.phase!=Phase::Liquid)throw std::invalid_argument("Not liquid");
        return m.density_kg_m3*g*at(x,y).h; // hydrostatic bottom pressure
    }
};
class UnifiedPhysicsCore {
    MaterialLibrary materials_;
    VoxelGrid voxels_;
    std::vector<ShallowWater2D> water_;
public:
    UnifiedPhysicsCore(std::size_t nx,std::size_t ny,std::size_t nz,double dx)
       :voxels_(nx,ny,nz,dx){}
    MaterialLibrary& materials(){return materials_;}
    const MaterialLibrary& materials()const{return materials_;}
    VoxelGrid& voxels(){return voxels_;}
    const VoxelGrid& voxels()const{return voxels_;}
    ShallowWater2D& add_water(std::size_t nx,std::size_t ny,double dx,std::uint32_t material){
        if(materials_.at(material).phase!=Phase::Liquid)
            throw std::invalid_argument("Fluid requires a liquid material");
        water_.emplace_back(nx,ny,dx,material);
        return water_.back();
    }
    ShallowWater2D& water(std::size_t index){return water_.at(index);}
    std::size_t fluid_count()const{return water_.size();}
    void step(double dt){
        if(!std::isfinite(dt)||dt<=0||dt>10)throw std::invalid_argument("Invalid dt");
        for(auto& f:water_)f.step(dt);
    }
};
}
