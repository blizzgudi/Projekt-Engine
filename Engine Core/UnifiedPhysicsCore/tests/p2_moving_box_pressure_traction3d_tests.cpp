#include "upc/p2_moving_box_pressure_traction3d.hpp"
#include "upc/p2_moving_box_cut_cell_projection3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Box=upc::P2Box3D;
using Audit=upc::P2MovingBoxPressureTraction3D;
std::size_t assertions=0;
void check(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
void near(double actual,double expected,double tol,const char* why) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,why);
}
template<class F> void rejects(F fn,const char* why) {
    bool caught=false;
    try{fn();}catch(const std::exception&){caught=true;}
    check(caught,why);
}
std::size_t idx(int x,int y,int z,int n) {
    return (static_cast<std::size_t>(z)*n+y)*n+x;
}
void analytic(int n,int axis,int direction) {
    Box old{{3,3,3},{5,5,5}};
    Box next=old;
    next.min_m[axis]+=direction*0.2;
    next.max_m[axis]+=direction*0.2;
    std::vector<double> p(static_cast<std::size_t>(n)*n*n,0.0);
    const int plane=direction>0?3:2;
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const int pos[3]={x,y,z};
        if(pos[axis]==plane)p[idx(x,y,z,n)]=10.0;
    }
    const auto original=p;
    const auto r=Audit::audit(n,n,n,1.0,0.25,old,next,p);
    check(p==original,"no input mutation");
    for(int a=0;a<3;++a) {
        near(r.solid_force_n[a],a==axis?40.0:0.0,1e-11,
             "independent planar traction force");
        near(r.solid_torque_nm[a],0.0,1e-11,"symmetric torque");
        near(r.fluid_counter_impulse_kg_m_s[a],
             a==axis?-10.0:0.0,1e-11,"opposite impulse");
    }
    near(r.integrated_surface_area_m2,24.0,1e-11,"box area");
    near(r.solid_geometric_pressure_work_j,8.0*direction,1e-11,
         "independent swept cell work");
    near(r.fluid_geometric_pressure_work_j,-8.0*direction,1e-11,
         "equal opposite geometric work");
    near(r.midpoint_traction_work_j,8.0*direction,1e-11,
         "midpoint traction work");
    near(r.midpoint_work_difference_j,0.0,1e-11,
         "aligned step field work agreement");
    for(auto& v:p)v+=17.0;
    const auto gauge=Audit::audit(n,n,n,1.0,0.25,old,next,p);
    for(int a=0;a<3;++a) {
        near(gauge.solid_force_n[a],r.solid_force_n[a],1e-10,
             "pressure gauge invariant force");
        near(gauge.solid_torque_nm[a],r.solid_torque_nm[a],1e-10,
             "pressure gauge invariant torque");
    }
    near(gauge.solid_geometric_pressure_work_j,
         r.solid_geometric_pressure_work_j,1e-10,
         "pressure gauge invariant swept work");
    std::cout<<"TRACTION "<<n<<" "<<axis<<" "<<direction<<" "
             <<std::setprecision(17)<<r.solid_force_n[axis]<<" "
             <<r.solid_geometric_pressure_work_j<<" "
             <<r.midpoint_work_difference_j<<"\n";
}
void asymmetric() {
    Box old{{3,3,3},{5,5,5}},next=old;
    next.min_m[0]+=0.2;next.max_m[0]+=0.2;
    std::vector<double> p(8*8*8,0.0);
    p[idx(3,3,3,8)]=10.0;
    const auto r=Audit::audit(8,8,8,1.0,0.25,old,next,p);
    near(r.solid_force_n[0],10.0,1e-12,"patch x force");
    near(r.solid_force_n[1],0.0,1e-12,"patch y force");
    near(r.solid_force_n[2],0.0,1e-12,"patch z force");
    near(r.solid_torque_nm[0],0.0,1e-12,"patch torque x");
    near(r.solid_torque_nm[1],-5.0,1e-12,"patch torque y");
    near(r.solid_torque_nm[2],5.0,1e-12,"patch torque z");
    near(r.solid_geometric_pressure_work_j,2.0,1e-12,"patch swept work");
}
void projected() {
    const Box old{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    auto next=old;
    next.min_m[0]+=0.15;next.max_m[0]+=0.15;
    const auto q=upc::P2MovingBoxCutCellProjection3DBuilder::sample(
        8,8,8,1.0,0.25,1000.0,old,next);
    const auto r=Audit::audit(8,8,8,1.0,0.25,old,next,q.pressure_pa);
    check(std::isfinite(r.solid_force_n[0]),"projected pressure force finite");
    near(r.solid_geometric_pressure_work_j,
         -r.fluid_geometric_pressure_work_j,1e-8,
         "projected fluid/solid geometric pressure work");
    std::vector<double> shifted=q.pressure_pa;
    for(auto& v:shifted)v+=23.0;
    const auto g=Audit::audit(8,8,8,1.0,0.25,old,next,shifted);
    for(int a=0;a<3;++a)
        near(g.solid_force_n[a],r.solid_force_n[a],1e-8,
             "projected gauge force");
    near(g.solid_geometric_pressure_work_j,
         r.solid_geometric_pressure_work_j,1e-8,
         "projected gauge work");
    std::cout<<"PROJECTED "<<std::setprecision(17)
             <<r.solid_force_n[0]<<" "<<r.solid_geometric_pressure_work_j
             <<" "<<r.midpoint_work_difference_j<<"\n";
}
}
int main() {
    try {
        for(int n:{8,12})
            for(int axis=0;axis<3;++axis)
                for(int sign:{-1,1})analytic(n,axis,sign);
        asymmetric();
        projected();
        const Box b{{3,3,3},{5,5,5}};
        std::vector<double> p(8*8*8,0.0);
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,b,b,{});},
                "bad pressure shape");
        p[0]=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,b,b,p);},
                "nan pressure");
        p[0]=0.0;
        rejects([&]{Audit::audit(8,8,8,1.0,0.0,b,b,p);},
                "zero timestep");
        auto moved=b;
        moved.min_m[0]+=1.2;moved.max_m[0]+=1.2;
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,b,moved,p);},
                "geometry CFL");
        for(auto& v:p)v=17.0;
        const auto uniform=Audit::audit(8,8,8,1.0,0.25,b,b,p);
        for(int a=0;a<3;++a) {
            near(uniform.solid_force_n[a],0.0,1e-11,"uniform pressure force");
            near(uniform.solid_torque_nm[a],0.0,1e-11,"uniform pressure torque");
        }
        near(uniform.solid_geometric_pressure_work_j,0.0,1e-11,
             "stationary geometric pressure work");
        std::cout<<"PASS: "<<assertions
                 <<" native moving box pressure traction assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
