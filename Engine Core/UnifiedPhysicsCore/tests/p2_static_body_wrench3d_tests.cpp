// P2 PARTIAL: force and torque from static 2x2x2 voxel body boundary.
// Independent manufactured 3D MAC pressure field; NOT moving-body FSI.
#include "upc/static_pressure_load3d.hpp"
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
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
template<typename F> void rejects(F&& f,const char* msg) {
    bool caught=false;
    try {f();}catch(const std::invalid_argument&) {caught=true;}
    check(caught,msg);
}
std::array<double,3> cross(const std::array<double,3>& a,
                           const std::array<double,3>& b) {
    return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],
             a[0]*b[1]-a[1]*b[0]}};
}
void run(int n,bool closed,double offset) {
    const int nx=n,ny=n-1,nz=n-2;
    const int ox=nx/2-1,oy=ny/2-1,oz=nz/2-1;
    constexpr double dx=0.08,dt=0.02,rho=1000.0;
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
        return dt/(rho*dx)*(a!=b?2.0:1.0)*
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

    check(mac.max_divergence()>1e-4,"missing manufactured divergence");
    mac.project(dt,6000);
    const double div=mac.max_divergence();
    check(div<2e-8,"MAC projection divergence residual");
    double gauge=0,pressure_error=0;
    bool first=true;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)if(liquid(x,y,z)) {
        const double delta=mac.pressure_pa(x,y,z)-p(x,y,z);
        if(first){gauge=closed?delta:0.0;first=false;}
        pressure_error=std::max(pressure_error,std::abs(delta-gauge));
        check(std::isfinite(delta),"nonfinite recovered pressure");
    }
    check(!first&&pressure_error<3e-4,"MAC pressure recovery");

    upc::StaticSolidPressureProbe3D probe(mac,nx,ny,nz,dx,wet,solid);
    const std::array<double,3> origin{{0,0,0}};
    const std::array<double,3> centre{{
        (ox+1)*dx,(oy+1)*dx,(oz+1)*dx
    }};
    const auto load=probe.load_on_body(body,origin);
    const auto centred=probe.load_on_body(body,centre);
    const std::array<double,3> expected_force{{
        -12*slopes[0]*dx*dx,-12*slopes[1]*dx*dx,
        -12*slopes[2]*dx*dx
    }};
    const auto expected_torque=cross(centre,expected_force);
    check(load.solid_voxels==8&&load.wetted_faces==24,
          "2x2x2 body must have 24 exposed wet faces");
    check(std::abs(load.wetted_area_m2-24*dx*dx)<1e-13,
          "wrong body wetted area");
    check(centred.wetted_faces==load.wetted_faces,
          "torque origin must not change wet faces");
    double force_error=0,torque_error=0;
    std::array<double,3> single_sum{{0,0,0}};
    for(int z=oz;z<=oz+1;++z)for(int y=oy;y<=oy+1;++y)
    for(int x=ox;x<=ox+1;++x) {
        const auto one=probe.load_on_solid(x,y,z);
        for(int a=0;a<3;++a)single_sum[a]+=one.force_N[a];
    }
    for(int a=0;a<3;++a) {
        force_error=std::max(force_error,
                             std::abs(load.force_N[a]-expected_force[a]));
        torque_error=std::max(torque_error,
                             std::abs(load.torque_Nm[a]-expected_torque[a]));
        check(std::abs(load.force_N[a]-expected_force[a])<2e-5,
              "body force vs independent linear pressure reference");
        check(std::abs(load.torque_Nm[a]-expected_torque[a])<3e-5,
              "body torque vs independent linear pressure reference");
        check(std::abs(centred.torque_Nm[a])<3e-5,
              "symmetric body torque about centroid must vanish");
        check(std::abs(load.force_N[a]-single_sum[a])<1e-11,
              "aggregate force differs from eight individual cells");
        check(std::abs(load.force_N[a]-centred.force_N[a])<1e-11,
              "origin changes force");
    }
    if(n==8&&closed&&offset==150.0) {
        auto bad=body;
        bad[idx(1,1,1)]=1;
        rejects([&]{(void)probe.load_on_body(bad,origin);},
                "body must reject wet cell selection");
        bad=body;bad[idx(ox,oy,oz)]=2;
        rejects([&]{(void)probe.load_on_body(bad,origin);},
                "body must reject nonbinary mask");
        bad=body;bad.pop_back();
        rejects([&]{(void)probe.load_on_body(bad,origin);},
                "body must reject wrong mask size");
        bad.assign(body.size(),0);
        rejects([&]{(void)probe.load_on_body(bad,origin);},
                "body must reject empty selection");
        rejects([&]{
            (void)probe.load_on_body(body,{{
                std::numeric_limits<double>::quiet_NaN(),0.0,0.0}});
        },"body must reject nonfinite torque origin");
        std::vector<std::uint8_t> dry(wet.size(),0);
        upc::StaticSolidPressureProbe3D dry_probe(
            mac,nx,ny,nz,dx,dry,solid);
        const auto zero=dry_probe.load_on_body(body,origin);
        check(zero.solid_voxels==8&&zero.wetted_faces==0,
              "dry body must have zero wet faces");
        for(int a=0;a<3;++a) {
            check(zero.force_N[a]==0.0,"dry body force nonzero");
            check(zero.torque_Nm[a]==0.0,"dry body torque nonzero");
        }
    }
    std::cout<<(closed?"closed":"air")<<","<<nx<<","<<ny<<","<<nz
             <<","<<offset<<","<<load.solid_voxels<<","
             <<load.wetted_faces<<","<<load.force_N[0]<<","
             <<load.force_N[1]<<","<<load.force_N[2]<<","
             <<load.torque_Nm[0]<<","<<load.torque_Nm[1]<<","
             <<load.torque_Nm[2]<<","<<force_error<<","
             <<torque_error<<","<<pressure_error<<","<<div<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,nx,ny,nz,offset_pa,solid_voxels,wetted_faces,"
                 <<"force_x_N,force_y_N,force_z_N,torque_x_Nm,"
                 <<"torque_y_Nm,torque_z_Nm,max_force_error_N,"
                 <<"max_torque_error_Nm,max_pressure_error_pa,divergence\n";
        for(bool closed:{false,true})for(int n:{8,12,16})
        for(double offset:{150.0,1200.0})run(n,closed,offset);
        std::cout<<"PASS: P2 static body wrench assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: P2 static body wrench assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
