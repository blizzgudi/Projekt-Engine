// P2 PARTIAL: moving, spatially varying column waterline on a floor box.
// Independent exact piecewise-column surface/force/torque integrals.
#include "upc/p2_marin_box_wall_traction3d.hpp"
#include <algorithm>
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
static void check(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
static void near(double a,double b,double eps,const char* why) {
    check(std::isfinite(a)&&std::abs(a-b)<=eps,why);
}
template<class F>static void reject(F f,const char* why) {
    bool thrown=false;
    try{f();}catch(const std::exception&){thrown=true;}
    check(thrown,why);
}
struct Exact {
    std::array<double,3> force{},torque{};
    double area=0;
};
static Exact reference(std::size_t nx,std::size_t ny,std::size_t nz,
                       double dx,const Box& box,
                       const std::vector<double>& height,double p) {
    (void)nz;
    Exact result;
    const std::array<double,3> c{{
        .5*(box.min_m[0]+box.max_m[0]),
        .5*(box.min_m[1]+box.max_m[1]),
        .5*(box.min_m[2]+box.max_m[2])}};
    const auto column=[&](std::size_t x,std::size_t y) {
        return height[y*nx+x];
    };
    const auto clip=[](double lo,double hi,double a,double b) {
        return std::max(0.,std::min(hi,b)-std::max(lo,a));
    };
    for(int side:{-1,1}) {
        const double xwall=side<0?box.min_m[0]:box.max_m[0];
        const auto xi=std::size_t(side<0?
            std::floor(std::nextafter(xwall/dx-.5,-INFINITY)):
            std::ceil(std::nextafter(xwall/dx-.5,INFINITY)));
        for(std::size_t y=0;y<ny;++y) {
            const double width=clip(y*dx,(y+1)*dx,
                                    box.min_m[1],box.max_m[1]);
            const double wet=std::min(column(xi,y),box.max_m[2]);
            if(width<=0||wet<=0)continue;
            const double ymid=.5*(std::max(y*dx,box.min_m[1])+
                                  std::min((y+1)*dx,box.max_m[1]));
            const double fx=-side*p*width*wet;
            result.force[0]+=fx;
            result.torque[1]+=(.5*wet-c[2])*fx;
            result.torque[2]-=(ymid-c[1])*fx;
            result.area+=width*wet;
        }
    }
    for(int side:{-1,1}) {
        const double ywall=side<0?box.min_m[1]:box.max_m[1];
        const auto yi=std::size_t(side<0?
            std::floor(std::nextafter(ywall/dx-.5,-INFINITY)):
            std::ceil(std::nextafter(ywall/dx-.5,INFINITY)));
        for(std::size_t x=0;x<nx;++x) {
            const double width=clip(x*dx,(x+1)*dx,
                                    box.min_m[0],box.max_m[0]);
            const double wet=std::min(column(x,yi),box.max_m[2]);
            if(width<=0||wet<=0)continue;
            const double xmid=.5*(std::max(x*dx,box.min_m[0])+
                                  std::min((x+1)*dx,box.max_m[0]));
            const double fy=-side*p*width*wet;
            result.force[1]+=fy;
            result.torque[0]-=(.5*wet-c[2])*fy;
            result.torque[2]+=(xmid-c[0])*fy;
            result.area+=width*wet;
        }
    }
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        if(column(x,y)<box.max_m[2])continue;
        const double wx=clip(x*dx,(x+1)*dx,
                             box.min_m[0],box.max_m[0]);
        const double wy=clip(y*dx,(y+1)*dx,
                             box.min_m[1],box.max_m[1]);
        if(wx<=0||wy<=0)continue;
        const double mx=.5*(std::max(x*dx,box.min_m[0])+
                             std::min((x+1)*dx,box.max_m[0]));
        const double my=.5*(std::max(y*dx,box.min_m[1])+
                             std::min((y+1)*dx,box.max_m[1]));
        const double fz=-p*wx*wy;
        result.force[2]+=fz;
        result.torque[0]+=(my-c[1])*fz;
        result.torque[1]-=(mx-c[0])*fz;
        result.area+=wx*wy;
    }
    return result;
}
int main() {
 try {
    constexpr double pressure=3000.;
    const Box box{{.6635,.295,0},{.8245,.698,.161}};
    std::cout<<std::setprecision(17);
    for(std::size_t nx:{32u,48u,64u}) {
        const std::size_t ny=nx*10/32,nz=ny;
        const double dx=3.2/double(nx);
        std::vector<double> p(nx*ny*nz,pressure);
        std::vector<double> height(nx*ny,0.0);
        for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x)
            height[y*nx+x]=.025+.020*double((x+2*y)%9);
        const auto got=Audit::audit_heightfield(nx,ny,nz,dx,box,p,height);
        const auto exact=reference(nx,ny,nz,dx,box,height,pressure);
        near(got.wetted_surface_area_m2,exact.area,2e-12,"nonuniform wet area");
        for(int a=0;a<3;++a) {
            near(got.solid_force_n[a],exact.force[a],2e-8,"nonuniform force");
            near(got.solid_torque_nm[a],exact.torque[a],2e-8,"nonuniform torque");
        }
        std::fill(height.begin(),height.end(),.103);
        const auto uniform=Audit::audit_heightfield(nx,ny,nz,dx,box,p,height);
        const auto old=Audit::audit_wetted(nx,ny,nz,dx,box,p,.103);
        near(uniform.wetted_surface_area_m2,old.wetted_surface_area_m2,
             2e-12,"uniform area backwards compatibility");
        for(int a=0;a<3;++a) {
            near(uniform.solid_force_n[a],old.solid_force_n[a],
                 2e-8,"uniform force backwards compatibility");
            near(uniform.solid_torque_nm[a],old.solid_torque_nm[a],
                 2e-8,"uniform torque backwards compatibility");
        }
        check(uniform.pressure_tiles==old.pressure_tiles,
              "uniform tile count backwards compatibility");
        std::fill(height.begin(),height.end(),0.);
        const auto dry=Audit::audit_heightfield(nx,ny,nz,dx,box,p,height);
        near(dry.wetted_surface_area_m2,0,1e-14,"dry area");
        for(double f:dry.solid_force_n)near(f,0,1e-14,"dry force");
        height.pop_back();
        reject([&]{Audit::audit_heightfield(nx,ny,nz,dx,box,p,height);},
               "invalid height shape");
        height.push_back(0.);
        height[0]=std::numeric_limits<double>::quiet_NaN();
        reject([&]{Audit::audit_heightfield(nx,ny,nz,dx,box,p,height);},
               "nonfinite column height");
        height[0]=-.01;
        reject([&]{Audit::audit_heightfield(nx,ny,nz,dx,box,p,height);},
               "negative column height");
        height[0]=nz*dx+.01;
        reject([&]{Audit::audit_heightfield(nx,ny,nz,dx,box,p,height);},
               "column height beyond tank");
        std::cout<<"HEIGHTFIELD_TRACTION nx="<<nx
                 <<" area="<<got.wetted_surface_area_m2
                 <<" fx="<<got.solid_force_n[0]
                 <<" fy="<<got.solid_force_n[1]
                 <<" fz="<<got.solid_force_n[2]
                 <<" tx="<<got.solid_torque_nm[0]
                 <<" ty="<<got.solid_torque_nm[1]
                 <<" tz="<<got.solid_torque_nm[2]<<"\n";
    }
    std::cout<<"PASS HEIGHTFIELD_TRACTION assertions="<<assertions
             <<" live_vof=false measured_validation=false"
             <<" phase_complete=false\n";
    return 0;
 }catch(const std::exception& e) {
    std::cerr<<"FAIL HEIGHTFIELD_TRACTION "<<e.what()
             <<" assertions="<<assertions<<"\n";
    return 1;
 }
}
