// P2 PARTIAL: simultaneous SPD global 3D water/air tangential shear.
#include "upc/p2_two_phase_global_shear3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
int assertions=0;
void check(bool ok,const char* message) {
    ++assertions;if(!ok)throw std::runtime_error(message);
}
void near(double a,double b,double tol,const char* message) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,message);
}
template<class F>void rejects(F f) {
    bool threw=false;try{f();}catch(const std::invalid_argument&){threw=true;}
    check(threw,"Invalid global shear input accepted");
}
using upc::P2TwoPhaseGridShear3D;
using upc::p2_two_phase_global_shear_step3d;
P2TwoPhaseGridShear3D planar() {
    P2TwoPhaseGridShear3D g;
    g.nx=4;g.ny=4;g.nz=4;g.dx_m=.1;g.dt_s=.05;
    g.water_density_kg_m3=1000;g.air_density_kg_m3=1.2;
    g.water_viscosity_pa_s=1;g.air_viscosity_pa_s=.1;
    for(int z=0;z<4;++z)for(int y=0;y<4;++y)for(int x=0;x<4;++x) {
        const bool wet=x<2;
        g.phase.push_back(wet?1:0);
        g.velocity_m_s.push_back({0,wet?0.0:1.0,0});
    }
    return g;
}
void planar_reference() {
    auto g=planar();
    const auto l=p2_two_phase_global_shear_step3d(g);
    const double mw=1000*.001,ma=1.2*.001;
    const double b=.05*.01/(.05/1+.05/.1);
    const double slip=1/(1+b*(1/mw+1/ma));
    const double impulse=b*slip;
    check(l.interface_faces==16&&l.axis_faces[0]==16&&
          l.axis_faces[1]==0&&l.axis_faces[2]==0,"Planar interfaces");
    near(l.water_mass_kg,32,1e-12,"Water mass");
    near(l.air_mass_kg,32*ma,1e-12,"Air mass");
    for(int z=0;z<4;++z)for(int y=0;y<4;++y) {
        const std::size_t wi=(std::size_t(z)*4+y)*4+1,ai=wi+1;
        near(g.velocity_m_s[wi][1],impulse/mw,1e-13,"Exact Couette water");
        near(g.velocity_m_s[ai][1],1-impulse/ma,1e-13,"Exact Couette air");
        near(g.velocity_m_s[wi][0],0,1e-14,"Normal water velocity");
        near(g.velocity_m_s[ai][0],0,1e-14,"Normal air velocity");
    }
    for(double r:l.momentum_residual_n_s)near(r,0,1e-12,"Planar momentum");
    near(l.energy_residual_j,0,1e-12,"Planar energy");
    check(l.kinetic_after_j<l.kinetic_before_j,"Planar energy not dissipated");
    check(l.pcg_iterations[1]>0,"PCG did not solve tangential field");
    std::cout<<std::setprecision(17)<<"GLOBAL_PLANAR faces="<<l.interface_faces
             <<" slip="<<g.velocity_m_s[2][1]-g.velocity_m_s[1][1]
             <<" water_y="<<g.velocity_m_s[1][1]
             <<" air_y="<<g.velocity_m_s[2][1]
             <<" energy_residual="<<l.energy_residual_j<<"\n";
}
P2TwoPhaseGridShear3D checkerboard() {
    P2TwoPhaseGridShear3D g;
    g.nx=3;g.ny=3;g.nz=3;g.dx_m=.08;g.dt_s=.03;
    g.water_density_kg_m3=1000;g.air_density_kg_m3=1.2;
    g.water_viscosity_pa_s=1.3;g.air_viscosity_pa_s=.18;
    for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x) {
        g.phase.push_back((x+y+z)%2==0?1:0);
        g.velocity_m_s.push_back({.1*x+.03*y,-.2*y+.04*z,.12*z+.02*x});
    }
    return g;
}
void three_dimensional_reference() {
    auto g=checkerboard();
    const auto original=g;
    const auto l=p2_two_phase_global_shear_step3d(g);
    check(l.interface_faces==54&&l.axis_faces[0]==18&&
          l.axis_faces[1]==18&&l.axis_faces[2]==18,"3D interface topology");
    near(l.water_mass_kg,14*1000*.08*.08*.08,1e-12,"3D water mass");
    near(l.air_mass_kg,13*1.2*.08*.08*.08,1e-12,"3D air mass");
    for(double r:l.momentum_residual_n_s)near(r,0,1e-12,"3D momentum");
    near(l.energy_residual_j,0,1e-12,"3D energy");
    check(l.kinetic_after_j<l.kinetic_before_j,"3D shear did not dissipate");
    for(double r:l.pcg_residual_kg_m_s)check(r<1e-12,"3D PCG residual");
    for(int i:l.pcg_iterations)check(i>0,"3D PCG iterations absent");
    auto shifted=original;
    for(auto& v:shifted.velocity_m_s) {
        v[0]+=.3;v[1]-=.2;v[2]+=.1;
    }
    const auto ls=p2_two_phase_global_shear_step3d(shifted);
    for(std::size_t i=0;i<g.velocity_m_s.size();++i)
        for(int k=0;k<3;++k) {
            const double shift=k==0?.3:(k==1?-.2:.1);
            near(shifted.velocity_m_s[i][k]-shift,g.velocity_m_s[i][k],
                 2e-12,"Galilean invariance");
        }
    for(double r:ls.momentum_residual_n_s)near(r,0,1e-12,"Shifted momentum");
    near(ls.energy_residual_j,0,1e-12,"Shifted energy");
    auto again=original;
    p2_two_phase_global_shear_step3d(again);
    for(std::size_t i=0;i<g.velocity_m_s.size();++i)
        for(int k=0;k<3;++k)
            near(again.velocity_m_s[i][k],g.velocity_m_s[i][k],0,"Deterministic global PCG");
    const auto v0=g.velocity_m_s[0],v13=g.velocity_m_s[13],v26=g.velocity_m_s[26];
    double maxmom=0,maxpcg=0;
    for(double r:l.momentum_residual_n_s)maxmom=std::max(maxmom,std::abs(r));
    for(double r:l.pcg_residual_kg_m_s)maxpcg=std::max(maxpcg,std::abs(r));
    std::cout<<std::setprecision(17)<<"GLOBAL_3D faces="<<l.interface_faces
             <<" axes="<<l.axis_faces[0]<<","<<l.axis_faces[1]<<","<<l.axis_faces[2]
             <<" v0="<<v0[0]<<","<<v0[1]<<","<<v0[2]
             <<" v13="<<v13[0]<<","<<v13[1]<<","<<v13[2]
             <<" v26="<<v26[0]<<","<<v26[1]<<","<<v26[2]
             <<" momentum_residual="<<maxmom
             <<" energy_residual="<<l.energy_residual_j
             <<" pcg_max="<<maxpcg<<"\n";
}
void surface_source() {
    auto g=planar();g.marangoni_gradient_pa={0,2,0};
    const auto l=p2_two_phase_global_shear_step3d(g);
    near(l.surface_impulse_n_s[1],16*.05*.01*2,1e-14,"Surface impulse");
    for(double r:l.momentum_residual_n_s)near(r,0,1e-12,"Source momentum");
    near(l.energy_residual_j,0,1e-12,"Source energy");
    check(l.surface_work_j>0,"Surface source work");
}
void no_interface_and_invalid() {
    auto g=planar();
    std::fill(g.phase.begin(),g.phase.end(),1);
    const auto before=g.velocity_m_s;
    const auto l=p2_two_phase_global_shear_step3d(g);
    check(l.interface_faces==0&&g.velocity_m_s==before,"No interface changes state");
    g=planar();g.marangoni_gradient_pa={1,0,0};
    const auto prior=g.velocity_m_s;
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    check(g.velocity_m_s==prior,"Normal surface source changed state");
    g=planar();g.phase[0]=2;
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.velocity_m_s[0][0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.dx_m=0;
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.air_viscosity_pa_s=std::numeric_limits<double>::infinity();
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.velocity_m_s.pop_back();
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.nx=129;
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
}
}
int main() {
    try {
        planar_reference();three_dimensional_reference();surface_source();
        no_interface_and_invalid();
        std::cout<<"PASS: GLOBAL_TWO_PHASE_SHEAR assertions="<<assertions<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
