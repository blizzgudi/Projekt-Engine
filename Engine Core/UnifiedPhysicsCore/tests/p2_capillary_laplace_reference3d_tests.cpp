// P2 PARTIAL: independent discrete Young-Laplace reference, not a full
// stress-consistent moving-interface or experimental CFD acceptance.
#include "upc/p2_capillary_laplace_reference3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace upc;
namespace {
std::size_t assertions=0;
void check(bool ok,const char* msg){
    ++assertions;if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double tolerance,const char* msg){
    if(!(std::isfinite(a)&&std::abs(a-b)<=tolerance))
        std::cerr<<std::setprecision(17)<<"MISMATCH "<<msg
                 <<" actual="<<a<<" expected="<<b<<"\n";
    check(std::isfinite(a)&&std::abs(a-b)<=tolerance,msg);
}
template<class F>void reject(F fn,const char* msg){
    bool threw=false;
    try{fn();}catch(const std::exception&){threw=true;}
    check(threw,msg);
}
P2TwoPhaseMacVofState3D make(int n,double dx){
    P2TwoPhaseMacVofState3D s;
    auto& m=s.mac;
    m.nx=m.ny=m.nz=n;m.dx_m=dx;m.dt_s=.0005;
    m.density_kg_m3.resize(n*n*n);
    m.u_m_s.assign((n+1)*n*n,0);
    m.v_m_s.assign(n*(n+1)*n,0);
    m.w_m_s.assign(n*n*(n+1),0);
    s.water_fraction.resize(n*n*n);
    const double center=n*dx*.5;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x){
        const double X=(x+.5)*dx-center;
        const double Y=(y+.5)*dx-center;
        const double Z=(z+.5)*dx-center;
        const double r=std::sqrt(X*X+Y*Y+Z*Z);
        const double alpha=std::clamp(.5+(.31-r)/(2*dx),0.,1.);
        const auto i=(z*n+y)*n+x;
        s.water_fraction[i]=alpha;
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*alpha;
    }
    return s;
}
void reference(int n,double dx){
    auto s=make(n,dx);
    const auto unchanged=s;
    const auto a=P2CapillaryLaplaceReference3D::audit(s,.072,.31);
    check(!a.phase_complete&&!a.measured_cfd_validated,
          "manufactured reference must not claim P2 full");
    check(a.pure_water_cells>0&&a.pure_air_cells>0,
          "resolved two-phase pure cells");
    check(a.max_predicted_velocity_m_s>1e-9,
          "nontrivial exact-curvature source");
    near(a.analytic_jump_pa,2*.072/.31,1e-14,
         "Young-Laplace analytic jump");
    near(a.recovered_pure_phase_jump_pa,a.analytic_jump_pa,1e-7,
         "actual PCG pressure jump matches Young-Laplace");
    check(a.max_pressure_field_error_pa<1e-7,
          "manufactured pressure field reference");
    check(a.max_post_projection_velocity_m_s<1e-8,
          "balanced exact-curvature source cancels after PCG");
    check(a.pressure.divergence_after_s_inv<1e-9,
          "post-pressure 3D divergence");
    near(a.pressure.energy_residual_j,0,1e-10,
         "independent PCG projection energy identity");
    for(double r:a.pressure.momentum_residual_n_s)
        near(r,0,1e-10,"independent wall impulse identity");
    check(s.water_fraction==unchanged.water_fraction&&
          s.mac.u_m_s==unchanged.mac.u_m_s&&
          s.mac.density_kg_m3==unchanged.mac.density_kg_m3,
          "reference never mutates original solver state");
    const auto diffuse=p2_two_phase_capillary_project3d(s,.072);
    check(diffuse.max_post_projection_velocity_m_s>
          a.max_post_projection_velocity_m_s+1e-8,
          "diffuse curvature creates measurable extra spurious current");
    std::cout<<std::setprecision(17)
             <<"LAPLACE_REFERENCE grid="<<n<<"x"<<n<<"x"<<n
             <<" dx_m="<<dx
             <<" analytic_jump_pa="<<a.analytic_jump_pa
             <<" measured_jump_pa="<<a.recovered_pure_phase_jump_pa
             <<" max_pressure_error_pa="<<a.max_pressure_field_error_pa
             <<" ideal_post_velocity_m_s="<<a.max_post_projection_velocity_m_s
             <<" diffuse_post_velocity_m_s="<<diffuse.max_post_projection_velocity_m_s
             <<" pure_water="<<a.pure_water_cells
             <<" pure_air="<<a.pure_air_cells
             <<" phase_complete=false\n";
}
void negatives(){
    const auto base=make(8,.15);
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(base,-.072,.31);},
           "negative sigma rejected");
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(
        base,std::numeric_limits<double>::quiet_NaN(),.31);},
           "NaN sigma rejected");
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(base,.072,0);},
           "zero radius rejected");
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(base,.072,
        std::numeric_limits<double>::infinity());},
           "infinite radius rejected");
    auto bad=base;bad.mac.density_kg_m3[0]*=1.2;
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(bad,.072,.31);},
           "inconsistent phase density rejected");
    bad=base;bad.mac.u_m_s[1]=.1;
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(bad,.072,.31);},
           "nonrest initial velocity rejected");
    bad=base;bad.mac.dt_s=1.;
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(bad,.072,.31);},
           "unstable capillary timestep rejected");
    bad=base;bad.water_fraction.assign(bad.water_fraction.size(),.5);
    for(auto& rho:bad.mac.density_kg_m3)
        rho=.5*(bad.water_density_kg_m3+bad.air_density_kg_m3);
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(bad,.072,.31);},
           "unresolved pure phases rejected");
    bad=base;
    reject([&]{(void)P2CapillaryLaplaceReference3D::audit(bad,.072,.31,1);},
           "underconverged PCG rejected");
    check(base.mac.u_m_s[1]==0&&base.water_fraction[0]>=0,
          "failed diagnostics do not mutate caller");
}
}
int main(){
    try{
        reference(8,.15);
        reference(12,.1);
        reference(16,.075);
        negatives();
        std::cout<<"PASS P2_LAPLACE_REFERENCE assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_LAPLACE_REFERENCE "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
