#include "upc/p2_two_phase_capillary_global_energy3d.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace upc;
namespace {
int assertions=0;
void require(bool ok,const char* why){++assertions;if(!ok)throw std::runtime_error(why);}
void near(double a,double b,double tol,const char* why){
    if(!(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol)){
        std::cerr<<std::setprecision(17)<<why<<" got="<<a<<" expected="<<b<<"\n";
        throw std::runtime_error(why);
    }
    ++assertions;
}
template<class F>void rejected(F f,const char* why){
    bool did=false;try{f();}catch(const std::exception&){did=true;}
    require(did,why);
}
P2TwoPhaseMacVofState3D setup(int n,double dx,bool droplet){
    P2TwoPhaseMacVofState3D s;auto& m=s.mac;
    m.nx=m.ny=m.nz=n;m.dx_m=dx;m.dt_s=.0005;
    m.u_m_s.assign((n+1)*n*n,0);
    m.v_m_s.assign(n*(n+1)*n,0);
    m.w_m_s.assign(n*n*(n+1),0);
    m.density_kg_m3.resize(n*n*n);
    s.water_fraction.resize(n*n*n);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        const double px=((x+.5)-.5*n)*dx,py=((y+.5)-.5*n)*dx,
                     pz=((z+.5)-.5*n)*dx;
        const double r=std::sqrt(px*px+py*py+pz*pz);
        const double a=droplet?
            std::max(0.,std::min(1.,.5+(.31-r)/(2*dx))):
            (x+.5)/double(n);
        const int i=(z*n+y)*n+x;
        s.water_fraction[i]=a;
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*a;
    }
    return s;
}
void manufactured(){
    for(int n:{4,7,10}){
        const double dx=.1;
        auto s=setup(n,dx,false);
        const auto e=p2_capillary_energy_inventory3d(s,.072,{0,0,0});
        const double side=n*dx,vol=side*side*side;
        near(e.diffuse_area_m2,side*side,1e-13,"independent planar TV area");
        near(e.surface_energy_proxy_j,.072*side*side,1e-13,"planar energy");
        near(e.water_mass_kg,500*vol,1e-11,"water inventory");
        near(e.air_mass_kg,.6*vol,1e-12,"air inventory");
        near(e.mac_kinetic_j,0,0,"zero MAC kinetic");
        near(e.gravity_potential_j,0,0,"zero potential");
        require(e.total_proxy_j>0,"positive interface energy");
        std::cout<<std::setprecision(17)<<"CAP_ENERGY_PLANAR n="<<n
                 <<" area="<<e.diffuse_area_m2<<" analytic="<<side*side
                 <<" phase_complete=false\n";
    }
}
void live(){
    auto s=setup(6,.15,true);
    const auto initial=s;
    const auto e=p2_capillary_global_energy_step3d(
        s,.072,4000,{0,0,-.1});
    const double initial_water= [&](){
        double v=0;for(double a:initial.water_fraction)
            v+=a*initial.water_density_kg_m3*std::pow(.15,3);
        return v;
    }();
    near(e.before.water_mass_kg,initial_water,1e-10,
         "independent pre-VOF mass quadrature");
    near(e.water_mass_residual_kg,0,1e-9,"water mass");
    near(e.air_mass_residual_kg,0,1e-9,"air mass");
    near(e.surface_area_delta_m2,
         e.after.diffuse_area_m2-e.before.diffuse_area_m2,1e-13,"area delta");
    near(e.surface_energy_delta_j,.072*e.surface_area_delta_m2,
         1e-12,"surface energy delta");
    near(e.total_energy_delta_j,e.kinetic_delta_j+
         e.potential_delta_j+e.surface_energy_delta_j,1e-10,
         "independent 3-channel energy partition");
    near(e.unresolved_global_energy_j,
         e.total_energy_delta_j+e.final_viscous_pressure_loss_j,
         1e-12,"explicit unclosed energy gap");
    near(e.after.gravity_potential_j-e.before.gravity_potential_j,
         e.live.live.momentum.fluid.potential_delta_direct_j,1e-9,
         "independent live gravity potential");
    require(e.live.capillary_applied,"real capillary source active");
    require(s.water_fraction!=initial.water_fraction,"real moving VOF");
    require(!e.full_P2_accepted&&!e.global_physical_energy_closed,
            "no fabricated global closure");
    require(std::isfinite(e.unresolved_global_energy_j),
            "finite measured energy gap");
    std::cout<<std::setprecision(17)<<"CAP_ENERGY_LIVE"
             <<" before_j="<<e.before.total_proxy_j
             <<" after_j="<<e.after.total_proxy_j
             <<" surface_delta_j="<<e.surface_energy_delta_j
             <<" kinetic_delta_j="<<e.kinetic_delta_j
             <<" potential_delta_j="<<e.potential_delta_j
             <<" final_loss_j="<<e.final_viscous_pressure_loss_j
             <<" unresolved_j="<<e.unresolved_global_energy_j
             <<" water_residual_kg="<<e.water_mass_residual_kg
             <<" air_residual_kg="<<e.air_mass_residual_kg
             <<" phase_complete=false\n";
}
void negatives(){
    auto s=setup(6,.15,true);
    const auto initial=s;
    rejected([&]{p2_capillary_global_energy_step3d(s,.072,1,{0,0,-.1});},
             "underiterated pressure PCG fails closed");
    require(s.water_fraction==initial.water_fraction&&
            s.mac.u_m_s==initial.mac.u_m_s,"PCG rollback");
    rejected([&]{p2_capillary_global_energy_step3d(s,-.01);},
             "negative tension");
    rejected([&]{p2_capillary_global_energy_step3d(s,.072,4000,
        {0,0,std::numeric_limits<double>::quiet_NaN()});},
             "NaN gravity");
    s.water_fraction[0]=1.1;
    rejected([&]{p2_capillary_energy_inventory3d(s,.072);},
             "invalid VOF rejected");
    s=initial;s.mac.u_m_s[10]=std::numeric_limits<double>::infinity();
    rejected([&]{p2_capillary_energy_inventory3d(s,.072);},
             "nonfinite face velocity rejected");
    std::cout<<"CAP_ENERGY_NEGATIVE invalid_vof=reject"
             <<" nonfinite_velocity=reject underiterated_pcg=rollback"
             <<" phase_complete=false\n";
}
}
int main(){
    try{manufactured();live();negatives();
        std::cout<<"PASS P2_CAPILLARY_GLOBAL_ENERGY assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_CAPILLARY_GLOBAL_ENERGY "<<e.what()<<"\n";
        return 1;
    }
}
