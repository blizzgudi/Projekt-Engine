// P2 optional masked 3D MAC viscosity reference: stationary solids,
// free-surface air faces, finite/energy/axis and explicit CFL invariants.
// Does not imply live CoupledMacWater3D integration or full P2 closure.
#include "upc/viscous_mac3d.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace upc;
static std::size_t checks=0;
static void require(bool yes,const char* reason){
 ++checks;if(!yes)throw std::runtime_error(reason);
}
static void near(double a,double b,double tol,const char* reason){
 require(std::isfinite(a)&&std::abs(a-b)<=tol,reason);
}
int main(){
try{
 constexpr std::size_t nx=10,ny=10,nz=10;
 constexpr double dx=0.1,nu=0.012,dt=0.03;
 auto cell=[&](std::size_t x,std::size_t y,std::size_t z){
   return (z*ny+y)*nx+x;
 };
 ViscousMacDiffusion3D visc(nx,ny,nz,dx);
 std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0);
 // Central wet droplet; isolated air-only faces should be cleared.
 for(std::size_t z=3;z<7;++z)
 for(std::size_t y=3;y<7;++y)
 for(std::size_t x=3;x<7;++x)wet[cell(x,y,z)]=1;
 solid[cell(5,5,5)]=1;wet[cell(5,5,5)]=0;
 MacProjection3D mac(nx,ny,nz,dx);
 mac.set_face_velocity_x(4,4,4,1.0);
 mac.set_face_velocity_y(4,4,4,0.6);
 mac.set_face_velocity_z(4,4,4,0.4);
 mac.set_face_velocity_x(5,5,5,2.0); // blocked solid face must become zero
 mac.set_face_velocity_x(1,1,1,9.0); // air-only face must become zero
 mac.set_face_velocity_y(0,0,0,3.0); // domain wall must become zero
 require(visc.step_masked(mac,dt,nu,wet,solid)==1,"masked stable CFL");
 near(mac.face_velocity_x(5,5,5),0,0,"solid normal velocity not zero");
 near(mac.face_velocity_x(1,1,1),0,0,"air-only face retained ghost flow");
 near(mac.face_velocity_y(0,0,0),0,0,"domain wall retained ghost flow");
 require(mac.face_velocity_x(4,4,4)>0&&mac.face_velocity_x(4,4,4)<1,
         "wet fluid failed to diffuse");
 const auto first=visc.face_energy_proxy(mac);
 double prev=first;
 for(int step=0;step<30;++step){
    visc.step_masked(mac,dt,nu,wet,solid);
    const double now=visc.face_energy_proxy(mac);
    require(now>=0&&now<=prev+1e-12,"masked viscosity energy grew");
    prev=now;
 }
 require(prev<first,"masked viscosity not dissipative");
 // Independent analytic one-step isolated interface stencil:
 // central active face, 1 active zero neighbor, 5 air-only neighbors.
 std::fill(wet.begin(),wet.end(),0);
 std::fill(solid.begin(),solid.end(),0);
 wet[cell(4,4,4)]=1;
 MacProjection3D free_surface(nx,ny,nz,dx);
 free_surface.set_face_velocity_x(5,4,4,1.0);
 visc.step_masked(free_surface,dt,nu,wet,solid);
 const double alpha=nu*dt/(dx*dx);
 // x- neighbor active but initially zero, x+ neighbor air-only,
 // other four neighbors air-only -> exact v = 1-alpha.
 near(free_surface.face_velocity_x(5,4,4),1-alpha,1e-13,
      "free-surface zero-gradient stencil wrong");
 near(free_surface.face_velocity_x(6,4,4),0,0,
      "air-only face did not stay zero");
 // A fully wet uniform interior mode must have no internal gradients.
 std::fill(wet.begin(),wet.end(),1);
 MacProjection3D uniform(nx,ny,nz,dx);
 for(std::size_t z=0;z<nz;++z)
 for(std::size_t y=0;y<ny;++y)
 for(std::size_t x=1;x<nx;++x)uniform.set_face_velocity_x(x,y,z,1.0);
 visc.step_masked(uniform,dt,nu,wet,solid);
 near(uniform.face_velocity_x(5,5,5),1.0,1e-13,
      "uniform wet bulk velocity diffused");
 near(uniform.face_velocity_x(0,5,5),0,0,"no-slip wall x=0");
 // Independent Couette-style one-step shear injection from a fixed
 // impermeable solid wall with prescribed tangential velocity (not FSI).
 // Solid x<4, wet x>=4; interior y-face (4,5,5) has exactly one
 // solid x-neighbor, so explicit Laplacian gives alpha*wall_velocity_y.
 std::fill(wet.begin(),wet.end(),1);
 std::fill(solid.begin(),solid.end(),0);
 for(std::size_t z=0;z<nz;++z)
 for(std::size_t y=0;y<ny;++y)
 for(std::size_t x=0;x<4;++x) {
     wet[cell(x,y,z)]=0;solid[cell(x,y,z)]=1;
 }
 MacProjection3D moving_wall(nx,ny,nz,dx),stationary_wall(nx,ny,nz,dx);
 // x component is NORMAL to this wall and must not leak into x-faces.
 const std::array<double,3> prescribed_tangent{0.7,0.4,0.0};
 require(visc.step_masked(moving_wall,dt,nu,wet,solid,prescribed_tangent)==1,
         "fixed moving-wall shear step count");
 require(visc.step_masked(stationary_wall,dt,nu,wet,solid)==1,
         "stationary wall step count");
 near(moving_wall.face_velocity_y(4,5,5),alpha*prescribed_tangent[1],1e-13,
      "prescribed tangential wall shear stencil mismatch");
 near(stationary_wall.face_velocity_y(4,5,5),0,0,
      "stationary wall injected tangential momentum");
 near(moving_wall.face_velocity_y(3,5,5),0,0,
      "solid face retained tangential velocity");
 near(moving_wall.face_velocity_x(4,5,5),0,0,
      "wall normal face leaked velocity");
 near(moving_wall.face_velocity_x(5,5,5),0,0,
      "normal wall component leaked into adjacent liquid face");
 near(moving_wall.face_velocity_y(5,5,5),0,0,
      "one-step explicit shear reached nonadjacent face");
 bool invalid_wall=false;
 try {
     visc.step_masked(moving_wall,dt,nu,wet,solid,
         {0.0,std::numeric_limits<double>::quiet_NaN(),0.0});
 } catch(const std::invalid_argument&) {invalid_wall=true;}
 require(invalid_wall,"nonfinite wall velocity accepted");
 near(moving_wall.face_velocity_y(4,5,5),alpha*prescribed_tangent[1],1e-13,
      "invalid wall input modified prior fluid state");
 // Exact no-op for nu=0, including arbitrary user-provided face values.
 MacProjection3D inviscid(nx,ny,nz,dx);
 inviscid.set_face_velocity_x(5,5,5,7.0);
 require(visc.step_masked(inviscid,dt,0,wet,solid)==0,
         "zero nu changed number of steps");
 near(inviscid.face_velocity_x(5,5,5),7.0,0,"zero nu altered face");
 bool bad=false;
 try{visc.step_masked(inviscid,dt,nu,std::vector<std::uint8_t>{},solid);}
 catch(const std::invalid_argument&){bad=true;}
 require(bad,"invalid wet mask accepted");
 bad=false;wet[cell(5,5,5)]=1;solid[cell(5,5,5)]=1;
 try{visc.step_masked(inviscid,dt,nu,wet,solid);}
 catch(const std::invalid_argument&){bad=true;}
 require(bad,"wet/solid overlap accepted");
 bad=false;
 try{visc.step_masked(inviscid,dt,-1,wet,solid);}
 catch(const std::invalid_argument&){bad=true;}
 require(bad,"negative viscosity accepted");
 // Large dt requires adaptive substeps and remains finite.
 std::fill(solid.begin(),solid.end(),0);
 MacProjection3D big(nx,ny,nz,dx);
 big.set_face_velocity_x(5,5,5,1.0);
 const auto steps=visc.step_masked(big,0.2,0.5,wet,solid);
 require(steps>50,"masked large dt was not split");
 require(std::isfinite(visc.face_energy_proxy(big)),"masked CFL nonfinite");
 std::cout.precision(15);
 std::cout<<"PASS P2 masked 3D MAC viscosity: checks="<<checks
   <<" free_surface_one_step="<<free_surface.face_velocity_x(5,4,4)
   <<" expected="<<1-alpha
   <<" energy_first="<<first<<" energy_after="<<prev
   <<" adaptive_substeps="<<steps
   <<" wall_shear="<<moving_wall.face_velocity_y(4,5,5)
   <<" wall_shear_expected="<<alpha*prescribed_tangent[1]<<"\n";
 return 0;
}catch(const std::exception& e){
 std::cerr<<"FAIL P2 masked 3D viscosity after "<<checks<<": "<<e.what()<<"\n";
 return 1;
}
}
