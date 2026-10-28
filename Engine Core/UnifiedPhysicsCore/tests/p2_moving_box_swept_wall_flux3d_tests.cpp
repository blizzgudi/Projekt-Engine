// P2 PARTIAL: exact 3D geometric wall swept-volume GCL for translating box.
// Deliberately not a fluid pressure projection, moving-body momentum solver,
// or a global mass/energy-conserving two-way FSI implementation.
#include "upc/p2_moving_box_swept_wall_flux3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Box=upc::P2Box3D;
using Result=upc::P2MovingBoxSweptWallFlux3D;
using Solver=upc::P2MovingBoxSweptWallFlux3DBuilder;
std::size_t assertions=0;
void check(bool ok,const char* why) {
    ++assertions;
    if(!ok) throw std::runtime_error(why);
}
void eq(double a,double b,double tol,const char* why) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
Box moved(Box b,int axis,double delta) {
    b.min_m[axis]+=delta;
    b.max_m[axis]+=delta;
    return b;
}
Result run(int n,const Box& old_box,const Box& new_box,double dt=0.25) {
    const auto r=Solver::sample(n,n,n,1.0,dt,old_box,new_box);
    check(r.u_solid_m3.size()==static_cast<std::size_t>((n+1)*n*n),
          "u swept count");
    check(r.v_solid_m3.size()==static_cast<std::size_t>(n*(n+1)*n),
          "v swept count");
    check(r.w_solid_m3.size()==static_cast<std::size_t>(n*n*(n+1)),
          "w swept count");
    eq(r.signed_open_volume_change_m3,0.0,2e-12,
       "closed box cannot change total open volume");
    eq(r.boundary_solid_flux_m3,0.0,2e-12,
       "no solid flux through domain boundary");
    check(r.max_local_gcl_residual_m3<=5e-10,
          "every cut cell obeys geometric conservation");
    for(const auto* faces:{&r.u_solid_m3,&r.v_solid_m3,&r.w_solid_m3})
        for(double q:*faces)
            check(std::isfinite(q)&&std::abs(q)<=1.0+1e-12,
                  "swept volume bounded by geometry CFL");
    return r;
}
double flux(const Result& r,int n,int axis,int plane) {
    const int x=axis==0?plane:2;
    const int y=axis==1?plane:2;
    const int z=axis==2?plane:2;
    if(axis==0)return r.u_solid_m3[(z*n+y)*(n+1)+x];
    if(axis==1)return r.v_solid_m3[(z*(n+1)+y)*n+x];
    return r.w_solid_m3[(z*n+y)*n+x];
}
void reverse_flux(const Result& a,const Result& b) {
    const std::array<std::array<const std::vector<double>*,2>,3> pairs{{
        {{&a.u_solid_m3,&b.u_solid_m3}},
        {{&a.v_solid_m3,&b.v_solid_m3}},
        {{&a.w_solid_m3,&b.w_solid_m3}}
    }};
    for(const auto& pair:pairs)
        for(std::size_t k=0;k<pair[0]->size();++k)
            eq((*pair[0])[k],-(*pair[1])[k],2e-13,
               "time reversal negates signed swept solid volume");
}
void axis_reference(int n,int axis,int sign) {
    Box b{{2.0,2.0,2.0},{3.0,3.0,3.0}};
    b.min_m[axis]=sign>0?2.25:3.75;
    b.max_m[axis]=sign>0?3.25:4.75;
    const Box after=moved(b,axis,static_cast<double>(sign));
    const auto r=run(n,b,after);
    const int near=sign>0?3:4,far=sign>0?4:3;
    const double qnear=flux(r,n,axis,near);
    const double qfar=flux(r,n,axis,far);
    eq(qnear,0.75*sign,2e-13,"independent 3/4 face sweep");
    eq(qfar,0.25*sign,2e-13,"independent 1/4 face sweep");
    eq(flux(r,n,axis,0),0.0,0.0,"no boundary sweep");
    eq(flux(r,n,axis,n),0.0,0.0,"no opposite boundary sweep");
    reverse_flux(r,run(n,after,b));
    // The same displacement at a different dt integrates to same volume.
    const auto other_dt=run(n,b,after,0.125);
    eq(flux(other_dt,n,axis,near),qnear,2e-13,
       "swept volume independent of timestep parametrization");
    std::cout<<"axis,"<<n<<","<<axis<<","<<sign<<","
             <<qnear<<","<<qfar<<","
             <<r.max_local_gcl_residual_m3<<"\n";
}
void diagonal_reference(int n,int axis) {
    Box b{{2.25,2.25,2.25},{3.25,3.25,3.25}};
    Box after=b;
    after=moved(after,axis,1.0);
    after=moved(after,(axis+1)%3,0.5);
    after=moved(after,(axis+2)%3,-0.25);
    const auto r=run(n,b,after);
    // At face 3 in the axis of unit translation, time-averaged
    // blocked area = int_0^(3/4) (0.75-0.5t)*(0.75+0.25t) dt
    // = 0.3515625 = 45/128, independently integrated polynomial.
    const double q=flux(r,n,axis,3);
    eq(q,45.0/128.0,2e-13,"mixed-axis exact polynomial sweep");
    reverse_flux(r,run(n,after,b));
    std::cout<<"diagonal,"<<n<<","<<axis<<","<<q<<","
             <<r.max_local_gcl_residual_m3<<"\n";
}
void stationary(int n) {
    const Box b{{2.2,2.3,2.4},{3.4,3.5,3.6}};
    const auto r=run(n,b,b);
    for(const auto* f:{&r.u_solid_m3,&r.v_solid_m3,&r.w_solid_m3})
        for(double q:*f)eq(q,0.0,0.0,"stationary solid has zero sweep");
    eq(r.max_local_gcl_residual_m3,0.0,0.0,
       "stationary box exact local GCL");
}
void deterministic_subcell(int n) {
    // Diverse fully contained subcell boxes and simultaneous signed
    // three-axis displacements; deterministic, no random seed dependency.
    for(int i=0;i<18;++i) {
        Box b{};
        for(int a=0;a<3;++a) {
            const double offset=0.11+0.023*((i*7+a*11)%23);
            const double width=0.61+0.07*((i+a)%6);
            b.min_m[a]=2.0+offset;
            b.max_m[a]=b.min_m[a]+width;
        }
        Box after=b;
        for(int a=0;a<3;++a) {
            const double shift=0.04*((i*13+a*17)%19-9);
            after=moved(after,a,shift);
        }
        const auto r=run(n,b,after);
        reverse_flux(r,run(n,after,b));
    }
}
template<class F>void reject(F fn,const char* why) {
    bool threw=false;
    try{fn();}catch(const std::invalid_argument&){threw=true;}
    check(threw,why);
}
void invalid() {
    const Box b{{2.25,2.0,2.0},{3.25,3.0,3.0}};
    const Box unchanged=b;
    reject([&]{Solver::sample(0,8,8,1,0.25,b,b);},"zero grid");
    reject([&]{Solver::sample(8,8,8,1,0,b,b);},"zero dt");
    reject([&]{Solver::sample(8,8,8,0,0.25,b,b);},"zero dx");
    reject([&]{Solver::sample(8,8,8,1,0.25,b,moved(b,0,1.1));},
           "geometry CFL");
    Box bad=b;bad.max_m[0]+=0.1;
    reject([&]{Solver::sample(8,8,8,1,0.25,b,bad);},"deformation");
    bad=b;bad.min_m[0]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{Solver::sample(8,8,8,1,0.25,b,bad);},"nonfinite box");
    check(b.min_m==unchanged.min_m&&b.max_m==unchanged.max_m,
          "invalid samples never mutate input");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        for(int n:{8,12,16}) {
            for(int axis=0;axis<3;++axis) {
                for(int sign:{-1,1})axis_reference(n,axis,sign);
                diagonal_reference(n,axis);
            }
            stationary(n);
            deterministic_subcell(n);
        }
        invalid();
        std::cout<<"PASS: moving-box swept-wall GCL assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
