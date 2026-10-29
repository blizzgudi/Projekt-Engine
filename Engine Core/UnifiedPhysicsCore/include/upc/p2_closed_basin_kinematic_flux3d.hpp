#pragma once
// P2 PARTIAL: reconstruct a divergence-consistent, integrated 3D water-volume
// flux between two CLOSED-BASIN hydrostatic inventories. This is a minimum
// weighted graph-norm flux, NOT a Navier-Stokes momentum/pressure solution.
// In particular it does not apply a body counterimpulse or fluid kinetic energy.
#include "upc/p2_closed_basin_hydrostatic3d.hpp"
#include "upc/p2_moving_box_mac_apertures3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2ClosedBasinKinematicFluxResult3D {
    // Integrated face volumes [m^3], positive in the positive axis direction.
    // Domain-boundary faces are identically zero.
    std::vector<double> u_m3, v_m3, w_m3;
    double max_cell_continuity_residual_m3=0.0;
    double net_boundary_flux_m3=0.0;
    double total_water_change_m3=0.0;
    double max_open_face_speed_proxy_m_s=0.0;
    std::size_t cg_iterations=0;
    double cg_residual_l2_m3=0.0;
};

class P2ClosedBasinKinematicFlux3D {
    struct Edge {std::size_t a,b,face;int axis;double weight;};
public:
    static P2ClosedBasinKinematicFluxResult3D reconstruct(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx,double dt,
        const P2Box3D& old_box,const P2ClosedBasinHydrostaticResult3D& state) {
        const auto geom=P2MovingBoxFaceApertures3D::sample(
            nx,ny,nz,dx,dt,old_box,state.box_after);
        const std::size_t n=nx*ny*nz;
        if(state.water_fraction_before.size()!=n ||
           state.water_fraction_after.size()!=n)
            throw std::invalid_argument("Closed-basin flux inventory size mismatch");
        const double cell=dx*dx*dx;
        P2ClosedBasinKinematicFluxResult3D out;
        out.u_m3.assign((nx+1)*ny*nz,0.0);
        out.v_m3.assign(nx*(ny+1)*nz,0.0);
        out.w_m3.assign(nx*ny*(nz+1),0.0);
        std::vector<double> b(n,0.0);
        double volume=0.0;
        for(std::size_t i=0;i<n;++i) {
            const double fo=state.water_fraction_before[i];
            const double fn=state.water_fraction_after[i];
            if(!std::isfinite(fo)||!std::isfinite(fn)||
               fo<0||fn<0||fo>1||fn>1)
                throw std::invalid_argument("Invalid water fraction");
            const double oldv=geom.cell_geometry.alpha_old[i]*fo*cell;
            const double newv=geom.cell_geometry.alpha_new[i]*fn*cell;
            b[i]=oldv-newv; // desired integrated net OUTFLOW
            volume+=oldv;
            out.total_water_change_m3+=newv-oldv;
        }
        if(std::abs(out.total_water_change_m3)>1e-9*std::max(1.0,volume))
            throw std::invalid_argument("Nonconservative water inventory");
        std::vector<Edge> edges;
        edges.reserve(3*n);
        const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        const auto add=[&](std::size_t a,std::size_t c,std::size_t face,
                           int axis,double aperture) {
            // Both old and new wet supports are admitted. This is a kinematic
            // graph metric, not a physical wet-face stress/VOF upwind flux.
            const double wet=std::max({
                state.water_fraction_before[a],state.water_fraction_after[a],
                state.water_fraction_before[c],state.water_fraction_after[c]});
            const double w=aperture*wet;
            if(w>1e-14)edges.push_back({a,c,face,axis,w});
        };
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const auto a=id(x,y,z);
            if(x+1<nx)add(a,id(x+1,y,z),(z*ny+y)*(nx+1)+x+1,0,
                          geom.u_open[(z*ny+y)*(nx+1)+x+1]);
            if(y+1<ny)add(a,id(x,y+1,z),(z*(ny+1)+y+1)*nx+x,1,
                          geom.v_open[(z*(ny+1)+y+1)*nx+x]);
            if(z+1<nz)add(a,id(x,y,z+1),((z+1)*ny+y)*nx+x,2,
                          geom.w_open[((z+1)*ny+y)*nx+x]);
        }
        // The weighted Laplacian has one nullspace per disconnected wet
        // component. A globally balanced inventory is NOT sufficient:
        // each component must conserve its own water volume. Fail before CG
        // instead of returning an arbitrary pressure potential or a cryptic
        // breakdown. Dry isolated cells with nonzero inventory change fail too.
        std::vector<std::size_t> parent(n);
        std::iota(parent.begin(),parent.end(),std::size_t{0});
        const auto root=[&](std::size_t i) {
            while(parent[i]!=i) {
                parent[i]=parent[parent[i]];
                i=parent[i];
            }
            return i;
        };
        for(const auto& e:edges) {
            const auto a=root(e.a),c=root(e.b);
            if(a!=c)parent[c]=a;
        }
        std::vector<double> component_balance(n,0.0);
        for(std::size_t i=0;i<n;++i)
            component_balance[root(i)]+=b[i];
        const double component_tolerance=1e-12*std::max(1.0,volume);
        for(std::size_t i=0;i<n;++i)
            if(parent[i]==i &&
               std::abs(component_balance[i])>component_tolerance)
                throw std::invalid_argument(
                    "Disconnected wet component has nonconservative inventory");
        const auto dot=[&](const std::vector<double>& a,
                           const std::vector<double>& c) {
            double s=0.0;
            for(std::size_t i=0;i<n;++i)s+=a[i]*c[i];
            return s;
        };
        const auto apply=[&](const std::vector<double>& x,
                             std::vector<double>& y) {
            std::fill(y.begin(),y.end(),0.0);
            for(const auto& e:edges) {
                const double q=e.weight*(x[e.a]-x[e.b]);
                y[e.a]+=q;y[e.b]-=q;
            }
        };
        std::vector<double> phi(n,0.0),r=b,d=b,Ad(n,0.0);
        double rr=dot(r,r);
        const double tolerance=1e-12*std::max(1.0,volume);
        const double stop=tolerance*tolerance;
        const std::size_t max_iterations=std::min<std::size_t>(20000,20*n);
        while(rr>stop&&out.cg_iterations<max_iterations) {
            apply(d,Ad);
            const double denom=dot(d,Ad);
            if(!std::isfinite(denom)||denom<=0.0)
                throw std::runtime_error("Disconnected wet flux graph");
            const double alpha=rr/denom;
            for(std::size_t i=0;i<n;++i) {
                phi[i]+=alpha*d[i];
                r[i]-=alpha*Ad[i];
            }
            const double next=dot(r,r);
            if(!std::isfinite(next))
                throw std::runtime_error("Nonfinite flux CG residual");
            ++out.cg_iterations;
            if(next<=stop) {rr=next;break;}
            const double beta=next/rr;
            for(std::size_t i=0;i<n;++i)d[i]=r[i]+beta*d[i];
            rr=next;
        }
        out.cg_residual_l2_m3=std::sqrt(rr);
        if(rr>stop)
            throw std::runtime_error("Closed-basin flux CG did not converge");
        std::vector<double> divergence(n,0.0);
        for(const auto& e:edges) {
            const double q=e.weight*(phi[e.a]-phi[e.b]);
            auto& faces=e.axis==0?out.u_m3:(e.axis==1?out.v_m3:out.w_m3);
            faces[e.face]=q;
            divergence[e.a]+=q;divergence[e.b]-=q;
            const auto& apertures=e.axis==0?geom.u_open:
                                  (e.axis==1?geom.v_open:geom.w_open);
            const double speed=std::abs(q)/(dt*dx*dx*apertures[e.face]);
            out.max_open_face_speed_proxy_m_s=
                std::max(out.max_open_face_speed_proxy_m_s,speed);
        }
        for(std::size_t i=0;i<n;++i)
            out.max_cell_continuity_residual_m3=
                std::max(out.max_cell_continuity_residual_m3,
                         std::abs(divergence[i]-b[i]));
        if(out.max_cell_continuity_residual_m3>1e-9*std::max(1.0,volume))
            throw std::runtime_error("Kinematic water continuity failed");
        return out;
    }
};
} // namespace upc
