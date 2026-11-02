#include "upc/p2_two_phase_mac_vof3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>
using namespace upc;
namespace {
int assertions=0;
void check(bool ok,const char* what){++assertions;if(!ok)throw std::runtime_error(what);}
void near(double x,double y,double tol,const char* what){
    check(std::isfinite(x)&&std::isfinite(y)&&std::abs(x-y)<=tol,what);
}
std::size_t ci(int x,int y,int z,int nx,int ny){return (z*ny+y)*nx+x;}
std::size_t ui(int x,int y,int z,int nx,int ny){return (z*ny+y)*(nx+1)+x;}
std::size_t vi(int x,int y,int z,int nx,int ny){return (z*(ny+1)+y)*nx+x;}
std::size_t wi(int x,int y,int z,int nx,int ny){return (z*ny+y)*nx+x;}
P2TwoPhaseMacVofState3D make(int nx,int ny,int nz,bool tilted){
    P2TwoPhaseMacVofState3D s;
    auto& m=s.mac;m.nx=nx;m.ny=ny;m.nz=nz;m.dx_m=.1;m.dt_s=.005;
    s.water_fraction.resize(nx*ny*nz);m.density_kg_m3.resize(nx*ny*nz);
    m.u_m_s.assign((nx+1)*ny*nz,0);
    m.v_m_s.assign(nx*(ny+1)*nz,0);
    m.w_m_s.assign(nx*ny*(nz+1),0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
        const double alpha=z<(nz/2+(tilted&&(x<nx/2)?1:0))?.90:.05;
        auto i=ci(x,y,z,nx,ny);s.water_fraction[i]=alpha;
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*alpha;
    }
    return s;
}
double mass(const P2TwoPhaseMacVofState3D& s){
    return std::accumulate(s.water_fraction.begin(),s.water_fraction.end(),0.)*
        s.mac.dx_m*s.mac.dx_m*s.mac.dx_m*s.water_density_kg_m3;
}
void hydrostatic(int nx,int ny,int nz){
    auto s=make(nx,ny,nz,false);
    const auto initial=s.water_fraction;
    const double g=-9.81,dx=s.mac.dx_m,dt=s.mac.dt_s;
    double max_dp=0,max_velocity=0,max_residual=0;
    for(int step=0;step<4;++step){
        auto l=p2_two_phase_mac_vof_step3d(s,4000,{0,0,g});
        check(l.pressure.pcg_iterations>0,"gravity projection PCG missing");
        for(int z=1;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
            const double rho=.5*(s.mac.density_kg_m3[ci(x,y,z,nx,ny)]+
                s.mac.density_kg_m3[ci(x,y,z-1,nx,ny)]);
            const double dp=l.pressure.pressure_pa[ci(x,y,z,nx,ny)]-
                l.pressure.pressure_pa[ci(x,y,z-1,nx,ny)];
            max_dp=std::max(max_dp,std::abs(dp-rho*g*dx));
        }
        for(double v:s.mac.u_m_s)max_velocity=std::max(max_velocity,std::abs(v));
        for(double v:s.mac.v_m_s)max_velocity=std::max(max_velocity,std::abs(v));
        for(double v:s.mac.w_m_s)max_velocity=std::max(max_velocity,std::abs(v));
        for(std::size_t i=0;i<initial.size();++i)
            near(s.water_fraction[i],initial[i],2e-9,"hydrostatic interface moved");
        near(l.water_mass_before_kg,l.water_mass_after_kg,1e-9,"hydrostatic mass");
        near(l.gravity_source_projection_energy_residual_j,0,1e-8,"gravity energy ledger");
        for(int k=0;k<3;++k)
            near(l.gravity_source_projection_momentum_residual_kg_m_s[k],0,1e-8,
                "gravity pressure impulse ledger");
        check(l.gravity_source_work_j>=0,"gravity source work should be nonnegative");
        check(l.gravity_source_impulse_kg_m_s[2]<0,"gravity impulse sign");
        max_residual=std::max(max_residual,
            std::abs(l.gravity_source_projection_energy_residual_j));
    }
    check(max_dp<1e-7,"independent hydrostatic layer pressure");
    check(max_velocity<1e-7,"hydrostatic equilibrium failed");
    std::cout<<std::setprecision(17)<<"GRAVITY_HYDRO grid="<<nx<<"x"<<ny<<"x"<<nz
        <<" pressure_error_pa="<<max_dp<<" velocity_error_m_s="<<max_velocity
        <<" energy_residual_j="<<max_residual<<" steps=4\n";
}
void baroclinic(int nx,int ny,int nz){
    auto s=make(nx,ny,nz,true);
    const double original=mass(s);
    const auto start=s.water_fraction;
    double peak_u=0,peak_w=0,peak_div=0,peak_mass=0,peak_energy=0;
    for(int step=0;step<8;++step){
        auto l=p2_two_phase_mac_vof_step3d(s,4000,{0,0,-9.81});
        for(double v:s.mac.u_m_s)peak_u=std::max(peak_u,std::abs(v));
        for(double v:s.mac.w_m_s)peak_w=std::max(peak_w,std::abs(v));
        peak_div=std::max(peak_div,l.pressure.divergence_after_s_inv);
        peak_mass=std::max(peak_mass,std::abs(mass(s)-original));
        peak_energy=std::max(peak_energy,
            std::abs(l.gravity_source_projection_energy_residual_j));
        near(l.water_mass_before_kg,l.water_mass_after_kg,1e-9,"moving gravity mass");
        near(l.gravity_source_projection_energy_residual_j,0,1e-8,
             "moving gravity discrete energy ledger");
        for(int k=0;k<3;++k)
            near(l.gravity_source_projection_momentum_residual_kg_m_s[k],0,1e-8,
                 "moving gravity pressure impulse ledger");
        check(l.vof.max_outgoing_courant<1,"moving gravity CFL");
    }
    double moved=0;
    for(std::size_t i=0;i<start.size();++i)
        moved=std::max(moved,std::abs(start[i]-s.water_fraction[i]));
    check(peak_u>1e-5&&peak_w>1e-5&&moved>1e-6,
          "tilted interface did not generate 3D buoyancy circulation");
    check(peak_div<1e-9&&peak_mass<1e-8&&peak_energy<1e-8,
          "moving gravity divergence/mass/energy failure");
    std::cout<<std::setprecision(17)<<"GRAVITY_TILTED grid="<<nx<<"x"<<ny<<"x"<<nz
        <<" u_m_s="<<peak_u<<" w_m_s="<<peak_w<<" alpha_delta="<<moved
        <<" div_s_inv="<<peak_div<<" mass_error_kg="<<peak_mass
        <<" energy_residual_j="<<peak_energy<<" steps=8\n";
}
void negatives(){
    auto s=make(6,5,4,false);
    const auto before=s;
    bool threw=false;
    try{p2_two_phase_mac_vof_step3d(s,4000,{0,0,std::numeric_limits<double>::quiet_NaN()});}
    catch(const std::exception&){threw=true;}
    check(threw&&s.water_fraction==before.water_fraction&&
          s.mac.w_m_s==before.mac.w_m_s,"nonfinite gravity mutated state");
    threw=false;
    auto tilted=make(6,5,4,true);const auto tilted_before=tilted;
    try{p2_two_phase_mac_vof_step3d(tilted,4000,{0,0,1e7});}
    catch(const std::exception&){threw=true;}
    check(threw&&tilted.water_fraction==tilted_before.water_fraction&&
          tilted.mac.w_m_s==tilted_before.mac.w_m_s,"excess gravity CFL did not rollback");
    const auto zero=p2_two_phase_mac_vof_step3d(s,4000,{0,0,0});
    near(zero.gravity_source_work_j,0,0,"zero gravity source work");
    for(double v:zero.gravity_source_impulse_kg_m_s)
        near(v,0,0,"zero gravity impulse");
}
}
int main(){
    try{
        hydrostatic(4,4,4);hydrostatic(6,5,4);hydrostatic(8,6,5);
        baroclinic(6,5,4);baroclinic(8,6,5);
        negatives();
        std::cout<<"PASS: GRAVITY_TWO_PHASE_VOF assertions="<<assertions<<"\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
