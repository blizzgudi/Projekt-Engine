#pragma once
// P2 PARTIAL: periodic constant-density 3D MAC/VOF energy bookkeeping.
// Exact discrete kinetic-energy identity and pressure work on UNIQUE MAC
// faces; NOT a free-surface, moving-wall, or total FSI energy budget.
#include "upc/p2_periodic_mac_vof3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2PeriodicMacEnergyLedger3D {
    double initial_kinetic_j=0.0;
    double advected_kinetic_j=0.0;
    double projected_kinetic_j=0.0;
    double advection_change_j=0.0;
    double projection_change_j=0.0;
    double pressure_work_j=0.0;
    double correction_energy_j=0.0;
    double pressure_identity_residual_j=0.0;
    double projected_gradient_orthogonality_j=0.0;
    double max_pressure_gradient_velocity_residual_m_s=0.0;
};
struct P2PeriodicMacVofEnergyStep3D {
    P2PeriodicMacVofAdvance3D state;
    P2PeriodicMacEnergyLedger3D ledger;
};
class P2PeriodicMacVofEnergy3D {
    using Vec=std::vector<double>;
    static std::size_t face(int c,std::size_t i,std::size_t j,
                            std::size_t k,std::size_t nx,std::size_t ny) {
        if(c==0)return (k*ny+j)*(nx+1)+i;
        if(c==1)return (k*(ny+1)+j)*nx+i;
        return (k*ny+j)*nx+i;
    }
    static double energy(std::size_t nx,std::size_t ny,std::size_t nz,
                         double scale,const Vec& u,const Vec& v,const Vec& w) {
        double sum=0.0;
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i) {
            const double x=u[face(0,i,j,k,nx,ny)];
            const double y=v[face(1,i,j,k,nx,ny)];
            const double z=w[face(2,i,j,k,nx,ny)];
            sum+=x*x+y*y+z*z;
        }
        return 0.5*scale*sum;
    }
