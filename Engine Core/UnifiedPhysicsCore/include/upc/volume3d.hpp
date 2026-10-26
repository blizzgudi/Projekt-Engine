#pragma once
// P2 research prototype: conservative volume-of-fluid TRANSPORT on a true 3D grid.
// Moving water by prescribed bulk velocity is NOT solving 3D Navier-Stokes,
// pressure projection, viscosity, or fluid-solid coupling.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>
namespace upc {
class VolumeTransport3D {
    struct Cell{double fill=0;bool solid=false;};
    struct Flow{std::size_t from,to;double volume;};
    std::size_t nx_,ny_,nz_;
    double dx_;
    std::vector<Cell> cells_;
    std::array<double,3> velocity_{0,0,0};
    std::size_t idx(std::size_t x,std::size_t y,std::size_t z)const{
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("Invalid 3D cell index");
        return (z*ny_+y)*nx_+x;
    }
public:
    VolumeTransport3D(std::size_t nx,std::size_t ny,std::size_t nz,double spacing_m)
     :nx_(nx),ny_(ny),nz_(nz),dx_(spacing_m){
        if(!nx||!ny||!nz||!std::isfinite(dx_)||dx_<=0||
            nx>std::numeric_limits<std::size_t>::max()/ny||
            nx*ny>std::numeric_limits<std::size_t>::max()/nz)
            throw std::invalid_argument("Invalid volume transport domain");
        cells_.resize(nx*ny*nz);
    }
    void set_fill(std::size_t x,std::size_t y,std::size_t z,double f){
        if(!std::isfinite(f)||f<0||f>1)throw std::invalid_argument("Invalid fraction");
        auto& c=cells_[idx(x,y,z)];
        if(c.solid&&f!=0)throw std::invalid_argument("Water overlaps solid");
        c.fill=f;
    }
    void set_solid(std::size_t x,std::size_t y,std::size_t z,bool solid=true){
        auto& c=cells_[idx(x,y,z)];
        if(solid&&c.fill>0)throw std::invalid_argument("Solid overlaps water");
        c.solid=solid;
    }
    double fill(std::size_t x,std::size_t y,std::size_t z)const{
        return cells_[idx(x,y,z)].fill;
    }
    bool is_solid(std::size_t x,std::size_t y,std::size_t z)const{
        return cells_[idx(x,y,z)].solid;
    }
    void set_uniform_velocity(double vx,double vy,double vz){
        if(!std::isfinite(vx)||!std::isfinite(vy)||!std::isfinite(vz))
            throw std::invalid_argument("Non-finite velocity");
        velocity_={vx,vy,vz};
    }
    double volume_m3()const{
        double volume=0;
        const double cell=dx_*dx_*dx_;
        for(const auto& c:cells_)volume+=c.fill*cell;
        return volume;
    }
    std::size_t cell_count()const{return cells_.size();}
    void advect(double duration_seconds){
        if(!std::isfinite(duration_seconds)||duration_seconds<=0||duration_seconds>10)
            throw std::invalid_argument("Invalid transport dt");
        const double max_speed=std::max({std::abs(velocity_[0]),std::abs(velocity_[1]),std::abs(velocity_[2])});
        const double max_dt=max_speed>0?0.2*dx_/max_speed:duration_seconds;
        const int steps=static_cast<int>(std::ceil(duration_seconds/max_dt));
        if(steps>200000)throw std::invalid_argument("Too many substeps");
        const double dt=duration_seconds/steps;
        for(int iteration=0;iteration<steps;++iteration){
            std::vector<Flow> flows;
            std::vector<double> outgoing(cells_.size(),0),incoming(cells_.size(),0);
            flows.reserve(cells_.size()*3);
            for(std::size_t z=0;z<nz_;++z)
             for(std::size_t y=0;y<ny_;++y)
              for(std::size_t x=0;x<nx_;++x){
                const auto a=idx(x,y,z);
                for(int axis=0;axis<3;++axis){
                    const std::size_t xx=x+(axis==0),yy=y+(axis==1),zz=z+(axis==2);
                    if(xx>=nx_||yy>=ny_||zz>=nz_)continue;
                    const auto b=idx(xx,yy,zz);
                    if(cells_[a].solid||cells_[b].solid)continue;
                    const double speed=velocity_[axis];
                    if(speed==0)continue;
                    const std::size_t donor=speed>0?a:b,receiver=speed>0?b:a;
                    // First-order upwind, with receiver capacity. All fluxes are
                    // globally budget-limited below to preserve 0<=fill<=1.
                    const double desired=dt*std::abs(speed)/dx_*
                        std::min(cells_[donor].fill,1-cells_[receiver].fill);
                    if(desired<=0)continue;
                    flows.push_back({donor,receiver,desired});
                    outgoing[donor]+=desired;incoming[receiver]+=desired;
                }
              }
            std::vector<double> scale_out(cells_.size(),1),scale_in(cells_.size(),1);
            for(std::size_t i=0;i<cells_.size();++i){
                if(outgoing[i]>cells_[i].fill)
                    scale_out[i]=cells_[i].fill/outgoing[i];
                if(incoming[i]>1-cells_[i].fill)
                    scale_in[i]=(1-cells_[i].fill)/incoming[i];
            }
            auto next=cells_;
            for(const auto& f:flows){
                const double d=f.volume*std::min(scale_out[f.from],scale_in[f.to]);
                next[f.from].fill-=d;
                next[f.to].fill+=d;
            }
            for(auto& c:next){
                if(!std::isfinite(c.fill)||c.fill< -1e-10||c.fill>1+1e-10)
                    throw std::runtime_error("3D transport fraction stability");
                c.fill=std::clamp(c.fill,0.0,1.0);
            }
            cells_.swap(next);
        }
    }
};
}
