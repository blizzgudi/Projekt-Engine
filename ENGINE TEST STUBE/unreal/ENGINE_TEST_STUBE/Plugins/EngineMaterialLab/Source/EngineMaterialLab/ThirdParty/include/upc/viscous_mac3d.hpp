#pragma once
// P2 follow-on: optional explicit 3D kinematic-viscosity diffusion for
// face-centred MAC velocities. Independent primitive, not yet integrated
// into CoupledMacWater3D. Homogeneous liquid, stationary no-slip exterior.
// Not a multi-phase viscosity, not moving-solid boundary handling.
#include "upc/mac3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
class ViscousMacDiffusion3D {
    std::size_t nx_,ny_,nz_;
    double dx_;
    static std::size_t index(std::size_t x,std::size_t y,std::size_t z,
                             std::size_t nx,std::size_t ny) {
        return (z*ny+y)*nx+x;
    }
    template<class Read,class Write>
    void diffuse_component(std::size_t sx,std::size_t sy,std::size_t sz,
                           int axis,Read read,Write write,double alpha)const {
        const auto count=sx*sy*sz;
        std::vector<double> before(count),after(count);
        for(std::size_t z=0;z<sz;++z)
        for(std::size_t y=0;y<sy;++y)
        for(std::size_t x=0;x<sx;++x)
            before[index(x,y,z,sx,sy)]=read(x,y,z);
        const auto sample=[&](long long x,long long y,long long z){
            if(x<0||y<0||z<0||x>=static_cast<long long>(sx)||
               y>=static_cast<long long>(sy)||z>=static_cast<long long>(sz))
                return 0.0; // no-slip exterior ghost velocity
            return before[index(static_cast<std::size_t>(x),
                                static_cast<std::size_t>(y),
                                static_cast<std::size_t>(z),sx,sy)];
        };
        for(std::size_t z=0;z<sz;++z)
        for(std::size_t y=0;y<sy;++y)
        for(std::size_t x=0;x<sx;++x){
            const bool normal_wall=(axis==0&&(x==0||x==sx-1))||
                (axis==1&&(y==0||y==sy-1))||
                (axis==2&&(z==0||z==sz-1));
            const auto i=index(x,y,z,sx,sy);
            if(normal_wall){after[i]=0;continue;}
            const auto xx=static_cast<long long>(x);
            const auto yy=static_cast<long long>(y);
            const auto zz=static_cast<long long>(z);
            const double v=before[i];
            after[i]=v+alpha*(sample(xx-1,yy,zz)+sample(xx+1,yy,zz)+
                sample(xx,yy-1,zz)+sample(xx,yy+1,zz)+
                sample(xx,yy,zz-1)+sample(xx,yy,zz+1)-6*v);
            if(!std::isfinite(after[i]))
                throw std::runtime_error("Nonfinite MAC viscosity diffusion");
        }
        for(std::size_t z=0;z<sz;++z)
        for(std::size_t y=0;y<sy;++y)
        for(std::size_t x=0;x<sx;++x)
            write(x,y,z,after[index(x,y,z,sx,sy)]);
    }
public:
    ViscousMacDiffusion3D(std::size_t nx,std::size_t ny,std::size_t nz,double dx)
        :nx_(nx),ny_(ny),nz_(nz),dx_(dx){
        if(nx<2||ny<2||nz<2||!std::isfinite(dx)||dx<=0||
           nx>static_cast<std::size_t>(std::numeric_limits<int>::max()-1)||
           ny>static_cast<std::size_t>(std::numeric_limits<int>::max()-1)||
           nz>static_cast<std::size_t>(std::numeric_limits<int>::max()-1)||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz)
            throw std::invalid_argument("Invalid viscosity grid");
    }
    // Explicit 6-neighbor diffusion with adaptive substepping to satisfy
    // alpha=nu*dt/dx^2 <= 1/6. We use 0.15 to provide a margin.
    std::size_t step(MacProjection3D& mac,double dt,double nu)const {
        if(!std::isfinite(dt)||dt<=0||!std::isfinite(nu)||nu<0)
            throw std::invalid_argument("Invalid viscosity or timestep");
        if(nu==0)return 0; // exactly preserve legacy inviscid behavior
        const double requested=nu*dt/(dx_*dx_*0.15);
        if(!std::isfinite(requested)||requested>10000)
            throw std::invalid_argument("Viscous timestep exceeds substep cap");
        const auto substeps=std::max<std::size_t>(
            1,static_cast<std::size_t>(std::ceil(requested)));
        const double alpha=nu*(dt/static_cast<double>(substeps))/(dx_*dx_);
        if(alpha>1.0/6.0)
            throw std::runtime_error("Explicit viscosity CFL violated");
        for(std::size_t n=0;n<substeps;++n){
            diffuse_component(nx_+1,ny_,nz_,0,
                [&](auto x,auto y,auto z){return mac.face_velocity_x(x,y,z);},
                [&](auto x,auto y,auto z,double v){mac.set_face_velocity_x(x,y,z,v);},alpha);
            diffuse_component(nx_,ny_+1,nz_,1,
                [&](auto x,auto y,auto z){return mac.face_velocity_y(x,y,z);},
                [&](auto x,auto y,auto z,double v){mac.set_face_velocity_y(x,y,z,v);},alpha);
            diffuse_component(nx_,ny_,nz_+1,2,
                [&](auto x,auto y,auto z){return mac.face_velocity_z(x,y,z);},
                [&](auto x,auto y,auto z,double v){mac.set_face_velocity_z(x,y,z,v);},alpha);
        }
        return substeps;
    }
    // Optional mask-aware free-surface/solid diffusion primitive.
    // Wet masks are caller-owned snapshots (one per cell); no solver coupling.
    // Boundary model: no-slip normal solid/wall faces (Dirichlet zero),
    // zero-normal-gradient to inactive air faces (Neumann proxy).
    // Tangential moving-wall velocities and stress-jump physics not modeled.
    std::size_t step_masked(MacProjection3D& mac,double dt,double nu,
                            const std::vector<std::uint8_t>& wet,
                            const std::vector<std::uint8_t>& solid)const {
        if(!std::isfinite(dt)||dt<=0||!std::isfinite(nu)||nu<0)
            throw std::invalid_argument("Invalid masked viscosity timestep");
        const auto n=nx_*ny_*nz_;
        if(wet.size()!=n||solid.size()!=n)
            throw std::invalid_argument("Masked viscosity occupancy shape mismatch");
        for(std::size_t i=0;i<n;++i)
            if(wet[i]>1||solid[i]>1||(wet[i]&&solid[i]))
                throw std::invalid_argument("Masked viscosity invalid wet/solid overlap");
        if(nu==0)return 0;
        const double requested=nu*dt/(dx_*dx_*0.15);
        if(!std::isfinite(requested)||requested>10000)
            throw std::invalid_argument("Masked viscosity substep cap exceeded");
        const auto steps=std::max<std::size_t>(1,
            static_cast<std::size_t>(std::ceil(requested)));
        const double alpha=nu*dt/(dx_*dx_*static_cast<double>(steps));
        // 0: solid/wall, 1: active liquid interface, 2: air-only.
        const auto classify=[&](int axis,std::size_t x,std::size_t y,std::size_t z){
            long long ax=x,ay=y,az=z,bx=x,by=y,bz=z;
            if(axis==0)--ax;
            if(axis==1)--ay;
            if(axis==2)--az;
            const auto inside=[&](long long a,long long b,long long c){
                return a>=0&&b>=0&&c>=0&&a<(long long)nx_&&
                       b<(long long)ny_&&c<(long long)nz_;
            };
            if(!inside(ax,ay,az)||!inside(bx,by,bz))return 0;
            const auto a=index(static_cast<std::size_t>(ax),
                static_cast<std::size_t>(ay),static_cast<std::size_t>(az),nx_,ny_);
            const auto b=index(static_cast<std::size_t>(bx),
                static_cast<std::size_t>(by),static_cast<std::size_t>(bz),nx_,ny_);
            if(solid[a]||solid[b])return 0;
            return wet[a]||wet[b]?1:2;
        };
        const auto apply=[&](std::size_t sx,std::size_t sy,std::size_t sz,
                             int axis,auto read,auto write){
            const auto count=sx*sy*sz;
            std::vector<double> before(count),after(count);
            for(std::size_t z=0;z<sz;++z)
            for(std::size_t y=0;y<sy;++y)
            for(std::size_t x=0;x<sx;++x)
                before[index(x,y,z,sx,sy)]=read(x,y,z);
            const auto sample=[&](long long x,long long y,long long z,
                                   double center){
                if(x<0||y<0||z<0||x>=(long long)sx||
                   y>=(long long)sy||z>=(long long)sz)
                    return 0.0; // fixed no-slip exterior ghost
                const auto ux=static_cast<std::size_t>(x);
                const auto uy=static_cast<std::size_t>(y);
                const auto uz=static_cast<std::size_t>(z);
                const auto state=classify(axis,ux,uy,uz);
                return state==0?0.0:state==2?center:
                    before[index(ux,uy,uz,sx,sy)];
            };
            for(std::size_t z=0;z<sz;++z)
            for(std::size_t y=0;y<sy;++y)
            for(std::size_t x=0;x<sx;++x){
                const auto i=index(x,y,z,sx,sy);
                if(classify(axis,x,y,z)!=1){after[i]=0;continue;}
                const auto a=static_cast<long long>(x);
                const auto b=static_cast<long long>(y);
                const auto c=static_cast<long long>(z);
                const double v=before[i];
                after[i]=v+alpha*(sample(a-1,b,c,v)+sample(a+1,b,c,v)+
                    sample(a,b-1,c,v)+sample(a,b+1,c,v)+
                    sample(a,b,c-1,v)+sample(a,b,c+1,v)-6*v);
                if(!std::isfinite(after[i]))
                    throw std::runtime_error("Nonfinite masked viscosity");
            }
            for(std::size_t z=0;z<sz;++z)
            for(std::size_t y=0;y<sy;++y)
            for(std::size_t x=0;x<sx;++x)
                write(x,y,z,after[index(x,y,z,sx,sy)]);
        };
        for(std::size_t i=0;i<steps;++i){
            apply(nx_+1,ny_,nz_,0,
                [&](auto x,auto y,auto z){return mac.face_velocity_x(x,y,z);},
                [&](auto x,auto y,auto z,double v){mac.set_face_velocity_x(x,y,z,v);});
            apply(nx_,ny_+1,nz_,1,
                [&](auto x,auto y,auto z){return mac.face_velocity_y(x,y,z);},
                [&](auto x,auto y,auto z,double v){mac.set_face_velocity_y(x,y,z,v);});
            apply(nx_,ny_,nz_+1,2,
                [&](auto x,auto y,auto z){return mac.face_velocity_z(x,y,z);},
                [&](auto x,auto y,auto z,double v){mac.set_face_velocity_z(x,y,z,v);});
        }
        return steps;
    }
    // Staggered face-energy proxy, not a volume-integrated continuum energy.
    double face_energy_proxy(const MacProjection3D& mac)const {
        double total=0;
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x){
            const double v=mac.face_velocity_x(x,y,z);total+=v*v;
        }
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            const double v=mac.face_velocity_y(x,y,z);total+=v*v;
        }
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            const double v=mac.face_velocity_z(x,y,z);total+=v*v;
        }
        return 0.5*dx_*dx_*dx_*total;
    }
};
} // namespace upc
