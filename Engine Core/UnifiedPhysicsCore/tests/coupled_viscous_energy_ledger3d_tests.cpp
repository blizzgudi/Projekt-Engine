// P2 live MAC/VOF viscosity energy ledger: diagnostic-only partial gate.
// No continuum FSI/physical energy or moving-body claims.
#include "upc/coupled_water3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int checks=0;
static void require(bool ok,const char* msg){
    ++checks;
    if(!ok)throw std::runtime_error(msg);
}
static void near(double a,double b,double tol,const char* msg){
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
static void tank(CoupledMacWater3D& s){
    for(std::size_t z=0;z<s.nz();++z)
    for(std::size_t y=0;y<s.ny();++y)
    for(std::size_t x=0;x<s.nx();++x){
        if(x==2)s.set_solid(x,y,z);
        if(x>=3&&x<=5&&y>=1&&y<=3&&z<4)
            s.set_water(x,y,z,1.0);
    }
}
static void verify(const CoupledMacWater3D& s,double tol){
    const auto l=s.last_viscous_diffusion_ledger();
    require(l.substeps==s.last_viscosity_substeps(),"live ledger substeps disagree");
    const double scale=s.density_kg_m3()*std::pow(s.cell_size_m(),3);
    for(std::size_t k=0;k<3;++k){
        const double predicted=-l.internal_gradient_loss_proxy_m2_s2[k]
            -l.solid_gradient_loss_proxy_m2_s2[k]
            -l.exterior_gradient_loss_proxy_m2_s2[k]
            +l.prescribed_wall_work_proxy_m2_s2[k]
            +l.explicit_euler_correction_proxy_m2_s2[k];
        near(l.face_energy_change_proxy_m2_s2[k],predicted,tol,
             "live discrete energy budget does not close");
        near(l.energy_closure_residual_proxy_m2_s2[k],0,tol,
             "live energy residual nonzero");
        near(l.active_face_delta_sum_m_s[k],
             l.solid_boundary_source_sum_m_s[k]
                +l.exterior_boundary_source_sum_m_s[k],tol,
             "live face-momentum source closure");
        near(l.closure_residual_sum_m_s[k],0,tol,
             "live face-momentum residual nonzero");
        near(s.last_viscous_face_momentum_delta_proxy_kg_m_s()[k],
             scale*l.active_face_delta_sum_m_s[k],tol,
             "old momentum proxy and new raw ledger disagree");
        require(l.internal_gradient_loss_proxy_m2_s2[k]>=0
             && l.solid_gradient_loss_proxy_m2_s2[k]>=0
             && l.exterior_gradient_loss_proxy_m2_s2[k]>=0
             && l.explicit_euler_correction_proxy_m2_s2[k]>=0,
             "live diffusion loss or Euler correction negative");
    }
}
int main(){
try{
    constexpr double dx=0.1,dt=0.02;
    CoupledMacWater3D driven(8,8,6,dx),still(8,8,6,dx),
                      inviscid(8,8,6,dx);
    tank(driven);tank(still);tank(inviscid);
    driven.set_kinematic_viscosity(0.035);
    still.set_kinematic_viscosity(0.035);
    driven.set_wall_tangential_velocity({0,0.75,0});
    const double mass=driven.mass_kg();
    near(mass,36.0,1e-10,"initial 3D water mass not 36 kg");
    double positive_wall_work=0,positive_loss=0;
    std::size_t live_visc_substeps=0;
    for(int i=0;i<10;++i){
        driven.step(dt,180);
        still.step(dt,180);
        inviscid.step(dt,180);
        near(driven.mass_kg(),mass,1e-8,"driven VOF lost mass");
        near(still.mass_kg(),mass,1e-8,"still VOF lost mass");
        near(inviscid.mass_kg(),mass,1e-8,"inviscid VOF lost mass");
        verify(driven,2e-9);
        verify(still,2e-9);
        verify(inviscid,0);
        const auto l=driven.last_viscous_diffusion_ledger();
        const auto s=still.last_viscous_diffusion_ledger();
        const auto n=inviscid.last_viscous_diffusion_ledger();
        require(l.substeps>0 && s.substeps>0 && n.substeps==0,
                "live viscosity/inviscid substep distinction failed");
        near(s.prescribed_wall_work_proxy_m2_s2[1],0,0,
             "stationary wall did numerical work");
        for(double v:n.face_energy_change_proxy_m2_s2)
            near(v,0,0,"inviscid ledger not reset");
        positive_wall_work+=l.prescribed_wall_work_proxy_m2_s2[1];
        positive_loss+=l.internal_gradient_loss_proxy_m2_s2[1]
                      +l.solid_gradient_loss_proxy_m2_s2[1];
        live_visc_substeps+=l.substeps;
    }
    require(positive_wall_work>1e-8,
            "live prescribed shear did not perform diagnostic wall work");
    require(positive_loss>1e-8,
            "live viscosity has no numerical gradient dissipation");
    // A large nu exercises several *internal* viscosity substeps per call.
    driven.set_kinematic_viscosity(0.3);
    driven.step(0.04,180);
    verify(driven,2e-8);
    require(driven.last_viscous_diffusion_ledger().substeps>1,
            "high viscosity did not accumulate adaptive substeps");
    near(driven.mass_kg(),mass,1e-8,
         "adaptive viscous live step lost mass");
    // Reset occurs on the NEXT public step, not across successive calls.
    driven.set_kinematic_viscosity(0);
    driven.step(0.01,180);
    verify(driven,0);
    const auto reset=driven.last_viscous_diffusion_ledger();
    require(reset.substeps==0,"zero viscosity left old substeps");
    for(double v:reset.prescribed_wall_work_proxy_m2_s2)
        near(v,0,0,"zero viscosity left stale wall work");
    std::cout.precision(17);
    std::cout<<"PASS P2 live viscous energy ledger checks="<<checks
             <<" mass_kg="<<mass
             <<" wall_work="<<positive_wall_work
             <<" gradient_loss="<<positive_loss
             <<" viscosity_substeps="<<live_visc_substeps
             <<" phase_complete=false\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL P2 live energy ledger checks="<<checks
             <<" reason="<<e.what()<<"\n";
    return 1;
}
}
