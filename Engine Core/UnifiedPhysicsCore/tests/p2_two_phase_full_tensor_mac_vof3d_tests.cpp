// P2 PARTIAL: actual moving two-phase 3D MAC/VOF, all six symmetric
// viscous stresses and re-projected shared pressure, measured wall/energy
// residuals. No unproven global CFD or solid FSI acceptance.
#include "upc/p2_two_phase_full_tensor_mac_vof3d.hpp"
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
int checks=0;
void check(bool good,const char* reason){
    ++checks;if(!good)throw std::runtime_error(reason);
}
void near(double a,double b,double tol,const char* reason){
    if(!(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol))
        std::cerr<<std::setprecision(17)<<"MISMATCH "<<reason
                 <<" got="<<a<<" expect="<<b<<" tol="<<tol<<"\n";
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,reason);
}
P2TwoPhaseMacVofState3D make(int nx,int ny,int nz,bool oblique) {
    P2TwoPhaseMacVofState3D s;
    auto& m=s.mac;
    m.nx=nx;m.ny=ny;m.nz=nz;m.dx_m=.1;m.dt_s=.003;
    m.density_kg_m3.resize(nx*ny*nz);
    m.u_m_s.assign((nx+1)*ny*nz,0);
    m.v_m_s.assign(nx*(ny+1)*nz,0);
    m.w_m_s.assign(nx*ny*(nz+1),0);
    s.water_fraction.resize(nx*ny*nz);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x){
        const auto i=(z*ny+y)*nx+x;
        const double alpha=oblique?
            (z<nz/2+(x<nx/2?1:0)+(y<ny/2?1:0)?.87:.06):
            (z<nz/2?.9:.05);
        s.water_fraction[i]=alpha;
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*alpha;
    }
    return s;
}
double max_delta(const P2TwoPhaseMacPressureGrid3D& a,
                 const P2TwoPhaseMacPressureGrid3D& b){
    double biggest=0;
    for(std::size_t i=0;i<a.u_m_s.size();++i)
        biggest=std::max(biggest,std::abs(a.u_m_s[i]-b.u_m_s[i]));
    for(std::size_t i=0;i<a.v_m_s.size();++i)
        biggest=std::max(biggest,std::abs(a.v_m_s[i]-b.v_m_s[i]));
    for(std::size_t i=0;i<a.w_m_s.size();++i)
        biggest=std::max(biggest,std::abs(a.w_m_s[i]-b.w_m_s[i]));
    return biggest;
}
void run(int nx,int ny,int nz,bool oblique){
    auto s=make(nx,ny,nz,oblique);
    auto no_viscosity=s;
    const auto initial=s.water_fraction;
    const auto initial_mass=std::accumulate(
        initial.begin(),initial.end(),0.0);
    constexpr std::array<double,3> gravity{0.7,-.3,-9.81};
    double peak_flux=0,peak_viscous_change=0,peak_viscous_loss=0;
    double peak_post_projection_div=0,peak_energy_residual=0;
    double peak_wall_impulse_residual=0,peak_mass_difference=0;
    for(int step=0;step<8;++step){
        const auto before=s;
        const auto trace=p2_two_phase_full_tensor_mac_vof_step3d(
            s,4000,gravity,.05,.000018);
        (void)p2_two_phase_mac_vof_step3d(no_viscosity,4000,gravity);
        check(!trace.sharp_stress_interface_validated&&
              !trace.physical_total_energy_validated&&
              !trace.two_way_FSI_validated,
              "full stress, FSI and energy must not be silently accepted");
        check(trace.viscous.normal_stress_cells==std::size_t(nx*ny*nz),
              "real 3D symmetric normal stress missing");
        check(trace.viscous.xy_shear_edges==
              std::size_t((nx-1)*(ny-1)*nz),"real xy shear missing");
        check(trace.viscous.xz_shear_edges==
              std::size_t((nx-1)*ny*(nz-1)),"real xz shear missing");
        check(trace.viscous.yz_shear_edges==
              std::size_t(nx*(ny-1)*(nz-1)),"real yz shear missing");
        near(trace.water_mass_before_kg,trace.water_mass_after_kg,
             1e-9,"moving liquid mass");
        near(trace.air_mass_before_kg,trace.air_mass_after_kg,
             1e-9,"moving gas mass");
        const auto sum=std::accumulate(
            s.water_fraction.begin(),s.water_fraction.end(),0.0);
        near(sum,initial_mass,1e-11,"real 3D eight-step moving VOF conservation");
        for(std::size_t i=0;i<s.water_fraction.size();++i)
            near(s.mac.density_kg_m3[i],s.air_density_kg_m3+
                 (s.water_density_kg_m3-s.air_density_kg_m3)*
                 s.water_fraction[i],1e-10,
                 "real two-phase stress coupled to updated mixed fluid density");
        check(trace.maximum_wet_divergence_s_inv<1e-9,
              "real 3D variable-density pressure divergence after stress");
        near(trace.post_viscosity_energy_before_j,
             trace.viscous.kinetic_before_j,1e-12,
             "independent viscous stage kinetic energy start");
        near(trace.post_viscosity_energy_after_j,
             trace.post_viscosity_projection.kinetic_after_j,1e-12,
             "independent viscous+pressure stage kinetic energy end");
        near(trace.post_viscosity_energy_residual_j,0,1e-10,
             "3D viscous+pressure energy must close at fixed VOF");
        for(int k=0;k<3;++k){
            near(trace.actual_combined_impulse_change_kg_m_s[k],
                 trace.actual_combined_wall_impulse_kg_m_s[k],1e-10,
                 "3D live viscous traction + pressure wall counterimpulse");
            peak_wall_impulse_residual=std::max(
                peak_wall_impulse_residual,
                std::abs(trace.post_viscosity_impulse_residual_kg_m_s[k]));
        }
        peak_flux=std::max(peak_flux,
                          trace.moving.vof.max_outgoing_courant);
        peak_viscous_change=std::max(
            peak_viscous_change,trace.viscous.max_velocity_change_m_s);
        peak_viscous_loss=std::max(
            peak_viscous_loss,
            trace.viscous.kinetic_before_j-trace.viscous.kinetic_after_j);
        peak_post_projection_div=std::max(
            peak_post_projection_div,trace.maximum_wet_divergence_s_inv);
        peak_energy_residual=std::max(
            peak_energy_residual,
            std::abs(trace.post_viscosity_energy_residual_j));
        peak_mass_difference=std::max(
            peak_mass_difference,
            std::abs(trace.water_mass_after_kg-trace.water_mass_before_kg));
        (void)before;
    }
    const double face_difference=max_delta(s.mac,no_viscosity.mac);
    if(oblique){
        check(peak_flux>1e-7,"oblique moving 3D VOF interface did not transport");
        check(peak_viscous_change>1e-10,
              "true 3D moving water/air symmetric stress had no effect");
        check(peak_viscous_loss>0,
              "nonzero viscous physical kinetic dissipation missing");
        check(face_difference>1e-10,
              "viscous and inviscid live 3D MAC paths unexpectedly identical");
    }
    std::cout<<std::setprecision(17)
             <<"LIVE_TENSOR grid="<<nx<<"x"<<ny<<"x"<<nz
             <<" oblique="<<int(oblique)
             <<" steps=8 max_moving_vof_cfl="<<peak_flux
             <<" viscous_delta_m_s="<<peak_viscous_change
             <<" viscous_loss_j="<<peak_viscous_loss
             <<" vs_inviscid_face_delta_m_s="<<face_difference
             <<" pressure_divergence_s_inv="<<peak_post_projection_div
             <<" stress_pressure_energy_residual_j="<<peak_energy_residual
             <<" stress_pressure_impulse_residual_kg_m_s="<<peak_wall_impulse_residual
             <<" phase_mass_residual_kg="<<peak_mass_difference
             <<" full_phase=false\n";
}
void negative(){
    auto baseline=make(6,5,4,true),bad=baseline;
    bad.mac.dt_s=1000;
    const auto old_fraction=bad.water_fraction;
    const auto old_vel=bad.mac.w_m_s;
    bool failed=false;
    try{(void)p2_two_phase_full_tensor_mac_vof_step3d(bad);}
    catch(const std::exception&){failed=true;}
    check(failed&&bad.water_fraction==old_fraction&&bad.mac.w_m_s==old_vel,
          "enormous timestep must fail atomically");
    bad=baseline;failed=false;
    try{(void)p2_two_phase_full_tensor_mac_vof_step3d(
        bad,4000,{0,0,-9.81},-.1,.000018);}
    catch(const std::exception&){failed=true;}
    check(failed&&bad.mac.density_kg_m3==baseline.mac.density_kg_m3,
          "negative viscosity fails without altering fluid mixture");
    bad=baseline;failed=false;
    try{(void)p2_two_phase_full_tensor_mac_vof_step3d(
        bad,1,{0,0,-9.81},.05,.000018);}
    catch(const std::exception&){failed=true;}
    check(failed&&bad.mac.u_m_s==baseline.mac.u_m_s,
          "underiterated PCG rejected without changing live velocities");
    bad=baseline;failed=false;
    try{(void)p2_two_phase_full_tensor_mac_vof_step3d(
        bad,4000,{0,0,std::numeric_limits<double>::quiet_NaN()},.05,.000018);}
    catch(const std::exception&){failed=true;}
    check(failed&&bad.water_fraction==baseline.water_fraction,
          "NaN forcing must be fail-closed");
}
}
int main(){
    try{
        run(4,4,4,false);
        run(6,5,4,false);
        run(6,5,4,true);
        run(8,6,5,true);
        negative();
        std::cout<<"PASS P2_TWO_PHASE_MOVING_FULL_TENSOR assertions="
                 <<checks<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_TWO_PHASE_MOVING_FULL_TENSOR "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
