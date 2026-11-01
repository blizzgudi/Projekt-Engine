#pragma once
// P2 PARTIAL: conservative, opt-in finite-volume VOF advection allowing
// throughflow through saturated cells. This is a first-order donor-cell
// transport, not geometric PLIC or stress-consistent two-phase CFD.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2ThroughflowTransfer3D {
    std::size_t from=0,to=0;
    double fraction=0;
};
struct P2VofThroughflow3D {
    std::vector<double> fill;
    std::vector<P2ThroughflowTransfer3D> transfers;
    double initial_fraction_sum=0,final_fraction_sum=0;
    double max_six_face_cfl=0;
    std::size_t limited_iterations=0;
};

// Positive-face velocities are stored per cell and axis, even if zero at
// exterior boundaries. For each interior face signed velocity picks donor.
// The flux is c*fill(donor), independent of receiver's initial capacity;
// simultaneous throughflow into and out of a full cell is therefore possible.
// Iterative pairwise flux scaling enforces 0<=fill<=1 without local clipping or
// loss of mass. Failing to converge to bounded capacities raises an error.
inline P2VofThroughflow3D p2_vof_throughflow3d(
    const std::vector<double>& fill,
    const std::vector<std::uint8_t>& solid,
    const std::array<std::vector<double>,3>& plus_faces,
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt) {
    const std::size_t lim=std::numeric_limits<std::size_t>::max();
    if(nx<2||ny<2||nz<2||nx>lim/ny||nx*ny>lim/nz||
       nx*ny*nz>2000000||fill.size()!=nx*ny*nz||solid.size()!=fill.size()||
       !std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0)
        throw std::invalid_argument("Invalid throughflow grid");
    const auto n=fill.size();
    if(!std::isfinite(dt/dx)||dt/dx<=0)
        throw std::invalid_argument("Invalid throughflow CFL scaling");
    for(std::size_t i=0;i<n;++i)
        if(!std::isfinite(fill[i])||fill[i]<0||fill[i]>1||
           solid[i]>1||(solid[i]&&fill[i]!=0))
            throw std::invalid_argument("Invalid throughflow phase or solid");
    for(const auto& field:plus_faces) {
        if(field.size()!=n)
            throw std::invalid_argument("Invalid throughflow velocity sizes");
        for(double value:field)
            if(!std::isfinite(value))
                throw std::invalid_argument("Nonfinite throughflow face");
    }
    const auto idx=[&](std::size_t x,std::size_t y,std::size_t z) {
        return (z*ny+y)*nx+x;
    };
    P2VofThroughflow3D result;
    result.fill=fill;
    std::vector<double> cfl(n,0.0);
    std::vector<double> flux; // fractions, shared by one donor and receiver
    std::vector<std::pair<std::size_t,std::size_t>> links;
    flux.reserve(n*3);
    links.reserve(n*3);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto i=idx(x,y,z);
        for(int a=0;a<3;++a) {
            if((a==0&&x+1==nx)||(a==1&&y+1==ny)||
               (a==2&&z+1==nz)) {
                if(plus_faces[a][i]!=0)
                    throw std::invalid_argument("Nonzero exterior throughflow face");
                continue;
            }
            const auto j=a==0?idx(x+1,y,z):
                         a==1?idx(x,y+1,z):idx(x,y,z+1);
            const double s=plus_faces[a][i];
            if(solid[i]||solid[j]) {
                if(std::abs(s)>1e-12)
                    throw std::invalid_argument("Throughflow across solid face");
                continue;
            }
            const double courant=std::abs(s)*dt/dx;
            if(!std::isfinite(courant))
                throw std::invalid_argument("Nonfinite throughflow Courant");
            cfl[i]+=courant;cfl[j]+=courant;
            const auto donor=s>0?i:j,recv=s>0?j:i;
            const double proposed=courant*fill[donor];
            if(proposed<=0)continue;
            flux.push_back(proposed);
            links.push_back({donor,recv});
        }
    }
    for(double a:cfl) {
        result.max_six_face_cfl=std::max(result.max_six_face_cfl,a);
        if(a>1.0+1e-12)
            throw std::invalid_argument("Throughflow six-face CFL exceeded");
    }
    std::vector<double> incoming(n),outgoing(n);
    bool bounded=false;
    for(std::size_t iteration=0;iteration<64;++iteration) {
        std::fill(incoming.begin(),incoming.end(),0.0);
        std::fill(outgoing.begin(),outgoing.end(),0.0);
        for(std::size_t edge=0;edge<flux.size();++edge) {
            outgoing[links[edge].first]+=flux[edge];
            incoming[links[edge].second]+=flux[edge];
        }
        std::vector<double> out_scale(n,1.0),in_scale(n,1.0);
        bounded=true;
        for(std::size_t i=0;i<n;++i) {
            const double lower=fill[i]+incoming[i]-outgoing[i];
            const double upper=lower-1.0;
            if(lower < -1e-13) {
                bounded=false;
                if(outgoing[i]>0)
                    out_scale[i]=std::max(0.0,(fill[i]+incoming[i])/outgoing[i]);
            }
            if(upper > 1e-13) {
                bounded=false;
                if(incoming[i]>0)
                    in_scale[i]=std::max(0.0,(1-fill[i]+outgoing[i])/incoming[i]);
            }
        }
        if(bounded)break;
        for(std::size_t edge=0;edge<flux.size();++edge) {
            const auto [from,to]=links[edge];
            flux[edge]*=std::min(out_scale[from],in_scale[to]);
        }
        ++result.limited_iterations;
    }
    if(!bounded)
        throw std::runtime_error("Throughflow capacity limiter did not converge");
    result.transfers.reserve(flux.size());
    long double initial=0,final=0;
    for(double f:fill)initial+=f;
    for(std::size_t edge=0;edge<flux.size();++edge) {
        const auto [from,to]=links[edge];
        const double amount=flux[edge];
        result.fill[from]-=amount;
        result.fill[to]+=amount;
        result.transfers.push_back({from,to,amount});
    }
    for(double f:result.fill) {
        if(!std::isfinite(f)||f < -1e-12||f>1+1e-12)
            throw std::runtime_error("Throughflow VOF out of physical bounds");
        final+=f;
    }
    result.initial_fraction_sum=static_cast<double>(initial);
    result.final_fraction_sum=static_cast<double>(final);
    if(std::abs(final-initial)>1e-10L*std::max(1.0L,initial))
        throw std::runtime_error("Throughflow conservative mass ledger failed");
    // Endpoint-only roundoff is allowed, but no meaningful physical clipping.
    for(double& f:result.fill)f=std::clamp(f,0.0,1.0);
    return result;
}
} // namespace upc
