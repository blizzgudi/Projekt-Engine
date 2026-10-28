#pragma once
// P2 PARTIAL: conservative finite-volume upwind transport on periodic 3D MAC
// dual volumes with a spatially variable, discrete divergence-free advector.
// Not a pressure projection, free-surface/cut-cell solver, or FSI closure.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2VariableMacTransportResult3D {
    std::vector<double> u_m_s, v_m_s, w_m_s;
    double max_outgoing_courant=0.0;
    double max_cell_divergence_s_inv=0.0;
};

class P2VariableMacTransport3D {
    using Vec=std::vector<double>;
    using Triple=std::array<Vec,3>;
    static std::size_t wrap(long long a,std::size_t n) {
        const auto m=a%static_cast<long long>(n);
        return static_cast<std::size_t>(m<0?m+static_cast<long long>(n):m);
    }
    static std::size_t idx(int c,std::size_t i,std::size_t j,
                           std::size_t k,std::size_t nx,std::size_t ny) {
        if(c==0)return (k*ny+j)*(nx+1)+i;
        if(c==1)return (k*(ny+1)+j)*nx+i;
        return (k*ny+j)*nx+i;
    }
    static double get(const Triple& f,int c,const std::array<long long,3>& p,
                      std::size_t nx,std::size_t ny,std::size_t nz) {
        return f[c][idx(c,wrap(p[0],nx),wrap(p[1],ny),wrap(p[2],nz),nx,ny)];
    }
    static void validate(const Triple& f,std::size_t nx,
                         std::size_t ny,std::size_t nz) {
        for(int c=0;c<3;++c) {
            const auto expected=(nx+(c==0))*(ny+(c==1))*(nz+(c==2));
            if(f[c].size()!=expected)
                throw std::invalid_argument("Variable MAC component size");
            for(double v:f[c])
                if(!std::isfinite(v))
                    throw std::invalid_argument("Nonfinite variable MAC component");
            for(std::size_t k=0;k<nz;++k)
                for(std::size_t j=0;j<ny;++j)
                    for(std::size_t i=0;i<nx;++i) {
                        if((c==0&&i!=0)||(c==1&&j!=0)||(c==2&&k!=0))
                            continue;
                        auto p=std::array<long long,3>{
                            static_cast<long long>(i),static_cast<long long>(j),
                            static_cast<long long>(k)};
                        const double low=get(f,c,p,nx,ny,nz);
                        const double high=f[c][idx(c,c==0?nx:i,
                            c==1?ny:j,c==2?nz:k,nx,ny)];
                        if(std::abs(low-high)>1e-12)
                            throw std::invalid_argument("Nonperiodic MAC seam");
                    }
        }
    }
    // Velocity on the +axis boundary of the staggered dual control volume
    // for transported component c. Transverse velocities are averages of
    // two neighboring MAC faces straddling that dual-volume boundary.
    static double face_speed(const Triple& a,int c,int axis,
                             std::array<long long,3> p,
                             std::size_t nx,std::size_t ny,std::size_t nz) {
        auto next=p;
        ++next[axis];
        if(c==axis)
            return 0.5*(get(a,axis,p,nx,ny,nz)+
                        get(a,axis,next,nx,ny,nz));
        auto other=next;
        --other[c];
        return 0.5*(get(a,axis,next,nx,ny,nz)+
                    get(a,axis,other,nx,ny,nz));
    }
    static Vec transport(int c,const Triple& adv,const Triple& q,
                         std::size_t nx,std::size_t ny,std::size_t nz,
                         double lambda,double& max_cfl) {
        Vec dst(q[c].size(),0.0);
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i) {
                    const auto p=std::array<long long,3>{
                        static_cast<long long>(i),static_cast<long long>(j),
                        static_cast<long long>(k)};
                    const double center=get(q,c,p,nx,ny,nz);
                    double flux_difference=0.0,outgoing=0.0;
                    for(int axis=0;axis<3;++axis) {
                        auto prev=p, next=p;
                        --prev[axis];++next[axis];
                        const double right=face_speed(adv,c,axis,p,nx,ny,nz);
                        const double left=face_speed(adv,c,axis,prev,nx,ny,nz);
                        const double fr=right*(right>=0.0?center:
                            get(q,c,next,nx,ny,nz));
                        const double fl=left*(left>=0.0?
                            get(q,c,prev,nx,ny,nz):center);
                        flux_difference+=fr-fl;
                        outgoing+=std::max(right,0.0)+std::max(-left,0.0);
                    }
                    const double courant=lambda*outgoing;
                    if(!std::isfinite(courant)||courant>1.0+1e-12)
                        throw std::invalid_argument("Variable MAC local CFL exceeded");
                    max_cfl=std::max(max_cfl,courant);
                    const double value=center-lambda*flux_difference;
                    if(!std::isfinite(value))
                        throw std::invalid_argument("Nonfinite variable MAC update");
                    dst[idx(c,i,j,k,nx,ny)]=value;
                }
        if(c==0)
            for(std::size_t k=0;k<nz;++k)
                for(std::size_t j=0;j<ny;++j)
                    dst[idx(0,nx,j,k,nx,ny)]=dst[idx(0,0,j,k,nx,ny)];
        if(c==1)
            for(std::size_t k=0;k<nz;++k)
                for(std::size_t i=0;i<nx;++i)
                    dst[idx(1,i,ny,k,nx,ny)]=dst[idx(1,i,0,k,nx,ny)];
        if(c==2)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i)
                    dst[idx(2,i,j,nz,nx,ny)]=dst[idx(2,i,j,0,nx,ny)];
        return dst;
    }
