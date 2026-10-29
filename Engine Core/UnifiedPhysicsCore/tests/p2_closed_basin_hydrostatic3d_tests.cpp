#include "upc/p2_closed_basin_hydrostatic3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using Solver=upc::P2ClosedBasinHydrostatic3D;
std::size_t checks=0;
void require(bool ok,const char* msg) {
    ++checks;if(!ok)throw std::runtime_error(msg);
}
void near(double x,double y,double tol,const char* msg) {
    require(std::isfinite(x)&&std::isfinite(y)&&std::abs(x-y)<=tol,msg);
}
upc::P2Box3D box() {return {{3.2,3.2,3.2},{4.8,4.8,4.8}};}
upc::RigidPressureState3D body(double v=0.0,double mass=10000.0) {
    upc::RigidPressureState3D b;
    b.centre_m={{4,4,4}};b.velocity_m_s[2]=v;b.mass_kg=mass;
    return b;
}
double volume(int n,double h) {
    return n*n*h-2.56*std::clamp(h-3.2,0.0,1.6);
}
double numerical_buoyancy(double z,double vol,int n,double g) {
    const double A=n*n, Ab=2.56, H=1.6;
    const double wet=std::clamp(
        (vol-A*(z-H/2))/(A-Ab),0.0,H);
    return 1000*g*Ab*wet;
}
double mean_reference(double z0,double z1,double vol,int n,double g) {
    if(z0==z1)return numerical_buoyancy(z0,vol,n,g);
    constexpr int count=8192;
    double sum=0;
    for(int k=0;k<count;++k)
        sum+=numerical_buoyancy(
            z0+(z1-z0)*(k+0.5)/count,vol,n,g);
    return sum/count;
}
double water_potential(double z,double h,double g,int n) {
    const double bottom=z-0.8;
    const double upper=std::min(h,z+0.8);
    const double excluded=h>bottom?2.56*(upper*upper-bottom*bottom):0.0;
    return 500*g*(n*n*h*h-excluded);
}
template<class F>void rejects(F f,const char* msg) {
    bool thrown=false;try{f();}catch(const std::exception&){thrown=true;}
    require(thrown,msg);
}
void reference(int n,double level,double g,double v) {
    const double vol=volume(n,level);
    const auto b=box(); const auto s=body(v);
    const auto r=Solver::step(n,n,n,1,0.01,1000,g,vol,b,s);
    const double dz=r.after.centre_m[2]-4;
    const double force=mean_reference(4,4+dz,vol,n,g);
    near(r.water_level_before_m,level,2e-13,"initial surface");
    near(r.water_volume_before_m3,vol,2e-11,"initial cut-cell mass");
    near(r.water_volume_after_m3,vol,2e-11,"final cut-cell mass");
    near(r.mean_buoyancy_N,force,2e-4,"independent path-averaged buoyancy");
    near(10000*(r.after.velocity_m_s[2]-v),
         (force-10000*g)*0.01,3e-6,"body impulse");
    near(dz,0.005*(v+r.after.velocity_m_s[2]),1e-12,
         "midpoint motion");
    near(r.buoyancy_reaction_unapplied_Ns,-r.mean_buoyancy_N*0.01,
         1e-10,"unapplied fluid reaction");
    near(r.gravity_impulse_Ns,-10000*g*0.01,1e-10,"gravity separate");
    near(r.water_gravity_change_J,
         water_potential(4+dz,r.water_level_after_m,g,n)-
         water_potential(4,level,g,n),1e-7,
         "independent displaced water gravitational potential");
    near(r.equilibrium_energy_defect_J,0.0,1e-7,
         "body+quasistatic-water potential energy");
    near(r.after.centre_m[0],4,0,"x unchanged");
    near(r.after.centre_m[1],4,0,"y unchanged");
    require(r.water_fraction_before.size()==std::size_t(n*n*n),
            "full 3D initial VOF field");
    require(r.water_fraction_after.size()==std::size_t(n*n*n),
            "full 3D final VOF field");
    for(double f:r.water_fraction_after)
        require(std::isfinite(f)&&f>=0&&f<=1,"bounded open-cell VOF");
    if(level==4&&g==0)
        require(std::abs(r.water_level_after_m-level)>1e-8,
                "moving partially immersed body changes closed waterline");
    if(level==5.75||level==double(n))
        near(r.water_level_after_m,level,2e-13,
             "fully immersed box has fixed surface");
    std::cout<<"BASIN "<<n<<" "<<std::setprecision(17)<<level<<" "<<g
             <<" "<<v<<" "<<dz<<" "<<r.water_level_after_m<<" "
             <<r.mean_buoyancy_N<<" "<<r.equilibrium_energy_defect_J
             <<" "<<r.water_volume_after_m3<<"\n";
}
void neutral_and_reverse() {
    const auto b=box();
    const double vol=volume(8,4);
    const auto neutral=Solver::step(8,8,8,1,.1,1000,9.81,vol,
                                    b,body(0,2048));
    near(neutral.after.centre_m[2],4,1e-12,"neutral body equilibrium");
    near(neutral.water_level_after_m,4,1e-12,"neutral level");
    near(neutral.equilibrium_energy_defect_J,0,1e-7,"neutral energy");
    auto moving=body(2,2048);
    const auto f=Solver::step(8,8,8,1,.2,1000,9.81,
                              volume(8,4.35),b,moving);
    require(f.after.centre_m[2]>4.3,"dynamic body actually moves");
    auto reversed=f.after;
    reversed.velocity_m_s[2]*=-1;
    const auto back=Solver::step(8,8,8,1,.2,1000,9.81,
                                 volume(8,4.35),f.box_after,reversed);
    near(back.after.centre_m[2],4,1e-10,"time reversed position");
    near(back.after.velocity_m_s[2],-2,1e-9,"time reversed velocity");
    near(back.water_level_after_m,4.35,1e-10,"reversible surface");
}
void negative() {
    const auto b=box(); const auto s=body();
    const double vol=volume(8,4);
    rejects([&]{Solver::step(8,8,8,1,.01,1000,9.81,-1,b,s);},
            "negative water volume");
    rejects([&]{Solver::step(8,8,8,1,.01,1000,9.81,600,b,s);},
            "overfilled basin");
    rejects([&]{Solver::step(8,8,8,1,0,1000,9.81,vol,b,s);},
            "zero time step");
    rejects([&]{Solver::step(8,8,8,1,.01,1000,-1,vol,b,s);},
            "negative gravity");
    auto horizontal=s;horizontal.velocity_m_s[0]=1;
    rejects([&]{Solver::step(8,8,8,1,.01,1000,9.81,vol,b,horizontal);},
            "horizontal motion excluded");
    auto noncentred=s;noncentred.centre_m[2]+=0.1;
    rejects([&]{Solver::step(8,8,8,1,.01,1000,9.81,vol,b,noncentred);},
            "body and geometry must agree");
    auto too_fast=s;too_fast.velocity_m_s[2]=1000;
    rejects([&]{Solver::step(8,8,8,1,.01,1000,9.81,vol,b,too_fast);},
            "moving geometry CFL");
}
}
int main() {
    try {
        for(int n:{8,12})
        for(double level:{0.0,3.125,3.2,4.0,5.75,double(n)})
        for(double g:{0.0,9.81})
        for(double v:{-0.4,0.4})
            reference(n,level,g,v);
        neutral_and_reverse();
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native closed-basin hydrostatic assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
