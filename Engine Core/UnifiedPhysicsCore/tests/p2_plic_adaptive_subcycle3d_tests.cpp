#include "upc/p2_plic_adaptive_subcycle3d.hpp"
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
static std::size_t checks=0;
void check(bool ok,const char* label){
    ++checks;
    if(!ok)throw std::runtime_error(label);
}
void near(double a,double b,double tol,const char* label){
    ++checks;
    if(!std::isfinite(a)||std::abs(a-b)>tol)
        throw std::runtime_error(std::string(label)+" got "+std::to_string(a)+
                                 " expected "+std::to_string(b));
}
std::size_t id(std::size_t x,std::size_t y,std::size_t z,D d){
    return (z*d[1]+y)*d[0]+x;
}
V velocity(D d,std::array<double,3> s){
    V v;
    for(int a=0;a<3;++a)v[a].assign(d[0]*d[1]*d[2],s[a]);
    return v;
}
std::vector<double> box(D d,D lo,D hi){
    std::vector<double> f(d[0]*d[1]*d[2],0);
    for(std::size_t z=lo[2];z<hi[2];++z)
    for(std::size_t y=lo[1];y<hi[1];++y)
    for(std::size_t x=lo[0];x<hi[0];++x)
        f[id(x,y,z,d)]=1;
    return f;
}
double overlap(double a,double b,double c,double d){
    return std::max(0.0,std::min(b,d)-std::max(a,c));
}
double periodic_fraction(std::size_t i,std::size_t n,
                         std::size_t lo,std::size_t hi,double shift){
    double fraction=0;
    for(int wrap=-1;wrap<=1;++wrap)
        fraction+=overlap(double(i),double(i+1),
                          double(lo)+shift+wrap*double(n),
                          double(hi)+shift+wrap*double(n));
    return fraction;
}
void one_step_equivalence(){
    const D d{8,8,8};
    const auto f=box(d,{2,2,2},{5,5,5});
    const auto v=velocity(d,{.12,-.08,.1});
    const auto input_copy=f;
    const auto v_copy=v;
    const auto direct=upc::p2_plic_unsplit_mac3d(
        f,v,d[0],d[1],d[2],.1,.05);
    const auto r=upc::p2_plic_adaptive_subcycle3d(
        f,v,d[0],d[1],d[2],.1,.05);
    check(r.substeps==1,"single substep for safe Courant");
    check(r.water_fill==direct.water_fill,"bitwise one-step equivalence");
    near(r.max_six_face_courant_per_substep,.3,1e-14,"one-step CFL");
    near(r.mass_residual_m3,0,1e-12,"one-step conservation");
    check(f==input_copy&&v==v_copy,"input immutability");
}
void translated_box(D d,D lo,D hi,std::array<double,3> speed,
                    double dt,double target,const char* label){
    const double dx=.1;
    const auto f=box(d,lo,hi);
    const auto v=velocity(d,speed);
    const auto r=upc::p2_plic_adaptive_subcycle3d(
        f,v,d[0],d[1],d[2],dx,dt,target);
    const double full_cfl=2*(std::abs(speed[0])+std::abs(speed[1])+
                             std::abs(speed[2]))*dt/dx;
    const auto expected=static_cast<std::size_t>(
        std::ceil(full_cfl/target));
    check(r.substeps+1>=expected && r.substeps<=expected+1,
          "CFL substep count within floating-point ceiling");
    check(r.substeps>1,"actual subcycling");
    check(r.max_six_face_courant_per_substep<=target+1e-12,
          "substep CFL bounded");
    check(r.transported_faces>0,"geometric transfers");
    near(r.mass_residual_m3,0,1e-12,"translated mass");
    double total=0,l1=0;
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const double exact=
            periodic_fraction(x,d[0],lo[0],hi[0],speed[0]*dt/dx)*
            periodic_fraction(y,d[1],lo[1],hi[1],speed[1]*dt/dx)*
            periodic_fraction(z,d[2],lo[2],hi[2],speed[2]*dt/dx);
        const double value=r.water_fill[id(x,y,z,d)];
        check(value>=0&&value<=1,"bounded translated VOF");
        l1+=std::abs(value-exact);
        total+=f[id(x,y,z,d)];
    }
    near(r.initial_volume_m3,total*dx*dx*dx,1e-12,"initial volume");
    near(r.final_volume_m3,total*dx*dx*dx,1e-12,"final volume");
    const double relative=l1/total;
    check(relative<.25,"bounded reference translation error");
    std::cout<<label<<","<<d[0]<<","<<d[1]<<","<<d[2]<<","
             <<r.substeps<<","<<r.initial_volume_m3<<","
             <<r.final_volume_m3<<","<<relative<<","
             <<r.max_six_face_courant_per_substep<<","
             <<r.transported_faces<<"\n";
}
void variable_curl(){
    const D d{8,8,8};
    const double dx=.1,dt=.8;
    const double pi=std::acos(-1.0);
    const auto potential=[&](std::size_t x,std::size_t y,std::size_t z){
        return .01*std::sin(2*pi*x/d[0])*std::sin(2*pi*y/d[1])*
               (1+.2*std::cos(2*pi*z/d[2]));
    };
    auto v=velocity(d,{0,0,0});
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        v[0][id(x,y,z,d)]=(potential(x+1,y+1,z)-potential(x+1,y,z))/dx;
        v[1][id(x,y,z,d)]=-(potential(x+1,y+1,z)-potential(x,y+1,z))/dx;
    }
    const auto f=box(d,{2,2,2},{6,6,6});
    const auto r=upc::p2_plic_adaptive_subcycle3d(
        f,v,d[0],d[1],d[2],dx,dt,.25);
    check(r.substeps>1,"curl requires subcycling");
    check(r.max_divergence_s_inv<1e-12,"curl MAC divergence");
    check(r.max_six_face_courant_per_substep<=.25+1e-12,
          "curl substep CFL");
    check(r.reconstructed_partial_cell_visits>0,
          "curl updates reconstructed interface");
    near(r.mass_residual_m3,0,1e-12,"curl mass");
    check(r.water_fill!=f,"curl transports actual water");
    for(double x:r.water_fill)check(x>=0&&x<=1,"curl bounded");
    std::cout<<"curl,"<<d[0]<<","<<d[1]<<","<<d[2]<<","
             <<r.substeps<<","<<r.initial_volume_m3<<","
             <<r.final_volume_m3<<",0,"
             <<r.max_six_face_courant_per_substep<<","
             <<r.transported_faces<<"\n";
}
void reject_invalid(){
    const D d{6,6,6};
    const auto f=box(d,{2,2,2},{4,4,4});
    auto v=velocity(d,{.12,-.08,.1});
    auto rejected=[&](const std::vector<double>& water,const V& field,
                      double dt,double target,std::size_t max_steps){
        bool failed=false;
        try{(void)upc::p2_plic_adaptive_subcycle3d(
            water,field,d[0],d[1],d[2],.1,dt,target,max_steps);}
        catch(const std::invalid_argument&){failed=true;}
        check(failed,"invalid adaptive input rejected");
    };
    rejected(f,v,.05,0,256);
    rejected(f,v,.05,1.01,256);
    rejected(f,v,.05,std::numeric_limits<double>::quiet_NaN(),256);
    rejected(f,v,.05,.4,0);
    rejected(f,v,.05,.4,4097);
    rejected(f,v,0,.4,256);
    rejected(f,v,std::numeric_limits<double>::infinity(),.4,256);
    rejected(f,v,2,.4,2); // budget exceeded, no truncated physics
    v[0][0]+=.1;rejected(f,v,.05,.4,256); // non-solenoidal
    v[0][0]-=.1;v[1].pop_back();rejected(f,v,.05,.4,256);
    v=velocity(d,{0,0,0});
    const auto stopped=upc::p2_plic_adaptive_subcycle3d(
        f,v,d[0],d[1],d[2],.1,.05);
    check(stopped.substeps==1&&stopped.water_fill==f,
          "stationary VOF exactly unchanged");
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"case,nx,ny,nz,substeps,initial_volume,final_volume,"
                    "analytic_relative_l1,max_step_courant,transported_faces\n";
        one_step_equivalence();
        translated_box({12,10,8},{3,3,2},{7,6,5},
                       {.12,-.08,.1},.4,.4,"translation");
        translated_box({12,10,8},{10,3,2},{12,6,5},
                       {.12,0,0},.4,.4,"seam");
        variable_curl();
        reject_invalid();
        std::cout<<"PASS: adaptive PLIC subcycling assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: adaptive PLIC subcycling assertions="<<checks
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
