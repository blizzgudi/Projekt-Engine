#pragma once
// UPC P2: Coupled 3D free-surface MAC pressure + bounded VOF transport.
// Experimental first-order CPU solver (inviscid, non-surface-tension).
// Pressure is solved on wet MAC cells. MAC face velocities transport material
// fill fractions in all three axes. Volume fluxes are pairwise conservative.
// Velocity self-advection is semi-Lagrangian; it dissipates energy numerically.
// No FLIP/APIC particle transfers, no 3D splashes below grid resolution.
#include "upc/mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
class CoupledMacWater3D {
    struct Transfer {std::size_t from,to;double amount;};
    std::size_t nx_,ny_,nz_;
    double dx_,rho_;
    MacProjection3D mac_;
    std::vector<double> fill_;
    std::vector<std::uint8_t> solid_;
    double last_projection_divergence_=0;
    std::size_t substeps_=0;
    static constexpr double wet=1e-5;
    std::size_t cell(std::size_t x,std::size_t y,std::size_t z) const {
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("Coupled 3D index");
        return (z*ny_+y)*nx_+x;
    }
    std::size_t uidx(std::size_t x,std::size_t y,std::size_t z)const{
        return (z*ny_+y)*(nx_+1)+x;
    }
    std::size_t vidx(std::size_t x,std::size_t y,std::size_t z)const{
        return (z*(ny_+1)+y)*nx_+x;
    }
    std::size_t widx(std::size_t x,std::size_t y,std::size_t z)const{
        return (z*ny_+y)*nx_+x;
    }
    bool wet_or_empty(std::size_t x,std::size_t y,std::size_t z)const{
        return !solid_[cell(x,y,z)] && fill_[cell(x,y,z)]>wet;
    }
    void sync_occupancy(){
        for(std::size_t z=0;z<nz_;++z)
            for(std::size_t y=0;y<ny_;++y)
                for(std::size_t x=0;x<nx_;++x){
                    const auto i=cell(x,y,z);
                    mac_.set_cell(x,y,z,!solid_[i]&&fill_[i]>wet,solid_[i]!=0);
                }
    }
    struct Grid3 {double x,y,z;};
    // Sample face velocity via trilinear interpolation on each staggered grid.
    double component(int axis,Grid3 p)const{
        const auto sx=axis==0?nx_+1:nx_;
        const auto sy=axis==1?ny_+1:ny_;
        const auto sz=axis==2?nz_+1:nz_;
        const double gx=p.x/dx_-(axis==0?0.0:0.5);
        const double gy=p.y/dx_-(axis==1?0.0:0.5);
        const double gz=p.z/dx_-(axis==2?0.0:0.5);
        auto prepare=[](double coord,std::size_t size){
            coord=std::clamp(coord,0.0,static_cast<double>(size-1));
            auto a=static_cast<std::size_t>(coord);
            auto b=std::min(a+1,size-1);
            return std::array<double,3>{static_cast<double>(a),
                                         static_cast<double>(b),coord-a};
        };
        const auto ax=prepare(gx,sx),ay=prepare(gy,sy),az=prepare(gz,sz);
        double result=0;
        for(int k=0;k<2;++k)for(int j=0;j<2;++j)for(int i=0;i<2;++i){
            const auto x=static_cast<std::size_t>(ax[i]),y=static_cast<std::size_t>(ay[j]);
            const auto z=static_cast<std::size_t>(az[k]);
            const double weight=(i?ax[2]:1-ax[2])*(j?ay[2]:1-ay[2])*
                                 (k?az[2]:1-az[2]);
            double v=0;
            if(axis==0)v=mac_.face_velocity_x(x,y,z);
            else if(axis==1)v=mac_.face_velocity_y(x,y,z);
            else v=mac_.face_velocity_z(x,y,z);
            result+=weight*v;
        }
        return result;
    }
    void advect_velocity(double dt){
        std::vector<double> next_u((nx_+1)*ny_*nz_);
        std::vector<double> next_v(nx_*(ny_+1)*nz_);
        std::vector<double> next_w(nx_*ny_*(nz_+1));
        auto trace=[&](Grid3 p,int axis){
            const Grid3 vel{component(0,p),component(1,p),component(2,p)};
            const Grid3 mid{p.x-0.5*dt*vel.x,p.y-0.5*dt*vel.y,p.z-0.5*dt*vel.z};
            const Grid3 vm{component(0,mid),component(1,mid),component(2,mid)};
            return component(axis,{p.x-dt*vm.x,p.y-dt*vm.y,p.z-dt*vm.z});
        };
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x){
            next_u[uidx(x,y,z)]=trace({x*dx_,(y+0.5)*dx_,(z+0.5)*dx_},0);
        }
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            next_v[vidx(x,y,z)]=trace({(x+0.5)*dx_,y*dx_,(z+0.5)*dx_},1);
        }
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            next_w[widx(x,y,z)]=trace({(x+0.5)*dx_,(y+0.5)*dx_,z*dx_},2);
        }
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x)
            mac_.set_face_velocity_x(x,y,z,next_u[uidx(x,y,z)]);
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            mac_.set_face_velocity_y(x,y,z,next_v[vidx(x,y,z)]);
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            mac_.set_face_velocity_z(x,y,z,next_w[widx(x,y,z)]);
    }
    double max_velocity_sum()const{
        double mx=0,my=0,mz=0;
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x)
            mx=std::max(mx,std::abs(mac_.face_velocity_x(x,y,z)));
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            my=std::max(my,std::abs(mac_.face_velocity_y(x,y,z)));
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            mz=std::max(mz,std::abs(mac_.face_velocity_z(x,y,z)));
        return mx+my+mz;
    }
    void advect_fraction(double dt){
        // Conservative face flux exchange, always between two existing cells.
        // No transfer through solid or external boundaries. Availability and
        // free-capacity limiters guarantee fractional occupation in [0,1].
        std::vector<Transfer> transfers;
        transfers.reserve(3*fill_.size());
        std::vector<double> wants_out(fill_.size(),0),wants_in(fill_.size(),0);
        auto offer=[&](std::size_t a,std::size_t b,double face_velocity){
            if(solid_[a]||solid_[b]||face_velocity==0)return;
            const auto donor=face_velocity>0?a:b;
            const auto receiver=face_velocity>0?b:a;
            const double amount=dt/dx_*std::abs(face_velocity)*
                std::min(fill_[donor],1-fill_[receiver]);
            if(amount<=0)return;
            transfers.push_back({donor,receiver,amount});
            wants_out[donor]+=amount;
            wants_in[receiver]+=amount;
        };
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=1;x<nx_;++x)
            offer(cell(x-1,y,z),cell(x,y,z),mac_.face_velocity_x(x,y,z));
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=1;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            offer(cell(x,y-1,z),cell(x,y,z),mac_.face_velocity_y(x,y,z));
        for(std::size_t z=1;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            offer(cell(x,y,z-1),cell(x,y,z),mac_.face_velocity_z(x,y,z));

        std::vector<double> out_scale(fill_.size(),1),in_scale(fill_.size(),1);
        for(std::size_t i=0;i<fill_.size();++i){
            if(wants_out[i]>fill_[i])out_scale[i]=fill_[i]/wants_out[i];
            if(wants_in[i]>1-fill_[i])
                in_scale[i]=(1-fill_[i])/wants_in[i];
        }
        auto next=fill_;
        for(const auto& transfer:transfers){
            const double amount=transfer.amount*
                std::min(out_scale[transfer.from],in_scale[transfer.to]);
            next[transfer.from]-=amount;
            next[transfer.to]+=amount;
        }
        for(std::size_t i=0;i<next.size();++i){
            if(!std::isfinite(next[i])||next[i]<-1e-9||next[i]>1+1e-9)
                throw std::runtime_error("3D VOF stability failure");
            if(next[i]<0)next[i]=0;
            if(next[i]>1)next[i]=1;
            if(solid_[i]&&next[i]>1e-14)
                throw std::runtime_error("3D water entered a solid");
        }
        fill_.swap(next);
    }
    void substep(double dt,int pressure_iterations){
        sync_occupancy();
        advect_velocity(dt);
        mac_.apply_gravity(dt);
        mac_.project(dt,pressure_iterations);
        last_projection_divergence_=mac_.max_divergence();
        advect_fraction(dt);
    }
