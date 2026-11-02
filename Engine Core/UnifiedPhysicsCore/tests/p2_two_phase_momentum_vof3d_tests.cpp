#include "upc/p2_two_phase_momentum_vof3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace upc;
namespace {
std::size_t assertions=0;
void check(bool condition,const char* msg){++assertions;if(!condition)throw std::runtime_error(msg);}
void near(double a,double b,double eps,const char* msg) {
    if(!(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=eps)) {
        std::cerr<<std::setprecision(17)<<"MISMATCH "<<msg
                 <<" actual="<<a<<" expect="<<b<<" eps="<<eps<<"\n";
    }
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=eps,msg);
}
std::size_t ci(int x,int y,int z,int nx,int ny){return (z*ny+y)*nx+x;}
P2TwoPhaseMacVofState3D make(int nx,int ny,int nz) {
    P2TwoPhaseMacVofState3D s;auto& m=s.mac;
    m.nx=nx;m.ny=ny;m.nz=nz;m.dx_m=.1;m.dt_s=.003;
    m.density_kg_m3.resize(nx*ny*nz);
    m.u_m_s.assign((nx+1)*ny*nz,0);
    m.v_m_s.assign(nx*(ny+1)*nz,0);
    m.w_m_s.assign(nx*ny*(nz+1),0);
    s.water_fraction.resize(nx*ny*nz);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto i=ci(x,y,z,nx,ny);
        const double a=(z<nz/2+(x<nx/2?1:0) ? .7:.03)+
                       (y<ny/2?.02:0);
        s.water_fraction[i]=a;
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*a;
    }
    const double pi=3.14159265358979323846;
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
    auto state=make(nx,ny,nz);
    auto no_advection=state;
    const auto base=state;
    const std::array<double,3> g{.7,-.4,-9.81};
    const double initial_water=std::accumulate(
        state.water_fraction.begin(),state.water_fraction.end(),0.0);
    double max_velocity_effect=0,max_cell_flux_impulse=0;
    double max_unclosed_impulse=0,max_density_error=0;
    double max_loss=0;
    for(int step=0;step<6;++step) {
        const auto original=state;
        const auto adv=p2_two_phase_momentum_vof_step3d(state,4000,g);
        const auto simple=p2_two_phase_mac_vof_step3d(no_advection,4000,g);
        (void)simple;
        check(adv.fluid.pressure.divergence_after_s_inv<1e-9,
              "initial moving MAC projection diverged");
        check(adv.feedback_projection.divergence_after_s_inv<1e-9,
              "feedback moving MAC projection diverged");
        check(adv.convective.max_divergence_s_inv<1e-9,
              "shared mass-flux divergence not zero");
        check(adv.convective.upwind_kinetic_loss_j>=-1e-10,
              "conservative cell momentum donor cannot create kinetic energy");
        near(adv.convective.mass_before_kg,adv.convective.mass_after_kg,
             2e-10,"mass-conservative momentum advection");
        near(adv.convective.water_mass_before_kg,
             adv.convective.water_mass_after_kg,
             2e-10,"water mass conservative");
        near(adv.convective.air_mass_before_kg,
             adv.convective.air_mass_after_kg,
             2e-10,"air mass conservative");
        near(adv.fluid.total_mass_residual_kg,0,2e-10,
             "same physical water/air mass in real live VOF");
        for(int k=0;k<3;++k) {
            max_cell_flux_impulse=std::max(max_cell_flux_impulse,
                std::abs(adv.convective.internal_flux_impulse_residual_kg_m_s[k]));
            near(adv.convective.internal_flux_impulse_residual_kg_m_s[k],0,1e-10,
                 "conservative cell momentum flux impulse mismatch");
            max_unclosed_impulse=std::max(max_unclosed_impulse,
                std::abs(adv.remap_plus_feedback_impulse_change_kg_m_s[k]));
        }
        max_loss=std::max(max_loss,adv.convective.upwind_kinetic_loss_j);
        max_density_error=std::max(max_density_error,
                                   adv.max_density_agreement_kg_m3);
        for(std::size_t i=0;i<state.water_fraction.size();++i) {
            near(state.mac.density_kg_m3[i],
                 state.air_density_kg_m3+
                 (state.water_density_kg_m3-state.air_density_kg_m3)*
                 state.water_fraction[i],1e-9,
                 "true live two-phase pressure used remapped density");
            check(state.water_fraction[i]>=0&&state.water_fraction[i]<=1,
                  "strictly bounded live moving water fraction");
        }
        for(std::size_t i=0;i<state.mac.u_m_s.size();++i)
            max_velocity_effect=std::max(max_velocity_effect,
                std::abs(state.mac.u_m_s[i]-no_advection.mac.u_m_s[i]));
        const double total_alpha=std::accumulate(
            state.water_fraction.begin(),state.water_fraction.end(),0.0);
        near(total_alpha,initial_water,1e-10,
             "six-step moving two-phase water inventory");
        check(!adv.cell_to_mac_is_conservative,
              "must NOT claim physical MAC interpolation impulse closure");
        (void)original;
    }
    check(max_velocity_effect>1e-6,"cell momentum not fed back into MAC flow");
    check(max_unclosed_impulse>0,"must expose unresolved MAC remap impulse");
    check(max_density_error<1e-10,
          "mass consistent with independently advected water/air");
    check(state.water_fraction!=base.water_fraction,
          "live two-phase interface must actually move");
    std::cout<<std::setprecision(17)
             <<"LIVE_MOMENTUM grid="<<nx<<"x"<<ny<<"x"<<nz
             <<" steps=6 max_face_feedback_m_s="<<max_velocity_effect
             <<" cell_flux_impulse_error="<<max_cell_flux_impulse
             <<" unclosed_mac_impulse_kg_m_s="<<max_unclosed_impulse
             <<" donor_kinetic_loss_j="<<max_loss
             <<" density_error_kg_m3="<<max_density_error
             <<" full_phase=false\n";
}
void negative() {
    auto valid=make(6,5,4);
    auto bad=valid;
    bad.mac.u_m_s[0]=.1;
    const auto before=bad;
    bool fails=false;
    try{(void)p2_two_phase_momentum_vof_step3d(bad);}
    catch(const std::exception&){fails=true;}
    check(fails&&bad.mac.u_m_s==before.mac.u_m_s&&
          bad.water_fraction==before.water_fraction,
          "wall flux rejection must be transactional");
    bad=valid;
    const auto before_iters=bad;
    fails=false;
    try{(void)p2_two_phase_momentum_vof_step3d(bad,1);}
    catch(const std::exception&){fails=true;}
    check(fails&&bad.mac.density_kg_m3==before_iters.mac.density_kg_m3&&
          bad.mac.v_m_s==before_iters.mac.v_m_s,
          "PCG underiteration must preserve state");
    bad=valid;
    fails=false;
    try{(void)p2_two_phase_momentum_vof_step3d(
        bad,4000,{0,0,std::numeric_limits<double>::quiet_NaN()});}
    catch(const std::exception&){fails=true;}
    check(fails&&bad.water_fraction==valid.water_fraction,
          "NaN gravity fail closed");
}
}
int main() {
    try {
        run(4,4,4);
        run(6,5,4);
        run(8,6,5);
        negative();
        std::cout<<"PASS P2_TWO_PHASE_LIVE_MOMENTUM assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_TWO_PHASE_LIVE_MOMENTUM "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
