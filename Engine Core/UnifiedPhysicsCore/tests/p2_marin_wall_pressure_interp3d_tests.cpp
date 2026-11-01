// P2 PARTIAL: manufactured affine/quadratic pressure reconstruction
// and actual live MAC pressure readback, not a wall-stress CFD validation.
#include "upc/p2_marin_wall_pressure_interp3d.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
std::size_t assertions=0;
void check(bool pass,const char* msg){
    ++assertions;
    if(!pass)throw std::runtime_error(msg);
}
void near(double got,double expected,double tolerance,const char* msg){
    check(std::isfinite(got)&&std::abs(got-expected)<=tolerance,msg);
}
struct Grid {double dx;std::size_t nx,ny,nz;};
constexpr Grid coarse{.1,32,10,10},fine{.05,64,20,20};
double clamp_centre(double pos,double dx,std::size_t n){
    return std::clamp(pos,dx*.5,(n-.5)*dx);
}
auto always_wet=[](std::size_t,std::size_t,std::size_t){return 1.0;};
auto never_solid=[](std::size_t,std::size_t,std::size_t){return false;};

void manufactured_affine(const Grid& g){
    const auto pressure=[&g](std::size_t x,std::size_t y,std::size_t z){
        return 900.0+3.0*(x+.5)*g.dx+7.0*(y+.5)*g.dx+
               13.0*(z+.5)*g.dx;
    };
    const auto r=upc::P2MarinWallPressureInterpolation3D::sample_field(
        g.dx,g.nx,g.ny,g.nz,pressure,always_wet,never_solid);
    check(r.wet_count==8,"manufactured affine field all wet");
    for(std::size_t i=0;i<8;++i){
        const auto& p=r.probes[i];
        const auto& q=p.transducer_m;
        const bool front=i<4;
        const double x=front?p.normal_cell_centre_m[0]:
                              clamp_centre(q[0],g.dx,g.nx);
        const double y=clamp_centre(q[1],g.dx,g.ny);
        const double z=front?clamp_centre(q[2],g.dx,g.nz):
                              p.normal_cell_centre_m[2];
        near(p.pressure_pa,900+3*x+7*y+13*z,1e-10,
             "manufactured affine tangential interpolation exact");
        near(p.wet_stencil_weight,1.0,1e-12,"complete affine stencil");
        check(p.wet_stencil_cells>=1&&p.wet_stencil_cells<=4,
              "bounded bilinear stencil");
        check(p.water_wet,"manufactured pressure wet");
        check(p.normal_cell_centre_m[front?0:2]>q[front?0:2],
              "normal cell is outside the wall");
    }
}
double quadratic_error(const Grid& g,std::size_t probe){
    const auto pressure=[&g](std::size_t x,std::size_t y,std::size_t z){
        const double px=(x+.5)*g.dx,pz=(z+.5)*g.dx;
        return 2000+300*px*px+400*pz*pz;
    };
    const auto r=upc::P2MarinWallPressureInterpolation3D::sample_field(
        g.dx,g.nx,g.ny,g.nz,pressure,always_wet,never_solid);
    const auto& p=r.probes[probe];
    const bool front=probe<4;
    const double x=front?p.normal_cell_centre_m[0]:
                         clamp_centre(p.transducer_m[0],g.dx,g.nx);
    const double z=front?clamp_centre(p.transducer_m[2],g.dx,g.nz):
                         p.normal_cell_centre_m[2];
    return std::abs(p.pressure_pa-(2000+300*x*x+400*z*z));
}
void quadratic_convergence(){
    // Only wall-tangential manufactured interpolation is convergent here;
    // the true wall-normal pressure remains unresolved.
    for(auto i:{std::size_t(1),std::size_t(5)}){
        const auto e0=quadratic_error(coarse,i);
        const auto e1=quadratic_error(fine,i);
        check(e0>0&&e1>0,"nonzero quadratic interpolation truncation");
        check(e1<e0,"quadratic wall-tangent error shrinks with dx");
    }
}
void invalid_and_masked(){
    const auto p=[](std::size_t,std::size_t,std::size_t){return 500.0;};
    const auto dry=[](std::size_t,std::size_t,std::size_t){return 0.0;};
    const auto fully_dry=upc::P2MarinWallPressureInterpolation3D::sample_field(
        coarse.dx,coarse.nx,coarse.ny,coarse.nz,p,dry,never_solid);
    check(fully_dry.wet_count==0,"dry sensors not marked as wet");
    for(const auto& s:fully_dry.probes){
        near(s.pressure_pa,0,0,"dry sensor zero diagnostic");
        near(s.wet_stencil_weight,0,0,"dry stencil no weight");
    }
    const auto all_solid=[](std::size_t,std::size_t,std::size_t){return true;};
    const auto solid=upc::P2MarinWallPressureInterpolation3D::sample_field(
        coarse.dx,coarse.nx,coarse.ny,coarse.nz,p,always_wet,all_solid);
    check(solid.wet_count==0,"solid cells never read as wet fluid");
    const auto half_solid=[](std::size_t,std::size_t y,std::size_t){
        return y==4;
    };
    const auto partial=upc::P2MarinWallPressureInterpolation3D::sample_field(
        coarse.dx,coarse.nx,coarse.ny,coarse.nz,p,always_wet,half_solid);
    check(partial.wet_count==8,"partially masked pressure valid");
    for(const auto& s:partial.probes){
        near(s.pressure_pa,500,1e-10,"wet-only stencil renormalizes");
        check(s.wet_stencil_weight>0&&s.wet_stencil_weight<1,
              "masked stencil excludes solid weight");
    }
    const auto invalid=[](std::size_t,std::size_t,std::size_t){
        return std::numeric_limits<double>::quiet_NaN();
    };
    bool threw=false;
    try{
        (void)upc::P2MarinWallPressureInterpolation3D::sample_field(
            coarse.dx,coarse.nx,coarse.ny,coarse.nz,invalid,
            always_wet,never_solid);
    }catch(const std::runtime_error&){threw=true;}
    check(threw,"nonfinite MAC pressure rejected");
    threw=false;
    try{
        (void)upc::P2MarinWallPressureInterpolation3D::sample_field(
            0,coarse.nx,coarse.ny,coarse.nz,p,always_wet,never_solid);
    }catch(const std::invalid_argument&){threw=true;}
    check(threw,"zero grid spacing rejected");
}
void actual_mac(const Grid& g){
    upc::CoupledMacWater3D w(g.nx,g.ny,g.nz,g.dx,1000.0);
    const auto empty=upc::P2MarinWallPressureInterpolation3D::sample(w);
    check(empty.wet_count==0,"real empty MAC grid dry");
    for(std::size_t z=0;z<g.nz;++z)
    for(std::size_t y=0;y<g.ny;++y)
    for(std::size_t x=0;x<g.nx;++x){
        const double px=(x+.5)*g.dx,py=(y+.5)*g.dx,pz=(z+.5)*g.dx;
        if(px>=.6635&&px<.8245&&py>=.295&&py<.698&&pz<.161)
            w.set_solid(x,y,z);
        else w.set_water(x,y,z,1);
    }
    w.step(.001,150);
    const auto r=upc::P2MarinWallPressureInterpolation3D::sample(w);
    check(r.wet_count==8,"real MAC pressure all eight wet after step");
    for(const auto& s:r.probes){
        check(std::isfinite(s.pressure_pa),"real MAC interpolated pressure finite");
        check(s.wet_stencil_cells>=1&&s.wet_stencil_cells<=4,
              "real MAC pressure stencil bounded");
        check(s.wet_stencil_weight>0&&s.wet_stencil_weight<=1+1e-12,
              "real MAC pressure wet stencil normalized");
    }
    std::cout<<"P2_MARIN_WALL_INTERP_NATIVE dx="<<g.dx
             <<" probes="<<r.wet_count<<"\n";
}
}
int main(){
    try{
        manufactured_affine(coarse);
        manufactured_affine(fine);
        quadratic_convergence();
        invalid_and_masked();
        actual_mac(coarse);
        actual_mac(fine);
        std::cout<<"PASS P2_MARIN_WALL_PRESSURE_INTERP assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_MARIN_WALL_PRESSURE_INTERP "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
