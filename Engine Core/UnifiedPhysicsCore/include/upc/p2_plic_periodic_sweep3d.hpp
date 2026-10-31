#pragma once
// P2 PARTIAL: exact PLIC swept-slab volume on a frozen periodic 3D grid,
// prescribed uniform one-axis velocity. No general CFD, FSI or UE integration.
#include "upc/p2_oblique_plic_geometry3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace upc {
struct P2PlicPeriodicSweep3D {
    std::vector<double> water_fill;
    double initial_volume_m3=0,final_volume_m3=0,mass_residual_m3=0,courant=0;
    std::size_t transported_faces=0;
};
inline double p2_plic_outgoing_slab3d(
    double f,const std::array<double,3>& normal,
    int axis,int direction,double c) {
    using namespace p2_oblique_detail;
    if(!std::isfinite(f)||f<0||f>1||axis<0||axis>2||
       (direction!=1&&direction!=-1)||!std::isfinite(c)||c<0||c>1)
        throw std::invalid_argument("Invalid PLIC slab input");
    for(double v:normal)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite PLIC slab normal");
    if(f==0||c==0)return 0;
    if(f==1)return c;
    if(c==1)return f;
    const double scale=std::max({std::abs(normal[0]),
                                 std::abs(normal[1]),std::abs(normal[2])});
    if(scale<=0)throw std::invalid_argument("Zero partial PLIC normal");
    V n{normal[0]/scale,normal[1]/scale,normal[2]/scale};
    const double norm=std::sqrt(dot(n,n));
    for(double& a:n)a/=norm;
    double lo=0,hi=0;
    for(double a:n){lo+=std::min(0.0,a);hi+=std::max(0.0,a);}
    for(int k=0;k<64;++k){
        const double mid=(lo+hi)*0.5;
        if(clip_cube(n,mid).volume<f)lo=mid;else hi=mid;
    }
    const double cut=(lo+hi)*0.5;
    if(std::abs(clip_cube(n,cut).volume-f)>1e-10)
        throw std::runtime_error("PLIC plane volume mismatch");
    V slab_n=n;
    slab_n[axis]*=c;
    const double slab_cut=cut-(direction>0?n[axis]*(1-c):0.0);
    const double slab_norm=std::sqrt(dot(slab_n,slab_n));
    for(double& a:slab_n)a/=slab_norm;
    const double swept=c*clip_cube(slab_n,slab_cut/slab_norm).volume;
    if(!std::isfinite(swept)||swept < -1e-11||
       swept>std::min(f,c)+1e-11)
        throw std::runtime_error("PLIC slab outside geometric bounds swept="+std::to_string(swept)+" f="+std::to_string(f)+" c="+std::to_string(c)+" axis="+std::to_string(axis)+" direction="+std::to_string(direction));
    return std::clamp(swept,0.0,std::min(f,c));
}
inline P2PlicPeriodicSweep3D p2_plic_periodic_sweep3d(
    const std::vector<double>& fill,
    const std::vector<std::array<double,3>>& normals,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx,double dt,int axis,double velocity) {
    const auto max=std::numeric_limits<std::size_t>::max();
    if(!nx||!ny||!nz||nx>max/ny||nx*ny>max/nz||
       nx*ny*nz>2000000||axis<0||axis>2||
       (axis==0&&nx<2)||(axis==1&&ny<2)||(axis==2&&nz<2)||
       !std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0||
       !std::isfinite(velocity))
        throw std::invalid_argument("Invalid periodic PLIC sweep grid");
    const auto n=nx*ny*nz;
    if(fill.size()!=n||normals.size()!=n)
        throw std::invalid_argument("Periodic PLIC array mismatch");
    const double cell_volume=dx*dx*dx;
    const double c=std::abs(velocity)*(dt/dx);
    if(!std::isfinite(cell_volume)||cell_volume<=0||
       !std::isfinite(c)||c>1)
        throw std::invalid_argument("Invalid periodic PLIC CFL/volume");
    for(std::size_t i=0;i<n;++i){
        if(!std::isfinite(fill[i])||fill[i]<0||fill[i]>1)
            throw std::invalid_argument("Invalid periodic PLIC fill");
        for(double v:normals[i])if(!std::isfinite(v))
            throw std::invalid_argument("Nonfinite periodic PLIC normal");
        if(fill[i]>0&&fill[i]<1){
            const auto& v=normals[i];
            if(std::max({std::abs(v[0]),std::abs(v[1]),
                         std::abs(v[2])})==0)
                throw std::invalid_argument("Missing partial PLIC normal");
        }
    }
    P2PlicPeriodicSweep3D out;
    out.water_fill=fill;out.courant=c;
    double before=0,after=0;
    for(double f:fill)before+=f;
    const int direction=velocity<0?-1:1;
    const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const auto donor=ci(x,y,z);
        const double swept=p2_plic_outgoing_slab3d(
            fill[donor],normals[donor],axis,direction,c);
        if(swept==0)continue;
        std::array<std::size_t,3> next{x,y,z};
        const std::array<std::size_t,3> dims{nx,ny,nz};
        next[axis]=direction>0?(next[axis]+1)%dims[axis]:
                    (next[axis]+dims[axis]-1)%dims[axis];
        const auto receiver=ci(next[0],next[1],next[2]);
        out.water_fill[donor]-=swept;
        out.water_fill[receiver]+=swept;
        ++out.transported_faces;
    }
    for(double& f:out.water_fill){
        if(!std::isfinite(f)||f < -1e-10||f>1+1e-10)
            throw std::runtime_error("Periodic PLIC sweep unbounded");
        f=std::clamp(f,0.0,1.0);
        after+=f;
    }
    out.initial_volume_m3=before*cell_volume;
    out.final_volume_m3=after*cell_volume;
    out.mass_residual_m3=out.final_volume_m3-out.initial_volume_m3;
    if(!std::isfinite(out.initial_volume_m3)||
       !std::isfinite(out.final_volume_m3)||
       std::abs(after-before)>1e-10*std::max(1.0,before))
        throw std::runtime_error("Periodic PLIC mass imbalance");
    return out;
}
} // namespace upc
