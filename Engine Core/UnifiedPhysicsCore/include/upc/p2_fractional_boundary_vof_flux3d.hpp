#pragma once
// P2 PARTIAL: subcell open-volume GCL and conservative donor-cell water
// transport on a 3D MAC grid. Fluxes are supplied externally; this is
// NOT a moving-body pressure solver or a two-way FSI energy proof.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {

struct FractionalBoundaryVofResult3D {
    // Water volume per full grid-cell volume, NOT water saturation.
    std::vector<double> water_cell_fraction;
    double initial_water_m3=0.0, final_water_m3=0.0;
    double mass_residual_m3=0.0, open_volume_residual_m3=0.0;
    double max_gcl_residual_cell_volumes=0.0;
    double max_outgoing_cell_volumes=0.0;
    std::size_t active_faces=0;
};

class FractionalBoundaryVofFlux3D {
public:
    // alpha_old/new in [0,1] are fluid-accessible fractions of each
    // voxel. water_old in [0,alpha_old] is WATER VOLUME per full voxel.
    // u/v/w are time-averaged face velocities (m/s); dt/dx converts them
    // to signed swept cell volumes. No flow through domain boundaries.
    // For every voxel: alpha_new=alpha_old-sum(outward face volume).
    // A donor transports swept_volume*(water_old/alpha_old).
    // Outgoing swept volume <= alpha_old, ensuring positive water volume.
    // The caller must supply geometrically consistent swept fluxes; the
    // GCL test alone does not establish physically correct wall dynamics.
    // No inputs are mutated, including on exception.
    static FractionalBoundaryVofResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,double dt_s,
        const std::vector<double>& alpha_old,
        const std::vector<double>& alpha_new,
        const std::vector<double>& water_old,
        const std::vector<double>& u,
        const std::vector<double>& v,
        const std::vector<double>& w) {
        const auto lim=std::numeric_limits<std::size_t>::max();
        if(!nx||!ny||!nz||nx>lim/ny||nx*ny>lim/nz||
           nx==lim||ny==lim||nz==lim||
           !std::isfinite(dx_m)||dx_m<=0.0||
           !std::isfinite(dt_s)||dt_s<=0.0)
            throw std::invalid_argument("Invalid fractional GCL grid or timestep");
        const auto n=nx*ny*nz;
        if(n>2000000||nx+1>lim/(ny*nz)||
           ny+1>lim/(nx*nz)||nz+1>lim/(nx*ny))
            throw std::invalid_argument("Fractional GCL grid limit");
        const double volume=dx_m*dx_m*dx_m, factor=dt_s/dx_m;
        if(!std::isfinite(volume)||volume<=0.0||
           !std::isfinite(factor)||factor<=0.0)
            throw std::invalid_argument("Invalid fractional GCL units");
        if(alpha_old.size()!=n||alpha_new.size()!=n||
           water_old.size()!=n||u.size()!=(nx+1)*ny*nz||
           v.size()!=nx*(ny+1)*nz||w.size()!=nx*ny*(nz+1))
            throw std::invalid_argument("Fractional GCL array shape mismatch");
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
        double water_before=0.0, open_before=0.0, open_after=0.0;
        for(std::size_t k=0;k<n;++k) {
            if(!std::isfinite(alpha_old[k])||!std::isfinite(alpha_new[k])||
               !std::isfinite(water_old[k])||
               alpha_old[k]<0.0||alpha_old[k]>1.0||
               alpha_new[k]<0.0||alpha_new[k]>1.0||
               water_old[k]<0.0||water_old[k]>alpha_old[k])
                throw std::invalid_argument("Invalid fractional solid/water occupancy");
            water_before+=water_old[k];
            open_before+=alpha_old[k];
            open_after+=alpha_new[k];
        }
        for(double a:u)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite fractional u face");
        for(double a:v)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite fractional v face");
        for(double a:w)if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite fractional w face");

        FractionalBoundaryVofResult3D result;
        result.water_cell_fraction=water_old;
        result.initial_water_m3=water_before*volume;
        result.open_volume_residual_m3=(open_after-open_before)*volume;
        std::vector<double> net_out(n,0.0), outgoing(n,0.0);
        auto face=[&](double speed,bool has_l,std::size_t left,
                      bool has_r,std::size_t right) {
            if(speed==0.0)return;
            if(!has_l||!has_r)
                throw std::invalid_argument("Fractional flux crosses domain boundary");
            const auto donor=speed>0.0?left:right;
            const auto receiver=speed>0.0?right:left;
            if(alpha_old[donor]<=0.0||alpha_new[receiver]<=0.0)
                throw std::invalid_argument("Fractional flux crosses closed cell");
            const double c=std::abs(speed)*factor;
            if(!std::isfinite(c))
                throw std::invalid_argument("Nonfinite fractional swept volume");
            outgoing[donor]+=c;
            net_out[donor]+=c;
            net_out[receiver]-=c;
            const double water=c*(water_old[donor]/alpha_old[donor]);
            result.water_cell_fraction[donor]-=water;
            result.water_cell_fraction[receiver]+=water;
            ++result.active_faces;
        };
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<=nx;++x)
            face(u[ui(x,y,z)],x>0,x?ci(x-1,y,z):0,
                 x<nx,x<nx?ci(x,y,z):0);
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<=ny;++y)
        for(std::size_t x=0;x<nx;++x)
            face(v[vi(x,y,z)],y>0,y?ci(x,y-1,z):0,
                 y<ny,y<ny?ci(x,y,z):0);
        for(std::size_t z=0;z<=nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            face(w[wi(x,y,z)],z>0,z?ci(x,y,z-1):0,
                 z<nz,z<nz?ci(x,y,z):0);
        double water_after=0.0;
        for(std::size_t k=0;k<n;++k) {
            const double gcl=alpha_old[k]-net_out[k]-alpha_new[k];
            result.max_gcl_residual_cell_volumes=
                std::max(result.max_gcl_residual_cell_volumes,std::abs(gcl));
            result.max_outgoing_cell_volumes=
                std::max(result.max_outgoing_cell_volumes,outgoing[k]);
            if(!std::isfinite(gcl)||std::abs(gcl)>1e-11)
                throw std::invalid_argument("Fractional local GCL violation");
            if(!std::isfinite(outgoing[k])||
               outgoing[k]>alpha_old[k]+1e-12)
                throw std::invalid_argument("Fractional donor swept CFL violation");
            const double f=result.water_cell_fraction[k];
            if(!std::isfinite(f)||f<-1e-12||f>alpha_new[k]+1e-12)
                throw std::logic_error("Fractional water boundedness violation");
            result.water_cell_fraction[k]=
                std::max(0.0,std::min(alpha_new[k],f));
            water_after+=result.water_cell_fraction[k];
        }
        result.final_water_m3=water_after*volume;
        result.mass_residual_m3=(water_after-water_before)*volume;
        if(std::abs(open_after-open_before)>1e-10*std::max(1.0,open_before))
            throw std::logic_error("Fractional open volume not conserved");
        if(std::abs(water_after-water_before)>1e-10*std::max(1.0,water_before))
            throw std::logic_error("Fractional water volume not conserved");
        return result;
    }
};
} // namespace upc
