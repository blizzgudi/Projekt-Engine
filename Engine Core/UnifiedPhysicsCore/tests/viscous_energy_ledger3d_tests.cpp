// P2 bounded numerical verification: exact explicit MAC face-square energy ledger.
// NOT continuum energy, stress, or fluid-solid impulse accounting.
#include "upc/viscous_mac3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace upc;
static int checks=0;
static void require(bool yes,const char* why){
    ++checks;
    if(!yes)throw std::runtime_error(why);
}
static void near(double actual,double expected,double tol,const char* why){
    require(std::isfinite(actual)&&std::abs(actual-expected)<=tol,why);
}
static void closure(const MaskedMacMomentumLedger3D& l,double tol){
    for(std::size_t a=0;a<3;++a){
        const double predicted=-l.internal_gradient_loss_proxy_m2_s2[a]
            -l.solid_gradient_loss_proxy_m2_s2[a]
            -l.exterior_gradient_loss_proxy_m2_s2[a]
            +l.prescribed_wall_work_proxy_m2_s2[a]
            +l.explicit_euler_correction_proxy_m2_s2[a];
        near(l.face_energy_change_proxy_m2_s2[a],predicted,tol,
             "discrete face-square energy budget failed");
        near(l.energy_closure_residual_proxy_m2_s2[a],0,tol,
             "discrete face-square energy residual failed");
        require(l.internal_gradient_loss_proxy_m2_s2[a]>=0 &&
                l.solid_gradient_loss_proxy_m2_s2[a]>=0 &&
                l.exterior_gradient_loss_proxy_m2_s2[a]>=0 &&
                l.explicit_euler_correction_proxy_m2_s2[a]>=0,
                "negative diffusion loss or Euler correction");
    }
}
int main(){
try{
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
    MacProjection3D driven(nx,ny,nz,dx);
    driven.set_face_velocity_y(4,5,5,0.25);
    const double energy_before=diffusion.face_energy_proxy(driven);
    MaskedMacMomentumLedger3D l;
    require(diffusion.step_masked(driven,dt,nu,wet,solid,{0,0.4,0},&l)==1,
            "wrong explicit substeps");
    require(l.substeps==1,"substep count not captured");
    // Independent six-neighbor stencil: one solid-wall ghost at 0.4,
    // five active initially zero neighbors. All are far from domain edges.
    near(driven.face_velocity_y(4,5,5),0.25+alpha*(0.4-6*0.25),
         1e-13,"independent center stencil");
    near(driven.face_velocity_y(5,5,5),alpha*0.25,1e-13,
         "interior x+ neighbor stencil");
    // Four wall-adjacent neighbors receive BOTH the independent 0.25
    // interior pulse and the prescribed 0.4 wall ghost.
    for(const auto& pos:std::array<std::array<std::size_t,3>,4>{{
        {4,4,5},{4,6,5},{4,5,4},{4,5,6}}}){
        near(driven.face_velocity_y(pos[0],pos[1],pos[2]),
             alpha*(0.25+0.4),1e-13,
             "wall-adjacent neighbor stencil");
    }
    near(driven.face_velocity_y(4,1,1),alpha*0.4,1e-13,
         "unperturbed wall shear stencil");
    const double independent_gradient=5*alpha*0.25*0.25;
    const double independent_solid_loss=alpha*0.25*0.25;
    const double independent_wall_work=alpha*0.25*0.4;
    // Exactly 90 wall-adjacent y faces: center, 4 near-pulse
    // neighbors, 85 other wall faces; plus one interior x+ neighbor.
    const double independent_euler=0.5*(
        std::pow(alpha*(0.4-6*0.25),2)
        +4*std::pow(alpha*(0.4+0.25),2)
        +85*std::pow(alpha*0.4,2)
        +std::pow(alpha*0.25,2));
    const double independent_delta= -independent_gradient-
        independent_solid_loss+independent_wall_work+independent_euler;
    near(l.internal_gradient_loss_proxy_m2_s2[1],independent_gradient,
         1e-13,"five active pairs loss");
    near(l.solid_gradient_loss_proxy_m2_s2[1],independent_solid_loss,
         1e-13,"single solid boundary loss");
    near(l.prescribed_wall_work_proxy_m2_s2[1],independent_wall_work,
         1e-13,"single prescribed wall work");
    near(l.explicit_euler_correction_proxy_m2_s2[1],independent_euler,
         1e-13,"explicit Euler correction");
    near(l.face_energy_change_proxy_m2_s2[1],independent_delta,
         1e-13,"independent discrete energy delta");
    near((diffusion.face_energy_proxy(driven)-energy_before)/(dx*dx*dx),
         independent_delta,1e-13,"energy proxy not tied to actual face squares");
    closure(l,1e-12);

    // No prescribed wall motion: diffusion of the same initial impulse
    // must dissipate energy, with exactly zero wall-work term.
    MacProjection3D stationary(nx,ny,nz,dx);
    stationary.set_face_velocity_y(4,5,5,0.25);
    MaskedMacMomentumLedger3D stationary_ledger;
    diffusion.step_masked(stationary,dt,nu,wet,solid,{0,0,0},&stationary_ledger);
    near(stationary_ledger.prescribed_wall_work_proxy_m2_s2[1],0,0,
         "stationary wall generated work");
    require(stationary_ledger.face_energy_change_proxy_m2_s2[1]<0,
            "stationary-wall diffusion increased face energy");
    closure(stationary_ledger,1e-12);

    // Multi-substep nonuniform 3D state, signed wall work and free-surface
    // Neumann proxies; no independent continuum FSI claims.
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=7;x<nx;++x)wet[idx(x,y,z)]=0;
    MacProjection3D adaptive(nx,ny,nz,dx);
    adaptive.set_face_velocity_y(4,5,5,-0.25);
    adaptive.set_face_velocity_z(5,5,5,0.4);
    MaskedMacMomentumLedger3D many;
    const double before_many=diffusion.face_energy_proxy(adaptive);
    const auto steps=diffusion.step_masked(adaptive,0.12,0.1,wet,solid,
                                           {0,-0.3,0.1},&many);
    require(steps>=8&&many.substeps==steps,"adaptive substep ledger failed");
    closure(many,1e-10);
    double reported=0;
    for(double delta:many.face_energy_change_proxy_m2_s2)reported+=delta;
    near((diffusion.face_energy_proxy(adaptive)-before_many)/(dx*dx*dx),
         reported,1e-10,"adaptive total face-square change mismatch");
    require(many.prescribed_wall_work_proxy_m2_s2[1]>0,
            "signed wall-work term missing");

    // A zero-viscosity call must leave the MAC field unchanged and clear
    // any previous caller-owned ledger, not invent dissipation.
    const double saved=adaptive.face_velocity_y(4,5,5);
    require(diffusion.step_masked(adaptive,dt,0,wet,solid,
                                  {0,-0.3,0.1},&many)==0,
            "inviscid path ran viscosity");
    near(adaptive.face_velocity_y(4,5,5),saved,0,"inviscid path changed field");
    closure(many,0);
    require(many.substeps==0,"inviscid path left stale substeps");
    for(double v:many.face_energy_change_proxy_m2_s2)
        near(v,0,0,"inviscid path left stale energy ledger");

    // Invalid inputs must be rejected BEFORE clearing a previous ledger.
    many.substeps=99;
    bool rejected=false;
    try{
        diffusion.step_masked(adaptive,dt,nu,wet,solid,
            {0,std::numeric_limits<double>::quiet_NaN(),0},&many);
    }catch(const std::invalid_argument&){rejected=true;}
    require(rejected&&many.substeps==99,"invalid wall velocity mutated ledger");
    std::cout.precision(17);
    std::cout<<"PASS P2 discrete masked MAC energy ledger checks="<<checks
             <<" internal_loss="<<independent_gradient
             <<" solid_loss="<<independent_solid_loss
             <<" wall_work="<<independent_wall_work
             <<" euler_correction="<<independent_euler
             <<" energy_delta="<<independent_delta
             <<" adaptive_substeps="<<steps
             <<" phase_complete=false\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL P2 energy ledger after checks="<<checks<<": "<<e.what()<<"\n";
    return 1;
}
}
