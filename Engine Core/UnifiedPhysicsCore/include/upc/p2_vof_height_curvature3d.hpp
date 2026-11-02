#pragma once
// P2 PARTIAL: three-dimensional VOF height-function curvature on a locally
// single-valued free surface, water on the negative side of the selected axis.
// Independent geometric reconstruction; NOT connected to moving MAC/PCG,
// overhangs, multi-interface columns, PLIC stress, FSI or measured MARIN CFD.
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2VofHeightCurvatureSample3D {
    double curvature_inv_m=0;
    double interface_coordinate_m=0;
    std::array<double,3> water_to_air_normal{};
    std::array<double,2> tangential_slope{};
    bool locally_single_valued=false;
    bool moving_interface_validated=false;
    bool full_p2_accepted=false;
};

class P2VofHeightCurvature3D {
public:
    static P2VofHeightCurvatureSample3D sample(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        const std::vector<double>& water_fraction,
        int normal_axis,std::size_t tangent_u,std::size_t tangent_v) {
        const std::array<std::size_t,3> dims{nx,ny,nz};
        if(normal_axis<0||normal_axis>2||nx<4||ny<4||nz<4||
           nx>128||ny>128||nz>128||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           water_fraction.size()!=nx*ny*nz||
           !std::isfinite(dx)||dx<=0)
            throw std::invalid_argument("Invalid VOF height-function domain");
        const int u=(normal_axis+1)%3,v=(normal_axis+2)%3;
        if(tangent_u==0||tangent_v==0||
           tangent_u+1>=dims[u]||tangent_v+1>=dims[v])
            throw std::out_of_range("Height-function 3x3 stencil outside domain");
        for(double a:water_fraction)
            if(!std::isfinite(a)||a<0||a>1)
                throw std::invalid_argument("Invalid VOF water fraction");
        const auto at=[&](const std::array<std::size_t,3>& c){
            return water_fraction[(c[2]*ny+c[1])*nx+c[0]];
        };
        const auto column=[&](std::size_t tu,std::size_t tv,
                              bool require_mixed){
            std::array<std::size_t,3> c{};
            c[u]=tu;c[v]=tv;
            double height=0,previous=1;
            std::size_t mixed=0;
            for(std::size_t k=0;k<dims[normal_axis];++k){
                c[normal_axis]=k;
                const double a=at(c);
                if(k==0&&a<1-1e-12)
                    throw std::invalid_argument("VOF height not wet at bottom");
                if(k+1==dims[normal_axis]&&a>1e-12)
                    throw std::invalid_argument("VOF height not dry at top");
                if(a>previous+1e-12)
                    throw std::invalid_argument("Overhang or second VOF interface");
                previous=a;
                if(a>1e-12&&a<1-1e-12)++mixed;
                height+=a*dx;
            }
            if(mixed>1||(require_mixed&&mixed!=1))
                throw std::invalid_argument("Unresolved or absent VOF cut cell");
            if(!std::isfinite(height))
                throw std::invalid_argument("Nonfinite VOF column height");
            return height;
        };
        double h[3][3]{};
        for(int a=-1;a<=1;++a)
        for(int b=-1;b<=1;++b)
            h[a+1][b+1]=column(tangent_u+a,tangent_v+b,
                               a==0&&b==0);
        const double hu=(h[2][1]-h[0][1])/(2*dx);
        const double hv=(h[1][2]-h[1][0])/(2*dx);
        const double huu=(h[2][1]-2*h[1][1]+h[0][1])/(dx*dx);
        const double hvv=(h[1][2]-2*h[1][1]+h[1][0])/(dx*dx);
        const double huv=(h[2][2]-h[2][0]-h[0][2]+h[0][0])/
                          (4*dx*dx);
        const double denom=1+hu*hu+hv*hv;
        P2VofHeightCurvatureSample3D out;
        out.curvature_inv_m=-(huu*(1+hv*hv)+hvv*(1+hu*hu)-
                              2*hu*hv*huv)/(denom*std::sqrt(denom));
        out.interface_coordinate_m=h[1][1];
        out.tangential_slope={hu,hv};
        out.water_to_air_normal[normal_axis]=1/std::sqrt(denom);
        out.water_to_air_normal[u]=-hu/std::sqrt(denom);
        out.water_to_air_normal[v]=-hv/std::sqrt(denom);
        out.locally_single_valued=true;
        if(!std::isfinite(out.curvature_inv_m)||
           !std::isfinite(out.interface_coordinate_m))
            throw std::runtime_error("Nonfinite VOF height curvature");
        return out;
    }
};
} // namespace upc
