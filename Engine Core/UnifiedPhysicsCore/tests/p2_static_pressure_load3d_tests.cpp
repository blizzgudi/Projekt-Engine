// P2 PARTIAL: static solid voxel pressure traction from an actual 3D MAC
// projection, checked against an independent manufactured linear pressure
// field. This does NOT apply a force to a moving body (not FSI).
#include "upc/static_pressure_load3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool ok,const char* message) {
    ++assertions;
    if(!ok)throw std::runtime_error(message);
}
template<typename F> void rejects(F&& f,const char* message) {
    bool rejected=false;
    try {f();}catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,message);
}
struct Row {
    int n,fluid_cells;
    bool closed;
    double offset,force_x,force_y,force_z,force_error,pressure_error,divergence;
    std::size_t wetted_faces;
};
Row run(int n,bool closed,double offset) {
    const int nx=n,ny=n-1,nz=n-2;
    const int ox=nx/2,oy=ny/2,oz=nz/2;
    constexpr double dx=0.08,dt=0.02,rho=1000.0;
    constexpr std::array<double,3> slopes{{7.0,-11.0,5.0}};
    const auto idx=[&](int x,int y,int z) {
        return static_cast<std::size_t>((z*ny+y)*nx+x);
    };
    const auto inside=[&](int x,int y,int z) {
        return x>=0&&y>=0&&z>=0&&x<nx&&y<ny&&z<nz;
    };
    std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0);
    int fluid_cells=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const bool wall=x==0||y==0||y==ny-1||z==0||z==nz-1||
                        (closed&&x==nx-1)||(x==ox&&y==oy&&z==oz);
        const bool liquid=!wall&&(closed||x<nx-2);
        solid[idx(x,y,z)]=static_cast<std::uint8_t>(wall);
        wet[idx(x,y,z)]=static_cast<std::uint8_t>(liquid);
        fluid_cells+=static_cast<int>(liquid);
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

    check(mac.max_divergence()>1e-4,"manufactured divergence missing");
    mac.project(dt,6000);
    const double divergence=mac.max_divergence();
    check(divergence<2e-8,"projection divergence not removed");
    double gauge=0,max_pressure_error=0;
    bool first=true;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)if(liquid(x,y,z)) {
        const double diff=mac.pressure_pa(x,y,z)-p(x,y,z);
        if(first){gauge=closed?diff:0;first=false;}
        max_pressure_error=std::max(max_pressure_error,std::abs(diff-gauge));
        check(std::isfinite(diff),"nonfinite pressure recovery");
    }
    check(!first&&max_pressure_error<3e-4,"pressure reconstruction inaccurate");

    upc::StaticSolidPressureProbe3D probe(mac,nx,ny,nz,dx,wet,solid);
    const auto load=probe.load_on_solid(ox,oy,oz);
    check(load.wetted_faces==6,"internal obstacle must have six wet faces");
    check(std::abs(load.wetted_area_m2-6*dx*dx)<1e-14,"wrong wetted area");
    const std::array<double,3> expected{{
        -2*slopes[0]*dx*dx,-2*slopes[1]*dx*dx,-2*slopes[2]*dx*dx
    }};
    double error=0;
    for(std::size_t axis=0;axis<3;++axis) {
        error=std::max(error,std::abs(load.force_N[axis]-expected[axis]));
        check(std::isfinite(load.force_N[axis]),"nonfinite solid pressure force");
        check(std::abs(load.force_N[axis]-expected[axis])<3e-6,
              "static pressure load does not match analytic face traction");
    }
    rejects([&]{(void)probe.load_on_solid(1,1,1);},
            "wet cell must reject solid load");
    rejects([&]{(void)probe.load_on_solid(nx,oy,oz);},
            "out-of-range solid load must reject");
    if(n==8&&closed&&offset==150.0) {
        auto bad=wet;
        bad[idx(ox,oy,oz)]=1;
        rejects([&]{
            upc::StaticSolidPressureProbe3D invalid(mac,nx,ny,nz,dx,bad,solid);
            (void)invalid;
        },"overlapping masks must reject");
        bad=wet;bad.pop_back();
        rejects([&]{
            upc::StaticSolidPressureProbe3D invalid(mac,nx,ny,nz,dx,bad,solid);
            (void)invalid;
        },"wrong mask length must reject");
        rejects([&]{
            upc::StaticSolidPressureProbe3D invalid(
                mac,nx,ny,nz,std::numeric_limits<double>::quiet_NaN(),wet,solid);
            (void)invalid;
        },"nonfinite grid spacing must reject");
        // No fluid touches the solid: the load must be exactly zero.
        std::vector<std::uint8_t> dry(wet.size(),0);
        upc::StaticSolidPressureProbe3D dry_probe(mac,nx,ny,nz,dx,dry,solid);
        const auto no_load=dry_probe.load_on_solid(ox,oy,oz);
        check(no_load.wetted_faces==0&&no_load.wetted_area_m2==0.0,
              "dry solid must have zero wetted area");
        for(double f:no_load.force_N)check(f==0.0,"dry solid has pressure force");
    }
    return {n,fluid_cells,closed,offset,load.force_N[0],load.force_N[1],
            load.force_N[2],error,max_pressure_error,divergence,load.wetted_faces};
}
} // namespace

int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,nx,ny,nz,offset_pa,fluid_cells,wetted_faces,"
                 <<"force_x_N,force_y_N,force_z_N,max_force_error_N,"
                 <<"max_pressure_error_pa,final_divergence\n";
        for(bool closed:{false,true})for(int n:{8,12,16})
        for(double offset:{150.0,1200.0}) {
            const auto r=run(n,closed,offset);
            std::cout<<(r.closed?"closed":"air")<<","<<r.n<<","<<r.n-1
                     <<","<<r.n-2<<","<<r.offset<<","<<r.fluid_cells
                     <<","<<r.wetted_faces<<","<<r.force_x<<","
                     <<r.force_y<<","<<r.force_z<<","<<r.force_error
                     <<","<<r.pressure_error<<","<<r.divergence<<"\n";
        }
        std::cout<<"PASS: P2 static pressure traction assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: P2 static pressure traction after assertions="
                 <<assertions<<" reason="<<e.what()<<"\n";
        return 1;
    }
}
