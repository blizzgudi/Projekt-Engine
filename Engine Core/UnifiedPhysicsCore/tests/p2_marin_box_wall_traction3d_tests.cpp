// P2 PARTIAL: real Test-02 floor-mounted box geometry, independently
// manufactured liquid-side pressure fields and one native MAC snapshot.
// Not a measured six-second CFD/FSI acceptance.
#include "upc/p2_marin_box_wall_traction3d.hpp"
#include "upc/coupled_water3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Box=upc::P2Box3D;
using Audit=upc::P2MarinBoxWallTraction3D;
unsigned assertions=0;
void check(bool yes,const char* why) {
    ++assertions;
    if(!yes)throw std::runtime_error(why);
}
void near(double actual,double expected,double tol,const char* why) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,why);
}
template<class F>void rejects(F f,const char* why) {
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    check(threw,why);
}
Box marin() {return {{.6635,.295,0.0},{.8245,.698,.161}};}
std::size_t index(std::size_t x,std::size_t y,std::size_t z,
                  std::size_t nx,std::size_t ny) {
    return (z*ny+y)*nx+x;
}
void manufactured(const char* grid,std::size_t nx,std::size_t ny,
                  std::size_t nz,double dx) {
    const auto box=marin();
    const double sx=box.max_m[0]-box.min_m[0];
    const double sy=box.max_m[1]-box.min_m[1];
    const double sz=box.max_m[2]-box.min_m[2];
    const double volume=sx*sy*sz;
    const double top_area=sx*sy;
    const double wetted=2*(sx*sy+sx*sz+sy*sz)-top_area;
    const double mid_x=.5*(box.min_m[0]+box.max_m[0]);
    const double mid_y=.5*(box.min_m[1]+box.max_m[1]);
    std::vector<double> p(nx*ny*nz);
    const auto fill=[&](double p0,double a,double b,double c) {
        for(std::size_t z=0;z<nz;++z)
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            p[index(x,y,z,nx,ny)]=p0+
                a*(x+.5)*dx+b*(y+.5)*dx+c*(z+.5)*dx;
    };
    // p=rho*g*(H-z), exactly linear, independent hydrostatic wall load.
    fill(9810*.55,0,0,-9810);
    const auto hydro=Audit::audit(nx,ny,nz,dx,box,p);
    const double expected_z=-top_area*9810*(.55-.161);
    near(hydro.solid_force_n[0],0,1e-8,"hydrostatic x force");
    near(hydro.solid_force_n[1],0,1e-8,"hydrostatic y force");
    near(hydro.solid_force_n[2],expected_z,1e-8,
         "hydrostatic floor-contact downward roof load");
    for(double torque:hydro.solid_torque_nm)
        near(torque,0,1e-8,"hydrostatic torque");
    near(hydro.floor_contact_area_m2,top_area,1e-13,
         "exact excluded floor contact area");
    near(hydro.wetted_surface_area_m2,wetted,1e-12,
         "exact five-face wetted surface");
    check(hydro.pressure_tiles>0,"real cut-face pressure tiles");
    // A fully affine 3D field must reproduce all three exact net forces
    // regardless of box/grid alignment, including the floor contact.
    constexpr double p0=2200,a=1300,b=-700,c=-2000;
    fill(p0,a,b,c);
    const auto affine=Audit::audit(nx,ny,nz,dx,box,p);
    near(affine.solid_force_n[0],-a*volume,1e-8,
         "affine x pressure-gradient force");
    near(affine.solid_force_n[1],-b*volume,1e-8,
         "affine y pressure-gradient force");
    near(affine.solid_force_n[2],
         -top_area*(p0+a*mid_x+b*mid_y+c*box.max_m[2]),1e-8,
         "affine top wall pressure force");
    // Poison binary-solid centre cells. The traction must never use
    // pressure from inside the box (not even in tangential interpolation).
    auto poisoned=p;
    unsigned solid_cells=0;
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double xx=(x+.5)*dx,yy=(y+.5)*dx,zz=(z+.5)*dx;
        if(xx>=box.min_m[0]&&xx<box.max_m[0]&&
           yy>=box.min_m[1]&&yy<box.max_m[1]&&
           zz>=box.min_m[2]&&zz<box.max_m[2]) {
            poisoned[index(x,y,z,nx,ny)]=1e9;
            ++solid_cells;
        }
    }
    check(solid_cells>0,"binary solid centres exist");
    const auto clean=Audit::audit(nx,ny,nz,dx,box,poisoned);
    for(int axis=0;axis<3;++axis) {
        near(clean.solid_force_n[axis],affine.solid_force_n[axis],
             1e-8,"solid-cell pressure poisoning cannot change force");
        near(clean.solid_torque_nm[axis],affine.solid_torque_nm[axis],
             1e-8,"solid-cell pressure poisoning cannot change torque");
    }
    // For a floor-contacting solid, a uniform gauge offset changes the
    // roof load; it must NOT be falsely treated as an immersed closed body.
    fill(1000,0,0,0);
    const auto uniform=Audit::audit(nx,ny,nz,dx,box,p);
    near(uniform.solid_force_n[0],0,1e-8,"uniform x force");
    near(uniform.solid_force_n[1],0,1e-8,"uniform y force");
    near(uniform.solid_force_n[2],-1000*top_area,1e-8,
         "uniform pressure floor-contact roof load");
    std::cout<<std::setprecision(17)
        <<"MARIN_TRACTION grid="<<grid<<" dx="<<dx
        <<" hydro_fx="<<hydro.solid_force_n[0]
        <<" hydro_fy="<<hydro.solid_force_n[1]
        <<" hydro_fz="<<hydro.solid_force_n[2]
        <<" affine_fx="<<affine.solid_force_n[0]
        <<" affine_fy="<<affine.solid_force_n[1]
        <<" affine_fz="<<affine.solid_force_n[2]
        <<" wetted_m2="<<hydro.wetted_surface_area_m2
        <<" floor_m2="<<hydro.floor_contact_area_m2
        <<" tiles="<<hydro.pressure_tiles<<"\n";
}
void negative() {
    const Box b=marin();
    std::vector<double> p(32*10*10,1000);
    rejects([&]{Audit::audit(32,10,10,.1,b,{});},"reject missing pressure");
    p[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Audit::audit(32,10,10,.1,b,p);},"reject nan pressure");
    p[0]=1000;
    auto floating=b;floating.min_m[2]=.001;
    rejects([&]{Audit::audit(32,10,10,.1,floating,p);},
            "reject box not mounted on floor");
    auto side=b;side.min_m[0]=0;
    rejects([&]{Audit::audit(32,10,10,.1,side,p);},
            "reject second tank contact");
    auto out=b;out.max_m[1]=1.1;
    rejects([&]{Audit::audit(32,10,10,.1,out,p);},
            "reject box outside tank");
    auto no_outer_stencil=b;no_outer_stencil.max_m[0]=3.12;
    rejects([&]{Audit::audit(32,10,10,.1,no_outer_stencil,p);},
            "reject unavailable two-cell liquid-side stencil");
}
void native_snapshot() {
    constexpr std::size_t nx=32,ny=10,nz=10;
    constexpr double dx=.1;
    const auto b=marin();
    upc::CoupledMacWater3D water(nx,ny,nz,dx,1000);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double xx=(x+.5)*dx,yy=(y+.5)*dx,zz=(z+.5)*dx;
        if(xx>=b.min_m[0]&&xx<b.max_m[0]&&
           yy>=b.min_m[1]&&yy<b.max_m[1]&&zz<b.max_m[2])
            water.set_solid(x,y,z);
        else if(zz<.55)water.set_water(x,y,z,1);
    }
    water.set_pcg_pressure_projection(true);
    water.step_verified(.001,1000,1e-6);
    std::vector<double> p(nx*ny*nz);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x)
        p[index(x,y,z,nx,ny)]=water.pressure_pa(x,y,z);
    const auto result=Audit::audit(nx,ny,nz,dx,b,p);
    for(double f:result.solid_force_n)
        check(std::isfinite(f),"native MAC pressure load finite");
    check(water.volume_m3()>0,"native MAC water present");
    std::cout<<std::setprecision(17)
        <<"LIVE_TRACTION step_s=0.001 fx="<<result.solid_force_n[0]
        <<" fy="<<result.solid_force_n[1]
        <<" fz="<<result.solid_force_n[2]
        <<" volume_m3="<<water.volume_m3()
        <<" physical_cut_solver=false\n";
}
}
int main() {
    try {
        manufactured("coarse",32,10,10,.1);
        manufactured("medium",48,15,15,3.2/48.);
        manufactured("fine",64,20,20,.05);
        negative();
        native_snapshot();
        std::cout<<"PASS MARIN_TRACTION assertions="<<assertions
                 <<" floor_contact=true physical_cut_solver=false"
                 <<" measured_validation=false phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL MARIN_TRACTION "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
