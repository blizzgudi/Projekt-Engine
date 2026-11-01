// P2 PARTIAL: independent two-cell finite-volume tangential impulse/energy QA.
#include "upc/p2_two_phase_shear_exchange3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
int assertions=0;
void check(bool yes,const char* why){++assertions;if(!yes)throw std::runtime_error(why);}
void near(double a,double b,double tol,const char* why){
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
template<class F>void rejects(F fn){bool caught=false;try{fn();}catch(const std::invalid_argument&){caught=true;}check(caught,"Invalid patch accepted");}
using upc::P2TwoPhaseShearPatch3D;
using upc::p2_two_phase_shear_exchange3d;
P2TwoPhaseShearPatch3D fixture(){
    P2TwoPhaseShearPatch3D p;
    p.normal={1,0,0};p.water_velocity_m_s={1,0,0};
    p.air_velocity_m_s={1,1,0};
    p.water_mass_kg=2;p.air_mass_kg=1;
    p.water_viscosity_pa_s=2;p.air_viscosity_pa_s=.5;
    p.water_center_distance_m=.04;p.air_center_distance_m=.06;
    p.interface_area_m2=.01;p.dt_s=.2;
    return p;
}
void test_zero_gradient(){
    const auto p=fixture();
    const auto s=p2_two_phase_shear_exchange3d(p);
    // Independently derived implicit two-mass Couette reference.
    const double resistance=.04/2.0+.06/.5;
    const double b=.2*.01/resistance;
    const double expected_delta=1.0/(1.0+b*(1.0/2.0+1.0));
    const double expected_impulse=b*expected_delta;
    near(s.air_velocity_m_s[1]-s.water_velocity_m_s[1],expected_delta,1e-14,"Implicit slip");
    near(s.water_impulse_n_s[1],expected_impulse,1e-14,"Water shear impulse");
    near(s.air_impulse_n_s[1],-expected_impulse,1e-14,"Air shear impulse");
    near(s.water_traction_pa[1],expected_delta/resistance,1e-13,"Water traction");
    near(s.air_traction_pa[1],expected_delta/resistance,1e-13,"Air traction");
    near(s.water_velocity_m_s[0],1,1e-15,"Water normal unchanged");
    near(s.air_velocity_m_s[0],1,1e-15,"Air normal unchanged");
    near(s.water_impulse_n_s[0],0,1e-15,"No normal impulse");
    near(s.surface_work_j,0,1e-15,"No surface work");
    check(s.kinetic_after_j<s.kinetic_before_j,"Viscous energy did not dissipate");
    check(s.viscous_dissipation_j>0&&s.implicit_dissipation_j>0,"Missing dissipations");
    near(s.energy_residual_j,0,1e-13,"Energy identity");
    std::cout<<"SHEAR_ZERO slip="<<std::setprecision(16)<<expected_delta
             <<" impulse="<<s.water_impulse_n_s[1]
             <<" energy_residual_j="<<s.energy_residual_j<<"\n";
}
void test_marangoni_and_rotation(){
    auto p=fixture();p.marangoni_gradient_pa={0,3.295,-1.14};
    auto s=p2_two_phase_shear_exchange3d(p);
    for(int i=0;i<3;++i){
        near(s.water_impulse_n_s[i]+s.air_impulse_n_s[i],
             p.dt_s*p.interface_area_m2*p.marangoni_gradient_pa[i],1e-14,
             "Surface momentum source");
        near(s.water_traction_pa[i]-s.air_traction_pa[i],
             p.marangoni_gradient_pa[i],1e-12,"Traction jump");
    }
    near(s.energy_residual_j,0,1e-13,"Marangoni energy closure");
    near(s.water_velocity_m_s[0],1,1e-15,"Marangoni water normal");
    near(s.air_velocity_m_s[0],1,1e-15,"Marangoni air normal");
    const double inv=1/std::sqrt(2.0);
    p.normal={inv,inv,0};
    p.water_velocity_m_s={inv,inv,0};
    p.air_velocity_m_s={2*inv,0,0};
    p.marangoni_gradient_pa={3.295*inv,-3.295*inv,0};
    s=p2_two_phase_shear_exchange3d(p);
    near(upc::p2_shear_dot(s.water_velocity_m_s,p.normal),1,1e-14,"Rotated water normal");
    near(upc::p2_shear_dot(s.air_velocity_m_s,p.normal),1,1e-14,"Rotated air normal");
    near(s.energy_residual_j,0,1e-13,"Rotated energy closure");
    for(int i=0;i<3;++i)
        near(s.water_impulse_n_s[i]+s.air_impulse_n_s[i],
             p.dt_s*p.interface_area_m2*p.marangoni_gradient_pa[i],1e-14,
             "Rotated surface impulse");
    std::cout<<"SHEAR_MARANGONI surface_impulse_y="<<std::setprecision(16)
             <<p.dt_s*p.interface_area_m2*3.295
             <<" energy_residual_j="<<s.energy_residual_j<<"\n";
}
void test_stability_and_negative(){
    auto p=fixture();
    for(double dt:{.00001,.1,1.0,100.0,10000.0}){
        p.dt_s=dt;
        auto s=p2_two_phase_shear_exchange3d(p);
        check(s.kinetic_after_j<=s.kinetic_before_j+1e-13,"Unstable implicit shear");
        near(s.energy_residual_j,0,1e-12,"Large dt energy");
        near(s.water_impulse_n_s[1]+s.air_impulse_n_s[1],0,1e-13,"Large dt momentum");
    }
    p=fixture();p.water_mass_kg=0;rejects([&]{p2_two_phase_shear_exchange3d(p);});
    p=fixture();p.air_viscosity_pa_s=0;rejects([&]{p2_two_phase_shear_exchange3d(p);});
    p=fixture();p.water_center_distance_m=-1;rejects([&]{p2_two_phase_shear_exchange3d(p);});
    p=fixture();p.interface_area_m2=0;rejects([&]{p2_two_phase_shear_exchange3d(p);});
    p=fixture();p.dt_s=std::numeric_limits<double>::quiet_NaN();rejects([&]{p2_two_phase_shear_exchange3d(p);});
    p=fixture();p.normal={2,0,0};rejects([&]{p2_two_phase_shear_exchange3d(p);});
    p=fixture();p.marangoni_gradient_pa={1,0,0};rejects([&]{p2_two_phase_shear_exchange3d(p);});
    p=fixture();p.air_velocity_m_s={1,std::numeric_limits<double>::infinity(),0};rejects([&]{p2_two_phase_shear_exchange3d(p);});
    p=fixture();p.dt_s=std::numeric_limits<double>::infinity();rejects([&]{p2_two_phase_shear_exchange3d(p);});
}
}
int main(){
    try{
        test_zero_gradient();test_marangoni_and_rotation();test_stability_and_negative();
        std::cout<<"PASS: TWO_PHASE_SHEAR assertions="<<assertions<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
