#include "upc/p2_pressure_wrench_reference3d.hpp"
#include "upc/p2_moving_box_dynamic_pressure3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Box=upc::P2Box3D;
using Wrench=upc::P2PressureWrenchReference3D;
std::size_t checks=0;
void check(bool v,const char* what) {
    ++checks;
    if(!v)throw std::runtime_error(what);
}
void near(double a,double b,double tol,const char* what) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,what);
}
template<class F>void rejects(F fn,const char* what) {
    bool threw=false;
    try{fn();}catch(const std::exception&){threw=true;}
    check(threw,what);
}
std::vector<double> field(int n) {
    std::vector<double> p(static_cast<std::size_t>(n)*n*n);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const double X=x+0.5,Y=y+0.5,Z=z+0.5;
        p[(static_cast<std::size_t>(z)*n+y)*n+x]=
            100.0+3.0*X-2.0*Y+4.0*Z+
            0.25*std::pow(X-4.5,3)*(Y-4.5);
    }
    return p;
}
void run(int n,double dx,int sign) {
    const double dt=0.05;
    const Box old{{3.2*dx,3.1*dx,3.3*dx},
                  {5.7*dx,5.6*dx,5.8*dx}};
    Box next=old;
    for(int a=0;a<3;++a) {
        const double d=sign*(a==0?0.11:(a==1?-0.08:0.09))*dx;
        next.min_m[a]+=d;next.max_m[a]+=d;
    }
    const std::array<double,3> ref{{3.4*dx,4.9*dx,4.1*dx}};
    auto p=field(n);
    const auto copy=p;
    const auto r=Wrench::audit(n,n,n,dx,dt,old,next,p,ref);
    check(p==copy,"pressure input unchanged");
    check(std::abs(r.traction.solid_torque_nm[2])>1e-8,
          "nonzero intrinsic torque from nonlinear pressure");
    for(int a=0;a<3;++a) {
        const double c=0.25*(old.min_m[a]+old.max_m[a]+
                             next.min_m[a]+next.max_m[a]);
        const double arm=c-ref[a];
        const int b=(a+1)%3,k=(a+2)%3;
        (void)arm;(void)b;(void)k;
        near(r.body_linear_impulse_Ns[a],
             dt*r.traction.solid_force_n[a],1e-12,
             "linear pressure impulse");
        near(r.body_angular_impulse_Nms[a],
             dt*r.torque_about_reference_Nm[a],1e-12,
             "angular pressure impulse");
        near(r.body_linear_impulse_Ns[a]+
             r.fluid_linear_counterimpulse_Ns[a],0,1e-14,
             "unapplied linear reaction ledger");
        near(r.body_angular_impulse_Nms[a]+
             r.fluid_angular_counterimpulse_Nms[a],0,1e-14,
             "unapplied angular reaction ledger");
    }
    auto gauge=p;
    for(double& v:gauge)v+=500.0;
    const auto shifted=Wrench::audit(n,n,n,dx,dt,old,next,gauge,ref);
    for(int a=0;a<3;++a) {
        near(shifted.traction.solid_force_n[a],
             r.traction.solid_force_n[a],1e-10,"gauge invariant force");
        near(shifted.traction.solid_torque_nm[a],
             r.traction.solid_torque_nm[a],1e-10,"gauge invariant intrinsic torque");
        near(shifted.torque_about_reference_Nm[a],
             r.torque_about_reference_Nm[a],1e-10,"gauge invariant reference torque");
    }
    near(shifted.traction.solid_geometric_pressure_work_j,
         r.traction.solid_geometric_pressure_work_j,1e-10,
         "gauge invariant swept work");
    // No silent translation-only integration of a torque-bearing load.
    upc::RigidPressureState3D state;
    state.mass_kg=10.0;
    for(int a=0;a<3;++a)
        state.centre_m[a]=0.5*(old.min_m[a]+old.max_m[a]);
    rejects([&]{
        upc::P2MovingBoxDynamicPressure3D::step(
            n,n,n,dx,dt,old,state,p);
    },"dynamic axis-aligned solver must reject nonzero torque");
    auto bad=ref;
    bad[1]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Wrench::audit(n,n,n,dx,dt,old,next,p,bad);},
            "reject nonfinite reference");
    rejects([&]{Wrench::audit(n,n,n,dx,-dt,old,next,p,ref);},
            "reject invalid time step");
    std::cout<<std::setprecision(17)
             <<"CASE n="<<n<<" dx="<<dx<<" sign="<<sign
             <<" fx="<<r.traction.solid_force_n[0]
             <<" fy="<<r.traction.solid_force_n[1]
             <<" fz="<<r.traction.solid_force_n[2]
             <<" tx="<<r.traction.solid_torque_nm[0]
             <<" ty="<<r.traction.solid_torque_nm[1]
             <<" tz="<<r.traction.solid_torque_nm[2]
             <<" rx="<<r.torque_about_reference_Nm[0]
             <<" ry="<<r.torque_about_reference_Nm[1]
             <<" rz="<<r.torque_about_reference_Nm[2]
             <<" work="<<r.traction.solid_geometric_pressure_work_j
             <<" mismatch="<<r.traction.midpoint_work_difference_j<<"\n";
}
}
int main() {
    try {
        for(int n:{8,12})
            for(int sign:{-1,1})
                run(n,n==8?0.1:0.125,sign);
        std::cout<<"PASS: "<<checks
                 <<" native nonlinear 3D pressure-wrench assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
