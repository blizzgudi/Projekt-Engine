// P2 PARTIAL: independent manufactured disk integrals + native 3D MAC sample.
// A configurable footprint does not establish the MARIN sensor diameter or
// physically validated free-surface/wall traction.
#include "upc/p2_marin_footprint_pressure3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned assertions=0;
void check(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
void near(double got,double expected,double tol,const char* why) {
    check(std::isfinite(got)&&std::abs(got-expected)<=tol,why);
}
auto wet=[](std::size_t,std::size_t,std::size_t){return 1.0;};
auto solid=[](std::size_t,std::size_t,std::size_t){return false;};
auto constant=[](std::size_t,std::size_t,std::size_t){return 400.0;};
template<class F>void rejects(F f,const char* why) {
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    check(threw,why);
}
void manufactured_affine(double dx) {
    const auto p=[dx](std::size_t x,std::size_t y,std::size_t z) {
        return 100.0+5.0*(x+.5)*dx+7.0*(y+.5)*dx+11.0*(z+.5)*dx;
    };
    const auto result=upc::P2MarinFootprintPressure3D::sample_field(
        dx,std::size_t(std::ceil(3.3/dx)),std::size_t(std::ceil(1/dx)),
        std::size_t(std::ceil(1/dx)),.012,8,32,p,wet,solid);
    check(result.wet_count==8,"affine all eight gauges wet");
    const auto coords=upc::P2MarinPressureSensors3D::transducers_m();
    for(std::size_t i=0;i<8;++i) {
        const auto& v=result.probes[i];
        const auto& q=coords[i];
        near(v.area_mean_gauge_pressure_pa,
             100.0+5.0*q[0]+7.0*q[1]+11.0*q[2],1e-9,
             "affine footprint disk mean equals centre pressure");
        near(v.wet_area_fraction,1.0,1e-12,"affine full wet disk");
        check(v.wet_quadrature_points==256&&v.extrapolated_points==256&&
              v.fallback_points==0,"affine all disk points extrapolated");
    }
}
void manufactured_floor_hydrostatic(double dx) {
    // Independent linear hydrostatic field p(z)=rho*g*(H-z).
    // P1 is closer to the floor than half a cell on coarse/fine grids:
    // centre clamping biases its wall pressure by hundreds of Pa.
    const auto hydrostatic=[dx](std::size_t,std::size_t,std::size_t z) {
        return 9810.0*(1.0-(double(z)+0.5)*dx);
    };
    const auto result=upc::P2MarinFootprintPressure3D::sample_field(
        dx,std::size_t(std::ceil(3.3/dx)),
        std::size_t(std::ceil(1.0/dx)),
        std::size_t(std::ceil(1.0/dx)),
        .012,8,32,hydrostatic,wet,solid);
    const auto positions=upc::P2MarinPressureSensors3D::transducers_m();
    for(std::size_t i=0;i<8;++i) {
        const double reference=9810.0*(1.0-positions[i][2]);
        near(result.probes[i].area_mean_gauge_pressure_pa,reference,1e-7,
             "manufactured hydrostatic wall disk including floor subcell");
    }
    std::cout<<std::setprecision(17)
             <<"FLOOR_HYDROSTATIC dx_m="<<dx
             <<" p1_native_pa="<<result.probes[0].area_mean_gauge_pressure_pa
             <<" p1_analytic_pa="<<9810.0*(1.0-positions[0][2])
             <<" p1_clamped_centre_pa="<<9810.0*(1.0-.5*dx)
             <<" all_8_affine_pass=true\n";
}
void boundary_partial_wet_fallback() {
    constexpr double dx=.1;
    const auto hydrostatic=[](std::size_t,std::size_t,std::size_t z) {
        return 9810.0*(1.0-(double(z)+0.5)*.1);
    };
    const auto half=[](std::size_t,std::size_t,std::size_t){return .5;};
    const auto result=upc::P2MarinFootprintPressure3D::sample_field(
        dx,32,10,10,.012,8,32,hydrostatic,half,solid);
    // The boundary extension must be disabled at partial VOF patches:
    // signed weights must NEVER increase or reduce the wet disk area.
    near(result.probes[0].wet_area_fraction,.5,1e-12,
         "partial P1 wet footprint retains bounded geometric area");
    near(result.probes[0].area_mean_gauge_pressure_pa,
         .5*9810.0*(1.0-.5*dx),1e-8,
         "partial P1 falls back to original positive-weight stencil");
    check(result.probes[0].wet_quadrature_points==256,
          "all partial P1 disk points accounted for");
}
void manufactured_quadratic(){
    constexpr double dx=.005;
    const auto p=[](std::size_t x,std::size_t y,std::size_t z) {
        constexpr double h=.005;
        const double xx=(x+.5)*h-.8035;
        const double yy=(y+.5)*h-.471;
        const double zz=(z+.5)*h-.021;
        return 1000.0+100.0*xx*xx+200.0*yy*yy+300.0*zz*zz;
    };
    const auto a=upc::P2MarinFootprintPressure3D::sample_field(
        dx,650,200,200,1e-7,8,32,p,wet,solid);
    const auto b=upc::P2MarinFootprintPressure3D::sample_field(
        dx,650,200,200,.012,8,32,p,wet,solid);
    const double front_expected=(200.0+300.0)*.012*.012/4.0;
    const double roof_expected=(100.0+200.0)*.012*.012/4.0;
    near(b.probes[0].area_mean_gauge_pressure_pa-
         a.probes[0].area_mean_gauge_pressure_pa,
         front_expected,.004,"front disk quadratic area moment");
    near(b.probes[4].area_mean_gauge_pressure_pa-
         a.probes[4].area_mean_gauge_pressure_pa,
         roof_expected,.004,"roof disk quadratic area moment");
    check(b.probes[0].area_mean_gauge_pressure_pa>
          a.probes[0].area_mean_gauge_pressure_pa,
          "finite footprint differs from point sample");
}
void masks_and_negative() {
    const auto dry=[](std::size_t,std::size_t,std::size_t){return 0.0;};
    auto r=upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,.01,4,16,constant,dry,solid);
    check(r.wet_count==0,"dry footprint has zero wet probes");
    for(const auto& v:r.probes){
        near(v.area_mean_gauge_pressure_pa,0,0,"dry area pressure zero");
        near(v.wet_area_fraction,0,0,"dry area fraction zero");
    }
    const auto half=[](std::size_t,std::size_t,std::size_t){return 0.5;};
    r=upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,.01,4,16,constant,half,solid);
    check(r.wet_count==8,"half wet disk included");
    for(const auto& v:r.probes){
        near(v.area_mean_gauge_pressure_pa,200,1e-9,
             "half wet pressure is NOT renormalized to 400 Pa");
        near(v.wet_area_fraction,.5,1e-12,"half wet disk area");
    }
    const auto first_only=[](std::size_t x,std::size_t,std::size_t z) {
        return x<9&&z<3?1.0:0.0;
    };
    r=upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,.01,4,16,constant,first_only,solid);
    check(r.wet_count==8,"first normal plane remains wet");
    for(const auto& v:r.probes){
        near(v.area_mean_gauge_pressure_pa,400,1e-9,"dry second plane fallback");
        check(v.fallback_points==64&&v.extrapolated_points==0,
              "explicit normal fallback count");
    }
    const auto nan=[](std::size_t,std::size_t,std::size_t){
        return std::numeric_limits<double>::quiet_NaN();
    };
    rejects([&]{(void)upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,0,4,16,constant,wet,solid);},"reject zero radius");
    rejects([&]{(void)upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,.021,4,16,constant,wet,solid);},"reject radius beyond face");
    rejects([&]{(void)upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,.01,0,16,constant,wet,solid);},"reject zero radial rings");
    rejects([&]{(void)upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,.01,4,15,constant,wet,solid);},"reject asymmetric quadrature");
    rejects([&]{(void)upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,.01,4,16,nan,wet,solid);},"reject nonfinite wet pressure");
    rejects([&]{(void)upc::P2MarinFootprintPressure3D::sample_field(
        .1,32,10,10,.01,4,16,constant,nan,solid);},"reject nonfinite fraction");
    rejects([&]{(void)upc::P2MarinFootprintPressure3D::sample_field(
        .1,2,2,2,.01,4,16,constant,wet,solid);},"reject outside domain");
}
void native(double dx,std::size_t nx,std::size_t ny,std::size_t nz) {
    upc::CoupledMacWater3D w(nx,ny,nz,dx,1000);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x){
        const double xx=(x+.5)*dx,yy=(y+.5)*dx,zz=(z+.5)*dx;
        if(xx>=.6635&&xx<.8245&&yy>=.295&&yy<.698&&zz<.161)
            w.set_solid(x,y,z);
        else w.set_water(x,y,z,1);
    }
    w.set_pcg_pressure_projection(true);
    w.step_verified(.001,1000,1e-6);
    const auto r=upc::P2MarinFootprintPressure3D::sample(w,.01,4,16);
    check(r.wet_count==8,"native MAC all eight disk probes wet");
    for(const auto& v:r.probes) {
        check(std::isfinite(v.area_mean_gauge_pressure_pa),
              "native MAC finite disk pressure");
        check(v.wet_area_fraction>0&&v.wet_area_fraction<=1.00000001,
              "native MAC disk wet fraction");
    }
    std::cout<<"NATIVE_MAC_FOOTPRINT dx="<<dx
             <<" wet="<<r.wet_count
             <<" p1_pa="<<r.probes[0].area_mean_gauge_pressure_pa
             <<" p5_pa="<<r.probes[4].area_mean_gauge_pressure_pa<<"\n";
}
}
int main() {
    try {
        manufactured_affine(.01);
        manufactured_affine(.005);
        manufactured_floor_hydrostatic(.1);
        manufactured_floor_hydrostatic(.05);
        boundary_partial_wet_fallback();
        manufactured_quadratic();
        masks_and_negative();
        native(.1,32,10,10);
        native(.05,64,20,20);
        std::cout<<"PASS P2_MARIN_FOOTPRINT assertions="<<assertions
                 <<" radius_m_is_unverified=true phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL P2_MARIN_FOOTPRINT "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
