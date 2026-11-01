#pragma once
// P2 PARTIAL: externally supplied 3D signed-distance geometry (negative
// water) -> Young-Laplace capillary pressure on fractional MAC/PCG faces.
// One-phase approximation only; no dynamic VOF or air/viscous stress model.
#include "upc/mac3d.hpp"
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
namespace upc {
struct P2SdfCapillaryFace3D {
    int axis; std::size_t x,y,z;
    double theta,curvature_inv_m,pressure_pa;
};
struct P2SdfCapillaryResult3D {std::vector<P2SdfCapillaryFace3D> faces;};
inline P2SdfCapillaryResult3D p2_sdf_capillary_faces3d(
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,
    const std::vector<double>& phi,double sigma,double p_air=0){
    if(nx<5||ny<5||nz<5||nx>100000||ny>100000||nz>100000||
       nx>std::numeric_limits<std::size_t>::max()/ny||
       nx*ny>std::numeric_limits<std::size_t>::max()/nz||
       phi.size()!=nx*ny*nz||!std::isfinite(dx)||dx<=0||
       !std::isfinite(sigma)||sigma<0||!std::isfinite(p_air))
        throw std::invalid_argument("Invalid SDF capillary grid or parameters");
    for(double p:phi)if(!std::isfinite(p)||p==0)
        throw std::invalid_argument("Nonfinite or cell-centre-zero signed distance");
    const auto at=[&](std::size_t x,std::size_t y,std::size_t z){
        return phi[(z*ny+y)*nx+x];
    };
    const auto curvature=[&](std::size_t x,std::size_t y,std::size_t z){
        const double centre=at(x,y,z);
        const auto shift=[&](int axis,int sign){
            std::size_t a=x,b=y,c=z;
            std::size_t* v=axis==0?&a:axis==1?&b:&c;
            const std::size_t bound=axis==0?nx:axis==1?ny:nz;
            *v=sign<0?(*v?*v-1:0):(*v+1<bound?*v+1:bound-1);
            return at(a,b,c);
        };
        double g[3],H[3][3]{};
        for(int i=0;i<3;++i){
            const double plus=shift(i,1),minus=shift(i,-1);
            const bool edge=(i==0&&(x==0||x+1==nx))||
                            (i==1&&(y==0||y+1==ny))||
                            (i==2&&(z==0||z+1==nz));
            if(edge&&std::abs(plus-minus)>1e-12)
                throw std::invalid_argument("Interface normal reaches domain edge");
            g[i]=(plus-minus)/(2*dx);
            H[i][i]=(plus-2*centre+minus)/(dx*dx);
        }
        const auto mixed=[&](int i,int j,int si,int sj){
            std::size_t a=x,b=y,c=z;
            for(int t=0;t<2;++t){
                const int axis=t==0?i:j,sign=t==0?si:sj;
                std::size_t* v=axis==0?&a:axis==1?&b:&c;
                const std::size_t bound=axis==0?nx:axis==1?ny:nz;
                *v=sign<0?(*v?*v-1:0):(*v+1<bound?*v+1:bound-1);
            }
            return at(a,b,c);
        };
        for(int i=0;i<3;++i)for(int j=i+1;j<3;++j)
            H[i][j]=H[j][i]=(mixed(i,j,1,1)-mixed(i,j,1,-1)-
                              mixed(i,j,-1,1)+mixed(i,j,-1,-1))/(4*dx*dx);
        double g2=0,lap=0,projected=0;
        for(int i=0;i<3;++i){g2+=g[i]*g[i];lap+=H[i][i];}
        if(!std::isfinite(g2)||g2<1e-12)
            throw std::invalid_argument("Degenerate SDF gradient");
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)
            projected+=g[i]*H[i][j]*g[j];
        const double k=(lap*g2-projected)/(g2*std::sqrt(g2));
        if(!std::isfinite(k))throw std::invalid_argument("Nonfinite curvature");
        return k;
    };
    P2SdfCapillaryResult3D result;
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)for(int axis=0;axis<3;++axis){
        const std::size_t xx=x+(axis==0),yy=y+(axis==1),zz=z+(axis==2);
        if(xx>=nx||yy>=ny||zz>=nz)continue;
        const double a=at(x,y,z),b=at(xx,yy,zz);
        if((a<0)==(b<0))continue;
        const double wet=std::abs(a<0?a:b),dry=std::abs(a<0?b:a);
        const double theta=wet/(wet+dry);
        if(!std::isfinite(theta)||theta<=1e-6||theta>=1-1e-6)
            throw std::invalid_argument("Unresolved interface cut");
        const double ka=curvature(x,y,z),kb=curvature(xx,yy,zz);
        const double k=a<0?(1-theta)*ka+theta*kb:theta*ka+(1-theta)*kb;
        const double p=p_air+sigma*k;
        if(!std::isfinite(p))throw std::invalid_argument("Capillary pressure overflow");
        result.faces.push_back({axis,xx,yy,zz,theta,k,p});
    }
    if(result.faces.empty())throw std::invalid_argument("No SDF water/air interface");
    return result;
}
inline P2SdfCapillaryResult3D p2_apply_sdf_capillary3d(
    MacProjection3D& mac,std::size_t nx,std::size_t ny,std::size_t nz,
    double dx,const std::vector<double>& phi,double sigma,double p_air=0){
    if(mac.nx()!=nx||mac.ny()!=ny||mac.nz()!=nz||mac.cell_size_m()!=dx)
        throw std::invalid_argument("MAC/SDF shape mismatch");
    auto result=p2_sdf_capillary_faces3d(nx,ny,nz,dx,phi,sigma,p_air);
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        if(mac.solid_cell(x,y,z)||mac.fluid_cell(x,y,z)!=(phi[(z*ny+y)*nx+x]<0))
            throw std::invalid_argument("MAC/SDF wet or solid mask mismatch");
    MacProjection3D work=mac;
    work.clear_interface_distances();
    work.clear_interface_pressures();
    for(const auto& f:result.faces){
        if(f.axis==0){
            work.set_interface_distance_x(f.x,f.y,f.z,f.theta);
            work.set_interface_pressure_x(f.x,f.y,f.z,f.pressure_pa);
        }else if(f.axis==1){
            work.set_interface_distance_y(f.x,f.y,f.z,f.theta);
            work.set_interface_pressure_y(f.x,f.y,f.z,f.pressure_pa);
        }else{
            work.set_interface_distance_z(f.x,f.y,f.z,f.theta);
            work.set_interface_pressure_z(f.x,f.y,f.z,f.pressure_pa);
        }
    }
    mac=std::move(work);
    return result;
}
} // namespace upc
