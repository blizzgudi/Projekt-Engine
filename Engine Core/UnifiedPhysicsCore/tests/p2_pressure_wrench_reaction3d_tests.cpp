// P2 partial: apply BOTH linear and angular opposite pressure impulses to
// wet-wet MAC faces, then independently sum the six wrench components.
// Frozen mask and post-projection effects mean this is NOT full FSI.
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
void near(double a,double b,double tolerance,const char* why) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tolerance,why);
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
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x){
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
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_cell(x,y,z,wet[index(x,y,z)]!=0,solid[index(x,y,z)]!=0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<=nx;++x)
        mac.set_face_velocity_x(x,y,z,manufactured(x-1,y,z,x,y,z));
    for(int z=0;z<nz;++z)for(int y=0;y<=ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_face_velocity_y(x,y,z,manufactured(x,y-1,z,x,y,z));
    for(int z=0;z<=nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_face_velocity_z(x,y,z,manufactured(x,y,z-1,x,y,z));
    check(mac.max_divergence()>1e-4,"manufactured input must diverge");
    mac.project(projection_dt,6000);
    near(mac.max_divergence(),0,2e-8,"projected pressure divergence");
    const std::array<double,3> geometric{{(ox+1)*dx,(oy+1)*dx,(oz+1)*dx}};
    // Deliberately offset the centre to generate a NONZERO torque under
    // the manufactured linear pressure field.
    const std::array<double,3> centre{{
        geometric[0]+0.013,geometric[1]-0.017,geometric[2]+0.011
    }};
    upc::StaticSolidPressureProbe3D probe(mac,nx,ny,nz,dx,wet,solid);
    const auto load=probe.load_on_body(body,centre);
    upc::RigidPressureState3D state;
    state.mass_kg=2.5;
    state.inertia_world_kg_m2={{0.4,0.5,0.6}};
    state.centre_m=centre;
    state.velocity_m_s={{0.1,-0.2,0.3}};
    const auto body_step=upc::RigidPressureResponse3D::step(state,load,dt);
    const auto face_mass=rho*dx*dx*dx;
    const auto before=mac;
    const auto transfer=upc::FrozenPressureFluidWrench3D::apply(
        mac,nx,ny,nz,dx,rho,wet,solid,body,centre,
        body_step.fluid_reaction_impulse_Ns,
        body_step.fluid_reaction_angular_impulse_Nms);
    std::array<double,3> independent_impulse{{0,0,0}};
    std::array<double,3> independent_torque{{0,0,0}};
    std::array<std::size_t,3> changed_faces{{0,0,0}};
    double independent_energy=0;
    // Independently inspect every MAC face. The only legal changes are
    // wet-wet faces whose adjacent cell borders a selected body voxel.
    constexpr int dir[6][3]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    const auto body_near=[&](int x,int y,int z){
        if(!liquid(x,y,z))return false;
        for(const auto& d:dir){
            const int xx=x+d[0],yy=y+d[1],zz=z+d[2];
            if(inside(xx,yy,zz)&&body[index(xx,yy,zz)])return true;
        }
        return false;
    };
    for(int a=0;a<3;++a) {
        const int xmax=nx+(a==0),ymax=ny+(a==1),zmax=nz+(a==2);
        for(int z=0;z<zmax;++z)for(int y=0;y<ymax;++y)
        for(int x=0;x<xmax;++x) {
            const int ax=x-(a==0),ay=y-(a==1),az=z-(a==2);
            const int bx=x,by=y,bz=z;
            const bool eligible=liquid(ax,ay,az)&&liquid(bx,by,bz)&&
                (body_near(ax,ay,az)||body_near(bx,by,bz));
            const double old=a==0?before.face_velocity_x(x,y,z):
                             a==1?before.face_velocity_y(x,y,z):
                                  before.face_velocity_z(x,y,z);
            const double next=a==0?mac.face_velocity_x(x,y,z):
                              a==1?mac.face_velocity_y(x,y,z):
                                   mac.face_velocity_z(x,y,z);
            if(eligible) {
                ++changed_faces[a];
                const double dp=face_mass*(next-old);
                independent_impulse[a]+=dp;
                const std::array<double,3> position{{
                    (x+(a!=0?0.5:0.0))*dx,
                    (y+(a!=1?0.5:0.0))*dx,
                    (z+(a!=2?0.5:0.0))*dx
                }};
                const std::array<double,3> r{{
                    position[0]-centre[0],
                    position[1]-centre[1],
                    position[2]-centre[2]
                }};
                if(a==0){independent_torque[1]+=r[2]*dp;independent_torque[2]-=r[1]*dp;}
                if(a==1){independent_torque[0]-=r[2]*dp;independent_torque[2]+=r[0]*dp;}
                if(a==2){independent_torque[0]+=r[1]*dp;independent_torque[1]-=r[0]*dp;}
                independent_energy+=0.5*face_mass*(next*next-old*old);
                check(std::isfinite(next),"eligible face update finite");
            }else near(next,old,0,"no illegal face velocity changes");
        }
    }
    for(int a=0;a<3;++a) {
        check(changed_faces[a]>0&&changed_faces[a]==transfer.selected_face_count[a],
              "independent selected face counts");
        const double analytic_force=-12*slopes[a]*dx*dx;
        near(load.force_N[a],analytic_force,2e-5,"real projected force reference");
        near(body_step.body_linear_impulse_Ns[a],analytic_force*dt,1e-6,
             "body impulse reference");
        const std::array<double,3> r{{
            geometric[0]-centre[0],geometric[1]-centre[1],geometric[2]-centre[2]
        }};
        const std::array<double,3> f{{-12*slopes[0]*dx*dx,
                                      -12*slopes[1]*dx*dx,
                                      -12*slopes[2]*dx*dx}};
        const std::array<double,3> torque_ref{{
            r[1]*f[2]-r[2]*f[1],
            r[2]*f[0]-r[0]*f[2],
            r[0]*f[1]-r[1]*f[0]
        }};
        near(body_step.body_angular_impulse_Nms[a],
             torque_ref[a]*dt,1e-8,"offset-centre analytic torque reference");
        near(transfer.applied_linear_Ns[a],-body_step.body_linear_impulse_Ns[a],
             1e-12,"applied linear reaction sign");
        near(independent_impulse[a],-body_step.body_linear_impulse_Ns[a],
             1e-11,"measured face linear impulse closure");
        near(body_step.body_linear_impulse_Ns[a]+independent_impulse[a],0,
             1e-12,"body + actually changed fluid faces momentum closure");
        near(transfer.applied_angular_Nms[a],
             body_step.fluid_reaction_angular_impulse_Nms[a],1e-12,
             "angular impulse actually applied");
        near(independent_torque[a],
             body_step.fluid_reaction_angular_impulse_Nms[a],1e-11,
             "independently measured face torque closure");
        near(independent_torque[a]+body_step.body_angular_impulse_Nms[a],
             0,1e-11,"body+fluid angular impulse closure");
    }
    near(independent_energy,transfer.face_energy_delta_proxy_J,
         1e-12,"independent face energy proxy");
    // A separate no-op must not change the already modified field.
    const auto after=mac;
    const auto noop=upc::FrozenPressureFluidWrench3D::apply(
        mac,nx,ny,nz,dx,rho,wet,solid,body,centre,{{0,0,0}},{{0,0,0}});
    for(int a=0;a<3;++a)near(noop.applied_linear_Ns[a],0,0,"zero impulse");
    near(mac.max_divergence(),after.max_divergence(),0,"no-op leaves field intact");
    // Negative tests on actual projected field, with no partial writes.
    auto bad_wet=wet;bad_wet[index(ox,oy,oz)]=1;
    invalid([&]{upc::FrozenPressureFluidWrench3D::apply(
        mac,nx,ny,nz,dx,rho,bad_wet,solid,body,centre,{{1,0,0}},{{0,0,0}});},
        "overlapping wet/solid mask must be rejected");
    invalid([&]{upc::FrozenPressureFluidWrench3D::apply(
        mac,nx,ny,nz,dx,rho,wet,solid,body,centre,
        {{std::numeric_limits<double>::quiet_NaN(),0,0}},{{0,0,0}});},
        "nonfinite impulse must be rejected");
    invalid([&]{upc::FrozenPressureFluidWrench3D::apply(
        mac,nx,ny,nz,dx,rho,wet,solid,body,centre,{{1,0,0}},
        {{0,std::numeric_limits<double>::infinity(),0}});},
        "nonfinite angular impulse must be rejected");
    overflow([&]{upc::FrozenPressureFluidWrench3D::apply(
        mac,nx,ny,nz,1e200,rho,wet,solid,body,centre,{{1,0,0}},{{0,0,0}});},
        "overflowed face mass must be rejected");
    overflow([&]{upc::FrozenPressureFluidWrench3D::apply(
        mac,nx,ny,nz,dx,rho,wet,solid,body,centre,{{1e308,0,0}},{{0,0,0}});},
        "huge finite impulse must be rejected before any write");
    near(mac.max_divergence(),after.max_divergence(),0,
         "invalid/overflow inputs must not mutate projected field");
    // An isolated wet cell has no legal wet-wet face for any component.
    // This must fail closed, without using a solid-normal face.
    upc::MacProjection3D isolated(5,5,5,dx,rho);
    std::vector<std::uint8_t> iw(125,0),is(125,0),ib(125,0);
    const auto iid=[](int x,int y,int z){return (z*5+y)*5+x;};
    is[iid(2,2,2)]=1;ib[iid(2,2,2)]=1;iw[iid(3,2,2)]=1;
    isolated.set_cell(2,2,2,false,true);
    isolated.set_cell(3,2,2,true,false);
    invalid([&]{upc::FrozenPressureFluidWrench3D::apply(
        isolated,5,5,5,dx,rho,iw,is,ib,{{2*dx,2*dx,2*dx}},{{1,0,0}},{{0,0,0}});},
        "no eligible wet-wet face must fail closed");
    near(isolated.face_velocity_x(3,2,2),0,0,
         "isolated wet/solid boundary must not be modified");
    // Projection after the impulse is a distinct, NON-conservative step;
    // report its divergence, never claim global FSI conservation from it.
    const double pre_projection_divergence=mac.max_divergence();
    mac.project(projection_dt,6000);
    const double post_projection_divergence=mac.max_divergence();
    check(std::isfinite(pre_projection_divergence)&&
          std::isfinite(post_projection_divergence)&&
          post_projection_divergence<2e-8,"post-impulse projection converges");
    std::cout<<(closed?"closed":"air")<<","<<n<<","<<offset;
    for(int a=0;a<3;++a)std::cout<<","<<load.force_N[a];
    for(int a=0;a<3;++a)std::cout<<","<<body_step.body_linear_impulse_Ns[a];
    for(int a=0;a<3;++a)std::cout<<","<<independent_impulse[a];
    for(int a=0;a<3;++a)std::cout<<","<<body_step.body_angular_impulse_Nms[a];
    for(int a=0;a<3;++a)std::cout<<","<<independent_torque[a];
    for(int a=0;a<3;++a)std::cout<<","<<changed_faces[a];
    std::cout<<","<<independent_energy<<","<<pre_projection_divergence
             <<","<<post_projection_divergence<<"\n";
}
}
int main() {
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,offset_pa,force_x_N,force_y_N,force_z_N,"
                    "body_x_Ns,body_y_Ns,body_z_Ns,"
                    "fluid_x_Ns,fluid_y_Ns,fluid_z_Ns,"
                    "body_tx_Nms,body_ty_Nms,body_tz_Nms,"
                    "fluid_tx_Nms,fluid_ty_Nms,fluid_tz_Nms,"
                    "faces_x,faces_y,faces_z,face_energy_proxy_J,"
                    "divergence_pre_projection,divergence_post_projection\n";
        for(const bool closed:{false,true})
        for(const int n:{8,12,16})
        for(const double offset:{150.0,1200.0})run(n,closed,offset);
        std::cout<<"PASS: P2 six-component face wrench reaction assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" after "<<assertions<<" checks\n";
        return 1;
    }
}
