// Independent exact 1D wet-bed Riemann solution (Stoker-style):
// shallow-water two-wave, left rarefaction and right hydraulic shock.
// The star depth is solved from Rankine-Hugoniot shock and rarefaction invariants.
#include "upc/core.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace upc;
constexpr double grav=9.81, left_h=1.0, right_h=0.2, dam_x=8.0, t_final=0.3;
struct State{double h,hu;};
double branch(double h,double h_known){
    if(h<=h_known)return 2*(std::sqrt(grav*h)-std::sqrt(grav*h_known));
    return (h-h_known)*std::sqrt(0.5*grav*(1.0/h+1.0/h_known));
}
struct Star {double h,u,shock,left_tail;};
Star exact_star(){
    double a=right_h,b=left_h;
    for(int j=0;j<120;++j){
        const double m=(a+b)*0.5;
        if(branch(m,left_h)+branch(m,right_h)>0)b=m;else a=m;
    }
    const double h=(a+b)*0.5;
    const double u=(branch(h,right_h)-branch(h,left_h))*0.5;
    const double shock=h*u/(h-right_h);
    return {h,u,shock,u-std::sqrt(grav*h)};
}
State exact(double x,double t,const Star& s){
    double xi=(x-dam_x)/t;
    const double c=std::sqrt(grav*left_h);
    if(xi<=-c)return {left_h,0};
    if(xi<=s.left_tail){
        const double hh=(2*c-xi)*(2*c-xi)/(9*grav);
        const double u=2*(c+xi)/3;
        return {hh,hh*u};
    }
    if(xi<=s.shock)return {s.h,s.h*s.u};
    return {right_h,0};
}
struct Results {double dx,h_l1,hu_l1,volume_error,shock_position_error;};
Results run(double dx){
    const std::size_t nx=static_cast<std::size_t>(std::round(16./dx));
    ShallowWater2D w(nx,1,dx,1);
    const Star s=exact_star();
    if(!(s.h>right_h&&s.h<left_h&&s.u>0&&s.shock>s.u))
        throw std::runtime_error("Invalid exact wet-dam star state");
    for(std::size_t i=0;i<nx;++i)
        w.set_depth(i,0,((i+0.5)*dx<dam_x)?left_h:right_h);
    const double initial=w.volume_m3();
    w.step(t_final);
    double eh=0,ehu=0;
    double detected=-1;
    const double half=(s.h+right_h)*0.5;
    for(std::size_t i=0;i<nx;++i){
        const double x=(i+0.5)*dx;
        const WaterCell q=w.at(i,0);
        const auto ex=exact(x,t_final,s);
        eh+=std::abs(q.h-ex.h)*dx;
        ehu+=std::abs(q.hx-ex.hu)*dx;
        if(x>dam_x&&q.h>half)detected=x;
    }
    const double shock_x=dam_x+s.shock*t_final;
    return {dx,eh/16.,ehu/16.,std::abs(w.volume_m3()-initial),std::abs(detected-shock_x)};
}
int main(){
try{
    const auto s=exact_star();
    std::cout<<std::setprecision(13);
    std::cout<<"wet_star_h_m="<<s.h<<",star_u_m_s="<<s.u<<",shock_m_s="<<s.shock<<"\n";
    std::cout<<"dx_m,depth_mean_abs_error_m,momentum_mean_abs_error_m2_s,volume_error_m3,shock_position_error_m\n";
    const Results a=run(0.1),b=run(0.05),c=run(0.025);
    for(const auto& m:{a,b,c})
      std::cout<<m.dx<<","<<m.h_l1<<","<<m.hu_l1<<","<<m.volume_error<<","<<m.shock_position_error<<"\n";
    for(const auto& m:{a,b,c}){
        if(m.volume_error>1e-9)throw std::runtime_error("Wet bed volume drift");
        if(m.h_l1>0.02||m.hu_l1>0.04)throw std::runtime_error("Wet dam analytical L1 error too large");
        if(m.shock_position_error>0.20)throw std::runtime_error("Wet shock front position deviates >20 cm");
    }
    if(!(c.h_l1<b.h_l1&&b.h_l1<a.h_l1&&c.hu_l1<b.hu_l1&&b.hu_l1<a.hu_l1))
        throw std::runtime_error("Wet bed does not converge");
    if(!(c.shock_position_error<=a.shock_position_error))
        throw std::runtime_error("Shock front error fails to improve");
    std::cout<<"PASS: Stoker wet-bed Riemann, shock, mass and grid refinement\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
