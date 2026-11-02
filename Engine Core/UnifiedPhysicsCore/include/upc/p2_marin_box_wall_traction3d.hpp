#pragma once
// P2 PARTIAL: exact wetted face geometry of the floor-mounted MARIN Test-02
// box, with liquid-side, linearly reconstructed cell-centred pressure.
// Diagnostic only. No cut-cell pressure solve, applied FSI, or validation of
// a real pressure transducer's area/response.
#include "upc/p2_moving_box_open_volume3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MarinBoxWallTractionResult3D {
    std::array<double,3> solid_force_n{},solid_torque_nm{};
    double wetted_surface_area_m2=0.0;
    double floor_contact_area_m2=0.0;
    std::size_t pressure_tiles=0;
};
class P2MarinBoxWallTraction3D {
    struct Pair {
        std::size_t i0=0,i1=0;
        double t=0;
    };
    static Pair tangent(double pos,double dx,std::size_t n) {
        if(n<2 || !std::isfinite(pos))
            throw std::invalid_argument("Invalid MARIN traction tangent");
        const double q=pos/dx-0.5;
        const std::size_t i=static_cast<std::size_t>(
            std::clamp(std::floor(q),0.0,static_cast<double>(n-2)));
        return {i,i+1,q-static_cast<double>(i)};
    }
public:
    // Exactly one face (z-min) contacts the tank floor and is NOT wetted.
    // The other five faces integrate pressure from TWO fluid-side normal
    // cell centres. Bilinear tangential reconstruction is exact for affine
    // manufactured pressure. Input may contain pressure inside the box,
    // but those cells are never sampled. It is the caller's responsibility
    // to supply meaningful liquid-side pressure (including wet/dry state).
    static P2MarinBoxWallTractionResult3D audit(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,
        const P2Box3D& box,const std::vector<double>& pressure_pa) {
        const auto geometry=P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx,1.0,box,box);
        if(pressure_pa.size()!=geometry.alpha_old.size())
            throw std::invalid_argument("MARIN wall traction pressure shape");
        for(double p:pressure_pa)
            if(!std::isfinite(p))
                throw std::invalid_argument("Nonfinite MARIN wall pressure");
        const std::array<std::size_t,3> dims{{nx,ny,nz}};
        const double tolerance=1e-12*std::max(1.0,dx);
        if(std::abs(box.min_m[2])>tolerance ||
           box.min_m[0]<=0||box.min_m[1]<=0||
           box.max_m[0]>=nx*dx||box.max_m[1]>=ny*dx||
           box.max_m[2]>=nz*dx)
            throw std::invalid_argument("MARIN box must contact floor only");
        std::array<double,3> center{};
        for(int a=0;a<3;++a)
            center[a]=0.5*(box.min_m[a]+box.max_m[a]);
        const auto index=[&](const std::array<std::size_t,3>& v) {
            return (v[2]*ny+v[1])*nx+v[0];
        };
        P2MarinBoxWallTractionResult3D out;
        out.floor_contact_area_m2=
            (box.max_m[0]-box.min_m[0])*
            (box.max_m[1]-box.min_m[1]);
        for(int axis=0;axis<3;++axis)
        for(int side:{-1,1}) {
            if(axis==2&&side<0)continue; // dry tank-floor contact
            const double face=side<0?box.min_m[axis]:box.max_m[axis];
            // A cut cell whose centre lies INSIDE the solid must not be
            // mistaken for a liquid-side pressure sample.
            const double q=face/dx-0.5;
            const double directed=std::nextafter(
                q,side<0?-std::numeric_limits<double>::infinity():
                            std::numeric_limits<double>::infinity());
            const long long near_index=side<0?
                static_cast<long long>(std::floor(directed)):
                static_cast<long long>(std::ceil(directed));
            const long long far_index=near_index+side;
            if(near_index<0||far_index<0||
               near_index>=static_cast<long long>(dims[axis])||
               far_index>=static_cast<long long>(dims[axis]))
                throw std::invalid_argument("MARIN liquid-side stencil outside domain");
            const double near_c=(static_cast<double>(near_index)+0.5)*dx;
            const double far_c=(static_cast<double>(far_index)+0.5)*dx;
            const int b=(axis+1)%3,c=(axis+2)%3;
            for(std::size_t j=0;j<dims[b];++j)
            for(std::size_t k=0;k<dims[c];++k) {
                const double lo_b=std::max(static_cast<double>(j)*dx,box.min_m[b]);
                const double hi_b=std::min((static_cast<double>(j)+1)*dx,box.max_m[b]);
                const double lo_c=std::max(static_cast<double>(k)*dx,box.min_m[c]);
                const double hi_c=std::min((static_cast<double>(k)+1)*dx,box.max_m[c]);
                if(hi_b<=lo_b||hi_c<=lo_c)continue;
                const double area=(hi_b-lo_b)*(hi_c-lo_c);
                const double mid_b=0.5*(lo_b+hi_b),mid_c=0.5*(lo_c+hi_c);
                const auto tb=tangent(mid_b,dx,dims[b]);
                const auto tc=tangent(mid_c,dx,dims[c]);
                const auto at_normal=[&](std::size_t ni) {
                    double result=0;
                    for(int ib=0;ib<2;++ib)
                    for(int ic=0;ic<2;++ic) {
                        std::array<std::size_t,3> v{};
                        v[axis]=ni;
                        v[b]=ib?tb.i1:tb.i0;
                        v[c]=ic?tc.i1:tc.i0;
                        const double wb=ib?tb.t:1.0-tb.t;
                        const double wc=ic?tc.t:1.0-tc.t;
                        result+=wb*wc*pressure_pa[index(v)];
                    }
                    return result;
                };
                const double p_near=at_normal(static_cast<std::size_t>(near_index));
                const double p_far=at_normal(static_cast<std::size_t>(far_index));
                const double p_wall=p_near+(face-near_c)*
                    (p_far-p_near)/(far_c-near_c);
                if(!std::isfinite(p_wall))
                    throw std::runtime_error("Nonfinite reconstructed wall traction");
                const double f=-static_cast<double>(side)*p_wall*area;
                std::array<double,3> r{};
                r[axis]=face-center[axis];
                r[b]=mid_b-center[b];
                r[c]=mid_c-center[c];
                std::array<double,3> force{};
                force[axis]=f;
                out.solid_force_n[axis]+=f;
                out.solid_torque_nm[0]+=r[1]*force[2]-r[2]*force[1];
                out.solid_torque_nm[1]+=r[2]*force[0]-r[0]*force[2];
                out.solid_torque_nm[2]+=r[0]*force[1]-r[1]*force[0];
                out.wetted_surface_area_m2+=area;
                ++out.pressure_tiles;
            }
        }
        for(int a=0;a<3;++a)
            if(!std::isfinite(out.solid_force_n[a])||
               !std::isfinite(out.solid_torque_nm[a]))
                throw std::runtime_error("Nonfinite MARIN wall force/torque");
        return out;
    }
};
} // namespace upc
