// P2 PARTIAL: analytic pressure traction and work for a kinematic 3D wall.
// Pressure comes from a real frozen-mask MAC projection. NO dynamic FSI.
#include "upc/kinematic_wall_pressure_work3d.hpp"
#include "upc/kinematic_wall_projection3d.hpp"
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
void require(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
constexpr int nx=8,ny=7,nz=6;
constexpr double dx=0.08,rho=1000.0,dt=0.025,speed=0.12;
std::size_t idx(int x,int y,int z) {
    return static_cast<std::size_t>((z*ny+y)*nx+x);
}
using Vec=std::array<double,3>;
struct Scene {
    upc::MacProjection3D mac{nx,ny,nz,dx};
    std::vector<std::uint8_t> wet=std::vector<std::uint8_t>(nx*ny*nz,0);
    std::vector<std::uint8_t> solid=std::vector<std::uint8_t>(nx*ny*nz,1);
    std::vector<std::uint8_t> body=std::vector<std::uint8_t>(nx*ny*nz,0);
    Scene() {
        // Two independent one-cell wet pockets separated by a solid slab.
        for(int x:{3,4})body[idx(x,3,3)]=1;
        for(int x:{2,5}){wet[idx(x,3,3)]=1;solid[idx(x,3,3)]=0;}
        solid[idx(1,3,3)]=solid[idx(6,3,3)]=0; // air outlets
    }
    upc::KinematicWallProjectionResult3D project(double vx) {
        return upc::KinematicWallMacProjection3D::project(
            mac,nx,ny,nz,dx,rho,dt,wet,solid,body,
            {{4*dx,3.5*dx,3.5*dx}},{{vx,0,0}},{{0,0,0}},500);
    }
    upc::KinematicWallPressureWork3D work(
        const std::vector<double>& p,double vx) {
        return upc::KinematicWallPressureProbe3D::measure(
            nx,ny,nz,dx,dt,wet,solid,body,p,
            {{4*dx,3.5*dx,3.5*dx}},{{vx,0,0}},{{0,0,0}});
    }
};
void open_pockets(double vx,const char* name) {
    Scene s;
    const auto projected=s.project(vx);
    const auto load=s.work(projected.pressure_pa,vx);
    const double p=rho*dx*vx/(2*dt);
    // The left pocket has p=-p, right pocket p=+p; both resist motion.
    const double force=-2*p*dx*dx;
    const double power=force*vx;
    near(projected.pressure_pa[idx(2,3,3)],-p,1e-10,
         "independent left single-cell pressure");
    near(projected.pressure_pa[idx(5,3,3)],p,1e-10,
         "independent right single-cell pressure");
    require(load.wetted_faces==2,"two wetted body faces");
    near(load.wetted_area_m2,2*dx*dx,1e-16,"wetted area");
    near(load.force_N[0],force,1e-12,"analytic pressure force");
    near(load.force_N[1],0,1e-14,"no y force");
    near(load.force_N[2],0,1e-14,"no z force");
    for(double moment:load.torque_Nm)near(moment,0,1e-14,"no torque");
    near(load.pressure_power_W,power,1e-13,"analytic face pressure power");
    near(load.pressure_work_J,power*dt,1e-14,"analytic frozen-step work");
    near(load.body_impulse_Ns[0],force*dt,1e-14,"body impulse");
    near(load.body_angular_impulse_Nms[0],0,1e-14,"angular impulse");
    near(load.pressure_power_W,
         load.force_N[0]*vx,1e-14,"force velocity identity");
    require(projected.max_divergence_after_s_inv<1e-10,"divergence");
    std::cout<<name<<","<<load.wetted_faces<<","<<load.force_N[0]<<","
             <<load.torque_Nm[0]<<","<<load.pressure_power_W<<","
             <<load.pressure_work_J<<","<<load.body_impulse_Ns[0]<<","
             <<projected.max_divergence_after_s_inv<<"\n";
}
void closed_body() {
    // A fully wetted central 2x2x2 solid body: constant pressure gauge
    // cannot change total force, torque or pressure power.
    constexpr int bx=3,by=2,bz=2;
    std::vector<std::uint8_t> wet(nx*ny*nz,1),solid(nx*ny*nz,0),
                              body(nx*ny*nz,0);
    for(int z=bz;z<bz+2;++z)
    for(int y=by;y<by+2;++y)
    for(int x=bx;x<bx+2;++x) {
        wet[idx(x,y,z)]=0;
        solid[idx(x,y,z)]=body[idx(x,y,z)]=1;
    }
    upc::MacProjection3D mac(nx,ny,nz,dx);
    const Vec centre{{4*dx,3*dx,3*dx}};
    const Vec v{{0.12,-0.08,0.05}},omega{{0.4,-0.5,0.7}};
    const auto projected=upc::KinematicWallMacProjection3D::project(
        mac,nx,ny,nz,dx,rho,dt,wet,solid,body,centre,v,omega,4500);
    auto shifted=projected.pressure_pa;
    for(std::size_t i=0;i<shifted.size();++i)if(wet[i])shifted[i]+=73.0;
    const auto measure=[&](const std::vector<double>& p) {
        return upc::KinematicWallPressureProbe3D::measure(
            nx,ny,nz,dx,dt,wet,solid,body,p,centre,v,omega);
    };
    const auto a=measure(projected.pressure_pa),b=measure(shifted);
    require(a.wetted_faces==24,"full 2x2x2 wetted body");
    require(projected.max_divergence_after_s_inv<1e-8,"3D moving wall divergence");
    for(int i=0;i<3;++i) {
        near(a.force_N[i],b.force_N[i],1e-12,"force gauge invariant");
        near(a.torque_Nm[i],b.torque_Nm[i],1e-12,"torque gauge invariant");
        near(a.body_impulse_Ns[i],a.force_N[i]*dt,1e-14,"impulse conversion");
        near(a.body_angular_impulse_Nms[i],a.torque_Nm[i]*dt,1e-14,
             "angular impulse conversion");
    }
    double rigid_power=0;
    for(int i=0;i<3;++i)
        rigid_power+=a.force_N[i]*v[i]+a.torque_Nm[i]*omega[i];
    near(a.pressure_power_W,rigid_power,1e-12,
         "independent rigid-body force torque power identity");
    near(a.pressure_power_W,b.pressure_power_W,1e-12,
         "pressure power gauge invariant");
    require(std::isfinite(a.pressure_power_W),"finite moving-wall work");
    std::cerr<<"CLOSED_GAUGE: 24 wetted faces, +73 Pa gauge invariant\n";
}
void invalid_inputs() {
    Scene s;
    auto p=s.project(speed).pressure_pa;
    auto invalid=[&](const char* why) {
        bool thrown=false;
        try{(void)s.work(p,speed);}catch(const std::invalid_argument&){thrown=true;}
        require(thrown,why);
    };
    p[idx(2,3,3)]=std::numeric_limits<double>::quiet_NaN();
    invalid("reject NaN pressure");
    p.assign(nx*ny*nz,0);
    s.body[idx(2,3,3)]=1;
    invalid("reject body in wet voxel");
    s.body[idx(2,3,3)]=0;
    p.pop_back();
    invalid("reject wrong pressure shape");
    p.assign(nx*ny*nz,0);
    s.body.assign(nx*ny*nz,0);
    invalid("reject empty body");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"case,wet_faces,force_x_N,torque_x_Nm,power_W,"
                    "work_J,impulse_x_Ns,divergence_s_inv\n";
        open_pockets(speed,"open_positive");
        open_pockets(-speed,"open_negative");
        open_pockets(0,"open_stationary");
        closed_body();
        invalid_inputs();
        std::cout<<"PASS: P2 kinematic pressure-work assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
