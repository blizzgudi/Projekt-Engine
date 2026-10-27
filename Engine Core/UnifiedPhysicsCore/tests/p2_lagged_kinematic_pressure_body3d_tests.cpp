// P2 PARTIAL: real kinematic 3D MAC pressure -> dynamic rigid predictor.
// The fluid does not receive the opposite impulse; no moving mask or FSI PASS.
#include "upc/lagged_kinematic_pressure_body3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double actual,double expected,double tol,const char* msg) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,msg);
}
using Vec=std::array<double,3>;
constexpr double dx=0.08,rho=1000.0,dt=0.025,mass=4.0,iz=0.2;
struct Scene {
    int nx,ny,nz;
    upc::MacProjection3D mac;
    std::vector<std::uint8_t> wet,solid,body;
    upc::RigidPressureState3D state;
    Scene(int x,int y,int z,double speed):nx(x),ny(y),nz(z),
        mac(x,y,z,dx),wet(static_cast<std::size_t>(x*y*z),0),
        solid(wet.size(),1),body(wet.size(),0) {
        const int mid=x/2,cy=y/2,cz=z/2;
        body[idx(mid-1,cy,cz)]=body[idx(mid,cy,cz)]=1;
        for(int ix:{mid-2,mid+1}) {
            wet[idx(ix,cy,cz)]=1;
            solid[idx(ix,cy,cz)]=0;
        }
        // Two independent wet pockets with one zero-pressure air outlet each.
        solid[idx(mid-3,cy,cz)]=0;
        solid[idx(mid+2,cy,cz)]=0;
        state.mass_kg=mass;
        state.inertia_world_kg_m2={{0.3,0.4,iz}};
        state.centre_m={{mid*dx,(cy+0.5)*dx+0.04,(cz+0.5)*dx}};
        state.velocity_m_s={{speed,0,0}};
    }
    std::size_t idx(int x,int y,int z)const {
        return static_cast<std::size_t>((z*ny+y)*nx+x);
    }
    std::vector<double> snapshot()const {
        std::vector<double> v;
        for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
        for(int x=0;x<=nx;++x)v.push_back(mac.face_velocity_x(x,y,z));
        for(int z=0;z<nz;++z)for(int y=0;y<=ny;++y)
        for(int x=0;x<nx;++x)v.push_back(mac.face_velocity_y(x,y,z));
        for(int z=0;z<=nz;++z)for(int y=0;y<ny;++y)
        for(int x=0;x<nx;++x)v.push_back(mac.face_velocity_z(x,y,z));
        return v;
    }
    upc::LaggedKinematicPressureBodyResult3D run(int budget=5000) {
        return upc::LaggedKinematicPressureBody3D::step(
            mac,nx,ny,nz,dx,rho,dt,wet,solid,body,state,budget);
    }
};
void analytic(int n,double speed) {
    Scene s(n,n,n,speed);
    const auto original=s.snapshot();
    const auto result=s.run();
    const auto& w=result.wall;
    const auto& b=result.body;
    const double p=rho*dx*speed/(2*dt);
    const double f=-2*p*dx*dx;
    const double torque=0.04*f; // r_y=-0.04, torque_z=-r_y*F_x
    const double j=f*dt;
    const double jz=torque*dt;
    const double after_v=speed+j/mass;
    const double after_w=jz/iz;
    const double displacement=0.5*dt*(speed+after_v);
    const double angular_displacement=0.5*dt*after_w;
    const double frozen_work=f*speed*dt;
    const double kinetic_work=f*displacement+torque*angular_displacement;
    const double gap=0.5*(j*j/mass+jz*jz/iz);
    const int mid=n/2,cy=n/2,cz=n/2;
    near(result.projection.pressure_pa[s.idx(mid-2,cy,cz)],-p,1e-9,
         "independent left wet pressure");
    near(result.projection.pressure_pa[s.idx(mid+1,cy,cz)],p,1e-9,
         "independent right wet pressure");
    check(result.projection.max_divergence_after_s_inv<1e-8,
          "MAC divergence after projection");
    check(result.projection.open_liquid_components==2,
          "two independent open wet components");
    check(w.wetted_faces==2,"two wetted body faces");
    near(w.force_N[0],f,1e-10,"pressure force");
    near(w.torque_Nm[2],torque,1e-11,"offset pressure torque");
    near(w.pressure_work_J,frozen_work,1e-11,"old-velocity wall work");
    near(b.body_linear_impulse_Ns[0],j,1e-11,"linear body impulse");
    near(b.body_angular_impulse_Nms[2],jz,1e-11,"angular body impulse");
    near(b.after.velocity_m_s[0],after_v,1e-11,"dynamic linear response");
    near(b.after.angular_velocity_rad_s[2],after_w,1e-11,
         "dynamic angular response");
    near(b.after.centre_m[0]-s.state.centre_m[0],displacement,1e-12,
         "predicted displacement only");
    near(b.kinetic_energy_change_J,kinetic_work,1e-11,
         "midpoint impulse kinetic identity");
    near(b.energy_residual_J,0,1e-11,"rigid midpoint work closure");
    near(result.lagged_work_gap_J,gap,1e-11,
         "old wall work differs from midpoint work by impulse squared");
    near(b.fluid_reaction_impulse_Ns[0],-j,1e-11,
         "unapplied opposite impulse is bookkeeping only");
    check(speed==0 ? s.snapshot()==original : s.snapshot()!=original,
          "MAC publication must match nonzero/zero wall speed");
    // The caller's body state and solid occupancy have NOT been changed.
    near(s.state.velocity_m_s[0],speed,0,"caller state remains unchanged");
    check(s.body[s.idx(mid,cy,cz)]==1,"body mask frozen");
    std::cout<<n<<","<<speed<<","<<f<<","<<torque<<","
             <<j<<","<<jz<<","<<after_v<<","<<after_w<<","
             <<frozen_work<<","<<kinetic_work<<","<<gap<<","
             <<result.projection.max_divergence_after_s_inv<<"\n";
}
void failures_are_atomic() {
    Scene s(8,8,8,0.12);
    // Use a connected 3D liquid volume: a one-iteration pressure solve
    // cannot converge here, unlike the analytic one-cell open pockets.
    s.wet.assign(s.wet.size(),1);
    s.solid.assign(s.solid.size(),0);
    for(std::size_t i=0;i<s.body.size();++i)if(s.body[i]) {
        s.wet[i]=0;s.solid[i]=1;
    }
    // Start with nonzero fluid faces so a failed trial cannot be mistaken
    // for an unchanged all-zero MAC grid.
    s.mac.set_face_velocity_x(2,2,2,0.037);
    const auto before=s.snapshot();
    bool rejected=false;
    try {(void)s.run(1);}
    catch(const std::runtime_error&){rejected=true;}
    check(rejected,"underconverged pressure trial must be rejected");
    check(s.snapshot()==before,"failed pressure solve changed fluid");
    s.state.mass_kg=0;
    rejected=false;
    try {(void)s.run();}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"zero mass rejected");
    check(s.snapshot()==before,"invalid mass changed fluid");
    s.state.mass_kg=mass;
    s.state.inertia_world_kg_m2[2]=0;
    rejected=false;
    try {(void)s.run();}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"zero inertia rejected");
    check(s.snapshot()==before,"invalid inertia changed fluid");
    s.state.inertia_world_kg_m2[2]=iz;
    s.state.velocity_m_s[0]=std::numeric_limits<double>::quiet_NaN();
    rejected=false;
    try {(void)s.run();}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"NaN body velocity rejected");
    check(s.snapshot()==before,"nonfinite velocity changed fluid");
    s.state.velocity_m_s[0]=0.12;
    s.body[s.idx(2,4,4)]=1; // body mask overlaps wet
    rejected=false;
    try {(void)s.run();}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"wet body overlap rejected");
    check(s.snapshot()==before,"invalid mask changed fluid");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17)
                 <<"grid,speed_m_s,force_x_N,torque_z_Nm,impulse_x_Ns,"
                   "angular_impulse_z_Nms,body_vx_after_m_s,body_omega_z_after_rad_s,"
                   "frozen_work_J,kinetic_work_J,lagged_gap_J,divergence_s_inv\n";
        for(int n:{8,12,16})for(double v:{0.12,-0.12,0.0})analytic(n,v);
        failures_are_atomic();
        std::cout<<"PASS: P2 lagged pressure rigid predictor assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
