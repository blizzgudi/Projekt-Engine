// P2 PARTIAL: frozen pressure-wrench response from a REAL projected 3D MAC
// field. The reaction impulse is only a ledger; the fluid is not modified.
#include "upc/rigid_pressure_response3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool value,const char* msg) {
    ++assertions;
    if(!value)throw std::runtime_error(msg);
}
template<class F> void rejects(F&& f,const char* msg) {
    bool caught=false;
    try {f();}catch(const std::invalid_argument&) {caught=true;}
    check(caught,msg);
}
template<class F> void overflows(F&& f,const char* msg) {
    bool caught=false;
    try {f();}catch(const std::overflow_error&) {caught=true;}
    check(caught,msg);
}
void near(double actual,double expected,double tol,const char* msg) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<tol,msg);
}
void run(int n,bool closed,double offset) {
    const int nx=n,ny=n-1,nz=n-2;
    const int ox=nx/2-1,oy=ny/2-1,oz=nz/2-1;
    constexpr double dx=0.08,projection_dt=0.02,rho=1000.0;
    constexpr double step_dt=0.05;
    constexpr std::array<double,3> slopes{{7.0,-11.0,5.0}};
    const auto idx=[&](int x,int y,int z) {
        return static_cast<std::size_t>((z*ny+y)*nx+x);
    };
    const auto inside=[&](int x,int y,int z) {
        return x>=0&&y>=0&&z>=0&&x<nx&&y<ny&&z<nz;
    };
    const auto in_body=[&](int x,int y,int z) {
        return x>=ox&&x<=ox+1&&y>=oy&&y<=oy+1&&z>=oz&&z<=oz+1;
    };
    std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0),
                              body(nx*ny*nz,0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const bool wall=x==0||y==0||y==ny-1||z==0||z==nz-1||
                        (closed&&x==nx-1)||in_body(x,y,z);
        solid[idx(x,y,z)]=static_cast<std::uint8_t>(wall);
        wet[idx(x,y,z)]=static_cast<std::uint8_t>(!wall&&(closed||x<nx-2));
        body[idx(x,y,z)]=static_cast<std::uint8_t>(in_body(x,y,z));
    }
    const auto liquid=[&](int x,int y,int z) {
        return inside(x,y,z)&&wet[idx(x,y,z)]!=0;
    };
    const auto wall=[&](int x,int y,int z) {
        return !inside(x,y,z)||solid[idx(x,y,z)]!=0;
    };
    const auto p=[&](int x,int y,int z) {
        return liquid(x,y,z)?
            offset+slopes[0]*x+slopes[1]*y+slopes[2]*z:0.0;
    };
    const auto velocity=[&](int ax,int ay,int az,int bx,int by,int bz) {
        if(wall(ax,ay,az)||wall(bx,by,bz))return 0.0;
        const bool a=liquid(ax,ay,az),b=liquid(bx,by,bz);
        if(!a&&!b)return 0.0;
        return projection_dt/(rho*dx)*(a!=b?2.0:1.0)*
               (p(bx,by,bz)-p(ax,ay,az));
    };
    upc::MacProjection3D mac(nx,ny,nz,dx,rho);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_cell(x,y,z,wet[idx(x,y,z)]!=0,solid[idx(x,y,z)]!=0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<=nx;++x)
        mac.set_face_velocity_x(x,y,z,velocity(x-1,y,z,x,y,z));
    for(int z=0;z<nz;++z)for(int y=0;y<=ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_face_velocity_y(x,y,z,velocity(x,y-1,z,x,y,z));
    for(int z=0;z<=nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_face_velocity_z(x,y,z,velocity(x,y,z-1,x,y,z));
    check(mac.max_divergence()>1e-4,"manufactured MAC field must diverge");
    mac.project(projection_dt,6000);
    const double divergence=mac.max_divergence();
    check(divergence<2e-8,"MAC projection residual too large");
    upc::StaticSolidPressureProbe3D probe(mac,nx,ny,nz,dx,wet,solid);
    const std::array<double,3> centre{{(ox+1)*dx,(oy+1)*dx,(oz+1)*dx}};
    const auto load=probe.load_on_body(body,centre);
    check(load.solid_voxels==8&&load.wetted_faces==24,"invalid wetted body");
    upc::RigidPressureState3D initial;
    initial.mass_kg=2.5;
    initial.inertia_world_kg_m2={{0.4,0.5,0.6}};
    initial.centre_m=centre;
    initial.velocity_m_s={{0.1,-0.2,0.3}};
    initial.angular_velocity_rad_s={{0.2,-0.1,0.15}};
    const auto response=upc::RigidPressureResponse3D::step(initial,load,step_dt);
    const auto first=upc::RigidPressureResponse3D::step(initial,load,step_dt/2);
    const auto second=upc::RigidPressureResponse3D::step(first.after,load,step_dt/2);
    for(int a=0;a<3;++a) {
        const double force=-12*slopes[a]*dx*dx;
        near(load.force_N[a],force,2e-5,"pressure load vs analytical force");
        near(load.torque_Nm[a],0.0,3e-5,"pressure torque about centre");
        const double v=initial.velocity_m_s[a]+force*step_dt/initial.mass_kg;
        const double c=centre[a]+initial.velocity_m_s[a]*step_dt+
            0.5*force*step_dt*step_dt/initial.mass_kg;
        near(response.after.velocity_m_s[a],v,5e-7,
             "pressure velocity vs independent constant-force reference");
        near(response.after.centre_m[a],c,2e-8,
             "pressure displacement vs independent constant-force reference");
        near(response.body_linear_impulse_Ns[a],force*step_dt,1e-6,
             "body pressure impulse reference");
        near(response.fluid_reaction_impulse_Ns[a],-force*step_dt,1e-6,
             "unapplied fluid reaction obligation");
        near(response.body_linear_impulse_Ns[a]+
             response.fluid_reaction_impulse_Ns[a],0.0,1e-15,
             "linear impulse ledger must close");
        near(response.body_angular_impulse_Nms[a]+
             response.fluid_reaction_angular_impulse_Nms[a],0.0,1e-15,
             "angular impulse ledger must close");
        near(second.after.velocity_m_s[a],response.after.velocity_m_s[a],1e-14,
             "two half steps velocity vs full step");
        near(second.after.centre_m[a],response.after.centre_m[a],1e-14,
             "two half steps position vs full step");
        near(second.after.angular_velocity_rad_s[a],
             response.after.angular_velocity_rad_s[a],1e-14,
             "two half steps spin vs full step");
    }
    near(response.energy_residual_J,0.0,1e-12,
         "frozen-wrench body kinetic work ledger");
    near(first.frozen_pressure_work_J+second.frozen_pressure_work_J,
         response.frozen_pressure_work_J,1e-12,
         "two half-step work vs full step");
    near(first.kinetic_energy_change_J+second.kinetic_energy_change_J,
         response.kinetic_energy_change_J,1e-12,
         "two half-step energy vs full step");
    // A nonzero prescribed centre torque checks diagonal angular response
    // independently. This synthetic torque is NOT from the MAC field.
    auto driven=load;
    driven.torque_Nm={{0.12,-0.07,0.21}};
    const auto spun=upc::RigidPressureResponse3D::step(initial,driven,step_dt);
    for(int a=0;a<3;++a) {
        near(spun.after.angular_velocity_rad_s[a],
             initial.angular_velocity_rad_s[a]+
             driven.torque_Nm[a]*step_dt/initial.inertia_world_kg_m2[a],
             1e-14,"angular velocity vs constant-torque reference");
        near(spun.body_angular_impulse_Nms[a],
             driven.torque_Nm[a]*step_dt,1e-14,"angular impulse");
        near(spun.fluid_reaction_angular_impulse_Nms[a],
             -driven.torque_Nm[a]*step_dt,1e-14,"angular reaction ledger");
    }
    near(spun.energy_residual_J,0.0,1e-12,
         "driven torque work and kinetic energy closure");
    if(n==8&&closed&&offset==150.0) {
        rejects([&]{(void)upc::RigidPressureResponse3D::step(initial,load,0);},
                "zero timestep accepted");
        rejects([&]{(void)upc::RigidPressureResponse3D::step(
            initial,load,std::numeric_limits<double>::quiet_NaN());},
            "NaN timestep accepted");
        auto bad=initial;bad.mass_kg=0;
        rejects([&]{(void)upc::RigidPressureResponse3D::step(bad,load,step_dt);},
                "zero mass accepted");
        bad=initial;bad.inertia_world_kg_m2[1]=-1;
        rejects([&]{(void)upc::RigidPressureResponse3D::step(bad,load,step_dt);},
                "negative inertia accepted");
        bad=initial;bad.velocity_m_s[0]=
            std::numeric_limits<double>::infinity();
        rejects([&]{(void)upc::RigidPressureResponse3D::step(bad,load,step_dt);},
                "nonfinite body velocity accepted");
        auto broken=load;
        broken.force_N[2]=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{(void)upc::RigidPressureResponse3D::step(initial,broken,step_dt);},
                "NaN pressure force accepted");
        broken=load;broken.torque_Nm[1]=
            std::numeric_limits<double>::infinity();
        rejects([&]{(void)upc::RigidPressureResponse3D::step(initial,broken,step_dt);},
                "infinite pressure torque accepted");
        broken=load;broken.force_N[0]=1e300;
        overflows([&]{(void)upc::RigidPressureResponse3D::step(
            initial,broken,1e300);},"overflowing impulse not rejected");
        broken=load;broken.force_N={{0,0,0}};broken.torque_Nm={{0,0,0}};
        const auto free=upc::RigidPressureResponse3D::step(initial,broken,step_dt);
        for(int a=0;a<3;++a) {
            near(free.after.centre_m[a],
                 initial.centre_m[a]+step_dt*initial.velocity_m_s[a],
                 1e-14,"zero-force ballistic position");
            near(free.after.velocity_m_s[a],initial.velocity_m_s[a],
                 1e-14,"zero-force velocity");
        }
        near(free.kinetic_energy_change_J,0.0,1e-14,
             "zero-force kinetic energy");
    }
    std::cout<<(closed?"closed":"air")<<","<<n<<","<<offset
             <<","<<load.force_N[0]<<","<<load.force_N[1]
             <<","<<load.force_N[2];
    for(double v:response.after.velocity_m_s)std::cout<<","<<v;
    for(double v:response.after.centre_m)std::cout<<","<<v;
    for(double v:response.after.angular_velocity_rad_s)std::cout<<","<<v;
    for(double v:response.fluid_reaction_impulse_Ns)std::cout<<","<<v;
    std::cout<<","<<response.frozen_pressure_work_J
             <<","<<response.kinetic_energy_change_J
             <<","<<response.energy_residual_J<<","<<divergence<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,offset_pa,force_x_N,force_y_N,force_z_N,"
                 <<"velocity_x_m_s,velocity_y_m_s,velocity_z_m_s,"
                 <<"centre_x_m,centre_y_m,centre_z_m,"
                 <<"omega_x_rad_s,omega_y_rad_s,omega_z_rad_s,"
                 <<"reaction_x_Ns,reaction_y_Ns,reaction_z_Ns,"
                 <<"pressure_work_J,kinetic_delta_J,energy_residual_J,divergence\n";
        for(bool closed:{false,true})for(int n:{8,12,16})
        for(double offset:{150.0,1200.0})run(n,closed,offset);
        std::cout<<"PASS: P2 frozen pressure response assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: P2 frozen pressure response assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
