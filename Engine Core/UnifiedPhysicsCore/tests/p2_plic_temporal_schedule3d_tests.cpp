#include "upc/p2_plic_temporal_schedule3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using D=std::array<std::size_t,3>;
using V=std::array<std::vector<double>,3>;
static std::size_t assertions=0;
void check(bool ok,const char* why){++assertions;if(!ok)throw std::runtime_error(why);}
void near(double a,double b,double tol,const char* why){
    ++assertions;
    if(!std::isfinite(a)||std::abs(a-b)>tol)
        throw std::runtime_error(std::string(why)+" actual="+std::to_string(a)+
                                 " expected="+std::to_string(b));
}
std::size_t id(std::size_t x,std::size_t y,std::size_t z,D d){
    return (z*d[1]+y)*d[0]+x;
}
V velocities(D d,std::array<double,3> speed){
    V v;for(int a=0;a<3;++a)v[a].assign(d[0]*d[1]*d[2],speed[a]);return v;
}
std::vector<double> block(D d,D lo,D hi){
    std::vector<double> f(d[0]*d[1]*d[2],0);
    for(std::size_t z=lo[2];z<hi[2];++z)
    for(std::size_t y=lo[1];y<hi[1];++y)
    for(std::size_t x=lo[0];x<hi[0];++x)f[id(x,y,z,d)]=1;
    return f;
}
double overlap(double a,double b,double c,double d){
    return std::max(0.0,std::min(b,d)-std::max(a,c));
}
double periodic_fraction(std::size_t i,std::size_t n,
                         std::size_t lo,std::size_t hi,double shift){
    double f=0;
    for(int wrap=-1;wrap<=1;++wrap)
        f+=overlap(double(i),double(i+1),
                   double(lo)+shift+wrap*double(n),
                   double(hi)+shift+wrap*double(n));
    return f;
}
void one_segment_parity(){
    const D d{8,8,8};const auto f=block(d,{2,2,2},{5,5,5});
    const auto v=velocities(d,{.12,-.08,.1});
    const auto baseline=upc::p2_plic_adaptive_subcycle3d(
        f,v,d[0],d[1],d[2],.1,.05);
    const std::vector<upc::P2PlicTemporalSegment3D> seg{{.05,v}};
    const auto r=upc::p2_plic_temporal_schedule3d(f,seg,d[0],d[1],d[2],.1);
    check(r.water_fill==baseline.water_fill,"single segment bit parity");
    check(r.substeps==baseline.substeps&&r.segments==1,"single segment steps");
    near(r.mass_residual_m3,0,1e-12,"single segment mass");
    near(r.total_time_s,.05,1e-14,"single segment duration");
}
void timed_translation(){
    const D d{12,10,8};const D lo{10,3,2},hi{12,6,5};
    const auto f=block(d,lo,hi);
    const V v1=velocities(d,{.2,0,0});
    const V v2=velocities(d,{.1,0,0});
    const std::vector<upc::P2PlicTemporalSegment3D> seg{
        {.2,v1},{.2,v2}};
    const auto before=f;
    const auto r=upc::p2_plic_temporal_schedule3d(
        f,seg,d[0],d[1],d[2],.1,.4);
    const auto a=upc::p2_plic_adaptive_subcycle3d(
        f,v1,d[0],d[1],d[2],.1,.2,.4);
    const auto b=upc::p2_plic_adaptive_subcycle3d(
        a.water_fill,v2,d[0],d[1],d[2],.1,.2,.4);
    check(r.water_fill==b.water_fill,"two intervals exact manual parity");
    check(r.substeps==a.substeps+b.substeps,"interval count parity");
    check(r.substeps_per_segment.size()==2,"two interval counters");
    check(r.substeps_per_segment[0]==a.substeps &&
          r.substeps_per_segment[1]==b.substeps,"interval count detail");
    check(r.transported_faces==a.transported_faces+b.transported_faces,
          "transported face sum");
    check(f==before&&seg[0].positive_face_velocity==v1 &&
          seg[1].positive_face_velocity==v2,"immutable inputs");
    near(r.total_time_s,.4,1e-14,"time sum");
    near(r.initial_volume_m3,.018,1e-12,"initial volume");
    near(r.final_volume_m3,.018,1e-12,"final volume");
    near(r.mass_residual_m3,0,1e-12,"translation mass");
    double l1=0,total=0;
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const double exact=
            periodic_fraction(x,d[0],lo[0],hi[0],.6)*
            periodic_fraction(y,d[1],lo[1],hi[1],0)*
            periodic_fraction(z,d[2],lo[2],hi[2],0);
        const double val=r.water_fill[id(x,y,z,d)];
        check(val>=0&&val<=1,"bounded time-dependent VOF");
        l1+=std::abs(val-exact);
        total+=f[id(x,y,z,d)];
    }
    const double rel=l1/total;
    check(rel<.12,"analytic periodic translation error under 12pct");
    std::cout<<"translation,"<<r.segments<<","<<r.substeps<<","
             <<r.initial_volume_m3<<","<<r.final_volume_m3<<","
             <<rel<<","<<r.max_six_face_courant_per_substep<<"\n";
}
void time_varying_curl(){
    const D d{8,8,8};const double dx=.1;
    const double pi=std::acos(-1.);
    const auto potential=[&](std::size_t x,std::size_t y,std::size_t z){
        return .01*std::sin(2*pi*x/d[0])*std::sin(2*pi*y/d[1])*
               (1+.2*std::cos(2*pi*z/d[2]));
    };
    auto v1=velocities(d,{0,0,0});
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        v1[0][id(x,y,z,d)]=(potential(x+1,y+1,z)-potential(x+1,y,z))/dx;
        v1[1][id(x,y,z,d)]=-(potential(x+1,y+1,z)-potential(x,y+1,z))/dx;
    }
    auto v2=v1;
    for(auto& comp:v2)for(double& speed:comp)speed*=.5;
    const auto f=block(d,{2,2,2},{6,6,6});
    const std::vector<upc::P2PlicTemporalSegment3D> seg{
        {.4,v1},{.1,v2}};
    const auto r=upc::p2_plic_temporal_schedule3d(
        f,seg,d[0],d[1],d[2],dx,.25);
    const auto a=upc::p2_plic_adaptive_subcycle3d(
        f,v1,d[0],d[1],d[2],dx,.4,.25);
    const auto b=upc::p2_plic_adaptive_subcycle3d(
        a.water_fill,v2,d[0],d[1],d[2],dx,.1,.25);
    check(r.water_fill==b.water_fill,"curl two-interval parity");
    check(r.substeps==a.substeps+b.substeps,"curl count");
    check(r.substeps>2,"curl adaptive actual substeps");
    near(r.mass_residual_m3,0,1e-12,"curl mass");
    check(r.max_divergence_s_inv<1e-12,"curl divergence");
    check(r.max_six_face_courant_per_substep<=.25+1e-12,"curl CFL");
    for(double x:r.water_fill)check(x>=0&&x<=1,"curl VOF bounded");
    std::cout<<"curl,"<<r.segments<<","<<r.substeps<<","
             <<r.initial_volume_m3<<","<<r.final_volume_m3<<",0,"
             <<r.max_six_face_courant_per_substep<<"\n";
}
void reject_invalid(){
    const D d{6,6,6};const auto f=block(d,{2,2,2},{4,4,4});
    const auto v=velocities(d,{.12,-.08,.1});
    std::vector<upc::P2PlicTemporalSegment3D> s{{.1,v},{.1,v}};
    const auto rejected=[&](const auto& schedule,double target,
                            std::size_t budget){
        bool failed=false;
        try{(void)upc::p2_plic_temporal_schedule3d(
            f,schedule,d[0],d[1],d[2],.1,target,budget);}
        catch(const std::invalid_argument&){failed=true;}
        check(failed,"bad temporal schedule rejected");
    };
    rejected(std::vector<upc::P2PlicTemporalSegment3D>{},.4,1024);
    rejected(s,0,1024);
    rejected(s,.4,0);
    rejected(s,.4,1); // total work budget, not truncated
    s[1].duration_s=0;rejected(s,.4,1024);
    s[1].duration_s=std::numeric_limits<double>::infinity();
    rejected(s,.4,1024);
    s[1].duration_s=.1;
    s[1].positive_face_velocity[0][0]+=.2;
    rejected(s,.4,1024); // second interval not divergence-free
    s[1].positive_face_velocity[0][0]-=.2;
    s[1].positive_face_velocity[2].pop_back();
    rejected(s,.4,1024);
    s[1].positive_face_velocity=v;
    s[1].positive_face_velocity[0][0]=
        std::numeric_limits<double>::quiet_NaN();
    rejected(s,.4,1024);
    s[1].positive_face_velocity=v;
    s[0].duration_s=std::numeric_limits<double>::max();
    s[1].duration_s=std::numeric_limits<double>::max();
    rejected(s,.4,1024);
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"case,segments,substeps,initial_volume,final_volume,"
                    "analytic_relative_l1,max_step_courant\n";
        one_segment_parity();
        timed_translation();
        time_varying_curl();
        reject_invalid();
        std::cout<<"PASS: temporal MAC PLIC assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: temporal MAC PLIC assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
