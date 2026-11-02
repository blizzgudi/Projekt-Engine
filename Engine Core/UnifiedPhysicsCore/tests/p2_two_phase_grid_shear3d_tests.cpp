// P2 PARTIAL: native 3D two-phase fixed-interface cell-centered impulse tests.
#include "upc/p2_two_phase_grid_shear3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int assertions=0;
void check(bool b,const char* msg){++assertions;if(!b)throw std::runtime_error(msg);}
void near(double a,double b,double tol,const char* msg) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
template<class F>void rejects(F f){bool yes=false;try{f();}catch(const std::invalid_argument&){yes=true;}check(yes,"Invalid grid accepted");}
using upc::P2TwoPhaseGridShear3D;
using upc::p2_two_phase_grid_shear_step3d;
P2TwoPhaseGridShear3D planar(){
    P2TwoPhaseGridShear3D g;
    g.nx=4;g.ny=4;g.nz=4;g.dx_m=.1;g.dt_s=.05;
    g.water_density_kg_m3=1000;g.air_density_kg_m3=1.2;
    g.water_viscosity_pa_s=1;g.air_viscosity_pa_s=.1;
    for(int z=0;z<4;++z)for(int y=0;y<4;++y)for(int x=0;x<4;++x){
        const bool wet=x<2;
        g.phase.push_back(wet?1:0);
        g.velocity_m_s.push_back({0,wet?0.0:1.0,0});
    }
    return g;
}
void planar_analytic(){
    auto g=planar();
    const auto l=p2_two_phase_grid_shear_step3d(g);
    const double mw=1000*.001,ma=1.2*.001;
    const double b=.05*.01/(.05/1+.05/.1);
    const double slip=1/(1+b*(1/mw+1/ma));
    const double impulse=b*slip;
    check(l.interface_faces==16&&l.axis_faces[0]==16&&
          l.axis_faces[1]==0&&l.axis_faces[2]==0,"Planar face count");
    near(l.water_mass_kg,32,1e-12,"Water mass");
    near(l.air_mass_kg,32*ma,1e-13,"Air mass");
    for(int z=0;z<4;++z)for(int y=0;y<4;++y) {
        const std::size_t w=(std::size_t(z)*4+y)*4+1;
        const std::size_t a=w+1;
        near(g.velocity_m_s[w][1],impulse/mw,1e-14,"Water tangential update");
        near(g.velocity_m_s[a][1],1-impulse/ma,1e-14,"Air tangential update");
        near(g.velocity_m_s[w][0],0,1e-15,"Water normal unchanged");
        near(g.velocity_m_s[a][0],0,1e-15,"Air normal unchanged");
    }
    for(double r:l.momentum_residual_n_s)near(r,0,1e-12,"Global impulse closure");
    near(l.energy_residual_j,0,1e-12,"Global energy closure");
    check(l.kinetic_after_j<l.kinetic_before_j,"Planar viscosity did not dissipate");
    std::cout<<"GRID_PLANAR faces="<<l.interface_faces
             <<" water_mass="<<std::setprecision(16)<<l.water_mass_kg
             <<" air_mass="<<l.air_mass_kg
             <<" slip="<<slip
             <<" impulse_y="<<impulse
             <<" energy_residual="<<l.energy_residual_j<<"\n";
}
void marangoni(){
    auto g=planar();g.marangoni_gradient_pa={0,2,0};
    const auto l=p2_two_phase_grid_shear_step3d(g);
    near(l.surface_impulse_n_s[1],16*.05*.01*2,1e-14,"Marangoni source");
    for(int k=0;k<3;++k)near(l.momentum_residual_n_s[k],0,1e-12,"Marangoni momentum");
    near(l.energy_residual_j,0,1e-12,"Marangoni energy");
    check(l.surface_work_j>0,"Marangoni work absent");
}
void mixed_3d(){
    P2TwoPhaseGridShear3D g;
    g.nx=3;g.ny=3;g.nz=3;g.dx_m=.08;g.dt_s=.03;
    g.water_density_kg_m3=1000;g.air_density_kg_m3=1.2;
    g.water_viscosity_pa_s=1.3;g.air_viscosity_pa_s=.18;
    for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x){
        const bool wet=(x+y+z)%2==0;
        g.phase.push_back(wet?1:0);
        g.velocity_m_s.push_back({.1*x+.03*y,-.2*y+.04*z,.12*z+.02*x});
    }
    const auto original=g;
    auto l=p2_two_phase_grid_shear_step3d(g);
    check(l.interface_faces==54&&l.axis_faces[0]==18&&
          l.axis_faces[1]==18&&l.axis_faces[2]==18,"Three-axis interface enumeration");
    near(l.water_mass_kg,14*1000*.08*.08*.08,1e-13,"3D water mass");
    near(l.air_mass_kg,13*1.2*.08*.08*.08,1e-13,"3D air mass");
    for(double r:l.momentum_residual_n_s)near(r,0,1e-12,"3D global momentum");
    near(l.energy_residual_j,0,1e-12,"3D global energy");
    check(l.kinetic_after_j<=l.kinetic_before_j+1e-12,"3D viscosity increased energy");
    const auto after_one=g.velocity_m_s;
    const auto second=p2_two_phase_grid_shear_step3d(g);
    for(double r:second.momentum_residual_n_s)near(r,0,1e-12,"3D second-step momentum");
    near(second.energy_residual_j,0,1e-12,"3D second-step energy");
    auto repeated=original;
    p2_two_phase_grid_shear_step3d(repeated);
    for(std::size_t i=0;i<after_one.size();++i)for(int k=0;k<3;++k)
        near(repeated.velocity_m_s[i][k],after_one[i][k],0,"Deterministic pair ordering");
    // Galilean invariance of velocity increments, not kinetic energy (different frame).
    auto shifted=original;
    for(auto& v:shifted.velocity_m_s){v[0]+=.3;v[1]-=.2;v[2]+=.1;}
    p2_two_phase_grid_shear_step3d(shifted);
    for(std::size_t i=0;i<after_one.size();++i)for(int k=0;k<3;++k){
        const double shift=k==0?.3:(k==1?-.2:.1);
        near(shifted.velocity_m_s[i][k]-shift,after_one[i][k],2e-15,"Galilean update");
    }
    double max_momentum=0;
    for(double r:l.momentum_residual_n_s)max_momentum=std::max(max_momentum,std::abs(r));
    std::cout<<"GRID_3D faces="<<l.interface_faces<<" axes="<<l.axis_faces[0]
             <<","<<l.axis_faces[1]<<","<<l.axis_faces[2]
             <<" momentum_residual="<<std::setprecision(16)<<max_momentum
             <<" energy_residual="<<l.energy_residual_j<<"\n";
}
void invalid_transaction(){
    auto g=planar();
    auto before=g.velocity_m_s;
    g.marangoni_gradient_pa={1,0,0};
    rejects([&]{p2_two_phase_grid_shear_step3d(g);});
    check(g.velocity_m_s==before,"Normal Marangoni changed grid");
    g=planar();g.velocity_m_s.back()[1]=std::numeric_limits<double>::quiet_NaN();
    before=g.velocity_m_s;
    rejects([&]{p2_two_phase_grid_shear_step3d(g);});
    for(std::size_t i=0;i<before.size()-1;++i)
        check(g.velocity_m_s[i]==before[i],"NaN grid changed valid cells");
    g=planar();g.phase.back()=2;before=g.velocity_m_s;
    rejects([&]{p2_two_phase_grid_shear_step3d(g);});
    check(g.velocity_m_s==before,"Mixed cell changed grid");
    g=planar();g.velocity_m_s.pop_back();rejects([&]{p2_two_phase_grid_shear_step3d(g);});
    g=planar();g.dx_m=0;rejects([&]{p2_two_phase_grid_shear_step3d(g);});
    g=planar();g.dt_s=std::numeric_limits<double>::infinity();
    rejects([&]{p2_two_phase_grid_shear_step3d(g);});
    g=planar();g.air_density_kg_m3=-1;rejects([&]{p2_two_phase_grid_shear_step3d(g);});
    g=planar();g.nx=257;rejects([&]{p2_two_phase_grid_shear_step3d(g);});
}
}
int main(){
    try{
        planar_analytic();marangoni();mixed_3d();invalid_transaction();
        std::cout<<"PASS: TWO_PHASE_GRID_SHEAR assertions="<<assertions<<"\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
