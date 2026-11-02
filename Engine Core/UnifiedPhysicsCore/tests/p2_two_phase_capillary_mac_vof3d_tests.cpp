// P2 PARTIAL: moving two-phase MAC/VOF capillary CSF + real PCG + live
// conservative momentum/full tensor. NOT a PLIC or MARIN P2 full gate.
#include "upc/p2_two_phase_capillary_mac_vof3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace upc;
namespace {
std::size_t assertions=0;
void check(bool v,const char* msg){
    ++assertions;
    if(!v)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg){
    if(!(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol))
        std::cerr<<std::setprecision(17)<<"MISMATCH "<<msg
                 <<" actual="<<a<<" expected="<<b<<"\n";
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
template<class F>void reject(F f,const char* msg){
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    check(threw,msg);
}
P2TwoPhaseMacVofState3D make(int n,double dx,bool sphere){
    P2TwoPhaseMacVofState3D s;
    auto& m=s.mac;
    m.nx=m.ny=m.nz=n;m.dx_m=dx;m.dt_s=.0005;
    const auto N=std::size_t(n)*n*n;
    m.density_kg_m3.resize(N);
    m.u_m_s.assign((n+1)*n*n,0);
    m.v_m_s.assign(n*(n+1)*n,0);
    m.w_m_s.assign(n*n*(n+1),0);
    s.water_fraction.resize(N);
    const double center=n*dx*.5;
    const double radius=.31;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x){
        const auto i=(z*n+y)*n+x;
        const double rx=(x+.5)*dx-center;
        const double ry=(y+.5)*dx-center;
        const double rz=(z+.5)*dx-center;
        const double r=std::sqrt(rx*rx+ry*ry+rz*rz);
        const double alpha=sphere?
            std::max(0.,std::min(1.,.5+(radius-r)/(2*dx))):
            (x+.5)/double(n);
        s.water_fraction[i]=alpha;
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*alpha;
    }
    return s;
}
double face_max_difference(const P2TwoPhaseMacVofState3D& a,
                           const P2TwoPhaseMacVofState3D& b){
    double mx=0;
    for(int k=0;k<3;++k){
        const auto& aa=k==0?a.mac.u_m_s:(k==1?a.mac.v_m_s:a.mac.w_m_s);
        const auto& bb=k==0?b.mac.u_m_s:(k==1?b.mac.v_m_s:b.mac.w_m_s);
        check(aa.size()==bb.size(),"same face geometry");
        for(std::size_t i=0;i<aa.size();++i)
            mx=std::max(mx,std::abs(aa[i]-bb[i]));
    }
    return mx;
}
void planar(){
    auto s=make(7,.1,false);
    const auto before=s;
    const auto l=p2_two_phase_capillary_project3d(s,.072,4000);
    check(!l.full_P2_accepted&&!l.global_surface_energy_closed&&
          !l.sharp_plic_traction_validated,"no false full P2");
    check(l.active_capillary_faces==0,"linear planar VOF has zero curvature");
    near(l.max_abs_curvature_m_inv,0,1e-12,"planar curvature");
    near(l.max_source_velocity_change_m_s,0,1e-12,"planar force");
    near(l.max_post_projection_velocity_m_s,0,1e-12,"planar rest");
    near(face_max_difference(before,s),0,1e-12,"planar unchanged");
    check(s.water_fraction==before.water_fraction,"no capillary mass change");
    std::cout<<std::setprecision(17)<<"CAPILLARY_PLANAR grid=7x7x7"
             <<" curvature="<<l.max_abs_curvature_m_inv
             <<" velocity="<<l.max_post_projection_velocity_m_s
             <<" phase_complete=false\n";
}
void sphere(int n,double dx){
    auto s=make(n,dx,true);
    const auto before=s;
    const auto l=p2_two_phase_capillary_project3d(s,.072,4000);
    const double k=l.interface_curvature_sum_m_inv/
                   double(l.interface_curvature_cells);
    const double analytic=2/.31;
    check(l.interface_curvature_cells>10,"resolved 3D droplet interface");
    check(l.active_capillary_faces>10,"3D capillary traction is active");
    check(l.max_source_velocity_change_m_s>1e-8,"nonzero source");
    check(l.pressure.divergence_after_s_inv<1e-9,"post-CSF PCG divergence");
    check(std::isfinite(k)&&k>0,"spherical curvature sign");
    check(std::abs(k-analytic)<analytic*.75,
          "diffuse spherical curvature within documented coarse tolerance");
    check(s.water_fraction==before.water_fraction,"CSF does not advect VOF");
    check(s.mac.density_kg_m3==before.mac.density_kg_m3,
          "CSF does not change mass");
    // Independent pressure wall force and MAC impulse check.
    const auto& p=l.pressure.pressure_pa;
    const double area=dx*dx,dt=s.mac.dt_s;
    const auto ci=[&](int x,int y,int z){return (z*n+y)*n+x;};
    double walls[3]={};
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
        walls[0]+=dt*area*(p[ci(0,y,z)]-p[ci(n-1,y,z)]);
    for(int z=0;z<n;++z)for(int x=0;x<n;++x)
        walls[1]+=dt*area*(p[ci(x,0,z)]-p[ci(x,n-1,z)]);
    for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        walls[2]+=dt*area*(p[ci(x,y,0)]-p[ci(x,y,n-1)]);
    for(int axis=0;axis<3;++axis){
        near(walls[axis],l.pressure.wall_pressure_impulse_n_s[axis],
             1e-11,"independent pressure boundary quadrature");
        near(l.pressure.momentum_after_kg_m_s[axis]-
             l.pressure.momentum_before_kg_m_s[axis],walls[axis],
             1e-10,"pressure momentum vs wall force");
        near(l.pressure.momentum_after_kg_m_s[axis]-
             l.initial_momentum_kg_m_s[axis],
             l.source_impulse_kg_m_s[axis]+walls[axis],
             1e-10,"independent full CSF source plus pressure impulse");
        near(l.source_projection_impulse_residual_kg_m_s[axis],
             0,1e-10,"capillary+PCG impulse residual");
    }
    near(l.source_projection_energy_residual_j,0,1e-10,
         "CSF pressure energy identity");
    check(l.diffuse_surface_energy_proxy_j>0,
          "finite positive diffuse interface energy");
    std::cout<<std::setprecision(17)<<"CAPILLARY_SPHERE grid="
             <<n<<"x"<<n<<"x"<<n<<" curvature="<<k
             <<" analytic="<<analytic
             <<" active_faces="<<l.active_capillary_faces
             <<" source_velocity="<<l.max_source_velocity_change_m_s
             <<" spurious_velocity="<<l.max_post_projection_velocity_m_s
             <<" energy_proxy_j="<<l.diffuse_surface_energy_proxy_j
             <<" phase_complete=false\n";
}
void live(){
    auto s=make(6,.15,true);
    auto baseline=s;
    const auto initial=s;
    const auto l=p2_two_phase_capillary_momentum_viscous_vof_step3d(
        s,.072,4000,{0,0,-.1},.001,.000018);
    const auto b=p2_two_phase_capillary_momentum_viscous_vof_step3d(
        baseline,0,4000,{0,0,-.1},.001,.000018);
    check(l.capillary_applied&&!b.capillary_applied,
          "opt-in capillary only");
    check(!l.full_P2_accepted&&!l.live.full_P2_accepted,
          "no live false full acceptance");
    const double effect=face_max_difference(s,baseline);
    check(effect>1e-10,"capillary changes real live 3D MAC/VOF");
    check(l.live.last_divergence_s_inv<1e-9,"live final PCG");
    near(l.live.water_mass_residual_kg,0,1e-8,"live water conserved");
    near(l.live.air_mass_residual_kg,0,1e-8,"live air conserved");
    check(s.water_fraction!=initial.water_fraction,
          "moving VOF after capillary and momentum");
    std::cout<<std::setprecision(17)<<"CAPILLARY_LIVE grid=6x6x6"
             <<" effect="<<effect
             <<" divergence="<<l.live.last_divergence_s_inv
             <<" phase_complete=false\n";
}
void negatives(){
    auto base=make(6,.15,true);
    auto s=base;
    reject([&]{p2_two_phase_capillary_project3d(s,-.1);},
           "negative sigma rejected");
    check(s.water_fraction==base.water_fraction &&
          s.mac.u_m_s==base.mac.u_m_s,"invalid sigma transactional");
    reject([&]{p2_two_phase_capillary_project3d(s,
           std::numeric_limits<double>::quiet_NaN());},
           "NaN sigma rejected");
    s=base;s.mac.density_kg_m3[0]*=1.1;
    const auto corrupt=s;
    reject([&]{p2_two_phase_capillary_project3d(s,.072);},
           "inconsistent density rejected");
    check(s.mac.density_kg_m3==corrupt.mac.density_kg_m3,
          "invalid density transactional");
    s=base;s.mac.dt_s=100;
    reject([&]{p2_two_phase_capillary_project3d(s,.072);},
           "unstable capillary timestep rejected");
    check(s.mac.u_m_s==base.mac.u_m_s,"unstable step transactional");
    s=base;s.mac.u_m_s[0]=.01;
    reject([&]{p2_two_phase_capillary_project3d(s,.072);},
           "permeable wall rejected");
    near(s.mac.u_m_s[0],.01,0,"invalid wall transactional");
    s=base;
    reject([&]{p2_two_phase_capillary_momentum_viscous_vof_step3d(
        s,.072,1,{0,0,-.1});},"underconverged live PCG rejected");
    check(s.mac.u_m_s==base.mac.u_m_s&&
          s.water_fraction==base.water_fraction,
          "failed live PCG rollback");
}
}
int main(){
    try{
        planar();
        sphere(8,.15);
        sphere(12,.1);
        live();
        negatives();
        std::cout<<"PASS P2_TWO_PHASE_CAPILLARY assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_TWO_PHASE_CAPILLARY: "<<e.what()<<"\n";
        return 1;
    }
}
