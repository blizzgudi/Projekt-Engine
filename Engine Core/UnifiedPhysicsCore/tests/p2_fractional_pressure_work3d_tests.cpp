#include "upc/p2_moving_box_pressure_traction3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Box=upc::P2Box3D;
using Probe=upc::P2MovingBoxPressureTraction3D;
std::size_t assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double actual,double expected,double tolerance,const char* msg) {
    check(std::isfinite(actual) && std::abs(actual-expected)<=tolerance,msg);
}
template<class F>void rejects(F f,const char* msg) {
    bool thrown=false;
    try {f();} catch(const std::exception&) {thrown=true;}
    check(thrown,msg);
}
void scenario(int n,double dx,int sign,int motion) {
    const std::array<double,3> gradient{{12.0,-7.0,5.0}};
    Box old{{3.2*dx,3.1*dx,3.3*dx},
            {5.7*dx,5.6*dx,5.8*dx}};
    const std::array<double,3> fractional{{0.11,-0.08,0.09}};
    Box next=old;
    std::array<double,3> delta{};
    for(int a=0;a<3;++a) {
        delta[a]=(motion==3||motion==a ? 1.0 : 0.0)*sign*fractional[a]*dx;
        next.min_m[a]+=delta[a];
        next.max_m[a]+=delta[a];
    }
    std::vector<double> pressure(static_cast<std::size_t>(n)*n*n);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        pressure[(static_cast<std::size_t>(z)*n+y)*n+x]=
            100.0+gradient[0]*(x+0.5)+gradient[1]*(y+0.5)
                 +gradient[2]*(z+0.5);
    const auto original=pressure;
    const auto result=Probe::audit(n,n,n,dx,0.05,old,next,pressure);
    check(pressure==original,"input pressure mutated");
    double analytic_work=0.0;
    for(int a=0;a<3;++a) {
        // Both faces stay strictly within their initial grid cells.
        // Their cell-centered pressure difference is exactly 2*g[a].
        const double analytic_force=-2.0*gradient[a]*6.25*dx*dx;
        near(result.solid_force_n[a],analytic_force,2e-10,
             "independent fractional pressure force");
        near(result.fluid_counter_impulse_kg_m_s[a],
             -0.05*analytic_force,2e-11,
             "signed pressure counter-impulse diagnostic");
        analytic_work+=analytic_force*delta[a];
        check(std::isfinite(result.solid_torque_nm[a]),"finite torque");
    }
    near(result.integrated_surface_area_m2,37.5*dx*dx,2e-11,
         "six fractional box faces");
    near(result.solid_geometric_pressure_work_j,analytic_work,2e-10,
         "independent finite-volume swept pressure work");
    near(result.midpoint_traction_work_j,analytic_work,2e-10,
         "midpoint surface traction work");
    near(result.midpoint_work_difference_j,0.0,2e-10,
         "fractional work identity");
    near(result.fluid_geometric_pressure_work_j,-analytic_work,2e-10,
         "equal opposite geometric work diagnostic");

    for(double& p:pressure)p+=500.0;
    const auto shifted=Probe::audit(n,n,n,dx,0.05,old,next,pressure);
    for(int a=0;a<3;++a) {
        near(shifted.solid_force_n[a],result.solid_force_n[a],2e-10,
             "pressure gauge invariant force");
        near(shifted.solid_torque_nm[a],result.solid_torque_nm[a],2e-10,
             "pressure gauge invariant torque");
    }
    near(shifted.solid_geometric_pressure_work_j,
         result.solid_geometric_pressure_work_j,2e-10,
         "pressure gauge invariant swept work");
    if(motion==3) {
        std::cout<<std::setprecision(17)
                 <<"CASE n="<<n<<" dx="<<dx<<" sign="<<sign
                 <<" fx="<<result.solid_force_n[0]
                 <<" fy="<<result.solid_force_n[1]
                 <<" fz="<<result.solid_force_n[2]
                 <<" work="<<result.solid_geometric_pressure_work_j
                 <<" residual="<<result.midpoint_work_difference_j<<"\n";
    }
}
}
int main() {
    try {
        for(int n:{8,12})
            for(int sign:{-1,1})
                for(int motion=0;motion<4;++motion)
                    scenario(n,n==8?0.1:0.125,sign,motion);
        const Box old{{0.32,0.31,0.33},{0.57,0.56,0.58}};
        std::vector<double> p(8*8*8,100.0);
        rejects([&]{Probe::audit(8,8,8,0.1,0.05,old,old,{});},
                "reject invalid pressure shape");
        p[1]=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{Probe::audit(8,8,8,0.1,0.05,old,old,p);},
                "reject NaN pressure");
        p[1]=100.0;
        auto invalid=old;
        invalid.min_m[0]+=0.12;
        invalid.max_m[0]+=0.12;
        rejects([&]{Probe::audit(8,8,8,0.1,0.05,old,invalid,p);},
                "reject excessive swept-cell displacement");
        std::cout<<"PASS: "<<assertions
                 <<" native fractional 3D pressure-work assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
