// P2 partial: independent 3D manufactured-pressure projection with
// air Dirichlet (ghost-fluid factor two), solid no-flux and closed-Neumann
// obstacles. This tests the discrete pressure/velocity coupling, not CFD E2E.
#include "upc/mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool ok,const char* reason) {
    ++assertions;
    if(!ok)throw std::runtime_error(reason);
}
struct Row {
    int nx,ny,nz,fluid_cells;
    double initial_divergence,final_divergence;
    double max_pressure_error,max_face_speed,max_solid_flux;
};
Row run(int nx,bool closed) {
    const int ny=nx-1,nz=nx-2;
    constexpr double dx=0.08,dt=0.02,rho=1000.0;
    const auto idx=[&](int x,int y,int z) {
        return static_cast<std::size_t>((z*ny+y)*nx+x);
    };
    const auto inside=[&](int x,int y,int z) {
        return x>=0&&y>=0&&z>=0&&x<nx&&y<ny&&z<nz;
    };
    std::vector<unsigned char> fluid(nx*ny*nz,0),solid(nx*ny*nz,0);
    const int ox=nx/2,oy=ny/2,oz=nz/2;
    int fluid_cells=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const bool boundary=x==0||y==0||y==ny-1||z==0||z==nz-1
            ||(closed&&x==nx-1);
        const bool obstacle=x==ox&&y==oy&&z==oz;
        const bool wet=!boundary&&!obstacle&&(closed||x<nx-2);
        const auto i=idx(x,y,z);
        solid[i]=static_cast<unsigned char>(boundary||obstacle);
        fluid[i]=static_cast<unsigned char>(wet);
        fluid_cells+=static_cast<int>(wet);
    }
    const auto liquid=[&](int x,int y,int z) {
        return inside(x,y,z)&&fluid[idx(x,y,z)]!=0;
    };
    const auto wall=[&](int x,int y,int z) {
        return !inside(x,y,z)||solid[idx(x,y,z)]!=0;
    };
    const auto pressure=[&](int x,int y,int z) {
        if(!liquid(x,y,z))return 0.0;
        return 250.0+17.0*std::sin(0.27*x)+13.0*std::cos(0.41*y)
            +11.0*std::sin(0.23*z)+0.75*x*y/nx;
    };
    // Manufactured velocity is +dt/rho times the discrete pressure gradient.
    // The projection must exactly cancel it. Air faces use the independently
    // specified half-cell pressure jump (factor 2), solid faces are no flux.
    const auto manufactured=[&](int ax,int ay,int az,int bx,int by,int bz) {
        if(wall(ax,ay,az)||wall(bx,by,bz))return 0.0;
        const bool a=liquid(ax,ay,az),b=liquid(bx,by,bz);
        if(!a&&!b)return 0.0;
        const double ghost=(a!=b)?2.0:1.0;
        return (dt/(rho*dx))*ghost*
            (pressure(bx,by,bz)-pressure(ax,ay,az));
    };
    upc::MacProjection3D mac(nx,ny,nz,dx,rho);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_cell(x,y,z,fluid[idx(x,y,z)]!=0,solid[idx(x,y,z)]!=0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<=nx;++x)
        mac.set_face_velocity_x(x,y,z,manufactured(x-1,y,z,x,y,z));
    for(int z=0;z<nz;++z)for(int y=0;y<=ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_face_velocity_y(x,y,z,manufactured(x,y-1,z,x,y,z));
    for(int z=0;z<=nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        mac.set_face_velocity_z(x,y,z,manufactured(x,y,z-1,x,y,z));
    const double initial=mac.max_divergence();
    check(std::isfinite(initial)&&initial>1e-4,
          "manufactured pressure did not create 3D divergence");
    mac.project(dt,6000);
    const double final=mac.max_divergence();
    check(std::isfinite(final)&&final<2e-8,
          "projection did not remove manufactured divergence");
    // Closed pure-Neumann fluid has arbitrary pressure gauge. Compare
    // after subtracting the offset at the first wet cell.
    double offset=0;
    bool first=true;
    double max_pressure_error=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)if(liquid(x,y,z)) {
        const double diff=mac.pressure_pa(x,y,z)-pressure(x,y,z);
        if(first){offset=closed?diff:0;first=false;}
        max_pressure_error=std::max(max_pressure_error,std::abs(diff-offset));
        check(std::isfinite(diff),"nonfinite manufactured pressure");
    }
    check(!first&&fluid_cells>10,"no valid fluid cells");
    check(max_pressure_error<3e-4,
          "manufactured pressure recovery exceeded tolerance");
    double max_face_speed=0,max_solid_flux=0;
    const auto measure=[&](double value,int ax,int ay,int az,
                           int bx,int by,int bz) {
        check(std::isfinite(value),"nonfinite projected face velocity");
        max_face_speed=std::max(max_face_speed,std::abs(value));
        if(wall(ax,ay,az)||wall(bx,by,bz))
            max_solid_flux=std::max(max_solid_flux,std::abs(value));
    };
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<=nx;++x)
        measure(mac.face_velocity_x(x,y,z),x-1,y,z,x,y,z);
    for(int z=0;z<nz;++z)for(int y=0;y<=ny;++y)
    for(int x=0;x<nx;++x)
        measure(mac.face_velocity_y(x,y,z),x,y-1,z,x,y,z);
    for(int z=0;z<=nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        measure(mac.face_velocity_z(x,y,z),x,y,z-1,x,y,z);
    check(max_face_speed<2e-7,
          "projection failed to cancel manufactured face gradients");
    check(max_solid_flux==0,"solid or exterior face leaked");
    // No fake gauge condition: repeated projection must remain divergence-free.
    mac.project(dt,6000);
    check(mac.max_divergence()<2e-8,
          "second projection reintroduced divergence");
    return {nx,ny,nz,fluid_cells,initial,final,
            max_pressure_error,max_face_speed,max_solid_flux};
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,nx,ny,nz,fluid_cells,initial_divergence,final_divergence,"
                 <<"max_pressure_error_pa,max_face_speed_m_s,max_solid_flux_m_s\n";
        for(bool closed:{false,true})for(int n:{8,12,16}) {
            const auto r=run(n,closed);
            std::cout<<(closed?"closed":"air")<<","<<r.nx<<","<<r.ny<<","
                     <<r.nz<<","<<r.fluid_cells<<","<<r.initial_divergence
                     <<","<<r.final_divergence<<","<<r.max_pressure_error
                     <<","<<r.max_face_speed<<","<<r.max_solid_flux<<"\n";
        }
        std::cout<<"PASS: P2 3D manufactured projection assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: P2 3D manufactured projection after assertions="
                 <<assertions<<" reason="<<e.what()<<"\n";
        return 1;
    }
}