public:
    // Independent pressure-work identity:
    // Delta K = dt*volume*sum(p*div(u*)) + 0.5*rho*volume*||u-u*||^2.
    // Discrete integration by parts requires periodic unique-face counting.
    // The projected velocity should be orthogonal to the correction.
    static P2PeriodicMacEnergyLedger3D audit(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const Vec& old_u,const Vec& old_v,const Vec& old_w,
        const Vec& pred_u,const Vec& pred_v,const Vec& pred_w,
        const P2PeriodicMacProjectionResult3D& projected) {
        if(nx<2||ny<2||nz<2||nx>32768/ny||nx*ny>32768/nz||
           !std::isfinite(dx)||dx<=0||!std::isfinite(dt)||dt<=0||
           !std::isfinite(rho)||rho<=0)
            throw std::invalid_argument("Invalid periodic energy geometry");
        const std::size_t us=(nx+1)*ny*nz,vs=nx*(ny+1)*nz,ws=nx*ny*(nz+1);
        if(old_u.size()!=us||pred_u.size()!=us||projected.u_m_s.size()!=us||
           old_v.size()!=vs||pred_v.size()!=vs||projected.v_m_s.size()!=vs||
           old_w.size()!=ws||pred_w.size()!=ws||projected.w_m_s.size()!=ws||
           projected.pressure_pa.size()!=nx*ny*nz)
            throw std::invalid_argument("Invalid periodic energy field shape");
        for(const Vec* field:{&old_u,&old_v,&old_w,&pred_u,&pred_v,&pred_w,
                              &projected.u_m_s,&projected.v_m_s,
                              &projected.w_m_s,&projected.pressure_pa})
            for(double x:*field)
                if(!std::isfinite(x))
                    throw std::invalid_argument("Nonfinite periodic energy field");
        // The discrete integration-by-parts identity needs matching periodic
        // seam copies on all three input and output velocity fields.
        const auto seams=[&](const Vec& uu,const Vec& vv,const Vec& ww) {
            for(std::size_t k=0;k<nz;++k)
            for(std::size_t j=0;j<ny;++j)
                if(std::abs(uu[face(0,nx,j,k,nx,ny)]-
                            uu[face(0,0,j,k,nx,ny)])>1e-12)
                    throw std::invalid_argument("Energy U seam mismatch");
            for(std::size_t k=0;k<nz;++k)
            for(std::size_t i=0;i<nx;++i)
                if(std::abs(vv[face(1,i,ny,k,nx,ny)]-
                            vv[face(1,i,0,k,nx,ny)])>1e-12)
                    throw std::invalid_argument("Energy V seam mismatch");
            for(std::size_t j=0;j<ny;++j)
            for(std::size_t i=0;i<nx;++i)
                if(std::abs(ww[face(2,i,j,nz,nx,ny)]-
                            ww[face(2,i,j,0,nx,ny)])>1e-12)
                    throw std::invalid_argument("Energy W seam mismatch");
        };
        seams(old_u,old_v,old_w);
        seams(pred_u,pred_v,pred_w);
        seams(projected.u_m_s,projected.v_m_s,projected.w_m_s);
        const double vol=dx*dx*dx,scale=rho*vol;
        P2PeriodicMacEnergyLedger3D out;
        out.initial_kinetic_j=energy(nx,ny,nz,scale,old_u,old_v,old_w);
        out.advected_kinetic_j=energy(nx,ny,nz,scale,pred_u,pred_v,pred_w);
        out.projected_kinetic_j=energy(nx,ny,nz,scale,
            projected.u_m_s,projected.v_m_s,projected.w_m_s);
        double squared=0.0,orthogonal=0.0,pressure_div=0.0;
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i) {
            const auto ux=face(0,i,j,k,nx,ny);
            const auto vy=face(1,i,j,k,nx,ny);
            const auto wz=face(2,i,j,k,nx,ny);
            const double du=projected.u_m_s[ux]-pred_u[ux];
            const double dv=projected.v_m_s[vy]-pred_v[vy];
            const double dw=projected.w_m_s[wz]-pred_w[wz];
            const auto pressure=[&](std::size_t x,std::size_t y,std::size_t z) {
                return projected.pressure_pa[(z*ny+y)*nx+x];
            };
            const double factor=-dt/(rho*dx);
            const double eu=factor*(pressure(i,j,k)-pressure((i+nx-1)%nx,j,k));
            const double ev=factor*(pressure(i,j,k)-pressure(i,(j+ny-1)%ny,k));
            const double ew=factor*(pressure(i,j,k)-pressure(i,j,(k+nz-1)%nz));
            out.max_pressure_gradient_velocity_residual_m_s=std::max({
                out.max_pressure_gradient_velocity_residual_m_s,
                std::abs(du-eu),std::abs(dv-ev),std::abs(dw-ew)});
            squared+=du*du+dv*dv+dw*dw;
            orthogonal+=projected.u_m_s[ux]*du+
                        projected.v_m_s[vy]*dv+
                        projected.w_m_s[wz]*dw;
            const double div=(pred_u[face(0,i+1,j,k,nx,ny)]-pred_u[ux]+
                pred_v[face(1,i,j+1,k,nx,ny)]-pred_v[vy]+
                pred_w[face(2,i,j,k+1,nx,ny)]-pred_w[wz])/dx;
            pressure_div+=projected.pressure_pa[(k*ny+j)*nx+i]*div;
        }
        out.advection_change_j=out.advected_kinetic_j-out.initial_kinetic_j;
        out.projection_change_j=out.projected_kinetic_j-out.advected_kinetic_j;
        out.pressure_work_j=dt*vol*pressure_div;
        out.correction_energy_j=0.5*scale*squared;
        out.pressure_identity_residual_j=out.projection_change_j-
            (out.pressure_work_j+out.correction_energy_j);
        out.projected_gradient_orthogonality_j=scale*orthogonal;
        for(double value:{out.initial_kinetic_j,out.advected_kinetic_j,
            out.projected_kinetic_j,out.pressure_work_j,
            out.correction_energy_j,out.pressure_identity_residual_j,
            out.projected_gradient_orthogonality_j,
            out.max_pressure_gradient_velocity_residual_m_s})
            if(!std::isfinite(value))
                throw std::runtime_error("Nonfinite periodic energy ledger");
        const double tolerance=1e-8*std::max({1.0,out.initial_kinetic_j,
            out.advected_kinetic_j,out.projected_kinetic_j});
        if(std::abs(out.pressure_identity_residual_j)>tolerance||
           std::abs(out.projected_gradient_orthogonality_j)>tolerance||
           out.max_pressure_gradient_velocity_residual_m_s>1e-10)
            throw std::runtime_error("Periodic MAC pressure energy audit failed");
        return out;
    }
    // Opt-in diagnostic path; existing MAC/VOF solver remains unchanged.
    static P2PeriodicMacVofEnergyStep3D advance(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,double rho,
        const Vec& u,const Vec& v,const Vec& w,const Vec& fraction) {
        auto predicted=P2VariableMacTransport3D::self_advect(
            nx,ny,nz,dx,dt,u,v,w);
        auto projected=P2PeriodicMacProjection3D::project(
            nx,ny,nz,dx,dt,rho,
            predicted.u_m_s,predicted.v_m_s,predicted.w_m_s);
        auto ledger=audit(nx,ny,nz,dx,dt,rho,u,v,w,
            predicted.u_m_s,predicted.v_m_s,predicted.w_m_s,projected);
        auto vof=P2PeriodicMacVof3D::transport_fraction(nx,ny,nz,dx,dt,
            projected.u_m_s,projected.v_m_s,projected.w_m_s,fraction);
        return {{ {std::move(projected),predicted.max_outgoing_courant},
                   std::move(vof)},ledger};
    }
};
} // namespace upc
