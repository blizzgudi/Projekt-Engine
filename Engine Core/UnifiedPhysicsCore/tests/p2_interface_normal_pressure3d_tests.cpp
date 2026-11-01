// P2 partial: inhomogeneous liquid-side normal-pressure Dirichlet data
// at real fractional MAC/PCG water/air faces. No computed curvature,
// viscous normal stress, air flow, or full stress-continuity validation.
#include "upc/mac3d.hpp"
#include "upc/coupled_water3d.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
int assertions=0;
void check(bool ok,const char* why){++assertions;if(!ok)throw std::runtime_error(why);}
void near(double a,double b,double eps,const char* why){
    if(!std::isfinite(a)||std::abs(a-b)>eps)
        std::cerr<<"DIAGNOSTIC "<<why<<" actual="<<a<<" expected="<<b<<" eps="<<eps<<"\n";
    check(std::isfinite(a)&&std::abs(a-b)<=eps,why);
}
template<class F>void rejects(F fn){
    bool threw=false;try{fn();}catch(const std::exception&){threw=true;}
    check(threw,"bad boundary data must reject");
}
void isolated_cell(){
    constexpr double dx=.4,dt=.02,rho=997;
    upc::MacProjection3D m(5,5,5,dx,rho);
    m.set_cell(2,2,2,true);
    // Independent finite-volume reference: one wet cell surrounded by six
    // Dirichlet faces, A=sum(1/theta), b=sum(p_interface/theta).
    const std::array<double,6> theta{{.25,.5,.75,.25,.5,.75}};
    const std::array<double,6> pb{{100,400,800,-200,500,-100}};
    m.set_interface_distance_x(3,2,2,theta[0]);
    m.set_interface_distance_x(2,2,2,theta[1]);
    m.set_interface_distance_y(2,3,2,theta[2]);
    m.set_interface_distance_y(2,2,2,theta[3]);
    m.set_interface_distance_z(2,2,3,theta[4]);
    m.set_interface_distance_z(2,2,2,theta[5]);
    m.set_interface_pressure_x(3,2,2,pb[0]);
    m.set_interface_pressure_x(2,2,2,pb[1]);
    m.set_interface_pressure_y(2,3,2,pb[2]);
    m.set_interface_pressure_y(2,2,2,pb[3]);
    m.set_interface_pressure_z(2,2,3,pb[4]);
    m.set_interface_pressure_z(2,2,2,pb[5]);
    double numerator=0,denominator=0;
    for(int i=0;i<6;++i){numerator+=pb[i]/theta[i];denominator+=1/theta[i];}
    const double expected=numerator/denominator;
    const auto audit=m.project_pcg_verified(dt,100,1e-9,1e-12);
    near(m.pressure_pa(2,2,2),expected,1e-10,"independent weighted pressure");
    check(audit.liquid_cells==1&&audit.iterations==1,"one-cell PCG solve");
    const double scale=dt/(rho*dx);
    const std::array<double,6> faces{{
        m.face_velocity_x(3,2,2),m.face_velocity_x(2,2,2),
        m.face_velocity_y(2,3,2),m.face_velocity_y(2,2,2),
        m.face_velocity_z(2,2,3),m.face_velocity_z(2,2,2)}};
    for(int i=0;i<6;++i){
        const double sign=i%2==0?1.0:-1.0;
        near(faces[i],sign*scale*(expected-pb[i])/theta[i],1e-12,
             "manufactured signed interface pressure gradient");
    }
    near(m.divergence(2,2,2),0,1e-11,"discrete mass closure");
    near(audit.max_divergence_s_inv,0,1e-11,"verified PCG divergence");
    std::cout<<"NORMAL_PRESSURE_ISOLATED expected_pa="<<expected
             <<" divergence_s_inv="<<audit.max_divergence_s_inv
             <<" iterations="<<audit.iterations<<"\n";
    rejects([&]{m.set_interface_pressure_x(0,2,2,100);});
    rejects([&]{m.set_interface_pressure_y(2,0,2,100);});
    rejects([&]{m.set_interface_pressure_z(2,2,5,100);});
    rejects([&]{m.set_interface_pressure_x(3,2,2,
                      std::numeric_limits<double>::quiet_NaN());});
    rejects([&]{m.set_interface_pressure_y(2,3,2,
                      std::numeric_limits<double>::infinity());});
    rejects([&]{m.set_uniform_interface_pressure_pa(10);});
    m.clear_interface_pressures();
    m.set_uniform_interface_pressure_pa(0);
    rejects([&]{m.set_uniform_interface_pressure_pa(
                      std::numeric_limits<double>::infinity());});
}
void uniform_shift(int axis,bool reverse){
    constexpr int n=6;
    upc::MacProjection3D a(n,n,n,.2),b(n,n,n,.2);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        const int q=axis==0?x:axis==1?y:z;
        const bool liquid=reverse?q>=n-2:q<2;
        a.set_cell(x,y,z,liquid);
        b.set_cell(x,y,z,liquid);
    }
    b.set_uniform_interface_pressure_pa(650);
    const auto aa=a.project_pcg_verified(.01,500,1e-9,1e-12);
    const auto bb=b.project_pcg_verified(.01,500,1e-9,1e-12);
    near(aa.max_divergence_s_inv,0,1e-11,"default zero pressure divergence");
    near(bb.max_divergence_s_inv,0,1e-11,"constant pressure divergence");
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        const int q=axis==0?x:axis==1?y:z;
        const bool liquid=reverse?q>=n-2:q<2;
        if(liquid)near(b.pressure_pa(x,y,z)-a.pressure_pa(x,y,z),650,1e-8,
                       "uniform air stress must shift liquid pressure");
    }
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        near(b.face_velocity_x(x,y,z),a.face_velocity_x(x,y,z),1e-10,
             "constant stress x velocity invariance");
        near(b.face_velocity_y(x,y,z),a.face_velocity_y(x,y,z),1e-10,
             "constant stress y velocity invariance");
        near(b.face_velocity_z(x,y,z),a.face_velocity_z(x,y,z),1e-10,
             "constant stress z velocity invariance");
    }
}
void live_water(){
    upc::CoupledMacWater3D a(6,6,8,.1),b(6,6,8,.1);
    for(int z=0;z<4;++z)for(int y=0;y<6;++y)for(int x=0;x<6;++x){
        a.set_water(x,y,z,1);
        b.set_water(x,y,z,1);
    }
    const double mass=a.mass_kg();
    rejects([&]{b.set_uniform_air_interface_pressure_pa(650);});
    a.set_pcg_pressure_projection(true);
    b.set_pcg_pressure_projection(true);
    b.set_uniform_air_interface_pressure_pa(650);
    const double da=a.step_verified_reprojected(.01,1500,1e-7);
    const double db=b.step_verified_reprojected(.01,1500,1e-7);
    near(a.mass_kg(),mass,1e-8,"live default water mass");
    near(b.mass_kg(),mass,1e-8,"live pressure-shift water mass");
    near(da,db,1e-8,"live pressure shift divergence");
    for(int z=0;z<8;++z)for(int y=0;y<6;++y)for(int x=0;x<6;++x){
        near(a.water_fraction(x,y,z),b.water_fraction(x,y,z),1e-9,
             "live VOF invariant under uniform interface stress");
        if(a.water_fraction(x,y,z)>.5)
            near(b.pressure_pa(x,y,z)-a.pressure_pa(x,y,z),650,1e-5,
                 "live PCG pressure jump");
    }
    std::cout<<"NORMAL_PRESSURE_LIVE mass_kg="<<mass
             <<" divergence_default="<<da<<" divergence_shifted="<<db<<"\n";
}
}
int main(){
    try{
        isolated_cell();
        for(int axis=0;axis<3;++axis)for(bool reverse:{false,true})
            uniform_shift(axis,reverse);
        live_water();
        std::cout<<"PASS P2_INTERFACE_NORMAL_PRESSURE assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_INTERFACE_NORMAL_PRESSURE "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
