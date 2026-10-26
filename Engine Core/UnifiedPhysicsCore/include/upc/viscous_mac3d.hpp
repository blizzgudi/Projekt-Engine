#pragma once
// P2 follow-on: optional explicit 3D kinematic-viscosity diffusion for
// face-centred MAC velocities. Opt-in mask-aware path is integrated
// with CoupledMacWater3D. Free-surface gradient is a Neumann proxy.
// Not stress-consistent multiphase viscosity or moving-solid FSI.
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
// Staggered-face velocity-sum budget for the explicit masked viscosity
// stencil. NOT a physical 3D fluid/solid impulse or stress tensor: MAC
// face dual volumes overlap and this excludes pressure/advection/gravity.
// Multiply by rho*dx^3 for an explicitly labelled momentum *proxy*.
struct MaskedMacMomentumLedger3D {
    std::array<double,3> active_face_delta_sum_m_s{};
    std::array<double,3> solid_boundary_source_sum_m_s{};
    // Separate fixed-wall drive and current-fluid drag (face-sum proxies).
    // Neither component is a physical reaction impulse.
    std::array<double,3> solid_wall_drive_sum_m_s{};
    std::array<double,3> solid_wall_drag_sum_m_s{};
    std::array<double,3> exterior_boundary_source_sum_m_s{};
    std::array<double,3> closure_residual_sum_m_s{};
    std::size_t substeps=0;
};
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
    // Boundary model: impermeable normal solid/wall faces (Dirichlet zero),
    // zero-normal-gradient to inactive air faces (Neumann proxy).
    // Optional prescribed tangential solid-wall velocity is a fixed-geometry
    // Couette/shear boundary ONLY. No moving geometry, normal wall motion,
    // reaction impulse, or full free-surface stress condition is modeled.
    std::size_t step_masked(MacProjection3D& mac,double dt,double nu,
                            const std::vector<std::uint8_t>& wet,
                            const std::vector<std::uint8_t>& solid,
                            const std::array<double,3>& wall_tangential_velocity_m_s=
                                {0.0,0.0,0.0},
                            MaskedMacMomentumLedger3D* ledger=nullptr)const {
        if(!std::isfinite(dt)||dt<=0||!std::isfinite(nu)||nu<0)
            throw std::invalid_argument("Invalid masked viscosity timestep");
        for(double v:wall_tangential_velocity_m_s)
            if(!std::isfinite(v))
                throw std::invalid_argument("Nonfinite tangential solid-wall velocity");
        const auto n=nx_*ny_*nz_;
        if(wet.size()!=n||solid.size()!=n)
            throw std::invalid_argument("Masked viscosity occupancy shape mismatch");
        for(std::size_t i=0;i<n;++i)
            if(wet[i]>1||solid[i]>1||(wet[i]&&solid[i]))
                throw std::invalid_argument("Masked viscosity invalid wet/solid overlap");
        if(ledger)*ledger=MaskedMacMomentumLedger3D{};
        if(nu==0)return 0;
        const double requested=nu*dt/(dx_*dx_*0.15);
        if(!std::isfinite(requested)||requested>10000)
            throw std::invalid_argument("Masked viscosity substep cap exceeded");
        const auto steps=std::max<std::size_t>(1,
            static_cast<std::size_t>(std::ceil(requested)));
        const double alpha=nu*dt/(dx_*dx_*static_cast<double>(steps));
        // 0: exterior, 1: active liquid, 2: air-only, 3: solid face.
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
            if(solid[a]||solid[b])return 3;
            return wet[a]||wet[b]?1:2;
        };
        const auto apply=[&](std::size_t sx,std::size_t sy,std::size_t sz,
                             int axis,auto read,auto write){
            const auto count=sx*sy*sz;
            double before_active_sum=0,after_active_sum=0;
            const std::array<double,3> previous_boundary_source=ledger?
                std::array<double,3>{
                    ledger->solid_boundary_source_sum_m_s[0]+ledger->exterior_boundary_source_sum_m_s[0],
                    ledger->solid_boundary_source_sum_m_s[1]+ledger->exterior_boundary_source_sum_m_s[1],
                    ledger->solid_boundary_source_sum_m_s[2]+ledger->exterior_boundary_source_sum_m_s[2]}
                :std::array<double,3>{};
            std::vector<double> before(count),after(count);
            for(std::size_t z=0;z<sz;++z)
            for(std::size_t y=0;y<sy;++y)
            for(std::size_t x=0;x<sx;++x)
                before[index(x,y,z,sx,sy)]=read(x,y,z);
            const auto sample=[&](long long x,long long y,long long z,
                                   double center,int neighbor_axis){
                const bool outside=x<0||y<0||z<0||
                    x>=(long long)sx||y>=(long long)sy||z>=(long long)sz;
                const auto ux=outside?std::size_t{0}:static_cast<std::size_t>(x);
                const auto uy=outside?std::size_t{0}:static_cast<std::size_t>(y);
                const auto uz=outside?std::size_t{0}:static_cast<std::size_t>(z);
                const auto state=outside?0:classify(axis,ux,uy,uz);
                if(state==0||state==3){
                    // Fixed exterior ghost = 0. A solid ghost may prescribe
                    // tangential shear, but never normal motion.
                    const double ghost=state==3&&neighbor_axis!=axis?
                        wall_tangential_velocity_m_s[static_cast<std::size_t>(axis)]:0.0;
                    if(ledger){
                        auto& source=state==3?ledger->solid_boundary_source_sum_m_s:
                                               ledger->exterior_boundary_source_sum_m_s;
                        const auto k=static_cast<std::size_t>(axis);
                        source[k]+=alpha*(ghost-center);
                        if(state==3){
                            ledger->solid_wall_drive_sum_m_s[k]+=alpha*ghost;
                            ledger->solid_wall_drag_sum_m_s[k]-=alpha*center;
                        }
                    }
                    return ghost;
                }
                return state==2?center:before[index(ux,uy,uz,sx,sy)];
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
                if(ledger)before_active_sum+=v;
                after[i]=v+alpha*(sample(a-1,b,c,v,0)+sample(a+1,b,c,v,0)+
                    sample(a,b-1,c,v,1)+sample(a,b+1,c,v,1)+
                    sample(a,b,c-1,v,2)+sample(a,b,c+1,v,2)-6*v);
                if(!std::isfinite(after[i]))
                    throw std::runtime_error("Nonfinite masked viscosity");
                if(ledger)after_active_sum+=after[i];
            }
            if(ledger){
                const auto k=static_cast<std::size_t>(axis);
                const double delta=after_active_sum-before_active_sum;
                ledger->active_face_delta_sum_m_s[k]+=delta;
                ledger->closure_residual_sum_m_s[k]+=
                    delta-(ledger->solid_boundary_source_sum_m_s[k]+
                           ledger->exterior_boundary_source_sum_m_s[k]-
                           previous_boundary_source[k]);
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
        if(ledger)ledger->substeps=steps;
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
