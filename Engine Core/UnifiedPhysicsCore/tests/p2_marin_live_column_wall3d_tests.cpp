// P2 PARTIAL: real MAC pressure + strictly bottom-connected VOF columns
// integrated into the existing MARIN five-face cut-box traction diagnostic.
// Not a cut-cell pressure solver, real transducer model or P2 acceptance.
#include "upc/p2_marin_live_column_wall3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Bridge=upc::P2MarinLiveColumnWall3D;
using Box=upc::P2Box3D;
constexpr Box box{{.6635,.295,0.},{.8245,.698,.161}};
int checks=0;
void check(bool v,const char* message) {
    ++checks;if(!v)throw std::runtime_error(message);
}
void near(double v,double expected,double tol,const char* message) {
    check(std::isfinite(v)&&std::abs(v-expected)<=tol,message);
}
template<class F>void reject(F f,const char* why) {
    bool rejected=false;
    try{f();}catch(const std::exception&){rejected=true;}
    check(rejected,why);
}
void manufactured(std::size_t nx) {
    const double dx=3.2/double(nx);
    const std::size_t ny=nx*10/32,nz=ny;
    const std::size_t count=nx*ny*nz;
    std::vector<double> pressure(count,3000.),fraction(count,0.);
    std::vector<unsigned char> solid(count,0);
    const auto id=[&](std::size_t x,std::size_t y,std::size_t z) {
        return (z*ny+y)*nx+x;
    };
    const auto fill=[&](double height,bool varying) {
        std::fill(fraction.begin(),fraction.end(),0.);
        std::fill(solid.begin(),solid.end(),0);
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const double xc=(x+.5)*dx,yc=(y+.5)*dx;
            const bool in_box=xc>=box.min_m[0]&&xc<box.max_m[0]&&
                              yc>=box.min_m[1]&&yc<box.max_m[1];
            const double h=varying?
                .08+.025*double((x+3*y)%5):height;
            for(std::size_t z=0;z<nz;++z) {
                const auto i=id(x,y,z);
                if(in_box&&(z+.5)*dx<box.max_m[2]) {
                    solid[i]=1;continue;
                }
                fraction[i]=std::clamp((h-z*dx)/dx,0.,1.);
            }
        }
    };
    const auto sample=[&] {
        return Bridge::audit_field(nx,ny,nz,dx,box,
            [&](std::size_t x,std::size_t y,std::size_t z){return pressure[id(x,y,z)];},
            [&](std::size_t x,std::size_t y,std::size_t z){return fraction[id(x,y,z)];},
            [&](std::size_t x,std::size_t y,std::size_t z){return solid[id(x,y,z)]!=0;});
    };
    const double lx=box.max_m[0]-box.min_m[0];
    const double ly=box.max_m[1]-box.min_m[1];
    fill(.10,false);
    const auto low=sample();
    near(low.traction.wetted_surface_area_m2,2*(lx+ly)*.10,1e-12,
         "independent dry-roof wetted area");
    near(low.traction.solid_force_n[2],0,1e-9,"dry roof no downward load");
    for(int a=0;a<2;++a)
        near(low.traction.solid_force_n[a],0,1e-9,"opposed uniform side forces");
    fill(.30,false);
    const auto high=sample();
    near(high.traction.wetted_surface_area_m2,
         2*(lx+ly)*box.max_m[2]+lx*ly,1e-12,
         "independent submerged five-face area");
    near(high.traction.solid_force_n[2],-3000*lx*ly,1e-8,
         "independent uniform roof load");
    for(int a=0;a<2;++a)
        near(high.traction.solid_force_n[a],0,1e-8,"balanced side pressure");
    for(double torque:high.traction.solid_torque_nm)
        near(torque,0,1e-8,"uniform pressure zero torque");
    check(high.wet_columns==nx*ny,"all columns wet");
    fill(0,true);
    const auto varying=sample();
    check(varying.traction.wetted_surface_area_m2>low.traction.wetted_surface_area_m2,
          "spatially varying waterline increases area over low baseline");
    // Deliberately disconnected water above a dry cell must be rejected,
    // rather than inventing a single free surface.
    fraction[id(0,0,0)]=0;
    fraction[id(0,0,2)]=.5;
    reject(sample,"disconnected VOF column rejected");
    fill(.30,false);
    fraction[id(0,0,0)]=std::numeric_limits<double>::quiet_NaN();
    reject(sample,"nonfinite VOF rejected");
    fill(.30,false);
    fraction[id(0,0,0)]=1.01;
    reject(sample,"overfilled VOF rejected");
    fill(.30,false);
    solid[id(0,0,3)]=1;
    reject(sample,"floating solid above fluid rejected");
    fill(.30,false);
    pressure[id(0,0,0)]=std::numeric_limits<double>::quiet_NaN();
    reject(sample,"nonfinite pressure rejected");
    std::cout<<"COLUMN_MANUFACTURED nx="<<nx
             <<" low_area="<<low.traction.wetted_surface_area_m2
             <<" high_area="<<high.traction.wetted_surface_area_m2
             <<" high_fz="<<high.traction.solid_force_n[2]<<"\n";
}
void live_native() {
    constexpr std::size_t nx=32,ny=10,nz=10;
    constexpr double dx=.1;
    upc::CoupledMacWater3D w(nx,ny,nz,dx,1000);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double xc=(x+.5)*dx,yc=(y+.5)*dx,zc=(z+.5)*dx;
        if(xc>=box.min_m[0]&&xc<box.max_m[0]&&
           yc>=box.min_m[1]&&yc<box.max_m[1]&&zc<box.max_m[2])
            w.set_solid(x,y,z);
        else w.set_water(x,y,z,1.);
    }
    w.set_pcg_pressure_projection(true);
    w.step_verified(.001,1200,1e-6);
    const auto r=Bridge::audit_live(w,box);
    check(r.wet_columns==nx*ny,"live native wet columns");
    check(r.traction.pressure_tiles>0,"live native wall pressure tiles");
    for(int a=0;a<3;++a) {
        check(std::isfinite(r.traction.solid_force_n[a]),"native MAC force finite");
        check(std::isfinite(r.traction.solid_torque_nm[a]),"native MAC torque finite");
    }
    std::cout<<"COLUMN_NATIVE_MAC area="<<r.traction.wetted_surface_area_m2
             <<" fz="<<r.traction.solid_force_n[2]
             <<" divergence="<<w.last_projection_divergence()<<"\n";
}
}
int main() {
    try {
        manufactured(32);manufactured(48);manufactured(64);
        live_native();
        std::cout<<"PASS P2_MARIN_LIVE_COLUMN assertions="<<checks
                 <<" phase_complete=false measured_validation=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL P2_MARIN_LIVE_COLUMN "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
