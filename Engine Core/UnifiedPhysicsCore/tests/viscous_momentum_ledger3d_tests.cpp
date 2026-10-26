// P2 bounded partial: masked-MAC viscous face-momentum proxy ledger.
// Not a continuum wall force, moving-body FSI or validated stress tensor.
#include "upc/coupled_water3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace upc;
static int checks=0;
static void check(bool pass,const char* why){
    ++checks;
    if(!pass)throw std::runtime_error(why);
}
static void near(double actual,double expected,double tol,const char* why){
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,why);
}
static void closed(const MaskedMacMomentumLedger3D& l,double tol){
    for(std::size_t a=0;a<3;++a){
        near(l.active_face_delta_sum_m_s[a],
             l.solid_boundary_source_sum_m_s[a]+
             l.exterior_boundary_source_sum_m_s[a],tol,
             "masked face-velocity ledger not closed");
        near(l.closure_residual_sum_m_s[a],0,tol,
             "masked ledger residual nonzero");
    }
}
int main(){
try {
    constexpr std::size_t nx=10,ny=10,nz=10;
    constexpr double dx=0.1,nu=0.012,dt=0.03;
    constexpr double alpha=nu*dt/(dx*dx);
    std::vector<std::uint8_t> wet(nx*ny*nz,1),solid(nx*ny*nz,0);
    const auto idx=[=](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;
    };
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<4;++x){
        wet[idx(x,y,z)]=0;
        solid[idx(x,y,z)]=1;
    }
    ViscousMacDiffusion3D diffusion(nx,ny,nz,dx);
    MacProjection3D mac(nx,ny,nz,dx);
    MaskedMacMomentumLedger3D ledger;
    const std::array<double,3> prescribed{0.7,0.4,0.0};
    check(diffusion.step_masked(mac,dt,nu,wet,solid,prescribed,&ledger)==1,
          "Couette ledger wrong substep count");
    check(ledger.substeps==1,"ledger did not record step");
    // Exactly 10 z-slices * 9 interior y-faces touch the x=4 wall.
    // Each receives alpha * 0.4 from the fixed solid tangential ghost.
    const double independent_wall_source=90.0*alpha*prescribed[1];
    near(mac.face_velocity_y(4,5,5),alpha*prescribed[1],1e-13,
         "Couette discrete y-face mismatch");
    near(ledger.solid_boundary_source_sum_m_s[1],
         independent_wall_source,1e-12,
         "solid wall face-momentum source differs from stencil count");
    near(ledger.active_face_delta_sum_m_s[1],
         independent_wall_source,1e-12,
         "active fluid face-velocity change differs from wall source");
    near(ledger.solid_boundary_source_sum_m_s[0],0,1e-14,
         "normal solid wall spuriously injected x velocity");
    near(ledger.solid_boundary_source_sum_m_s[2],0,1e-14,
         "solid wall spuriously injected z velocity");
    closed(ledger,1e-12);
    // Negative wall velocity reverses the source, not its magnitude.
    MacProjection3D reversed(nx,ny,nz,dx);
    MaskedMacMomentumLedger3D negative;
    diffusion.step_masked(reversed,dt,nu,wet,solid,{0,-0.4,0},&negative);
    near(negative.solid_boundary_source_sum_m_s[1],
         -independent_wall_source,1e-12,"wall source sign not reversible");
    closed(negative,1e-12);
    // Several internal diffusion substeps with nonzero pre-existing fluid
    // velocity test active-to-active cancellation and exterior losses.
    MacProjection3D multistep(nx,ny,nz,dx);
    multistep.set_face_velocity_y(6,5,5,1.2);
    MaskedMacMomentumLedger3D multi;
    const auto n=diffusion.step_masked(multistep,0.12,0.1,wet,solid,
                                      {0,0.3,0},&multi);
    check(n>=8&&multi.substeps==n,"adaptive ledger substeps not counted");
    closed(multi,1e-10);
    check(multi.solid_boundary_source_sum_m_s[1]>0,
          "prescribed wall did not inject momentum proxy");
    // nu=0 is exactly no-op, even when a caller reuses the ledger.
    const double before=multistep.face_velocity_y(6,5,5);
    check(diffusion.step_masked(multistep,dt,0,wet,solid,
                                {0,0.3,0},&multi)==0,
          "zero nu performed diffusion");
    near(multistep.face_velocity_y(6,5,5),before,0,
         "zero nu changed velocity");
    check(multi.substeps==0,"zero nu left stale ledger");
    closed(multi,0);
    // Validation failure must not modify a previous caller ledger.
    MaskedMacMomentumLedger3D invalid;
    invalid.substeps=99;
    bool rejected=false;
    try {
        diffusion.step_masked(multistep,dt,nu,wet,solid,
                              {0,std::numeric_limits<double>::quiet_NaN(),0},
                              &invalid);
    } catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&invalid.substeps==99,
          "invalid wall speed altered ledger or was accepted");

    CoupledMacWater3D live(8,8,6,dx);
    for(std::size_t z=0;z<6;++z)
    for(std::size_t y=0;y<8;++y)
    for(std::size_t x=0;x<8;++x){
        if(x==2)live.set_solid(x,y,z);
        if(x>=3&&x<=5&&y>=1&&y<=3&&z<4)
            live.set_water(x,y,z,1);
    }
    live.set_kinematic_viscosity(0.035);
    live.set_wall_tangential_velocity({0,0.75,0});
    const double mass=live.mass_kg();
    live.step(0.02,180);
    near(live.mass_kg(),mass,1e-8,"live ledger changed VOF mass");
    check(live.last_viscosity_substeps()>0,"live ledger not invoked");
    const auto fluid=live.last_viscous_face_momentum_delta_proxy_kg_m_s();
    const auto solid_source=live.last_viscous_solid_wall_source_proxy_kg_m_s();
    const auto exterior=live.last_viscous_exterior_source_proxy_kg_m_s();
    const auto residual=live.last_viscous_closure_residual_proxy_kg_m_s();
    check(solid_source[1]>0,"live wall shear momentum source absent");
    for(std::size_t a=0;a<3;++a){
        near(fluid[a],solid_source[a]+exterior[a],1e-8,
             "live momentum proxy failed wall/exterior closure");
        near(residual[a],0,1e-8,"live closure residual nonzero");
    }
    live.set_kinematic_viscosity(0);
    live.step(0.02,180);
    for(double v:live.last_viscous_face_momentum_delta_proxy_kg_m_s())
        near(v,0,0,"inviscid step retained momentum ledger");
    for(double v:live.last_viscous_solid_wall_source_proxy_kg_m_s())
        near(v,0,0,"inviscid step retained solid wall ledger");
    std::cout.precision(17);
    std::cout<<"PASS P2 masked MAC face-momentum proxy ledger checks="<<checks
             <<" discrete_wall_source_m_s="<<independent_wall_source
             <<" adaptive_substeps="<<n
             <<" live_mass_kg="<<mass
             <<" live_wall_y_source_proxy_kg_m_s="<<solid_source[1]
             <<" live_y_closure_residual_proxy_kg_m_s="<<residual[1]
             <<" phase_complete=false\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL P2 face-momentum proxy after "<<checks<<": "<<e.what()<<"\n";
    return 1;
}
}
