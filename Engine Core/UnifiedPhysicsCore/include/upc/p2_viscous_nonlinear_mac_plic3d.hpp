#pragma once
// P2 PARTIAL: optional periodic constant-density MAC viscosity, followed by
// the existing nonlinear momentum/projection and passive geometric PLIC step.
// This is a split, first-order numerical model, NOT stress-consistent two-phase
// Navier-Stokes, moving-solid FSI or a global physical energy balance.
#include "upc/p2_nonlinear_mac_plic3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct P2ViscousNonlinearMacPlicStep3D {
    P2NonlinearMacPlicStep3D coupled;
    std::size_t viscous_substeps=0;
    double face_square_sum_before=0.0; // unique staggered faces, numerical proxy
    double face_square_sum_after=0.0;  // before nonlinear advection/projection
    std::array<double,3> unique_face_sum_delta{{0,0,0}};
};

// Explicit periodic 7-point diffusion on the three staggered MAC fields.
// Duplicated periodic seam faces are rebuilt after every substep. The
// dimensionless substep coefficient is <= 1/8 (strictly below 1/6 stability
// limit). The zero-viscosity path calls the existing operator unchanged.
inline P2ViscousNonlinearMacPlicStep3D p2_viscous_nonlinear_mac_plic_step3d(
    const P2NonlinearMacPlicState3D& initial,
    std::size_t nx,std::size_t ny,std::size_t nz,
    double dx_m,double dt_s,double density_kg_m3,
    double nu_m2_s,
    double target_six_face_courant=0.4,
    std::size_t max_plic_substeps=256,
    std::size_t max_viscous_substeps=256) {
    if(!std::isfinite(nu_m2_s)||nu_m2_s<0)
        throw std::invalid_argument("Invalid periodic kinematic viscosity");
    if(nu_m2_s==0) {
        P2ViscousNonlinearMacPlicStep3D out;
        out.coupled=p2_nonlinear_mac_plic_step3d(initial,nx,ny,nz,
            dx_m,dt_s,density_kg_m3,target_six_face_courant,max_plic_substeps);
        return out;
    }
    const auto lim=std::numeric_limits<std::size_t>::max();
    if(nx<3||ny<3||nz<3||nx>lim/ny||nx*ny>lim/nz||nx*ny*nz>32768||
       !std::isfinite(dx_m)||dx_m<=0||!std::isfinite(dt_s)||dt_s<=0||
       !std::isfinite(density_kg_m3)||density_kg_m3<=0||
       max_viscous_substeps==0)
        throw std::invalid_argument("Invalid viscous MAC grid, units or budget");
    const long double total_alpha=static_cast<long double>(nu_m2_s)*
        static_cast<long double>(dt_s)/
        (static_cast<long double>(dx_m)*dx_m);
    if(!std::isfinite(total_alpha)||total_alpha<=0||
       total_alpha>static_cast<long double>(max_viscous_substeps)/8.0L)
        throw std::invalid_argument("Viscous CFL exceeds substep budget");
    const auto substeps=static_cast<std::size_t>(std::ceil(total_alpha*8.0L));
    if(substeps==0||substeps>max_viscous_substeps)
        throw std::invalid_argument("Viscous substep budget exhausted");
    const double alpha=static_cast<double>(total_alpha/substeps);
    if(!std::isfinite(alpha)||alpha<=0||alpha>0.125000000000001)
        throw std::invalid_argument("Invalid viscous substep coefficient");

    const std::array<std::size_t,3> lengths{{
        (nx+1)*ny*nz,nx*(ny+1)*nz,nx*ny*(nz+1)}};
    const auto& source=initial.staggered_velocity;
    for(int c=0;c<3;++c) {
        if(source[c].size()!=lengths[c])
            throw std::invalid_argument("Invalid viscous staggered field size");
        for(double v:source[c])
            if(!std::isfinite(v))
                throw std::invalid_argument("Nonfinite viscous velocity");
    }
    auto index=[&](int c,std::size_t i,std::size_t j,std::size_t k) {
        if(c==0)return (k*ny+j)*(nx+1)+i;
        if(c==1)return (k*(ny+1)+j)*nx+i;
        return (k*ny+j)*nx+i;
    };
    // Reject nonperiodic seam values instead of silently overwriting them.
    for(int c=0;c<3;++c)
    for(std::size_t k=0;k<nz;++k)
    for(std::size_t j=0;j<ny;++j)
    for(std::size_t i=0;i<nx;++i) {
        if(c==0&&i==0&&std::abs(source[c][index(c,nx,j,k)]-
                                       source[c][index(c,0,j,k)])>1e-12)
            throw std::invalid_argument("Nonperiodic viscous U seam");
        if(c==1&&j==0&&std::abs(source[c][index(c,i,ny,k)]-
                                       source[c][index(c,i,0,k)])>1e-12)
            throw std::invalid_argument("Nonperiodic viscous V seam");
        if(c==2&&k==0&&std::abs(source[c][index(c,i,j,nz)]-
                                       source[c][index(c,i,j,0)])>1e-12)
            throw std::invalid_argument("Nonperiodic viscous W seam");
    }
    auto state=initial;
    P2ViscousNonlinearMacPlicStep3D out;
    out.viscous_substeps=substeps;
    std::array<double,3> before_sum{{0,0,0}},after_sum{{0,0,0}};
    auto measure=[&](const std::array<std::vector<double>,3>& field,
                     std::array<double,3>& sums) {
        double energy=0;
        for(int c=0;c<3;++c)
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i) {
            const double v=field[c][index(c,i,j,k)];
            sums[c]+=v;
            energy+=v*v;
        }
        return energy;
    };
    out.face_square_sum_before=measure(state.staggered_velocity,before_sum);
    for(std::size_t step=0;step<substeps;++step) {
        auto next=state.staggered_velocity;
        for(int c=0;c<3;++c)
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i) {
            const auto xp=(i+1)%nx,xm=(i+nx-1)%nx;
            const auto yp=(j+1)%ny,ym=(j+ny-1)%ny;
            const auto zp=(k+1)%nz,zm=(k+nz-1)%nz;
            const auto& v=state.staggered_velocity[c];
            const auto id=index(c,i,j,k);
            next[c][id]=v[id]+alpha*(
                v[index(c,xp,j,k)]+v[index(c,xm,j,k)]+
                v[index(c,i,yp,k)]+v[index(c,i,ym,k)]+
                v[index(c,i,j,zp)]+v[index(c,i,j,zm)]-6*v[id]);
            if(!std::isfinite(next[c][id]))
                throw std::runtime_error("Nonfinite viscous MAC update");
        }
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
            next[0][index(0,nx,j,k)]=next[0][index(0,0,j,k)];
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t i=0;i<nx;++i)
            next[1][index(1,i,ny,k)]=next[1][index(1,i,0,k)];
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i)
            next[2][index(2,i,j,nz)]=next[2][index(2,i,j,0)];
        state.staggered_velocity=std::move(next);
    }
    out.face_square_sum_after=measure(state.staggered_velocity,after_sum);
    for(int c=0;c<3;++c)out.unique_face_sum_delta[c]=after_sum[c]-before_sum[c];
    const double tolerance=1e-10*std::max(1.0,out.face_square_sum_before);
    if(!std::isfinite(out.face_square_sum_after)||
       out.face_square_sum_after>out.face_square_sum_before+tolerance)
        throw std::runtime_error("Periodic viscous face energy increased");
    out.coupled=p2_nonlinear_mac_plic_step3d(state,nx,ny,nz,
        dx_m,dt_s,density_kg_m3,target_six_face_courant,max_plic_substeps);
    return out;
}
} // namespace upc
