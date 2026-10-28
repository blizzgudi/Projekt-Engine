#pragma once
// P2 PARTIAL: closed-domain, constant-density cut-cell pressure correction
// for a translating axis-aligned box. Solves only a geometrically forced
// incompressible MAC face-volume flux; NOT full Navier-Stokes or two-way FSI.
#include "upc/p2_moving_box_swept_wall_flux3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MovingBoxCutCellProjection3D {
    P2MovingBoxSweptWallFlux3D wall;
    std::vector<double> u_fluid_m3, v_fluid_m3, w_fluid_m3;
    std::vector<double> pressure_pa;
    double max_continuity_residual_m3=0.0;
    double max_closed_boundary_flux_m3=0.0;
    double fluid_flux_l1_m3=0.0;
    std::size_t cg_iterations=0;
};

class P2MovingBoxCutCellProjection3DBuilder {
    struct Edge {
        std::size_t left,right,face;
        int axis;
        double aperture;
    };
public:
    // q_ij = a_ij * (phi_i - phi_j), where phi is a face-volume
    // potential. The Poisson RHS is (alpha_old-alpha_new)*dx^3.
    // Thus alpha_new=alpha_old-div(q_fluid)/dx^3, with zero outer
    // flux. A=weighted graph Laplacian, solved by Jacobi-PCG.
    // Pressure is rho*phi/(dt^2*dx), consistent with
    // u_new = u_old - dt/rho * grad(p), here u_old=0.
    // All outputs are local until convergence; input boxes unchanged.
    static P2MovingBoxCutCellProjection3D sample(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx_m,double dt_s,double density_kg_m3,
        const P2Box3D& old_box,const P2Box3D& new_box,
        std::size_t max_iterations=4000) {
        if(!std::isfinite(density_kg_m3)||density_kg_m3<=0.0||
           max_iterations==0||nx*ny*nz>32768)
            throw std::invalid_argument("Invalid cut-cell projection parameters");
        P2MovingBoxCutCellProjection3D out;
        out.wall=P2MovingBoxSweptWallFlux3DBuilder::sample(
            nx,ny,nz,dx_m,dt_s,old_box,new_box);
        const auto n=nx*ny*nz;
        const double vol=dx_m*dx_m*dx_m;
        const auto& g=out.wall.geometry;
        out.u_fluid_m3.assign(g.u_open.size(),0.0);
        out.v_fluid_m3.assign(g.v_open.size(),0.0);
        out.w_fluid_m3.assign(g.w_open.size(),0.0);
        const auto ci=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        const auto ui=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*(nx+1)+x;
        };
        const auto vi=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*(ny+1)+y)*nx+x;
        };
        const auto wi=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        std::vector<Edge> edges;
        edges.reserve(3*n);
        std::vector<double> diag(n,0.0), b(n,0.0), phi(n,0.0);
        for(std::size_t i=0;i<n;++i)
            b[i]=(g.cell_geometry.alpha_old[i]-
                  g.cell_geometry.alpha_new[i])*vol;
        const auto add=[&](std::size_t i,std::size_t j,
                           std::size_t face,int axis,double aperture) {
            if(aperture<=1e-12)return;
            edges.push_back({i,j,face,axis,aperture});
            diag[i]+=aperture;
            diag[j]+=aperture;
        };
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=1;x<nx;++x)
            add(ci(x-1,y,z),ci(x,y,z),ui(x,y,z),0,g.u_open[ui(x,y,z)]);
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=1;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            add(ci(x,y-1,z),ci(x,y,z),vi(x,y,z),1,g.v_open[vi(x,y,z)]);
        for(std::size_t z=1;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            add(ci(x,y,z-1),ci(x,y,z),wi(x,y,z),2,g.w_open[wi(x,y,z)]);
        // A disconnected sealed component needs its OWN zero net
        // volume source. Global zero alone is insufficient.
        std::vector<std::vector<std::size_t>> neighbors(n);
        for(const auto& e:edges) {
            neighbors[e.left].push_back(e.right);
            neighbors[e.right].push_back(e.left);
        }
        std::vector<unsigned char> visited(n,0);
        const double tol=1e-12*vol;
        for(std::size_t seed=0;seed<n;++seed) {
            if(visited[seed])continue;
            std::queue<std::size_t> queue;
            queue.push(seed);
            visited[seed]=1;
            double sum=0.0;
            while(!queue.empty()) {
                const auto i=queue.front();queue.pop();
                sum+=b[i];
                for(const auto j:neighbors[i])if(!visited[j]) {
                    visited[j]=1;queue.push(j);
                }
            }
            if(std::abs(sum)>tol)
                throw std::invalid_argument("Sealed cut-cell component incompatible");
        }
        const auto multiply=[&](const std::vector<double>& x,
                                std::vector<double>& ax) {
            std::fill(ax.begin(),ax.end(),0.0);
            for(const auto& e:edges) {
                const double d=e.aperture*(x[e.left]-x[e.right]);
                ax[e.left]+=d;
                ax[e.right]-=d;
            }
        };
        std::vector<double> r=b,z(n,0.0),p(n,0.0),ap(n,0.0);
        const auto maxabs=[&](const std::vector<double>& x) {
            double m=0.0;
            for(const double v:x)m=std::max(m,std::abs(v));
            return m;
        };
        const auto dot=[&](const std::vector<double>& a,
                           const std::vector<double>& c) {
            double s=0.0;
            for(std::size_t i=0;i<n;++i)s+=a[i]*c[i];
            return s;
        };
        double rz=0.0;
        if(maxabs(r)>tol) {
            for(std::size_t i=0;i<n;++i) {
                if(diag[i]==0.0&&std::abs(b[i])>tol)
                    throw std::invalid_argument("Unreachable moving cut-cell source");
                z[i]=diag[i]>0.0?r[i]/diag[i]:0.0;
                p[i]=z[i];
            }
            rz=dot(r,z);
            if(!std::isfinite(rz)||rz<=0.0)
                throw std::runtime_error("Invalid cut-cell PCG initial residual");
            for(std::size_t iter=0;iter<max_iterations;++iter) {
                multiply(p,ap);
                const double pap=dot(p,ap);
                if(!std::isfinite(pap)||pap<=0.0)
                    throw std::runtime_error("Cut-cell PCG breakdown");
                const double step=rz/pap;
                for(std::size_t i=0;i<n;++i) {
                    phi[i]+=step*p[i];
                    r[i]-=step*ap[i];
                }
                out.cg_iterations=iter+1;
                if(maxabs(r)<=tol)break;
                for(std::size_t i=0;i<n;++i)
                    z[i]=diag[i]>0.0?r[i]/diag[i]:0.0;
                const double next=dot(r,z);
                if(!std::isfinite(next)||next<=0.0)
                    throw std::runtime_error("Cut-cell PCG preconditioner breakdown");
                const double beta=next/rz;
                for(std::size_t i=0;i<n;++i)p[i]=z[i]+beta*p[i];
                rz=next;
            }
        }
        multiply(phi,ap);
        for(std::size_t i=0;i<n;++i) {
            out.max_continuity_residual_m3=std::max(
                out.max_continuity_residual_m3,std::abs(ap[i]-b[i]));
        }
        if(out.max_continuity_residual_m3>tol)
            throw std::runtime_error("Cut-cell projection did not converge");
        const double pressure_scale=density_kg_m3/(dt_s*dt_s*dx_m);
        if(!std::isfinite(pressure_scale))
            throw std::invalid_argument("Cut-cell pressure scale overflow");
        out.pressure_pa.resize(n);
        for(std::size_t i=0;i<n;++i) {
            out.pressure_pa[i]=phi[i]*pressure_scale;
            if(!std::isfinite(out.pressure_pa[i]))
                throw std::runtime_error("Nonfinite cut-cell pressure");
        }
        for(const auto& e:edges) {
            const double q=e.aperture*(phi[e.left]-phi[e.right]);
            if(!std::isfinite(q))
                throw std::runtime_error("Nonfinite cut-cell fluid flux");
            auto& faces=e.axis==0?out.u_fluid_m3:
                        e.axis==1?out.v_fluid_m3:out.w_fluid_m3;
            faces[e.face]=q;
            out.fluid_flux_l1_m3+=std::abs(q);
        }
        return out;
    }
};
} // namespace upc
