// P2 PARTIAL: prescribed rigid normal velocities on a static 3D voxel body.
// No moving geometry or reciprocal pressure impulse. Native numerical checks.
#include "upc/kinematic_wall_projection3d.hpp"
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
void check(bool yes,const char* why) {
    ++assertions;
    if(!yes)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
template<class F>void invalid(F&& f,const char* why) {
    bool thrown=false;
    try{f();}catch(const std::invalid_argument&){thrown=true;}
    check(thrown,why);
}
struct Case {
    upc::MacProjection3D mac;
    upc::KinematicWallProjectionResult3D result;
    Case(int nx,int ny,int nz,double dx):mac(nx,ny,nz,dx){}
};
void run(int n,bool free_surface,int motion) {
    const int nx=n,ny=n-1,nz=n-2;
    constexpr double dx=0.08,rho=1000.0,dt=0.025;
    const int ox=nx/2-1,oy=ny/2-1,oz=nz/2-1;
    const auto idx=[&](int x,int y,int z){return (z*ny+y)*nx+x;};
    const auto body_at=[&](int x,int y,int z){
        return x>=ox&&x<ox+2&&y>=oy&&y<oy+2&&z>=oz&&z<oz+2;
    };
    const std::array<double,3> centre{{
        (ox+1)*dx,(oy+1)*dx,(oz+1)*dx
    }};
    const std::array<double,3> linear=motion==0?
        std::array<double,3>{{0.15,-0.11,0.07}}:
        std::array<double,3>{{0.12,-0.08,0.05}};
    const std::array<double,3> angular=motion==0?
        std::array<double,3>{{0,0,0}}:
        std::array<double,3>{{0.4,-0.5,0.7}};
    std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0),body(nx*ny*nz,0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const bool b=body_at(x,y,z);
        body[idx(x,y,z)]=solid[idx(x,y,z)]=static_cast<std::uint8_t>(b);
        wet[idx(x,y,z)]=static_cast<std::uint8_t>(!b&&
                                                  (!free_surface||z<nz-1));
    }
    Case positive(nx,ny,nz,dx),negative(nx,ny,nz,dx);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        positive.mac.set_cell(x,y,z,wet[idx(x,y,z)]!=0,solid[idx(x,y,z)]!=0);
        negative.mac.set_cell(x,y,z,wet[idx(x,y,z)]!=0,solid[idx(x,y,z)]!=0);
    }
    const auto solve=[&](Case& c,int sign) {
        std::array<double,3> lv=linear,av=angular;
        for(int a=0;a<3;++a){lv[a]*=sign;av[a]*=sign;}
        c.result=upc::KinematicWallMacProjection3D::project(
            c.mac,nx,ny,nz,dx,rho,dt,wet,solid,body,centre,lv,av,4500);
    };
    solve(positive,1);
    solve(negative,-1);
    const auto& r=positive.result;
    check(r.moving_wall_faces==std::array<std::size_t,3>{{8,8,8}},
          "2x2x2 body has eight moving-normal faces per axis");
    near(r.net_body_wall_flux_m3_s,0,1e-14,"rigid body has zero net volume source");
    check(r.max_abs_pressure_pa>0.001,"moving body creates pressure");
    check(r.pressure_iterations>0&&r.pressure_iterations<=4500,"solver iterations");
    check(r.max_divergence_after_s_inv<1e-7,"3D MAC incompressibility");
    near(r.max_divergence_after_s_inv,positive.mac.max_divergence(),1e-12,
         "solver diagnostic agrees with MAC");
    check(r.pressure_pa.size()==wet.size(),"pressure grid dimensions");
    near(negative.result.net_body_wall_flux_m3_s,0,1e-14,"reverse volume flux");
    near(negative.result.max_divergence_after_s_inv,
         r.max_divergence_after_s_inv,1e-9,"reverse divergence symmetry");
    for(std::size_t i=0;i<wet.size();++i) {
        near(r.pressure_pa[i],-negative.result.pressure_pa[i],1e-8,
             "pressure odd in prescribed rigid motion");
        if(!wet[i])near(r.pressure_pa[i],0,0,"pressure only in wet cells");
    }
    double face_energy=0;
    std::size_t independently_counted=0;
    for(int axis=0;axis<3;++axis)
    for(int z=0;z<nz+(axis==2);++z)
    for(int y=0;y<ny+(axis==1);++y)
    for(int x=0;x<nx+(axis==0);++x) {
        const int ax=x-(axis==0),ay=y-(axis==1),az=z-(axis==2);
        const int bx=x,by=y,bz=z;
        const auto inside=[&](int xx,int yy,int zz){
            return xx>=0&&yy>=0&&zz>=0&&xx<nx&&yy<ny&&zz<nz;
        };
        const bool ia=inside(ax,ay,az),ib=inside(bx,by,bz);
        const bool ba=ia&&body[idx(ax,ay,az)],bb=ib&&body[idx(bx,by,bz)];
        const bool wa=ia&&wet[idx(ax,ay,az)],wb=ib&&wet[idx(bx,by,bz)];
        const double v=axis==0?positive.mac.face_velocity_x(x,y,z):
            axis==1?positive.mac.face_velocity_y(x,y,z):
                    positive.mac.face_velocity_z(x,y,z);
        const double reversed=axis==0?negative.mac.face_velocity_x(x,y,z):
            axis==1?negative.mac.face_velocity_y(x,y,z):
                    negative.mac.face_velocity_z(x,y,z);
        near(v,-reversed,1e-9,"velocity odd in wall motion");
        if(!ia||!ib)near(v,0,0,"external boundary is impermeable");
        if((ba&&wb)||(bb&&wa)) {
            const std::array<double,3> lever{{
                (x+(axis!=0?0.5:0))*dx-centre[0],
                (y+(axis!=1?0.5:0))*dx-centre[1],
                (z+(axis!=2?0.5:0))*dx-centre[2]
            }};
            const std::array<double,3> cross{{
                angular[1]*lever[2]-angular[2]*lever[1],
                angular[2]*lever[0]-angular[0]*lever[2],
                angular[0]*lever[1]-angular[1]*lever[0]
            }};
            near(v,linear[axis]+cross[axis],1e-14,
                 "independent rigid body normal velocity");
            ++independently_counted;
        }
        if(wa||wb)face_energy+=0.5*rho*dx*dx*dx*v*v;
    }
    check(independently_counted==24,"independent body face count");
    check(face_energy>0&&std::isfinite(face_energy),"nonzero finite fluid face motion");
    // Independent finite-volume divergence from six MAC faces per wet cell.
    double max_independent=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)if(wet[idx(x,y,z)]) {
        const double div=(
            positive.mac.face_velocity_x(x+1,y,z)-positive.mac.face_velocity_x(x,y,z)+
            positive.mac.face_velocity_y(x,y+1,z)-positive.mac.face_velocity_y(x,y,z)+
            positive.mac.face_velocity_z(x,y,z+1)-positive.mac.face_velocity_z(x,y,z))/dx;
        max_independent=std::max(max_independent,std::abs(div));
    }
    near(max_independent,r.max_divergence_after_s_inv,1e-12,
         "independent divergence matches solver diagnostic");
    std::cout<<(free_surface?"air":"closed")<<","<<n<<","<<motion<<","
             <<r.max_divergence_after_s_inv<<","
             <<independently_counted<<","<<r.net_body_wall_flux_m3_s<<","
             <<r.max_abs_pressure_pa<<","<<r.pressure_iterations<<","
             <<face_energy<<"\n";
    if(n==8&&!free_surface&&motion==0) {
        auto bad=wet;
        bad[idx(ox,oy,oz)]=1;
        invalid([&]{upc::KinematicWallMacProjection3D::project(
            positive.mac,nx,ny,nz,dx,rho,dt,bad,solid,body,centre,
            linear,angular);},"reject wet/solid overlap");
        bad=wet;bad.pop_back();
        invalid([&]{upc::KinematicWallMacProjection3D::project(
            positive.mac,nx,ny,nz,dx,rho,dt,bad,solid,body,centre,
            linear,angular);},"reject mask size");
        bad=body;bad.assign(body.size(),0);
        invalid([&]{upc::KinematicWallMacProjection3D::project(
            positive.mac,nx,ny,nz,dx,rho,dt,wet,solid,bad,centre,
            linear,angular);},"reject missing body");
        auto nan=linear;nan[0]=std::numeric_limits<double>::quiet_NaN();
        invalid([&]{upc::KinematicWallMacProjection3D::project(
            positive.mac,nx,ny,nz,dx,rho,dt,wet,solid,body,centre,
            nan,angular);},"reject nonfinite wall speed");
        // A body intersecting an external wall cannot translate normally
        // into a fully enclosed incompressible liquid volume.
        std::vector<std::uint8_t> closed_wet(nx*ny*nz,1);
        std::vector<std::uint8_t> edge_solid(nx*ny*nz,0);
        std::vector<std::uint8_t> edge_body(nx*ny*nz,0);
        const auto edge=idx(0,1,1);
        closed_wet[edge]=0;
        edge_solid[edge]=edge_body[edge]=1;
        invalid([&]{upc::KinematicWallMacProjection3D::project(
            positive.mac,nx,ny,nz,dx,rho,dt,closed_wet,edge_solid,
            edge_body,centre,linear,angular);},
            "reject incompatible closed-domain volume flux");
        near(positive.mac.face_velocity_x(ox,oy,oz),linear[0],1e-14,
             "failed preflight preserves previous wall velocity");
    }
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,motion,divergence_s_inv,body_faces,wall_flux_m3_s,"
                    "max_pressure_pa,iterations,face_energy_proxy_J\n";
        for(bool air:{false,true})
        for(int n:{8,12,16})
        for(int motion:{0,1})run(n,air,motion);
        std::cout<<"PASS: P2 kinematic moving-wall projection assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
