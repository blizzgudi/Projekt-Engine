// P2 PARTIAL: manufactured normal-gradient pressure + native MAC smoke.
// These checks do not validate wall traction or MARIN pressure agreement.
#include "upc/p2_marin_wall_normal_pressure3d.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
int assertions=0;
void check(bool yes,const char* why){++assertions;if(!yes)throw std::runtime_error(why);}
void near(double a,double b,double tol,const char* why){
    check(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
auto wet=[](std::size_t,std::size_t,std::size_t){return 1.0;};
auto solid=[](std::size_t,std::size_t,std::size_t){return false;};
void affine(double dx,std::size_t nx,std::size_t ny,std::size_t nz){
    auto p=[dx](std::size_t x,std::size_t y,std::size_t z){
        return 900+31*(x+.5)*dx+7*(y+.5)*dx+13*(z+.5)*dx;
    };
    auto r=upc::P2MarinWallNormalPressure3D::sample_field(dx,nx,ny,nz,p,wet,solid);
    check(r.wet_count==8&&r.extrapolated_count==8&&r.fallback_count==0,
          "affine 3D all probes extrapolated");
    const auto coords=upc::P2MarinPressureSensors3D::transducers_m();
    for(std::size_t i=0;i<8;++i){
        const auto& q=coords[i];
        const double x=i<4?q[0]:std::clamp(q[0],.5*dx,(nx-.5)*dx);
        const double y=std::clamp(q[1],.5*dx,(ny-.5)*dx);
        const double z=i<4?std::clamp(q[2],.5*dx,(nz-.5)*dx):q[2];
        near(r.probes[i].pressure_pa,900+31*x+7*y+13*z,1e-10,
             "affine normal+tangent reconstruction exact");
        check(r.probes[i].first_plane_wet_weight>0.999999,
              "first plane full");
        check(r.probes[i].second_plane_wet_weight>0.999999,
              "second plane full");
        check(r.probes[i].wall_to_first_centre_m<0,
              "wall lies before liquid centre");
    }
}
double quadratic_error(double dx,std::size_t nx,std::size_t ny,std::size_t nz,
                       std::size_t i){
    auto p=[dx](std::size_t x,std::size_t,std::size_t z){
        const double xx=(x+.5)*dx,zz=(z+.5)*dx;
        return 3000+100*xx*xx+200*zz*zz;
    };
    auto r=upc::P2MarinWallNormalPressure3D::sample_field(dx,nx,ny,nz,p,wet,solid);
    const auto q=upc::P2MarinPressureSensors3D::transducers_m()[i];
    const double x=i<4?q[0]:std::clamp(q[0],.5*dx,(nx-.5)*dx);
    const double z=i<4?std::clamp(q[2],.5*dx,(nz-.5)*dx):q[2];
    return std::abs(r.probes[i].pressure_pa-(3000+100*x*x+200*z*z));
}
void quadratic(){
    for(std::size_t i:{std::size_t(0),std::size_t(4)}){
        const auto e0=quadratic_error(.1,32,10,10,i);
        const auto e1=quadratic_error(.05,64,20,20,i);
        check(e0>1e-6&&e1>1e-7,"nonzero quadratic truncation");
        check(e1<e0,"wall-normal quadratic error decreases with dx");
    }
}
void masks_and_errors(){
    const auto p=[](std::size_t,std::size_t,std::size_t){return 500.0;};
    const auto dry=[](std::size_t,std::size_t,std::size_t){return 0.0;};
    auto r=upc::P2MarinWallNormalPressure3D::sample_field(.1,32,10,10,p,dry,solid);
    check(r.wet_count==0&&r.extrapolated_count==0,"dry probes excluded");
    for(const auto& v:r.probes)near(v.pressure_pa,0,0,"dry pressure zero");
    // Deliberately wet only the first liquid-side x/z plane, not the second.
    const auto partial=[](std::size_t x,std::size_t,std::size_t z){
        return ((x==8&&z!=3)||(z==2&&x!=9))?1.0:0.0;
    };
    r=upc::P2MarinWallNormalPressure3D::sample_field(.1,32,10,10,p,partial,solid);
    check(r.wet_count==8&&r.extrapolated_count==0&&r.fallback_count==8,
          "second plane dry forces explicit fallback");
    for(const auto& v:r.probes)near(v.pressure_pa,500,1e-12,"fallback first pressure");
    const auto nan=[](std::size_t,std::size_t,std::size_t){
        return std::numeric_limits<double>::quiet_NaN();
    };
    bool threw=false;
    try{(void)upc::P2MarinWallNormalPressure3D::sample_field(
        .1,32,10,10,nan,wet,solid);}
    catch(const std::runtime_error&){threw=true;}
    check(threw,"reject nonfinite pressure");
    threw=false;
    try{(void)upc::P2MarinWallNormalPressure3D::sample_field(
        .1,32,10,10,p,nan,solid);}
    catch(const std::invalid_argument&){threw=true;}
    check(threw,"reject nonfinite liquid fraction");
    threw=false;
    try{(void)upc::P2MarinWallNormalPressure3D::sample_field(
        0,32,10,10,p,wet,solid);}
    catch(const std::invalid_argument&){threw=true;}
    check(threw,"reject zero cell size");
}
void native(double dx,std::size_t nx,std::size_t ny,std::size_t nz){
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
    auto r=upc::P2MarinWallNormalPressure3D::sample(w);
    check(r.wet_count==8,"native 3D MAC all probes wet");
    for(const auto& v:r.probes)
        check(std::isfinite(v.pressure_pa),"native wall-normal pressure finite");
    std::cout<<"NATIVE_PCG dx="<<dx<<" extrapolated="<<r.extrapolated_count
             <<" fallback="<<r.fallback_count<<"\n";
}
}
int main(){
    try{
        affine(.1,32,10,10);affine(.05,64,20,20);
        quadratic();masks_and_errors();
        native(.1,32,10,10);native(.05,64,20,20);
        std::cout<<"PASS P2_MARIN_WALL_NORMAL assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_MARIN_WALL_NORMAL "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
