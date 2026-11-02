// P2 PARTIAL: exact wet/dry clipping of floor-mounted MARIN Test-02
// pressure traction, checked against independent affine and hydrostatic laws.
#include "upc/p2_marin_box_wall_traction3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using Audit=upc::P2MarinBoxWallTraction3D;
using Box=upc::P2Box3D;
static unsigned assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
template<class F>void reject(F f,const char* msg) {
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    check(threw,msg);
}
int main() {
 try {
    const Box box{{.6635,.295,0},{.8245,.698,.161}};
    constexpr double sx=.161,sy=.403,sz=.161,roof=sx*sy;
    for(const auto& grid:std::array<std::array<int,3>,3>{{{{32,10,10}},{{48,15,15}},{{64,20,20}}}}) {
        const std::size_t nx=grid[0],ny=grid[1],nz=grid[2];
        const double dx=3.2/static_cast<double>(nx);
        std::vector<double> p(nx*ny*nz);
        const auto fill=[&](double p0,double a,double b,double c) {
            for(std::size_t z=0;z<nz;++z)
            for(std::size_t y=0;y<ny;++y)
            for(std::size_t x=0;x<nx;++x)
                p[(z*ny+y)*nx+x]=p0+a*(x+.5)*dx+
                                     b*(y+.5)*dx+c*(z+.5)*dx;
        };
        for(double h:{0.,.03,.103,.161,.55}) {
            fill(2200,1300,-700,-2000);
            const auto result=Audit::audit_wetted(nx,ny,nz,dx,box,p,h);
            const double clipped=std::min(h,sz);
            const double area=2*(sx+sy)*clipped+(h>=sz?roof:0.);
            const double fx=-1300*roof*clipped;
            const double fy=+700*roof*clipped;
            const double fz=h>=sz?
                -roof*(2200+1300*(.6635+sx/2)-700*(.295+sy/2)-2000*sz):0.;
            near(result.wetted_surface_area_m2,area,2e-12,"clipped face area");
            near(result.floor_contact_area_m2,roof,2e-12,"floor remains dry");
            near(result.solid_force_n[0],fx,2e-8,"clipped affine x force");
            near(result.solid_force_n[1],fy,2e-8,"clipped affine y force");
            near(result.solid_force_n[2],fz,2e-8,"clipped affine roof load");
            check(h==0?result.pressure_tiles==0:result.pressure_tiles>0,
                  "no dry pressure tiles");
            // Hydrostatic pressure vanishes at the true horizontal waterline.
            fill(9810*h,0,0,-9810);
            const auto hydro=Audit::audit_wetted(nx,ny,nz,dx,box,p,h);
            near(hydro.solid_force_n[0],0,2e-8,"hydro x symmetry");
            near(hydro.solid_force_n[1],0,2e-8,"hydro y symmetry");
            near(hydro.solid_force_n[2],
                 h>=sz?-9810*roof*(h-sz):0.,2e-8,"hydro roof pressure");
            near(hydro.wetted_surface_area_m2,area,2e-12,"hydro wet area");
            for(double torque:hydro.solid_torque_nm)
                near(torque,0,2e-8,"hydrostatic torque symmetry");
            std::cout<<std::setprecision(17)
                <<"WET_TRACTION nx="<<nx<<" h="<<h
                <<" area="<<result.wetted_surface_area_m2
                <<" fx="<<result.solid_force_n[0]
                <<" fy="<<result.solid_force_n[1]
                <<" fz="<<result.solid_force_n[2]
                <<" hydro_fz="<<hydro.solid_force_n[2]<<"\n";
        }
        fill(9810*.55,0,0,-9810);
        const auto old=Audit::audit(nx,ny,nz,dx,box,p);
        const auto wet=Audit::audit_wetted(nx,ny,nz,dx,box,p,.55);
        for(int a=0;a<3;++a)
            near(old.solid_force_n[a],wet.solid_force_n[a],2e-8,
                 "fully submerged backwards compatibility");
        reject([&]{Audit::audit_wetted(nx,ny,nz,dx,box,p,-.001);},
               "negative waterline");
        reject([&]{Audit::audit_wetted(nx,ny,nz,dx,box,p,
                    std::numeric_limits<double>::quiet_NaN());},
               "nan waterline");
        reject([&]{Audit::audit_wetted(nx,ny,nz,dx,box,p,nz*dx+.01);},
               "waterline outside tank");
    }
    std::cout<<"PASS WET_TRACTION assertions="<<assertions
             <<" measured_validation=false cut_solver=false phase_complete=false\n";
    return 0;
 }catch(const std::exception& e) {
    std::cerr<<"FAIL WET_TRACTION "<<e.what()<<" assertions="<<assertions<<"\n";
    return 1;
 }
}
