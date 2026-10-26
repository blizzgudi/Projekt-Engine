// P2 partial follow-on: LIVE CoupledMacWater3D masked-viscosity integration.
// Independent from the isolated masked operator test. Not full 3D CFD validation.
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace upc;
static std::size_t checks=0;
static void require(bool pass,const char* reason) {
    ++checks;
    if(!pass)throw std::runtime_error(reason);
}
static void near(double actual,double expected,double tol,const char* reason) {
    require(std::isfinite(actual)&&std::abs(actual-expected)<=tol,reason);
}
static void fill_tank(CoupledMacWater3D& water) {
    for(std::size_t z=0;z<water.nz();++z)
      for(std::size_t y=0;y<water.ny();++y)
        for(std::size_t x=0;x<water.nx();++x) {
            if(x==5&&y>=1&&y<=4&&z<5)water.set_solid(x,y,z);
            if(x<3&&z<5)water.set_water(x,y,z,1.0);
        }
}
int main(){
 try {
    constexpr double dx=0.1,dt=0.02;
    CoupledMacWater3D legacy(9,6,7,dx),zero(9,6,7,dx),viscous(9,6,7,dx);
    fill_tank(legacy);fill_tank(zero);fill_tank(viscous);
    zero.set_kinematic_viscosity(0);
    viscous.set_kinematic_viscosity(0.035);
    const double initial=viscous.mass_kg();
    near(initial,90.0,1e-10,"unexpected initial mass");
    double max_diff=0,positive_dissipation=0;
    std::size_t total_visc_steps=0;
    for(int t=0;t<16;++t) {
        legacy.step(dt,180);
        zero.step(dt,180);
        viscous.step(dt,180);
        near(legacy.mass_kg(),initial,1e-8,"legacy water mass drift");
        near(zero.mass_kg(),initial,1e-8,"nu=0 water mass drift");
        near(viscous.mass_kg(),initial,1e-8,"viscous water mass drift");
        require(zero.last_viscosity_substeps()==0,"zero viscosity took diffusion steps");
        require(viscous.last_viscosity_substeps()>0,"live viscous operator not called");
        total_visc_steps+=viscous.last_viscosity_substeps();
        const double before=viscous.last_viscous_energy_before();
        const double after=viscous.last_viscous_energy_after();
        require(std::isfinite(before)&&std::isfinite(after)&&after>=0,
                "nonfinite live viscosity energy");
        require(after<=before+1e-11,"viscous energy proxy increased");
        positive_dissipation+=before-after;
        require(std::isfinite(viscous.last_projection_divergence())&&
                viscous.last_projection_divergence()<8,
                "viscous MAC pressure projection diverged");
        for(std::size_t z=0;z<7;++z)
          for(std::size_t y=0;y<6;++y)
            for(std::size_t x=0;x<9;++x) {
                const double v=viscous.water_fraction(x,y,z);
                const double l=legacy.water_fraction(x,y,z);
                const double n=zero.water_fraction(x,y,z);
                require(std::isfinite(v)&&v>=0&&v<=1,
                        "viscous VOF fraction outside [0,1]");
                require(l==n,"zero viscosity changed legacy inviscid result");
                if(viscous.is_solid(x,y,z))
                    near(v,0,0,"water entered static solid");
                max_diff=std::max(max_diff,std::abs(v-l));
            }
    }
    require(positive_dissipation>1e-8,
            "live viscosity did not dissipate measurable face energy");
    require(max_diff>1e-7,
            "viscosity did not alter actual coupled water transport");
    // Invalid input is rejected before touching the coupled state.
    bool rejected=false;
    try{viscous.set_kinematic_viscosity(-0.01);}
    catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"negative nu accepted");
    rejected=false;
    try{viscous.set_kinematic_viscosity(std::numeric_limits<double>::quiet_NaN());}
    catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"NaN nu accepted");
    rejected=false;
    try{viscous.set_kinematic_viscosity(std::numeric_limits<double>::infinity());}
    catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"infinite nu accepted");
    CoupledMacWater3D thin(1,3,3,dx);
    rejected=false;
    try{thin.set_kinematic_viscosity(0.01);}
    catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"invalid 1-cell viscosity grid accepted");
    // Verify the adaptive internal diffusion CFL under a large nu.
    CoupledMacWater3D high(5,5,5,dx);
    for(std::size_t z=0;z<3;++z)
      for(std::size_t y=0;y<5;++y)
        for(std::size_t x=0;x<3;++x)high.set_water(x,y,z,1);
    high.set_kinematic_viscosity(0.3);
    const auto high_mass=high.mass_kg();
    high.step(0.04,180);
    require(high.last_viscosity_substeps()>1,"large nu did not substep");
    near(high.mass_kg(),high_mass,1e-8,"large nu lost mass");
    // Live static-wall Couette shear: a prescribed tangential speed must
    // influence actual coupled VOF motion, not just the isolated operator.
    // The wall x=2 is a static solid mask; no rigid-body motion/FSI claim.
    CoupledMacWater3D still(8,8,6,dx),explicit_zero(8,8,6,dx),
                      driven(8,8,6,dx);
    for(std::size_t z=0;z<6;++z)
      for(std::size_t y=0;y<8;++y)
        for(std::size_t x=0;x<8;++x) {
            if(x==2) {
                still.set_solid(x,y,z);
                explicit_zero.set_solid(x,y,z);
                driven.set_solid(x,y,z);
            }
            if(x>=3&&x<=5&&y>=1&&y<=3&&z<4) {
                still.set_water(x,y,z,1);
                explicit_zero.set_water(x,y,z,1);
                driven.set_water(x,y,z,1);
            }
        }
    still.set_kinematic_viscosity(0.035);
    explicit_zero.set_kinematic_viscosity(0.035);
    driven.set_kinematic_viscosity(0.035);
    explicit_zero.set_wall_tangential_velocity({0.0,0.0,0.0});
    driven.set_wall_tangential_velocity({0.0,0.75,0.0});
    near(driven.wall_tangential_velocity_m_s()[1],0.75,0,
         "wall speed getter failed");
    const double wall_initial_mass=driven.mass_kg();
    double wall_driven_vof_difference=0;
    for(int t=0;t<12;++t) {
        still.step(dt,180);
        explicit_zero.step(dt,180);
        driven.step(dt,180);
        near(still.mass_kg(),wall_initial_mass,1e-8,
             "stationary wall mass drift");
        near(driven.mass_kg(),wall_initial_mass,1e-8,
             "prescribed shear changed total water mass");
        require(driven.last_viscosity_substeps()>0,
                "live wall-driven viscosity not executed");
        require(std::isfinite(driven.last_projection_divergence())&&
                driven.last_projection_divergence()<8,
                "wall-driven projection diverged");
        for(std::size_t z=0;z<6;++z)
          for(std::size_t y=0;y<8;++y)
            for(std::size_t x=0;x<8;++x) {
                const double d=driven.water_fraction(x,y,z);
                const double s=still.water_fraction(x,y,z);
                const double e=explicit_zero.water_fraction(x,y,z);
                require(std::isfinite(d)&&d>=0&&d<=1,
                        "wall-driven VOF fraction out of bounds");
                require(s==e,
                        "explicit zero-wall setting changed legacy viscous flow");
                if(x==2)near(d,0,0,
                             "prescribed tangential wall caused normal penetration");
                wall_driven_vof_difference=std::max(
                    wall_driven_vof_difference,std::abs(d-s));
            }
    }
    require(wall_driven_vof_difference>1e-9,
            "prescribed wall shear did not change live VOF flow");
    // Invalid wall speed must leave the last valid configuration intact.
    rejected=false;
    try{driven.set_wall_tangential_velocity(
            {0.0,std::numeric_limits<double>::infinity(),0.0});}
    catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"nonfinite coupled wall velocity accepted");
    near(driven.wall_tangential_velocity_m_s()[1],0.75,0,
         "invalid wall speed modified existing configuration");
    std::cout.precision(15);
    std::cout<<"PASS P2 live masked viscosity: checks="<<checks
             <<" initial_mass_kg="<<initial
             <<" final_mass_kg="<<viscous.mass_kg()
             <<" total_visc_substeps="<<total_visc_steps
             <<" dissipated_face_energy_proxy="<<positive_dissipation
             <<" max_vof_difference_vs_inviscid="<<max_diff
             <<" large_nu_substeps="<<high.last_viscosity_substeps()
             <<" wall_shear_vof_difference="<<wall_driven_vof_difference
             <<" wall_shear_mass_kg="<<driven.mass_kg()
             <<"\n";
    return 0;
 } catch(const std::exception& e) {
    std::cerr<<"FAIL P2 live masked viscosity after "<<checks
             <<": "<<e.what()<<"\n";
    return 1;
 }
}
