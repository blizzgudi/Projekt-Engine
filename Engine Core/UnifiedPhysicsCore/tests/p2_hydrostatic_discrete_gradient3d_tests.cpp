#include "upc/p2_hydrostatic_discrete_gradient3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Solver=upc::P2HydrostaticDiscreteGradient3D;
std::size_t checks=0;
void require(bool b,const char* msg) {
    ++checks;if(!b)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
std::vector<double> water(int n,double level) {
    std::vector<double> fill(n*n*n);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        fill[(z*n+y)*n+x]=std::clamp(level-z,0.0,1.0);
    return fill;
}
double buoyancy(double z,double level,double g) {
    return 1000.0*g*2.56*std::clamp(level-(z-0.8),0.0,1.6);
}
double numerical_mean_force(double z0,double z1,double level,double g) {
    if(z1==z0)return buoyancy(z0,level,g);
    double total=0;
    constexpr int slices=4096;
    for(int k=0;k<slices;++k)
        total+=buoyancy(z0+(z1-z0)*(k+0.5)/slices,level,g);
    return total/slices;
}
template<class F>void rejects(F f,const char* msg) {
    bool caught=false;try{f();}catch(const std::exception&){caught=true;}
    require(caught,msg);
}
void case_test(int n,double level,double g,double v0,double dt=0.01) {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=10000.0;body.centre_m={{4,4,4}};
    body.velocity_m_s[2]=v0;
    const auto fill=water(n,level),copy=fill;
    const auto result=Solver::step(n,n,n,1,dt,1000,g,box,body,fill);
    const double dz=result.after.centre_m[2]-4.0;
    const double v1=result.after.velocity_m_s[2];
    const double avg=numerical_mean_force(4.0,4.0+dz,level,g);
    near(dz,0.5*dt*(v0+v1),1e-12,"midpoint kinematics");
    near(result.average_buoyancy_force_N,avg,2e-4,
         "independent midpoint-integrated Archimedes force");
    near(10000.0*(v1-v0),
         (result.average_buoyancy_force_N-10000.0*g)*dt,1e-7,
         "body impulse from integrated buoyancy");
    near(result.buoyancy_reaction_unapplied_Ns,
         -avg*dt,2e-5,"unapplied water reaction excludes gravity");
    near(result.gravity_impulse_Ns,-10000.0*g*dt,1e-9,
         "external gravity impulse");
    near(result.kinetic_energy_change_J+
         result.gravity_potential_change_J+
         result.buoyancy_potential_change_J,0.0,1e-7,
         "exact body potential-kinetic ledger");
    near(result.mechanical_defect_J,0.0,1e-7,
         "discrete gradient energy residual");
    near(result.box_after.min_m[2],box.min_m[2]+dz,1e-12,
         "moving geometry follows body");
    near(result.after.centre_m[0],4.0,0.0,"no x drift");
    near(result.after.centre_m[1],4.0,0.0,"no y drift");
    require(fill==copy,"VOF input unchanged");
    require(body.centre_m[2]==4.0,"rigid input unchanged");
    require(result.bisection_iterations==100,"bracket iteration bound");
    std::cout<<"DG "<<n<<" "<<std::setprecision(17)<<level<<" "<<g
             <<" "<<v0<<" "<<dz<<" "<<result.average_buoyancy_force_N
             <<" "<<result.mechanical_defect_J<<"\n";
}
void crossing_reversibility() {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=2048;body.centre_m={{4,4,4}};
    body.velocity_m_s[2]=2;
    const auto fill=water(8,4.35);
    const auto first=Solver::step(8,8,8,1,0.2,1000,9.81,box,body,fill);
    require(first.after.centre_m[2]>4.4,"actual partial-surface crossing");
    require(std::abs(first.frozen_reference.fixed_level_mechanical_defect_J)>100,
            "frozen comparator exposes energy defect");
    near(first.mechanical_defect_J,0,1e-7,"crossing energy closure");
    auto reverse=first.after;
    reverse.velocity_m_s[2]*=-1;
    const auto back=Solver::step(8,8,8,1,0.2,1000,9.81,
                                 first.box_after,reverse,fill);
    near(back.after.centre_m[2],4.0,1e-10,"time-symmetric reverse position");
    near(back.after.velocity_m_s[2],-2.0,1e-9,"time-symmetric reverse velocity");
    near(back.mechanical_defect_J,0,1e-7,"reverse energy closure");
}
void neutral_equilibrium() {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=2048.0;body.centre_m={{4,4,4}};
    const auto r=Solver::step(8,8,8,1,0.1,1000,9.81,
                              box,body,water(8,4.0));
    near(r.after.centre_m[2],4.0,1e-12,"neutral position");
    near(r.after.velocity_m_s[2],0.0,1e-11,"neutral velocity");
    near(r.mechanical_defect_J,0.0,1e-9,"neutral energy");
    near(r.average_buoyancy_force_N,2048.0*9.81,1e-8,
         "neutral Archimedes");
}
void wet_dry_transitions() {
    const auto fill=water(8,4.35);
    for(int direction:{-1,1}) {
        const double z0=direction>0?5.1:4.0;
        const upc::P2Box3D box{{3.2,3.2,z0-0.8},
                                {4.8,4.8,z0+0.8}};
        upc::RigidPressureState3D body;
        body.mass_kg=2048.0;body.centre_m={{4,4,z0}};
        body.velocity_m_s[2]=direction>0?2.0:-3.0;
        const auto out=Solver::step(8,8,8,1,0.2,1000,9.81,
                                    box,body,fill);
        const double z1=out.after.centre_m[2];
        if(direction>0)require(z1-0.8>4.35,
                               "body fully exits water");
        else require(z1+0.8<4.35,
                     "body becomes fully submerged");
        near(out.average_buoyancy_force_N,
             numerical_mean_force(z0,z1,4.35,9.81),3e-3,
             "independent piecewise wet-dry integral");
        near(out.mechanical_defect_J,0,1e-7,
             "exact energy across hydrostatic regime transition");
    }
}
void negative() {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=2048;body.centre_m={{4,4,4}};
    const auto fill=water(8,4.0);
    rejects([&]{Solver::step(8,8,8,1,0,1000,9.81,box,body,fill);},
            "zero dt rejected");
    rejects([&]{Solver::step(8,8,8,1,.01,1000,-1,box,body,fill);},
            "negative gravity rejected");
    auto invalid=fill;invalid[0]=0.5;
    rejects([&]{Solver::step(8,8,8,1,.01,1000,9.81,box,body,invalid);},
            "nonhorizontal water rejected");
    auto too_fast=body;too_fast.velocity_m_s[2]=1000;
    rejects([&]{Solver::step(8,8,8,1,.01,1000,9.81,box,too_fast,fill);},
            "moving geometry CFL rejected");
}
}
int main() {
    try {
        for(int n:{8,12})
        for(double h:{0.0,3.125,4.0,4.35,5.75,double(n)})
        for(double g:{0.0,9.81})
        for(double v:{-0.4,0.4})
            case_test(n,h,g,v);
        crossing_reversibility();
        neutral_equilibrium();
        wet_dry_transitions();
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native hydrostatic discrete-gradient assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
