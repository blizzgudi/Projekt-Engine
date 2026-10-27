// P2 partial: independent before/reaction/second-projection MAC face audit.
// Demonstrates what pressure projection does to the applied six-DOF impulse;
// does NOT assert global FSI conservation after re-projection.
#include "upc/post_projection_wrench_audit3d.hpp"
#include "upc/pressure_wrench_reaction3d.hpp"
#include "upc/rigid_pressure_response3d.hpp"
#include "upc/static_pressure_load3d.hpp"
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
void check(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
void near(double got,double expected,double tol,const char* why) {
    check(std::isfinite(got)&&std::isfinite(expected)&&
          std::abs(got-expected)<=tol,why);
}
template<class F>void invalid(F&& f,const char* why) {
    bool caught=false;
    try{f();}catch(const std::invalid_argument&){caught=true;}
    check(caught,why);
}
template<class F>void overflow(F&& f,const char* why) {
    bool caught=false;
    try{f();}catch(const std::overflow_error&){caught=true;}
    check(caught,why);
}
struct Independent {
    std::array<double,3> impulse{{0,0,0}},torque{{0,0,0}};
    double energy=0;
    std::size_t wetwet=0,wetair=0;
};
void run(int n,bool closed,double offset) {
    const int nx=n,ny=n-1,nz=n-2;
    const int ox=nx/2-1,oy=ny/2-1,oz=nz/2-1;
    constexpr double dx=0.08,rho=1000.0,projection_dt=0.02,dt=0.05;
    constexpr std::array<double,3> slopes{{7,-11,5}};
    const auto index=[&](int x,int y,int z){
        return static_cast<std::size_t>((z*ny+y)*nx+x);
    };
    const auto inside=[&](int x,int y,int z){
        return x>=0&&y>=0&&z>=0&&x<nx&&y<ny&&z<nz;
    };
    const auto in_body=[&](int x,int y,int z){
        return x>=ox&&x<=ox+1&&y>=oy&&y<=oy+1&&z>=oz&&z<=oz+1;
    };
    std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0),body(nx*ny*nz,0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x) {
        const bool wall=x==0||y==0||y==ny-1||z==0||z==nz-1||
                        (closed&&x==nx-1)||in_body(x,y,z);
        solid[index(x,y,z)]=static_cast<std::uint8_t>(wall);
        wet[index(x,y,z)]=static_cast<std::uint8_t>(!wall&&(closed||x<nx-2));
        body[index(x,y,z)]=static_cast<std::uint8_t>(in_body(x,y,z));
    }
    const auto liquid=[&](int x,int y,int z){
        return inside(x,y,z)&&wet[index(x,y,z)]!=0;
    };
    const auto wall=[&](int x,int y,int z){
        return !inside(x,y,z)||solid[index(x,y,z)]!=0;
    };
    const auto p=[&](int x,int y,int z){
        return liquid(x,y,z)?offset+slopes[0]*x+slopes[1]*y+slopes[2]*z:0.0;
    };
    const auto manufactured=[&](int ax,int ay,int az,int bx,int by,int bz){
        if(wall(ax,ay,az)||wall(bx,by,bz))return 0.0;
        const bool a=liquid(ax,ay,az),b=liquid(bx,by,bz);
        if(!a&&!b)return 0.0;
        return projection_dt/(rho*dx)*(a!=b?2.0:1.0)*
               (p(bx,by,bz)-p(ax,ay,az));
    };
    upc::MacProjection3D mac(nx,ny,nz,dx,rho);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)
        mac.set_cell(x,y,z,liquid(x,y,z),solid[index(x,y,z)]!=0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<=nx;++x)
        mac.set_face_velocity_x(x,y,z,manufactured(x-1,y,z,x,y,z));
    for(int z=0;z<nz;++z)for(int y=0;y<=ny;++y)for(int x=0;x<nx;++x)
        mac.set_face_velocity_y(x,y,z,manufactured(x,y-1,z,x,y,z));
    for(int z=0;z<=nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)
        mac.set_face_velocity_z(x,y,z,manufactured(x,y,z-1,x,y,z));
    check(mac.max_divergence()>1e-4,"manufactured pressure input must diverge");
    mac.project(projection_dt,6000);
    near(mac.max_divergence(),0,2e-8,"initial MAC projection");
    const auto before=mac;
    const std::array<double,3> centre{{
        (ox+1)*dx+0.013,(oy+1)*dx-0.017,(oz+1)*dx+0.011
    }};
    upc::StaticSolidPressureProbe3D probe(mac,nx,ny,nz,dx,wet,solid);
    const auto load=probe.load_on_body(body,centre);
    upc::RigidPressureState3D state;
    state.mass_kg=2.5;
    state.inertia_world_kg_m2={{0.4,0.5,0.6}};
    state.centre_m=centre;
    state.velocity_m_s={{0.1,-0.2,0.3}};
    const auto body_step=upc::RigidPressureResponse3D::step(state,load,dt);
    const auto transfer=upc::FrozenPressureFluidWrench3D::apply(
        mac,nx,ny,nz,dx,rho,wet,solid,body,centre,
        body_step.fluid_reaction_impulse_Ns,
        body_step.fluid_reaction_angular_impulse_Nms);
    const auto reacted=mac;
    check(reacted.max_divergence()>1e-5,"applied impulse creates divergence");
    mac.project(projection_dt,6000);
    const auto projected=mac;
    const auto audit=upc::FrozenMacPostProjectionAudit3D::analyze(
        before,reacted,projected,nx,ny,nz,dx,rho,wet,solid,centre,
        body_step.body_linear_impulse_Ns,body_step.body_angular_impulse_Nms);
    near(audit.max_divergence_after_s_inv,0,2e-8,"second projection divergence");
    const double face_mass=rho*dx*dx*dx;
    Independent independently[3];
    // This separate implementation loops the complete MAC face arrays,
    // reconstructing each face's wet/air category and SI lever arm.
    for(int axis=0;axis<3;++axis) {
        const int xmax=nx+(axis==0),ymax=ny+(axis==1),zmax=nz+(axis==2);
        for(int z=0;z<zmax;++z)for(int y=0;y<ymax;++y)
        for(int x=0;x<xmax;++x) {
            const int ax=x-(axis==0),ay=y-(axis==1),az=z-(axis==2);
            if(!inside(ax,ay,az)||!inside(x,y,z))continue;
            if(wall(ax,ay,az)||wall(x,y,z))continue;
            const bool wet_a=liquid(ax,ay,az),wet_b=liquid(x,y,z);
            if(!wet_a&&!wet_b)continue;
            const double velocities[3]={
                axis==0?before.face_velocity_x(x,y,z):
                axis==1?before.face_velocity_y(x,y,z):
                        before.face_velocity_z(x,y,z),
                axis==0?reacted.face_velocity_x(x,y,z):
                axis==1?reacted.face_velocity_y(x,y,z):
                        reacted.face_velocity_z(x,y,z),
                axis==0?projected.face_velocity_x(x,y,z):
                axis==1?projected.face_velocity_y(x,y,z):
                        projected.face_velocity_z(x,y,z)
            };
            const std::array<double,3> r{{
                (x+(axis!=0?0.5:0.0))*dx-centre[0],
                (y+(axis!=1?0.5:0.0))*dx-centre[1],
                (z+(axis!=2?0.5:0.0))*dx-centre[2]
            }};
            for(int stage=0;stage<3;++stage) {
                const double v0=velocities[stage==1?1:0];
                const double v1=velocities[stage==0?1:2];
                const double dp=face_mass*(v1-v0);
                auto& ref=independently[stage];
                ref.impulse[axis]+=dp;
                const std::array<double,3> impulse_vec{{
                    axis==0?dp:0,axis==1?dp:0,axis==2?dp:0
                }};
                ref.torque[0]+=r[1]*impulse_vec[2]-r[2]*impulse_vec[1];
                ref.torque[1]+=r[2]*impulse_vec[0]-r[0]*impulse_vec[2];
                ref.torque[2]+=r[0]*impulse_vec[1]-r[1]*impulse_vec[0];
                ref.energy+=0.5*face_mass*(v1*v1-v0*v0);
                if(wet_a&&wet_b)++ref.wetwet;
                else ++ref.wetair;
            }
        }
    }
    const upc::MacFaceWrenchDelta3D* observed[3]={
        &audit.reaction,&audit.projection,&audit.total
    };
    for(int stage=0;stage<3;++stage) {
        const auto& got=*observed[stage];
        const auto& ref=independently[stage];
        for(int a=0;a<3;++a) {
            near(got.linear_Ns[a],ref.impulse[a],1e-12,
                 "independent stage linear impulse");
            near(got.angular_Nms[a],ref.torque[a],1e-12,
                 "independent stage angular impulse");
        }
        near(got.face_energy_delta_proxy_J,ref.energy,1e-12,
             "independent stage energy");
        check(got.wet_wet_faces==ref.wetwet&&got.wet_air_faces==ref.wetair,
              "independent stage face categories");
    }
    check(audit.reaction.wet_wet_faces>0,"wet-wet audit coverage");
    if(!closed)check(audit.reaction.wet_air_faces>0,"air interface audit coverage");
    for(int a=0;a<3;++a) {
        near(audit.reaction.linear_Ns[a],transfer.applied_linear_Ns[a],
             1e-11,"reaction matches actually applied linear impulse");
        near(audit.reaction.angular_Nms[a],transfer.applied_angular_Nms[a],
             1e-11,"reaction matches actually applied angular impulse");
        near(audit.body_plus_reaction_linear_Ns[a],0,1e-11,
             "pre-projection linear proxy closure");
        near(audit.body_plus_reaction_angular_Nms[a],0,1e-11,
             "pre-projection angular proxy closure");
        near(audit.total.linear_Ns[a],
             audit.reaction.linear_Ns[a]+audit.projection.linear_Ns[a],
             1e-11,"linear telescoping over projection");
        near(audit.total.angular_Nms[a],
             audit.reaction.angular_Nms[a]+audit.projection.angular_Nms[a],
             1e-11,"angular telescoping over projection");
        near(audit.body_plus_total_linear_Ns[a],
             audit.projection.linear_Ns[a],1e-11,
             "post-projection linear residual equals projection contribution");
        near(audit.body_plus_total_angular_Nms[a],
             audit.projection.angular_Nms[a],1e-11,
             "post-projection angular residual equals projection contribution");
    }
    near(audit.total.face_energy_delta_proxy_J,
         audit.reaction.face_energy_delta_proxy_J+
         audit.projection.face_energy_delta_proxy_J,1e-11,
         "face energy telescoping (not total physical energy)");
    // Zero-delta snapshots must give zero, even after a nontrivial run.
    const auto noop=upc::FrozenMacPostProjectionAudit3D::analyze(
        projected,projected,projected,nx,ny,nz,dx,rho,wet,solid,centre,
        {{0,0,0}},{{0,0,0}});
    for(int a=0;a<3;++a) {
        near(noop.total.linear_Ns[a],0,0,"no-op linear");
        near(noop.total.angular_Nms[a],0,0,"no-op angular");
    }
    near(noop.total.face_energy_delta_proxy_J,0,0,"no-op energy");
    if(n==8&&offset==150.0) {
        auto bad=wet;bad[index(ox,oy,oz)]=1;
        invalid([&]{upc::FrozenMacPostProjectionAudit3D::analyze(
            before,reacted,projected,nx,ny,nz,dx,rho,bad,solid,centre,
            {{0,0,0}},{{0,0,0}});},"wet-solid overlap");
        bad=wet;bad.pop_back();
        invalid([&]{upc::FrozenMacPostProjectionAudit3D::analyze(
            before,reacted,projected,nx,ny,nz,dx,rho,bad,solid,centre,
            {{0,0,0}},{{0,0,0}});},"mask length");
        invalid([&]{upc::FrozenMacPostProjectionAudit3D::analyze(
            before,reacted,projected,nx,ny,nz,dx,rho,wet,solid,
            {{std::numeric_limits<double>::quiet_NaN(),0,0}},
            {{0,0,0}},{{0,0,0}});},"nonfinite origin");
        overflow([&]{upc::FrozenMacPostProjectionAudit3D::analyze(
            before,reacted,projected,nx,ny,nz,1e200,rho,wet,solid,centre,
            {{0,0,0}},{{0,0,0}});},"overflow face mass");
    }
    std::cout<<(closed?"closed":"air")<<","<<n<<","<<offset<<","
             <<audit.max_divergence_after_s_inv;
    for(const auto& ledger:{audit.reaction.linear_Ns,
                            audit.projection.linear_Ns,
                            audit.body_plus_total_linear_Ns,
                            audit.reaction.angular_Nms,
                            audit.projection.angular_Nms,
                            audit.body_plus_total_angular_Nms})
        for(double v:ledger)std::cout<<","<<v;
    std::cout<<","<<audit.reaction.face_energy_delta_proxy_J
             <<","<<audit.projection.face_energy_delta_proxy_J
             <<","<<audit.total.face_energy_delta_proxy_J
             <<","<<audit.total.wet_wet_faces
             <<","<<audit.total.wet_air_faces<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,offset_pa,divergence_post";
        for(const char* field:{"reaction","projection","residual"})
            for(const char* axis:{"x","y","z"})
                std::cout<<","<<field<<"_"<<axis<<"_Ns";
        for(const char* field:{"reaction","projection","residual"})
            for(const char* axis:{"x","y","z"})
                std::cout<<","<<field<<"_t"<<axis<<"_Nms";
        std::cout<<",energy_reaction_J,energy_projection_J,energy_total_J,"
                    "wetwet_faces,wetair_faces\n";
        for(bool closed:{false,true})
        for(int n:{8,12,16})
        for(double offset:{150.0,1200.0})run(n,closed,offset);
        std::cout<<"PASS: P2 post-projection face-wrench audit assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" after "<<assertions<<" checks\n";
        return 1;
    }
}
