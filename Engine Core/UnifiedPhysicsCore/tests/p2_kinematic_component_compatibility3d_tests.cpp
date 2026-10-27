// P2 PARTIAL: independent per-connected-component Neumann solvability checks.
// Two sealed chambers can cancel globally yet be individually incompatible.
// This is a frozen voxel-mask test, NOT moving-geometry FSI validation.
#include "upc/kinematic_wall_projection3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::size_t assertions=0;
void require(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
constexpr int nx=8,ny=7,nz=6;
constexpr double dx=0.08,rho=1000.0,dt=0.025,speed=0.12;
std::size_t idx(int x,int y,int z) {
    return static_cast<std::size_t>((z*ny+y)*nx+x);
}
struct Scenario {
    upc::MacProjection3D mac{nx,ny,nz,dx};
    std::vector<std::uint8_t> wet=std::vector<std::uint8_t>(nx*ny*nz,0);
    std::vector<std::uint8_t> solid=std::vector<std::uint8_t>(nx*ny*nz,1);
    std::vector<std::uint8_t> body=std::vector<std::uint8_t>(nx*ny*nz,0);
    Scenario(bool open_left,bool open_right) {
        // The 2-voxel rigid slab separates two one-cell liquid pockets.
        // Every other voxel is impermeable solid except optional air cells.
        for(int x:{3,4})body[idx(x,3,3)]=1;
        for(int x:{2,5}){wet[idx(x,3,3)]=1;solid[idx(x,3,3)]=0;}
        if(open_left)solid[idx(1,3,3)]=0;
        if(open_right)solid[idx(6,3,3)]=0;
        for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
        for(int x=0;x<nx;++x)
            mac.set_cell(x,y,z,wet[idx(x,y,z)]!=0,solid[idx(x,y,z)]!=0);
    }
    upc::KinematicWallProjectionResult3D project(double vx) {
        return upc::KinematicWallMacProjection3D::project(
            mac,nx,ny,nz,dx,rho,dt,wet,solid,body,
            {{4*dx,3.5*dx,3.5*dx}},{{vx,0,0}},{{0,0,0}},500);
    }
};
void expect_rejected(const char* name,bool open_left,bool open_right) {
    Scenario s(open_left,open_right);
    s.mac.set_face_velocity_x(2,3,3,0.037);
    s.mac.set_face_velocity_x(6,3,3,-0.019);
    bool rejected=false;
    try { (void)s.project(speed); }
    catch(const std::invalid_argument& e) {
        rejected=std::string(e.what()).find("closed wet-component")!=std::string::npos;
    }
    require(rejected,"sealed component source must be rejected");
    near(s.mac.face_velocity_x(2,3,3),0.037,0,
         "rejected preflight must not mutate left MAC face");
    near(s.mac.face_velocity_x(6,3,3),-0.019,0,
         "rejected preflight must not mutate right MAC face");
    std::cerr<<"REJECTED: "<<name<<" (component incompatibility, MAC unchanged)\n";
}
void accepted(const char* name,bool open_left,bool open_right,double vx,
              std::size_t expected_open,std::size_t expected_closed) {
    Scenario s(open_left,open_right);
    const auto r=s.project(vx);
    require(r.open_liquid_components==expected_open,"open component count");
    require(r.closed_liquid_components==expected_closed,"closed component count");
    require(r.moving_wall_faces==std::array<std::size_t,3>{{2,0,0}},
            "two wetted moving slab faces");
    near(r.net_body_wall_flux_m3_s,0,1e-16,"opposite body flux cancels globally");
    near(r.max_closed_component_net_flux_m3_s,0,1e-16,
         "every closed chamber must have zero net source");
    require(r.max_divergence_after_s_inv<1e-10,"component divergence");
    near(s.mac.divergence(2,3,3),0,1e-10,"left pocket divergence");
    near(s.mac.divergence(5,3,3),0,1e-10,"right pocket divergence");
    const double analytic_pressure=rho*dx*vx/(2*dt);
    const double p_left=r.pressure_pa[idx(2,3,3)];
    const double p_right=r.pressure_pa[idx(5,3,3)];
    near(p_left,open_left?-analytic_pressure:0,1e-10,
         "left independent single-cell pressure reference");
    near(p_right,open_right?analytic_pressure:0,1e-10,
         "right independent single-cell pressure reference");
    const double left=s.mac.face_velocity_x(2,3,3);
    const double right=s.mac.face_velocity_x(6,3,3);
    near(left,open_left?vx:0,1e-12,"left air outlet reference");
    near(right,open_right?vx:0,1e-12,"right air outlet reference");
    near(s.mac.face_velocity_x(3,3,3),vx,1e-14,"left moving solid face");
    near(s.mac.face_velocity_x(5,3,3),vx,1e-14,"right moving solid face");
    std::cout<<name<<","<<r.closed_liquid_components<<","
             <<r.open_liquid_components<<","
             <<(r.moving_wall_faces[0]+r.moving_wall_faces[1]+r.moving_wall_faces[2])<<","
             <<r.net_body_wall_flux_m3_s<<","
             <<r.max_divergence_after_s_inv<<","
             <<p_left<<","<<p_right<<","<<left<<","<<right<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"case,closed_components,open_components,body_faces,"
                    "net_body_flux_m3_s,max_divergence_s_inv,"
                    "pressure_left_pa,pressure_right_pa,"
                    "air_face_left_m_s,air_face_right_m_s\n";
        // Global zero flux cannot make either isolated Neumann pocket solvable.
        expect_rejected("two_closed_moving",false,false);
        // A separate open component must not mask an incompatible closed one.
        expect_rejected("one_open_one_closed_moving",true,false);
        accepted("both_open_positive",true,true,speed,2,0);
        accepted("both_open_negative",true,true,-speed,2,0);
        accepted("one_open_one_closed_stationary",true,false,0,1,1);
        accepted("both_closed_stationary",false,false,0,0,2);
        std::cout<<"PASS: P2 connected-component kinematic compatibility assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
