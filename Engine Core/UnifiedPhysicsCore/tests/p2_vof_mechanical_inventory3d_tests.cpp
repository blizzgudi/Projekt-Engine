// P2 PARTIAL: cell-centred VOF-mass-weighted water mechanical inventory.
// Analytic manufactured velocities, dry-face rejection and live MAC/VOF checks.
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int assertions=0;
void need(bool yes,const char* why) {
    ++assertions;
    if(!yes)throw std::runtime_error(why);
}
void near(double got,double expected,double tol,const char* why) {
    need(std::isfinite(got)&&std::abs(got-expected)<=tol,why);
}
template<class Fn> void rejects(Fn fn,const char* why) {
    bool rejected=false;
    try {fn();}catch(const std::invalid_argument&){rejected=true;}
    need(rejected,why);
}
void audit_row(const char* mode,int n,const upc::VofMechanicalDeltaAudit3D& d){
    std::cout<<"audit,"<<mode<<","<<n<<","<<d.mass_delta_kg
             <<","<<d.momentum_delta_kg_m_s[0]
             <<","<<d.momentum_delta_kg_m_s[1]
             <<","<<d.momentum_delta_kg_m_s[2]
             <<","<<d.kinetic_delta_j<<","<<d.gravitational_delta_j
             <<","<<d.reported_transport_gravitational_delta_j
             <<","<<d.gravitational_transport_residual_j
             <<","<<d.mechanical_delta_j
             <<","<<d.nontransport_mechanical_delta_j<<"\n";
}
void synthetic(int n) {
    constexpr double dx=.1,rho=1000;
    upc::MacProjection3D mac(n,n,n,dx,rho);
    std::vector<double> f(n*n*n,0);
    std::vector<std::uint8_t> solid(n*n*n,0);
    auto idx=[&](int x,int y,int z){return (z*n+y)*n+x;};
    f[idx(2,2,2)]=.25;
    f[idx(5,5,5)]=.75;
    solid[idx(4,4,4)]=1;
    auto velocity=[&](int x,int y,int z,double u,double v,double w) {
        mac.set_face_velocity_x(x,y,z,u);
        mac.set_face_velocity_x(x+1,y,z,u);
        mac.set_face_velocity_y(x,y,z,v);
        mac.set_face_velocity_y(x,y+1,z,v);
        mac.set_face_velocity_z(x,y,z,w);
        mac.set_face_velocity_z(x,y,z+1,w);
    };
    velocity(2,2,2,2,-1,.5);
    velocity(5,5,5,-1,0,-2);
    mac.set_face_velocity_x(0,0,0,1000); // dry ghost face excluded
    const auto a=upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,rho);
    constexpr double mass=1.0, px=-.25,py=-.25,pz=-1.375;
    constexpr double kinetic=2.53125, potential=4.65975;
    near(a.mass_kg,mass,1e-13,"analytic VOF mass");
    near(a.momentum_kg_m_s[0],px,1e-13,"analytic momentum x");
    near(a.momentum_kg_m_s[1],py,1e-13,"analytic momentum y");
    near(a.momentum_kg_m_s[2],pz,1e-13,"analytic momentum z");
    near(a.kinetic_j,kinetic,1e-13,"analytic kinetic energy");
    near(a.gravitational_j,potential,1e-13,"analytic potential energy");
    near(a.total_j,kinetic+potential,1e-13,"analytic total");
    need(a.occupied_cells==2,"two occupied cells");
    mac.set_face_velocity_x(0,0,0,-5000);
    const auto dry=upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,rho);
    near(dry.kinetic_j,a.kinetic_j,0,"dry face excluded");
    near(dry.momentum_kg_m_s[0],a.momentum_kg_m_s[0],0,"dry face momentum excluded");
    // Manufacture an isolated 0.125 kg upward VOF transfer at fixed
    // cell-centred velocities.  Momentum and KE change because the
    // transported mass is sampled at a different velocity.
    f[idx(2,2,2)]=.125;
    f[idx(5,5,5)]=.875;
    const auto moved=upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,rho);
    constexpr double transport_potential=.125*9.81*.3;
    const auto audit=upc::vof_mechanical_delta_audit3d(a,moved,transport_potential);
    near(audit.mass_delta_kg,0,1e-13,"analytic transport mass delta");
    near(audit.momentum_delta_kg_m_s[0],-.375,1e-13,"analytic transport px");
    near(audit.momentum_delta_kg_m_s[1],.125,1e-13,"analytic transport py");
    near(audit.momentum_delta_kg_m_s[2],-.3125,1e-13,"analytic transport pz");
    near(audit.kinetic_delta_j,-.015625,1e-13,"analytic kinetic redistribution");
    near(audit.gravitational_delta_j,transport_potential,1e-13,
         "analytic gravitational redistribution");
    near(audit.gravitational_transport_residual_j,0,1e-13,
         "analytic potential ledger closure");
    near(audit.mechanical_delta_j,.35225,1e-13,"analytic mechanical delta");
    near(audit.nontransport_mechanical_delta_j,-.015625,1e-13,
         "analytic nontransport change");
    const auto wrong=upc::vof_mechanical_delta_audit3d(a,moved,0);
    near(wrong.gravitational_transport_residual_j,transport_potential,1e-13,
         "missing transfer ledger must be observable");
    rejects([&]{(void)upc::vof_mechanical_delta_audit3d(
        a,moved,std::numeric_limits<double>::quiet_NaN());},
        "nonfinite transport ledger rejected");
    auto invalid=a;invalid.mass_kg=-1;
    rejects([&]{(void)upc::vof_mechanical_delta_audit3d(
        invalid,moved,transport_potential);},"invalid inventory rejected");
    audit_row("synthetic",n,audit);
    f[idx(2,2,2)]=.25;
    f[idx(5,5,5)]=.75;
    f[idx(2,2,2)]=-0.1;
    rejects([&]{(void)upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,rho);},"negative VOF rejected");
    f[idx(2,2,2)]=1.1;
    rejects([&]{(void)upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,rho);},"overfull VOF rejected");
    f[idx(2,2,2)]=.25;
    f[idx(4,4,4)]=.1;
    rejects([&]{(void)upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,rho);},"solid overlap rejected");
    f[idx(4,4,4)]=0;
    rejects([&]{(void)upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,0,rho);},"zero dx rejected");
    rejects([&]{(void)upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,-1);},"negative rho rejected");
    rejects([&]{(void)upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,
            std::numeric_limits<double>::infinity());},"infinite rho rejected");
    f.pop_back();
    rejects([&]{(void)upc::vof_mechanical_inventory3d(mac,f,solid,n,n,n,dx,rho);},"mismatched size rejected");
    std::cout<<"synthetic,"<<n<<","<<a.mass_kg<<","<<a.momentum_kg_m_s[0]
             <<","<<a.momentum_kg_m_s[1]<<","<<a.momentum_kg_m_s[2]
             <<","<<a.kinetic_j<<","<<a.gravitational_j<<","<<a.total_j
             <<","<<a.occupied_cells<<"\n";
}
void live(int n) {
    constexpr double dx=.1,rho=1000,dt=.01;
    const int c=n/2;
    upc::CoupledMacWater3D water(n,n,n,dx,rho);
    water.set_water(c,c,c,1);
    water.set_water(c,c,c+1,1);
    const auto initial=water.water_mechanical_inventory();
    near(initial.mass_kg,2,1e-13,"live initial mass");
    near(initial.kinetic_j,0,0,"live initial kinetic zero");
    near(initial.gravitational_j,water.water_gravitational_potential_j(),
         1e-12,"live initial potential parity");
    need(initial.occupied_cells==2,"live two cells");
    const double div=water.step_verified_reprojected(dt,4000,1e-8);
    const auto after=water.water_mechanical_inventory();
    near(after.mass_kg,water.mass_kg(),1e-10,"live mass parity");
    near(after.gravitational_j,water.water_gravitational_potential_j(),
         1e-10,"live potential parity");
    near(after.total_j,after.kinetic_j+after.gravitational_j,
         1e-12,"live mechanical sum");
    need(after.kinetic_j>=0&&std::isfinite(after.kinetic_j),
         "live nonnegative finite KE");
    need(div<=1e-8,"live projection divergence");
    const auto delta=upc::vof_mechanical_delta_audit3d(
        initial,after,water.last_vof_transport_potential_delta_j());
    near(delta.mass_delta_kg,0,1e-10,"live mass delta");
    for(int axis=0;axis<3;++axis)
        near(delta.momentum_delta_kg_m_s[axis],
             after.momentum_kg_m_s[axis]-initial.momentum_kg_m_s[axis],
             1e-12,"live momentum delta");
    near(delta.gravitational_transport_residual_j,0,1e-10,
         "live VOF potential transfer parity");
    near(delta.nontransport_mechanical_delta_j,delta.kinetic_delta_j,1e-10,
         "live unresolved mechanical delta is sampled KE delta");
    audit_row("live",n,delta);
    const auto snapshot=water.water_mechanical_inventory();
    near(snapshot.kinetic_j,after.kinetic_j,0,"read-only snapshot");
    bool rolled=false;
    try {water.step_verified_reprojected(.02,1,1e-14);}
    catch(const std::runtime_error&){rolled=true;}
    need(rolled,"underconvergence must roll back");
    const auto unchanged=water.water_mechanical_inventory();
    near(unchanged.kinetic_j,snapshot.kinetic_j,0,"rollback kinetic");
    near(unchanged.gravitational_j,snapshot.gravitational_j,0,"rollback potential");
    near(unchanged.mass_kg,snapshot.mass_kg,0,"rollback mass");
    std::cout<<"live,"<<n<<","<<after.mass_kg<<","
             <<after.momentum_kg_m_s[0]<<","<<after.momentum_kg_m_s[1]
             <<","<<after.momentum_kg_m_s[2]<<","<<after.kinetic_j<<","<<after.gravitational_j<<","
             <<after.total_j<<","<<after.occupied_cells<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,mass,px,py,pz,kinetic,potential,total,occupied\n";
        synthetic(8);synthetic(12);live(8);live(12);
        std::cout<<"PASS: vof mechanical inventory assertions="<<assertions
                 <<" phase_complete=false\n";
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
    return 0;
}
