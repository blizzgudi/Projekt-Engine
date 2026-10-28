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
using State=upc::RigidPressureState3D;
using Step=upc::P2MovingBoxDynamicPressure3D;
std::size_t assertions=0;
void check(bool yes,const char* msg) {
    ++assertions;
    if(!yes)throw std::runtime_error(msg);
}
void near(double actual,double expected,double tol,const char* msg) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,msg);
}
template<class F>void rejects(F f,const char* msg) {
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    check(threw,msg);
}
std::vector<double> gradient(int n,int axis,int sign) {
    std::vector<double> p(static_cast<std::size_t>(n)*n*n);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const int q[3]={x,y,z};
        p[(static_cast<std::size_t>(z)*n+y)*n+x]=
            100.0+10.0*sign*(q[axis]+0.5);
    }
    return p;
}
void analytic(int n,int axis,int sign) {
    const Box initial{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    const auto p=gradient(n,axis,sign);
    const auto copy=p;
    State before;
    before.mass_kg=10.0;
    before.centre_m={{4,4,4}};
    const double force=-25.6*sign,dt=0.1;
    Box box=initial;
    double total_work=0.0,total_counter=0.0;
    for(int k=1;k<=2;++k) {
        const auto r=Step::step(n,n,n,1.0,dt,box,before,p);
        near(r.frozen_pressure.solid_force_n[axis],force,1e-10,
             "independent planar gradient force");
        near(r.swept_pressure.solid_force_n[axis],force,1e-10,
             "midpoint traction matches frozen force");
        near(r.rigid.after.velocity_m_s[axis],
             k*force*dt/10.0,1e-12,"analytic rigid velocity");
        near(r.rigid.after.centre_m[axis],
             4.0+0.5*force/10.0*(k*dt)*(k*dt),1e-12,
             "analytic rigid centre");
        near(r.box_after.min_m[axis],
             3.2+0.5*force/10.0*(k*dt)*(k*dt),1e-12,
             "translated collision box");
        near(r.rigid.body_linear_impulse_Ns[axis],force*dt,1e-12,
             "body impulse applied");
        near(r.rigid.fluid_reaction_impulse_Ns[axis],-force*dt,1e-12,
             "equal opposite UNAPPLIED fluid ledger");
        near(r.rigid.energy_residual_J,0.0,1e-12,
             "rigid constant-force work energy identity");
        near(r.rigid_vs_geometric_pressure_work_J,0.0,1e-10,
             "aligned swept pressure work matches rigid work");
        for(int a=0;a<3;++a) {
            near(r.diagnostic_total_impulse_residual_Ns[a],0.0,1e-12,
                 "bookkeeping impulse identity");
            near(r.force_sampling_change_N[a],0.0,1e-10,
                 "frozen/midpoint force consistency");
            near(r.frozen_pressure.solid_torque_nm[a],0.0,1e-10,
                 "zero torque for planar gradient");
            if(a!=axis) {
                near(r.rigid.after.velocity_m_s[a],0.0,1e-12,
                     "no transverse velocity");
                near(r.rigid.after.centre_m[a],4.0,1e-12,
                     "no transverse displacement");
            }
        }
        total_work+=r.rigid.frozen_pressure_work_J;
        total_counter+=r.rigid.fluid_reaction_impulse_Ns[axis];
        box=r.box_after;
        before=r.rigid.after;
    }
    near(total_work,0.5*10.0*std::pow(2*force*dt/10.0,2),1e-12,
         "independent two-step kinetic energy");
    near(total_counter,-2*force*dt,1e-12,
         "two-step counter impulse obligation");
    check(p==copy,"pressure input remains unchanged");
    std::cout<<"DYNAMIC "<<n<<" "<<axis<<" "<<sign<<" "
             <<std::setprecision(17)<<before.velocity_m_s[axis]<<" "
             <<before.centre_m[axis]<<" "<<total_work<<" "
             <<total_counter<<"\n";
}
void gauge_and_ballistic() {
    const Box box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    State s;
    s.mass_kg=10.0;s.centre_m={{4,4,4}};
    auto p=gradient(8,0,1);
    const auto r=Step::step(8,8,8,1.0,0.1,box,s,p);
    for(auto& v:p)v+=123.0;
    const auto g=Step::step(8,8,8,1.0,0.1,box,s,p);
    for(int a=0;a<3;++a) {
        near(g.rigid.after.velocity_m_s[a],r.rigid.after.velocity_m_s[a],
             1e-11,"gauge-invariant rigid velocity");
        near(g.rigid.after.centre_m[a],r.rigid.after.centre_m[a],
             1e-11,"gauge-invariant displacement");
    }
    near(g.swept_pressure.solid_geometric_pressure_work_j,
         r.swept_pressure.solid_geometric_pressure_work_j,1e-10,
         "gauge-invariant swept work");
    for(auto& v:p)v=100.0;
    s.velocity_m_s={{0.1,-0.2,0.3}};
    const auto b=Step::step(8,8,8,1.0,0.1,box,s,p);
    for(int a=0;a<3;++a) {
        near(b.rigid.after.velocity_m_s[a],s.velocity_m_s[a],1e-12,
             "ballistic constant velocity");
        near(b.rigid.after.centre_m[a],4.0+0.1*s.velocity_m_s[a],1e-12,
             "ballistic centre");
        near(b.rigid.body_linear_impulse_Ns[a],0.0,1e-10,
             "zero force under constant pressure");
    }
    near(b.rigid.kinetic_energy_change_J,0.0,1e-11,
         "ballistic energy unchanged");
}
void negative_cases() {
    const Box box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    State s;
    s.mass_kg=10.0;s.centre_m={{4,4,4}};
    auto p=gradient(8,0,1);
    const auto original=p;
    auto off=s;off.centre_m[0]=4.1;
    rejects([&]{Step::step(8,8,8,1.0,0.1,box,off,p);},
            "reject off-centre body");
    off=s;off.angular_velocity_rad_s[1]=0.1;
    rejects([&]{Step::step(8,8,8,1.0,0.1,box,off,p);},
            "reject rotating body without orientation");
    off=s;off.mass_kg=0.0;
    rejects([&]{Step::step(8,8,8,1.0,0.1,box,off,p);},
            "reject zero mass");
    rejects([&]{Step::step(8,8,8,1.0,0.0,box,s,p);},
            "reject zero time");
    rejects([&]{Step::step(8,8,8,1.0,0.1,box,s,{});},
            "reject pressure shape");
    p[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Step::step(8,8,8,1.0,0.1,box,s,p);},
            "reject NaN pressure");
    p=original;
    off=s;off.velocity_m_s[0]=30.0;
    rejects([&]{Step::step(8,8,8,1.0,0.1,box,off,p);},
            "reject geometry CFL");
    off=s;off.velocity_m_s[0]=100.0;
    rejects([&]{Step::step(8,8,8,1.0,0.1,box,off,p);},
            "reject box leaving grid");
    for(auto& v:p)v=0.0;
    const Box torque_box{{3.2,2.2,2.2},{4.8,5.8,5.8}};
    p[(3*8+3)*8+3]=10.0;
    rejects([&]{Step::step(8,8,8,1.0,0.1,torque_box,s,p);},
            "reject torque without orientation");
    check(original==gradient(8,0,1),"independent reference not mutated");
}
}
int main() {
    try {
        for(int n:{8,12})for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})analytic(n,axis,sign);
        gauge_and_ballistic();
        negative_cases();
        std::cout<<"PASS: "<<assertions
                 <<" native dynamic pressure box assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
