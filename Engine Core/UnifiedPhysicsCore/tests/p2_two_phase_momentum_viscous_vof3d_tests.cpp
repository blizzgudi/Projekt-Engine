// P2 PARTIAL: LIVE 3D water/air MAC/VOF + conservative momentum + full
// symmetric normal/shear viscous tensor + final variable-density PCG.
// Kinetic/impulse sub-ledgers only; not sharp-interface/FSI CFD acceptance.
#include "upc/p2_two_phase_momentum_viscous_vof3d.hpp"
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
std::size_t assertions=0;
void check(bool ok,const char* what){
    ++assertions;if(!ok)throw std::runtime_error(what);
}
void near(double v,double ref,double tol,const char* what){
    if(!(std::isfinite(v)&&std::isfinite(ref)&&std::abs(v-ref)<=tol))
        std::cerr<<std::setprecision(17)<<"MISMATCH "<<what
                 <<" got="<<v<<" reference="<<ref<<"\n";
    check(std::isfinite(v)&&std::isfinite(ref)&&std::abs(v-ref)<=tol,what);
}
template<class Fn>void rejects(Fn fn,const char* msg){
    bool caught=false;try{fn();}catch(const std::exception&){caught=true;}
    check(caught,msg);
}
P2TwoPhaseMacVofState3D setup(int nx,int ny,int nz){
    P2TwoPhaseMacVofState3D s;auto& m=s.mac;
    m.nx=nx;m.ny=ny;m.nz=nz;m.dx_m=.1;m.dt_s=.003;
    m.density_kg_m3.resize(nx*ny*nz);
    m.u_m_s.assign((nx+1)*ny*nz,0);
    m.v_m_s.assign(nx*(ny+1)*nz,0);
    m.w_m_s.assign(nx*ny*(nz+1),0);
    s.water_fraction.resize(nx*ny*nz);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x){
        const auto id=(z*ny+y)*nx+x;
        const double alpha=(z<nz/2+(x<nx/2?1:0)?.7:.03)+
                           (y<ny/2?.02:0);
        s.water_fraction[id]=alpha;
        m.density_kg_m3[id]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*alpha;
    }
    constexpr double pi=3.14159265358979323846;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=1;x<nx;++x)
        m.u_m_s[(z*ny+y)*(nx+1)+x]=
            .032*std::sin(pi*x/nx)*std::sin(pi*(y+.5)/ny)*
            std::cos(pi*(z+.5)/nz);
    for(int z=0;z<nz;++z)for(int y=1;y<ny;++y)
    for(int x=0;x<nx;++x)
        m.v_m_s[(z*(ny+1)+y)*nx+x]=
            -.023*std::sin(pi*y/ny)*std::sin(pi*(x+.5)/nx);
    for(int z=1;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        m.w_m_s[(z*ny+y)*nx+x]=
            .018*std::sin(pi*z/nz)*std::cos(pi*(x+.5)/nx);
    return s;
}
void run(int nx,int ny,int nz) {
    auto s=setup(nx,ny,nz);
    auto without_viscosity=s;
    const auto alpha_start=s.water_fraction;
    const double vol=std::pow(s.mac.dx_m,3);
    const double water_initial=std::accumulate(
        alpha_start.begin(),alpha_start.end(),0.)*
        vol*s.water_density_kg_m3;
    const double box_volume=nx*ny*nz*vol;
    const double air_initial=(box_volume-water_initial/s.water_density_kg_m3)*
        s.air_density_kg_m3;
    std::array<double,3> g{.7,-.4,-9.81};
    double max_true_viscous_velocity_difference=0;
    double max_energy_error=0,max_wall_error=0,max_density_error=0;
    double total_viscous_work=0;
    for(int step=0;step<6;++step) {
        const auto before=s;
        const auto l=p2_two_phase_momentum_viscous_vof_step3d(
            s,4000,g,.03,.004);
        (void)p2_two_phase_momentum_vof_step3d(
            without_viscosity,4000,g);
        check(!l.full_P2_accepted&&
              !l.global_physical_energy_impulse_closed&&
              !l.moving_sharp_interface_stress_validated,
              "standalone tensor viscosity cannot approve multiphase P2");
        check(l.last_divergence_s_inv<1e-9,
              "actual 3D two-phase stress needs final PCG divergence control");
        check(l.viscous.xy_shear_edges==
              std::size_t((nx-1)*(ny-1)*nz),"real xy tensor stress");
        check(l.viscous.xz_shear_edges==
              std::size_t((nx-1)*ny*(nz-1)),"real xz tensor stress");
        check(l.viscous.yz_shear_edges==
              std::size_t(nx*(ny-1)*(nz-1)),"real yz tensor stress");
        check(l.viscous.normal_stress_cells==
              std::size_t(nx*ny*nz),"all diagonal stress cells");
        check(l.viscous.stress_dissipation_rate_w>0,
              "actual 3D stress must dissipate");
        check(l.viscous.max_velocity_change_m_s>1e-11,
              "tensor stress must modify 3D MAC velocity");
        total_viscous_work+=
            s.mac.dt_s*l.viscous.stress_dissipation_rate_w;
        max_energy_error=std::max(max_energy_error,
                std::abs(l.kinetic_change_residual_j));
        for(int k=0;k<3;++k) {
            max_wall_error=std::max(max_wall_error,
                 std::abs(l.viscous_plus_projection_impulse_residual_kg_m_s[k]));
            near(l.viscous_plus_projection_impulse_kg_m_s[k],
                 l.viscous_plus_projection_wall_impulse_n_s[k],1e-10,
                 "3D tensor+pressure wall reaction must equal momentum change");
            near(l.viscous_plus_projection_wall_impulse_n_s[k],
                 l.viscous.wall_normal_traction_impulse_n_s[k]+
                 l.final_projection.wall_pressure_impulse_n_s[k],1e-10,
                 "measured viscous wall traction and pressure wall impulse");
        }
        near(l.measured_kinetic_change_j,l.predicted_kinetic_change_j,
             1e-10,"symmetric tensor plus projection energy split");
        near(l.water_mass_residual_kg,0,1e-9,
             "live phase VOF water mass preserved");
        near(l.air_mass_residual_kg,0,1e-9,
             "live phase VOF air mass preserved");
        near(l.momentum.fluid.water_mass_after_kg,
             l.momentum.fluid.water_mass_before_kg,1e-9,
             "momentum-feedback live water mass remains conservative");
        check(s.water_fraction.size()==before.water_fraction.size(),
              "VOF live state size unchanged");
        for(std::size_t i=0;i<s.water_fraction.size();++i) {
            const double rho=s.air_density_kg_m3+
                (s.water_density_kg_m3-s.air_density_kg_m3)*
                s.water_fraction[i];
            max_density_error=std::max(max_density_error,
                std::abs(s.mac.density_kg_m3[i]-rho));
            near(s.mac.density_kg_m3[i],rho,1e-9,
                 "live tensor MAC uses transported mixture density");
            check(s.water_fraction[i]>=0&&s.water_fraction[i]<=1,
                  "live tensor moving VOF bounded");
        }
        for(std::size_t i=0;i<s.mac.u_m_s.size();++i)
            max_true_viscous_velocity_difference=std::max(
                max_true_viscous_velocity_difference,
                std::abs(s.mac.u_m_s[i]-without_viscosity.mac.u_m_s[i]));
        for(std::size_t i=0;i<s.mac.v_m_s.size();++i)
            max_true_viscous_velocity_difference=std::max(
                max_true_viscous_velocity_difference,
                std::abs(s.mac.v_m_s[i]-without_viscosity.mac.v_m_s[i]));
        for(std::size_t i=0;i<s.mac.w_m_s.size();++i)
            max_true_viscous_velocity_difference=std::max(
                max_true_viscous_velocity_difference,
                std::abs(s.mac.w_m_s[i]-without_viscosity.mac.w_m_s[i]));
    }
    const double alpha_end=std::accumulate(
        s.water_fraction.begin(),s.water_fraction.end(),0.);
    const double water_end=alpha_end*vol*s.water_density_kg_m3;
    const double air_end=(box_volume-alpha_end*vol)*s.air_density_kg_m3;
    near(water_end,water_initial,1e-8,
         "independent final 3D water mass conserved");
    near(air_end,air_initial,1e-9,
         "independent final 3D air mass conserved");
    check(max_true_viscous_velocity_difference>1e-10,
          "real 3D stress feedback must differ from inviscid baseline");
    check(total_viscous_work>1e-10,
          "nonzero accumulated physical viscosity work");
    check(max_wall_error<1e-10,"viscous + pressure wall impulse ledger");
    check(max_energy_error<1e-10,"viscous + pressure mechanical substep ledger");
    check(max_density_error<1e-9,"actual moving mixture matches density");
    std::cout<<std::setprecision(17)
             <<"LIVE_FULL_TENSOR grid="<<nx<<"x"<<ny<<"x"<<nz
             <<" steps=6 viscosity_effect_m_s="<<max_true_viscous_velocity_difference
             <<" stress_work_j="<<total_viscous_work
             <<" impulse_wall_error="<<max_wall_error
             <<" energy_substep_error="<<max_energy_error
             <<" density_error="<<max_density_error
             <<" full_phase=false\n";
}
void negative() {
    const auto valid=setup(6,5,4);
    auto bad=valid;
    const auto original=bad;
    bad.mac.u_m_s[0]=.05;
    const auto bad_input=bad.mac.u_m_s;
    rejects([&]{(void)p2_two_phase_momentum_viscous_vof_step3d(
        bad);},"real MAC wall leak fails");
    check(bad.mac.u_m_s==bad_input&&bad.water_fraction==original.water_fraction,
          "invalid wall flux preserves entire live VOF");
    bad=valid;
    rejects([&]{(void)p2_two_phase_momentum_viscous_vof_step3d(
        bad,1);},"underconverged pressure must fail");
    check(bad.mac.u_m_s==original.mac.u_m_s&&
          bad.mac.density_kg_m3==original.mac.density_kg_m3,
          "underconverged PCG preserves actual live density and velocity");
    bad=valid;
    rejects([&]{(void)p2_two_phase_momentum_viscous_vof_step3d(
        bad,4000,{0,0,std::numeric_limits<double>::quiet_NaN()});},
        "invalid gravity fails");
    check(bad.water_fraction==original.water_fraction,
          "invalid gravity does not mutate water");
    bad=valid;
    rejects([&]{(void)p2_two_phase_momentum_viscous_vof_step3d(
        bad,4000,{0,0,-9.81},0,.000018);},
        "zero water viscosity rejected");
    check(bad.mac.w_m_s==original.mac.w_m_s&&
          bad.water_fraction==original.water_fraction,
          "late viscosity rejection atomic");
}
}
int main(){
    try {
        run(4,4,4);
        run(6,5,4);
        run(8,6,5);
        negative();
        std::cout<<"PASS P2_TWO_PHASE_LIVE_FULL_TENSOR assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& ex) {
        std::cerr<<"FAIL P2_TWO_PHASE_LIVE_FULL_TENSOR "<<ex.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