public:
    CoupledMacWater3D(std::size_t nx,std::size_t ny,std::size_t nz,double dx,
                      double density=1000)
       :nx_(nx),ny_(ny),nz_(nz),dx_(dx),rho_(density),
        mac_(nx,ny,nz,dx,density),fill_(nx*ny*nz,0),solid_(nx*ny*nz,0) {
        if(!nx||!ny||!nz||!std::isfinite(dx)||dx<=0||
           !std::isfinite(density)||density<=0||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz)
            throw std::invalid_argument("Invalid coupled domain");
    }
    void set_water(std::size_t x,std::size_t y,std::size_t z,double fraction){
        if(!std::isfinite(fraction)||fraction<0||fraction>1)
            throw std::invalid_argument("Bad 3D water fraction");
        const auto i=cell(x,y,z);
        if(solid_[i]&&fraction>0)throw std::invalid_argument("Solid water overlap");
        fill_[i]=fraction;
    }
    void set_solid(std::size_t x,std::size_t y,std::size_t z,bool s=true){
        const auto i=cell(x,y,z);
        if(s&&fill_[i]>0)throw std::invalid_argument("Solid water overlap");
        solid_[i]=s?1:0;
    }
    double water_fraction(std::size_t x,std::size_t y,std::size_t z)const{
        return fill_[cell(x,y,z)];
    }
    bool is_solid(std::size_t x,std::size_t y,std::size_t z)const{
        return solid_[cell(x,y,z)]!=0;
    }
    double pressure_pa(std::size_t x,std::size_t y,std::size_t z)const{
        return mac_.pressure_pa(x,y,z);
    }
    double volume_m3()const{
        double v=0;
        for(double f:fill_)v+=f;
        return v*dx_*dx_*dx_;
    }
    double mass_kg()const{return rho_*volume_m3();}
    double last_projection_divergence()const{return last_projection_divergence_;}
    std::size_t last_substeps()const{return substeps_;}
    std::size_t cell_count()const{return fill_.size();}
    // Exact resident capacity for owned grid arrays; excludes transient vectors,
    // allocators, stack, and instrumentation.
    std::size_t resident_bytes_model()const{
        const std::size_t n=fill_.size();
        const std::size_t faces=(nx_+1)*ny_*nz_+nx_*(ny_+1)*nz_+nx_*ny_*(nz_+1);
        return n*(sizeof(double)+sizeof(std::uint8_t))+
               n*(sizeof(double)+sizeof(std::uint8_t)*2)+faces*sizeof(double);
    }
    void step(double seconds,int pressure_iterations=250){
        if(!std::isfinite(seconds)||seconds<=0||seconds>2 ||
           pressure_iterations<1||pressure_iterations>10000)
            throw std::invalid_argument("Invalid coupled step");
        double remaining=seconds;
        substeps_=0;
        while(remaining>1e-13){
            if(++substeps_>4000)throw std::runtime_error("Coupled solver substep cap");
            const double peak=max_velocity_sum();
            const double dt=std::min(remaining,peak>0?0.18*dx_/peak:remaining);
            substep(dt,pressure_iterations);
            remaining-=dt;
        }
    }
};
} // namespace upc
