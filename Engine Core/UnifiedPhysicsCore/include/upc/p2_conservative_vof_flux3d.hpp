#pragma once
// P2 PARTIAL: conservative donor-cell VOF advection on a FROZEN 3D MAC grid.
// Requires discretely divergence-free, impermeable face velocities.
// This is a physical finite-volume face-flux primitive, NOT a moving-boundary
// volume sweep, surface-tension model, high-order VOF or two-way FSI.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct ConservativeVofFluxResult3D {
    std::vector<double> water_fill;
    double initial_volume_m3=0.0;
    double final_volume_m3=0.0;
    double mass_residual_m3=0.0;
    double max_outgoing_courant=0.0;
    double max_divergence_s_inv=0.0;
    std::size_t active_faces=0;
};

class ConservativeVofFlux3D {
public:
    // u=(nx+1)*ny*nz, v=nx*(ny+1)*nz, w=nx*ny*(nz+1).
    // No-flux external boundaries and solid interfaces are enforced strictly.
    // Each fluid cell must have near-zero discrete divergence and sum of
    // outgoing Courant numbers <= 1. Under those conditions first-order
    // donor-cell fluxes preserve 0<=VOF<=1 and global water volume.
    // Inputs are const; invalid steps throw before committing any state.
    static ConservativeVofFluxResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,double dt_s,
        const std::vector<std::uint8_t>& solid,
        const std::vector<double>& fill,
        const std::vector<double>& u,
        const std::vector<double>& v,
        const std::vector<double>& w) {
        const auto max=std::numeric_limits<std::size_t>::max();
        if(!nx||!ny||!nz||nx>max/ny||nx*ny>max/nz||
           nx>=max||ny>=max||nz>=max||
           !std::isfinite(dx_m)||dx_m<=0||
           !std::isfinite(dt_s)||dt_s<=0||
           !std::isfinite(dx_m*dx_m*dx_m)||
           !std::isfinite(dt_s/dx_m))
            throw std::invalid_argument("Invalid MAC VOF flux domain or timestep");
        const auto n=nx*ny*nz;
        if(n>2000000||nx+1>max/(ny*nz)||
           ny+1>max/(nx*nz)||nz+1>max/(nx*ny))
            throw std::invalid_argument("MAC VOF flux grid limit");
        if(solid.size()!=n||fill.size()!=n||
           u.size()!=(nx+1)*ny*nz||
           v.size()!=nx*(ny+1)*nz||
           w.size()!=nx*ny*(nz+1))
            throw std::invalid_argument("MAC VOF flux array size mismatch");
        const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        const auto ui=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*(nx+1)+x;
        };
        const auto vi=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*(ny+1)+y)*nx+x;
        };
        const auto wi=[&](std::size_t x,std::size_t y,std::size_t z){
            return (z*ny+y)*nx+x;
        };
        double sum=0.0;
        for(std::size_t i=0;i<n;++i) {
            if(solid[i]>1||!std::isfinite(fill[i])||
               fill[i]<0||fill[i]>1||(solid[i]&&fill[i]!=0))
                throw std::invalid_argument("Invalid solid/VOF occupancy");
            sum+=fill[i];
        }
        for(double a:u)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite u face velocity");
        for(double a:v)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite v face velocity");
        for(double a:w)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite w face velocity");

        ConservativeVofFluxResult3D out;
        out.water_fill=fill;
        out.initial_volume_m3=sum*dx_m*dx_m*dx_m;
        const double factor=dt_s/dx_m;
        std::vector<double> outgoing(n,0.0);
        double max_speed=0.0;
        // Process each face exactly once; a nonzero face is only allowed
        // between two fluid cells. Boundary and solid faces must be zero.
        auto flux=[&](double velocity,bool has_left,std::size_t left,
                      bool has_right,std::size_t right) {
            max_speed=std::max(max_speed,std::abs(velocity));
            if(!has_left||!has_right||
               (has_left&&solid[left])||(has_right&&solid[right])) {
                if(velocity!=0.0)
                    throw std::invalid_argument("Flux crosses solid/domain boundary");
                return;
            }
            if(velocity==0.0)return;
            const std::size_t donor=velocity>0?left:right;
            const double c=std::abs(velocity)*factor;
            if(!std::isfinite(c))
                throw std::invalid_argument("Nonfinite face Courant number");
            outgoing[donor]+=c;
            const double transported=c*fill[donor];
            out.water_fill[donor]-=transported;
            out.water_fill[velocity>0?right:left]+=transported;
            ++out.active_faces;
        };
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x)
            flux(u[ui(x,y,z)],x>0,x?ci(x-1,y,z):0,
                 x<nx,x<nx?ci(x,y,z):0);
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x)
            flux(v[vi(x,y,z)],y>0,y?ci(x,y-1,z):0,
                 y<ny,y<ny?ci(x,y,z):0);
        for(std::size_t z=0;z<=nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            flux(w[wi(x,y,z)],z>0,z?ci(x,y,z-1):0,
                 z<nz,z<nz?ci(x,y,z):0);
        const double div_tol=1e-10*std::max(1.0,max_speed/dx_m);
        double after=0.0;
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const auto k=ci(x,y,z);
            if(solid[k])continue;
            const double div=((u[ui(x+1,y,z)]-u[ui(x,y,z)])+
                              (v[vi(x,y+1,z)]-v[vi(x,y,z)])+
                              (w[wi(x,y,z+1)]-w[wi(x,y,z)]))/dx_m;
            out.max_divergence_s_inv=std::max(out.max_divergence_s_inv,
                                              std::abs(div));
            out.max_outgoing_courant=std::max(out.max_outgoing_courant,
                                             outgoing[k]);
            if(!std::isfinite(div)||std::abs(div)>div_tol)
                throw std::invalid_argument("MAC field is not divergence-free");
            if(!std::isfinite(outgoing[k])||outgoing[k]>1.0+1e-12)
                throw std::invalid_argument("Outgoing VOF CFL exceeds unity");
            if(!std::isfinite(out.water_fill[k])||
               out.water_fill[k]<-1e-12||out.water_fill[k]>1.0+1e-12)
                throw std::logic_error("MAC VOF flux lost positivity/boundedness");
            // Only roundoff-scale clipping is allowed, never arbitrary
            // physical overfill correction.
            out.water_fill[k]=std::max(0.0,std::min(1.0,out.water_fill[k]));
        }
        for(double a:out.water_fill)after+=a;
        out.final_volume_m3=after*dx_m*dx_m*dx_m;
        out.mass_residual_m3=out.final_volume_m3-out.initial_volume_m3;
        if(std::abs(after-sum)>1e-10*std::max(1.0,sum))
            throw std::logic_error("MAC VOF face flux mass imbalance");
        return out;
    }
};
} // namespace upc
