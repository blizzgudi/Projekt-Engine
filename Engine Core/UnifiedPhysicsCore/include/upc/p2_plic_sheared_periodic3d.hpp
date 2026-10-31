#pragma once
// P2 PARTIAL: geometric PLIC one-axis transport on periodic Cartesian lanes
// with spatially varying transverse shear. Face speeds must be constant
// along each periodic lane (discrete zero divergence); not unsplit 3D CFD.
#include "upc/p2_plic_periodic_sweep3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2PlicShearedPeriodic3D {
    std::vector<double> water_fill;
    double initial_volume_m3=0, final_volume_m3=0, mass_residual_m3=0;
    double max_courant=0;
    std::size_t transported_faces=0, positive_faces=0, negative_faces=0;
};

// face_velocity[i] is the signed speed at the positive-axis face of cell i.
// Negative speed at face i transports water from cell i+1 back into cell i.
// All faces along an axis-parallel periodic lane must share one speed;
// otherwise the prescribed field is compressible and the bounded VOF
// interpretation requires a separate, not-yet-implemented volume update.
inline P2PlicShearedPeriodic3D p2_plic_sheared_periodic3d(
    const std::vector<double>& fill,
    const std::vector<std::array<double,3>>& normals,
    const std::vector<double>& face_velocity,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx,double dt,int axis) {
    const auto max=std::numeric_limits<std::size_t>::max();
    if(!nx||!ny||!nz||nx>max/ny||nx*ny>max/nz||
       nx*ny*nz>2000000||axis<0||axis>2||
       (axis==0&&nx<2)||(axis==1&&ny<2)||(axis==2&&nz<2)||
       !std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0)
        throw std::invalid_argument("Invalid sheared PLIC grid");
    const std::size_t n=nx*ny*nz;
    if(fill.size()!=n||normals.size()!=n||face_velocity.size()!=n)
        throw std::invalid_argument("Sheared PLIC array mismatch");
    const double volume=dx*dx*dx;
    if(!std::isfinite(volume)||volume<=0)
        throw std::invalid_argument("Invalid sheared PLIC cell volume");
    const std::array<std::size_t,3> dims{nx,ny,nz};
    const auto index=[&](const std::array<std::size_t,3>& p){
        return (p[2]*ny+p[1])*nx+p[0];
    };
    P2PlicShearedPeriodic3D out;
    out.water_fill=fill;
    double before=0,after=0;
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(p);
        if(!std::isfinite(fill[i])||fill[i]<0||fill[i]>1||
           !std::isfinite(face_velocity[i]))
            throw std::invalid_argument("Invalid sheared PLIC input");
        for(double v:normals[i])if(!std::isfinite(v))
            throw std::invalid_argument("Invalid sheared PLIC normal");
        if(fill[i]>0&&fill[i]<1&&
           std::max({std::abs(normals[i][0]),std::abs(normals[i][1]),
                     std::abs(normals[i][2])})==0)
            throw std::invalid_argument("Missing sheared PLIC normal");
        auto q=p;
        q[axis]=(q[axis]+1)%dims[axis];
        const double next=face_velocity[index(q)];
        const double speed=face_velocity[i];
        if(std::abs(next-speed)>1e-12*std::max({1.0,std::abs(speed),std::abs(next)}))
            throw std::invalid_argument("Sheared PLIC lane not divergence free");
        const double c=std::abs(speed)*(dt/dx);
        if(!std::isfinite(c)||c>1)
            throw std::invalid_argument("Sheared PLIC CFL exceeds one");
        out.max_courant=std::max(out.max_courant,c);
        before+=fill[i];
    }
    // Use a separate delta field: all transfers see the frozen input,
    // never partially updated donor fractions.
    std::vector<double> delta(n,0);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(p);
        const double speed=face_velocity[i];
        if(speed==0)continue;
        auto q=p;
        q[axis]=(q[axis]+1)%dims[axis];
        const auto j=index(q);
        const auto donor=speed>0?i:j;
        const auto receiver=speed>0?j:i;
        const double c=std::abs(speed)*(dt/dx);
        const double swept=p2_plic_outgoing_slab3d(
            fill[donor],normals[donor],axis,speed>0?1:-1,c);
        if(swept==0)continue;
        delta[donor]-=swept;
        delta[receiver]+=swept;
        ++out.transported_faces;
        if(speed>0)++out.positive_faces;else ++out.negative_faces;
    }
    for(std::size_t i=0;i<n;++i){
        double f=fill[i]+delta[i];
        if(!std::isfinite(f)||f < -1e-10||f>1+1e-10)
            throw std::runtime_error("Sheared PLIC unbounded");
        f=std::clamp(f,0.0,1.0);
        out.water_fill[i]=f;
        after+=f;
    }
    out.initial_volume_m3=before*volume;
    out.final_volume_m3=after*volume;
    out.mass_residual_m3=out.final_volume_m3-out.initial_volume_m3;
    if(!std::isfinite(out.initial_volume_m3)||
       !std::isfinite(out.final_volume_m3)||
       std::abs(after-before)>1e-10*std::max(1.0,before))
        throw std::runtime_error("Sheared PLIC mass imbalance");
    return out;
}
} // namespace upc
