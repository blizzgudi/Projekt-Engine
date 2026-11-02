#pragma once
// P2 PARTIAL: O(N) bulk height-function curvature for strictly single-valued
// 3D VOF graphs. The 3x3 finite-difference stencil uses nearest-interior
// extrapolation at tangential boundaries by default; optional static
// 90-degree contact angle uses reflected cell-centred ghost heights.
// This is NOT a sharp PLIC/CSF
// stress-jump or validated moving-interface/experimental CFD model.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2VofHeightCurvatureField3D {
    std::vector<double> cell_curvature_inv_m;
    std::size_t interface_columns=0;
    std::size_t interface_cut_cells=0;
    double max_abs_curvature_inv_m=0;
    int normal_axis=-1;
    bool boundary_extrapolated=true;
    bool boundary_neumann_90=false;
    bool full_p2_accepted=false;
};
class P2VofHeightCurvatureBulk3D {
public:
    static P2VofHeightCurvatureField3D reconstruct(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        const std::vector<double>& water_fraction,int normal_axis,
        bool wall_neumann_90=false) {
        const std::array<std::size_t,3> d{nx,ny,nz};
        if(normal_axis<0||normal_axis>2||nx<4||ny<4||nz<4||
           nx>128||ny>128||nz>128||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           nx*ny*nz>100000||
           water_fraction.size()!=nx*ny*nz||
           !(std::isfinite(dx)&&dx>0))
            throw std::invalid_argument("Invalid 3D bulk height domain");
        const int u=(normal_axis+1)%3,v=(normal_axis+2)%3;
        const std::size_t nu=d[u],nv=d[v],na=d[normal_axis];
        const auto cell=[&](const std::array<std::size_t,3>& c){
            return (c[2]*ny+c[1])*nx+c[0];
        };
        const auto col=[&](std::size_t iu,std::size_t iv){
            return iv*nu+iu;
        };
        std::vector<double> height(nu*nv,0);
        P2VofHeightCurvatureField3D out;
        out.normal_axis=normal_axis;
        out.boundary_extrapolated=!wall_neumann_90;
        out.boundary_neumann_90=wall_neumann_90;
        out.cell_curvature_inv_m.resize(nx*ny*nz);
        // Full preflight once. No silent diffuse fallback if a column
        // touches the axis boundary, overturns or contains multiple cuts.
        for(std::size_t iv=0;iv<nv;++iv)
        for(std::size_t iu=0;iu<nu;++iu){
            std::array<std::size_t,3> c{};
            c[u]=iu;c[v]=iv;
            double previous=1,sum=0;
            std::size_t mixed=0;
            for(std::size_t k=0;k<na;++k){
                c[normal_axis]=k;
                const double a=water_fraction[cell(c)];
                if(!(std::isfinite(a)&&a>=0&&a<=1))
                    throw std::invalid_argument("Invalid VOF fraction");
                if((k==0&&a<1-1e-12)||
                   (k+1==na&&a>1e-12)||
                   a>previous+1e-12)
                    throw std::invalid_argument("Nonmonotone or boundary VOF");
                if(a>1e-12&&a<1-1e-12)++mixed;
                previous=a;
                sum+=a;
            }
            if(mixed!=1)
                throw std::invalid_argument("Bulk height needs exactly one cut per column");
            height[col(iu,iv)]=sum*dx;
            if(!std::isfinite(height[col(iu,iv)]))
                throw std::invalid_argument("Nonfinite column height");
            ++out.interface_columns;
            ++out.interface_cut_cells;
        }
        std::vector<double> kappa(nu*nv);
        // At a closed tangential wall, a 90-degree contact angle means
        // zero wall-normal height gradient. Reflect cell-centred heights
        // into the ghost cell (h[-1]=h[0], h[N]=h[N-1]). This is a
        // prescribed static wall-contact model, not dynamic wetting.
        const auto h=[&](int iu,int iv){
            if(wall_neumann_90){
                iu=std::max(0,std::min(iu,int(nu)-1));
                iv=std::max(0,std::min(iv,int(nv)-1));
            }
            return height[col(std::size_t(iu),std::size_t(iv))];
        };
        for(std::size_t iv=0;iv<nv;++iv)
        for(std::size_t iu=0;iu<nu;++iu){
            const int i=wall_neumann_90?int(iu):
                int(std::max(std::size_t(1),std::min(iu,nu-2)));
            const int j=wall_neumann_90?int(iv):
                int(std::max(std::size_t(1),std::min(iv,nv-2)));
            const double hu=(h(i+1,j)-h(i-1,j))/(2*dx);
            const double hv=(h(i,j+1)-h(i,j-1))/(2*dx);
            const double huu=(h(i+1,j)-2*h(i,j)+h(i-1,j))/(dx*dx);
            const double hvv=(h(i,j+1)-2*h(i,j)+h(i,j-1))/(dx*dx);
            const double huv=(h(i+1,j+1)-h(i+1,j-1)-
                              h(i-1,j+1)+h(i-1,j-1))/(4*dx*dx);
            const double denom=1+hu*hu+hv*hv;
            const double value=-(huu*(1+hv*hv)+hvv*(1+hu*hu)-
                                 2*hu*hv*huv)/(denom*std::sqrt(denom));
            if(!std::isfinite(value))
                throw std::runtime_error("Nonfinite height curvature");
            kappa[col(iu,iv)]=value;
            out.max_abs_curvature_inv_m=std::max(
                out.max_abs_curvature_inv_m,std::abs(value));
        }
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x){
            const std::array<std::size_t,3> c{x,y,z};
            out.cell_curvature_inv_m[cell(c)]=kappa[col(c[u],c[v])];
        }
        return out;
    }
};
} // namespace upc
