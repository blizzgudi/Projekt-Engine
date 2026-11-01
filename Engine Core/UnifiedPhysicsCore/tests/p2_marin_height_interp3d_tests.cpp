// P2 PARTIAL: manufactured and real 3D MAC/VOF water-column interpolation.
// This does not validate laboratory water-level agreement or free-surface CFD.
#include "upc/p2_marin_height_interp3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t assertions=0;
void check(bool yes,const char* why) {
    ++assertions;
    if(!yes)throw std::runtime_error(why);
}
void near(double got,double expected,double tol,const char* why) {
    check(std::isfinite(got)&&std::abs(got-expected)<=tol,why);
}
struct Grid {double dx;std::size_t nx,ny,nz;};
constexpr Grid coarse{.1,32,10,10},fine{.05,64,20,20};
constexpr std::array<double,4> locations{{.496,.992,1.488,2.632}};
double clamp_centre(double p,double dx,std::size_t n) {
    return std::clamp(p,dx*.5,(n-.5)*dx);
}
void affine(const Grid& g) {
    const auto f=[&](std::size_t x,std::size_t y,std::size_t z) {
        return .1+.02*(x+.5)*g.dx+.03*(y+.5)*g.dx+.01*(z+.5)*g.dx;
    };
    for(double x:locations) {
        const auto result=upc::P2MarinHeightInterpolation3D::sample_field(
            g.dx,g.nx,g.ny,g.nz,x,.5,f);
        const double xx=clamp_centre(x,g.dx,g.nx);
        const double yy=clamp_centre(.5,g.dx,g.ny);
        const double h=g.nz*g.dx*(.1+.02*xx+.03*yy+.01*g.nz*g.dx*.5);
        near(result.height_m,h,1e-13,"affine manufactured height exact");
        near(result.interpolation_weight,1,1e-14,"interpolation weights sum to one");
        check(result.columns>=1&&result.columns<=4,"bounded 2D column stencil");
    }
}
double quadratic_error(const Grid& g,double x) {
    const auto f=[&](std::size_t i,std::size_t j,std::size_t) {
        const double px=(i+.5)*g.dx,py=(j+.5)*g.dx;
        return .1+.04*px*px+.01*py*py;
    };
    const auto r=upc::P2MarinHeightInterpolation3D::sample_field(
        g.dx,g.nx,g.ny,g.nz,x,.5,f);
    const double expected=g.nz*g.dx*(.1+.04*x*x+.01*.5*.5);
    return std::abs(r.height_m-expected);
}
void quadratic_convergence() {
    for(double x:locations) {
        const double a=quadratic_error(coarse,x);
        const double b=quadratic_error(fine,x);
        check(a>1e-12&&b>1e-12,"nonzero quadratic truncation");
        check(b<a,"quadratic interpolation converges under grid refinement");
    }
}
void dry_wet_edges_and_failures() {
    const auto full=[](std::size_t,std::size_t,std::size_t){return 1.0;};
    const auto empty=[](std::size_t,std::size_t,std::size_t){return 0.0;};
    for(const Grid& g:{coarse,fine}) {
        const auto wet=upc::P2MarinHeightInterpolation3D::sample_field(
            g.dx,g.nx,g.ny,g.nz,0,0,full);
        const auto dry=upc::P2MarinHeightInterpolation3D::sample_field(
            g.dx,g.nx,g.ny,g.nz,g.nx*g.dx-1e-12,.5,empty);
        near(wet.height_m,g.nz*g.dx,1e-14,"full-column volume height");
        near(dry.height_m,0,0,"dry-column height");
        check(wet.columns==1,"domain edge clamp samples first column");
    }
    const auto invalid=[](std::size_t,std::size_t,std::size_t) {
        return std::numeric_limits<double>::quiet_NaN();
    };
    for(double x:{-1.0,3.2,std::numeric_limits<double>::quiet_NaN()}) {
        bool threw=false;
        try {(void)upc::P2MarinHeightInterpolation3D::sample_field(
            .1,32,10,10,x,.5,full);}
        catch(const std::out_of_range&){threw=true;}
        check(threw,"out-of-domain/nonfinite gauge rejected");
    }
    bool threw=false;
    try {(void)upc::P2MarinHeightInterpolation3D::sample_field(
        .1,32,10,10,.496,.5,invalid);}
    catch(const std::invalid_argument&){threw=true;}
    check(threw,"nonfinite VOF rejected");
    threw=false;
    try {(void)upc::P2MarinHeightInterpolation3D::sample_field(
        .1,32,10,10,.496,.5,
        [](std::size_t,std::size_t,std::size_t){return 1.01;});}
    catch(const std::invalid_argument&){threw=true;}
    check(threw,"overfilled VOF rejected");
    threw=false;
    try {(void)upc::P2MarinHeightInterpolation3D::sample_field(
        0,32,10,10,.496,.5,full);}
    catch(const std::invalid_argument&){threw=true;}
    check(threw,"invalid dx rejected");
}
void live_mac(const Grid& g) {
    upc::CoupledMacWater3D water(g.nx,g.ny,g.nz,g.dx,1000.);
    for(std::size_t z=0;z<g.nz;++z)
    for(std::size_t y=0;y<g.ny;++y)
    for(std::size_t x=0;x<g.nx;++x) {
        if((x+.5)*g.dx<1.992&&(z+.5)*g.dx<.55)
            water.set_water(x,y,z,1);
    }
    water.step(.001,150);
    for(double x:locations) {
        const auto probe=upc::P2MarinHeightInterpolation3D::sample(water,x,.5);
        check(std::isfinite(probe.height_m)&&probe.height_m>=0&&
              probe.height_m<=g.nz*g.dx+1e-12,"live 3D VOF height bounded");
        near(probe.interpolation_weight,1,1e-12,"live probe weights normalized");
    }
}
}
int main() {
    try {
        affine(coarse);affine(fine);
        quadratic_convergence();
        dry_wet_edges_and_failures();
        live_mac(coarse);live_mac(fine);
        std::cout<<"PASS P2_MARIN_HEIGHT_INTERP assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL P2_MARIN_HEIGHT_INTERP "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
