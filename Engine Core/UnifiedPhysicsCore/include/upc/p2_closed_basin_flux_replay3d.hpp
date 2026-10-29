#pragma once
// P2 PARTIAL: replay an INTEGRATED closed-basin MAC face-volume flux against
// the old water inventory and new cut-cell geometry. Algebraic finite-volume
// update only: not physical VOF upwinding, a time integrator or two-way FSI.
#include "upc/p2_closed_basin_kinematic_flux3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2ClosedBasinFluxReplayResult3D {
    std::vector<double> water_fraction_after;
    double water_before_m3=0.0;
    double water_after_m3=0.0;
};

class P2ClosedBasinFluxReplay3D {
public:
    static P2ClosedBasinFluxReplayResult3D replay(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& old_box,const P2Box3D& new_box,
        const std::vector<double>& fraction_before,
        const P2ClosedBasinKinematicFluxResult3D& flux) {
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,old_box,new_box);
        const std::size_t n=nx*ny*nz;
        if(fraction_before.size()!=n ||
           flux.u_m3.size()!=(nx+1)*ny*nz ||
           flux.v_m3.size()!=nx*(ny+1)*nz ||
           flux.w_m3.size()!=nx*ny*(nz+1))
            throw std::invalid_argument("Closed-basin flux replay size mismatch");
        const double cell=dx*dx*dx;
        const double tolerance=1e-10*cell;
        // Validate all faces, including sealed boundaries and moving solids.
        const auto validate=[&](const std::vector<double>& q,
                                const std::vector<double>& open,
                                std::size_t axis) {
            for(std::size_t i=0;i<q.size();++i) {
                if(!std::isfinite(q[i]))
                    throw std::invalid_argument("Nonfinite replay face flux");
                const std::size_t x=axis==0?i%(nx+1):
                                    axis==1?i%nx:i%nx;
                const std::size_t y=axis==0?(i/(nx+1))%ny:
                                    axis==1?(i/nx)%(ny+1):(i/nx)%ny;
                const std::size_t z=axis==0?i/((nx+1)*ny):
                                    axis==1?i/(nx*(ny+1)):i/(nx*ny);
                const bool boundary=axis==0?(x==0||x==nx):
                                    axis==1?(y==0||y==ny):(z==0||z==nz);
                if((boundary||open[i]<=1e-14)&&std::abs(q[i])>tolerance)
                    throw std::invalid_argument(
                        "Replay flux crosses sealed boundary or solid face");
            }
        };
        validate(flux.u_m3,geom.u_open,0);
        validate(flux.v_m3,geom.v_open,1);
        validate(flux.w_m3,geom.w_open,2);
        const auto u=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*(nx+1)+x;
        };
        const auto v=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*(ny+1)+y)*nx+x;
        };
        const auto w=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        P2ClosedBasinFluxReplayResult3D out;
        out.water_fraction_after.assign(n,0.0);
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const std::size_t i=(z*ny+y)*nx+x;
            const double f=fraction_before[i];
            if(!std::isfinite(f)||f<0.0||f>1.0)
                throw std::invalid_argument("Invalid initial replay fraction");
            const double before=geom.cell_geometry.alpha_old[i]*f*cell;
            const double div=
                flux.u_m3[u(x+1,y,z)]-flux.u_m3[u(x,y,z)]+
                flux.v_m3[v(x,y+1,z)]-flux.v_m3[v(x,y,z)]+
                flux.w_m3[w(x,y,z+1)]-flux.w_m3[w(x,y,z)];
            const double after=before-div;
            const double capacity=geom.cell_geometry.alpha_new[i]*cell;
            if(!std::isfinite(after)||after< -tolerance||
               after>capacity+tolerance)
                throw std::invalid_argument(
                    "Replay creates negative water or exceeds open volume");
            const double bounded=std::clamp(after,0.0,capacity);
            if(capacity>1e-13*cell)
                out.water_fraction_after[i]=bounded/capacity;
            else if(bounded>tolerance)
                throw std::invalid_argument("Replay water in closed cell");
            out.water_before_m3+=before;
            out.water_after_m3+=bounded;
        }
        if(std::abs(out.water_after_m3-out.water_before_m3)>
           1e-9*std::max(1.0,out.water_before_m3))
            throw std::invalid_argument("Replay water mass not conserved");
        return out;
    }
};
} // namespace upc