public:
    static P2VariableMacTransportResult3D step(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,
        const Vec& adv_u,const Vec& adv_v,const Vec& adv_w,
        const Vec& u,const Vec& v,const Vec& w) {
        if(nx<2||ny<2||nz<2||nx>32768/ny||nx*ny>32768/nz||
           !std::isfinite(dx_m)||dx_m<=0.0||
           !std::isfinite(dt_s)||dt_s<=0.0)
            throw std::invalid_argument("Invalid variable MAC grid or time");
        const double lambda=dt_s/dx_m;
        if(!std::isfinite(lambda)||lambda<=0.0)
            throw std::invalid_argument("Invalid variable MAC dt/dx");
        const Triple adv={adv_u,adv_v,adv_w},q={u,v,w};
        validate(adv,nx,ny,nz);
        validate(q,nx,ny,nz);
        double max_speed=0.0;
        for(const auto& component:adv)
            for(double speed:component)
                max_speed=std::max(max_speed,std::abs(speed));
        double max_div=0.0;
        for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                for(std::size_t i=0;i<nx;++i) {
                    auto p=std::array<long long,3>{
                        static_cast<long long>(i),static_cast<long long>(j),
                        static_cast<long long>(k)};
                    double divergence=0.0;
                    for(int axis=0;axis<3;++axis) {
                        auto right=p;++right[axis];
                        divergence+=get(adv,axis,right,nx,ny,nz)-
                                    get(adv,axis,p,nx,ny,nz);
                    }
                    max_div=std::max(max_div,std::abs(divergence/dx_m));
                }
        if(max_div>1e-10*std::max(1.0,max_speed/dx_m))
            throw std::invalid_argument("Advector not discrete divergence-free");
        double max_cfl=0.0;
        auto out_u=transport(0,adv,q,nx,ny,nz,lambda,max_cfl);
        auto out_v=transport(1,adv,q,nx,ny,nz,lambda,max_cfl);
        auto out_w=transport(2,adv,q,nx,ny,nz,lambda,max_cfl);
        return {std::move(out_u),std::move(out_v),std::move(out_w),
                max_cfl,max_div};
    }
    static P2VariableMacTransportResult3D self_advect(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,const Vec& u,const Vec& v,const Vec& w) {
        return step(nx,ny,nz,dx_m,dt_s,u,v,w,u,v,w);
    }
};
} // namespace upc
