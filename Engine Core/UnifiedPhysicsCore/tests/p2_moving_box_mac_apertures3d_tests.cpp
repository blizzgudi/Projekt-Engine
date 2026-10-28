// P2 PARTIAL: exact time-integrated 3D cut-cell MAC face apertures.
// No prescribed velocity, pressure, impulse or FSI closure is claimed.
#include "upc/p2_moving_box_mac_apertures3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Box=upc::P2Box3D;
using Result=upc::P2MovingBoxMacApertures3D;
using Solver=upc::P2MovingBoxFaceApertures3D;
std::size_t assertions=0;
void check(bool b,const char* msg){
    ++assertions;
    if(!b)throw std::runtime_error(msg);
}
void eq(double a,double b,double tol,const char* msg){
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
template<class F>void reject(F fn,const char* msg){
    bool caught=false;
    try{fn();}catch(const std::invalid_argument&){caught=true;}
    check(caught,msg);
}
Box moved(Box b,int axis,double amount){
    b.min_m[axis]+=amount;b.max_m[axis]+=amount;return b;
}
Result run(int n,const Box& a,const Box& b){
    const auto r=Solver::sample(n,n,n,1.0,0.25,a,b);
    check(r.u_open.size()==static_cast<std::size_t>((n+1)*n*n),
          "U face count");
    check(r.v_open.size()==static_cast<std::size_t>(n*(n+1)*n),
          "V face count");
    check(r.w_open.size()==static_cast<std::size_t>(n*n*(n+1)),
          "W face count");
    for(const auto* f:{&r.u_open,&r.v_open,&r.w_open})
        for(double value:*f)
            check(std::isfinite(value)&&value>=0.0&&value<=1.0,
                  "All apertures physically bounded");
    eq(r.cell_geometry.old_open_m3,r.cell_geometry.new_open_m3,1e-10,
       "Rigid geometry volume conservation");
    return r;
}
double face(const Result& r,int n,int axis,int plane){
    const int x=axis==0?plane:2;
    const int y=axis==1?plane:2;
    const int z=axis==2?plane:2;
    if(axis==0)return r.u_open[(z*n+y)*(n+1)+x];
    if(axis==1)return r.v_open[(z*(n+1)+y)*n+x];
    return r.w_open[(z*n+y)*n+x];
}
void reverse_equal(const Result& a,const Result& b){
    for(const auto pair:std::array<std::array<const std::vector<double>*,2>,3>{{
        {{&a.u_open,&b.u_open}},{{&a.v_open,&b.v_open}},
        {{&a.w_open,&b.w_open}}}})
        for(std::size_t k=0;k<pair[0]->size();++k)
            eq((*pair[0])[k],(*pair[1])[k],2e-14,
               "Time reversal leaves average aperture unchanged");
}
Box baseline(int axis){
    Box b{{2.0,2.0,2.0},{3.0,3.0,3.0}};
    b.min_m[axis]=2.25;b.max_m[axis]=3.25;
    return b;
}
void normal(int n,int axis,int sign){
    Box b=baseline(axis);
    if(sign<0){
        b.min_m[axis]=3.75;b.max_m[axis]=4.75;
    }
    const Box after=moved(b,axis,static_cast<double>(sign));
    const auto r=run(n,b,after);
    // Exact normal-axis indicator integral: blocked for 3/4 or
    // 1/4 of the step at these two fixed MAC planes.
    const int near=sign>0?3:4,far=sign>0?4:3;
    const double near_open=face(r,n,axis,near);
    const double far_open=face(r,n,axis,far);
    eq(near_open,0.25,2e-13,"Normal swept face 3/4 blocked");
    eq(far_open,0.75,2e-13,"Normal swept face 1/4 blocked");
    eq(face(r,n,axis,0),1.0,0,"Domain face fully open");
    eq(face(r,n,axis,n),1.0,0,"Opposite domain face open");
    eq(face(r,n,axis,6),1.0,0,"Distant face open");
    reverse_equal(r,run(n,after,b));
    std::cout<<"normal,"<<n<<","<<axis<<","<<sign<<","
             <<near_open<<","<<far_open<<"\n";
}
void tangent(int n,int axis,int sign){
    const int t=(axis+1)%3;
    Box b=baseline(axis);
    b.min_m[t]=sign>0?2.25:2.75;
    b.max_m[t]=sign>0?3.25:3.75;
    const Box after=moved(b,t,sign*0.5);
    const auto r=run(n,b,after);
    // Tangential overlap varies linearly from 0.75 to 0.25.
    const double value=face(r,n,axis,3);
    eq(value,0.5,2e-13,"Linear tangential face aperture");
    reverse_equal(r,run(n,after,b));
    std::cout<<"tangent,"<<n<<","<<axis<<","<<sign<<","
             <<value<<"\n";
}
void diagonal(int n,int axis){
    Box b=baseline(axis);
    for(int t=0;t<3;++t)if(t!=axis){
        b.min_m[t]=2.25;b.max_m[t]=3.25;
    }
    Box after=b;
    for(int t=0;t<3;++t)if(t!=axis)
        after=moved(after,t,0.5);
    const auto r=run(n,b,after);
    // 1 - integral_0^1 (0.75-0.5t)^2 dt = 35/48.
    const double value=face(r,n,axis,3);
    eq(value,35.0/48.0,2e-13,"Quadratic diagonal aperture");
    reverse_equal(r,run(n,after,b));
    std::cout<<"diagonal,"<<n<<","<<axis<<","<<value<<"\n";
}
void mixed(int n,int axis){
    const int t=(axis+1)%3;
    Box b=baseline(axis);
    b.min_m[t]=2.25;b.max_m[t]=3.25;
    Box after=moved(moved(b,axis,1.0),t,0.5);
    const auto r=run(n,b,after);
    // Normal face blocked only until t=0.75; tangential overlap
    // 0.75-0.5t. Blocked integral = 27/64, open = 37/64.
    const double value=face(r,n,axis,3);
    eq(value,37.0/64.0,2e-13,"Piecewise normal/tangent aperture");
    reverse_equal(r,run(n,after,b));
    std::cout<<"mixed,"<<n<<","<<axis<<","<<value<<"\n";
}
void stationary(int n,int axis){
    Box b=baseline(axis);
    for(int t=0;t<3;++t)if(t!=axis){
        b.min_m[t]=2.25;b.max_m[t]=3.25;
    }
    const auto r=run(n,b,b);
    const double value=face(r,n,axis,3);
    eq(value,7.0/16.0,2e-13,"Stationary subcell aperture");
    eq(r.cell_geometry.changed_open_cell_volumes_l1,0.0,0.0,
       "Stationary cell geometry unchanged");
    std::cout<<"static,"<<n<<","<<axis<<","<<value<<"\n";
}
void aligned(int n,int axis){
    Box b{{2.0,2.0,2.0},{3.0,3.0,3.0}};
    const auto r=run(n,b,b);
    const double left=face(r,n,axis,2);
    const double right=face(r,n,axis,3);
    eq(left,0.0,0.0,"Face coincident with solid minimum closed");
    eq(right,0.0,0.0,"Face coincident with solid maximum closed");
    const int t=(axis+1)%3;
    b.min_m[t]=2.25;b.max_m[t]=3.25;
    const auto after=moved(b,t,0.5);
    const auto moving=run(n,b,after);
    const double sliding=face(moving,n,axis,2);
    eq(sliding,0.5,2e-13,"Tangential sliding on aligned wall");
    std::cout<<"aligned,"<<n<<","<<axis<<","
             <<left<<","<<right<<","<<sliding<<"\n";
}
void invalid(){
    Box b{{2.25,2.0,2.0},{3.25,3.0,3.0}};
    const auto original=b;
    reject([&]{Solver::sample(0,8,8,1,0.25,b,b);},"Zero grid");
    reject([&]{Solver::sample(8,8,8,0,0.25,b,b);},"Zero cell edge");
    reject([&]{Solver::sample(8,8,8,1,0,b,b);},"Zero timestep");
    reject([&]{Solver::sample(2000001,1,1,1,0.25,b,b);},
           "Oversized grid");
    reject([&]{Solver::sample(8,8,8,1,0.25,b,moved(b,0,1.01));},
           "Translation above cell CFL");
    Box bad=b;bad.max_m[0]+=0.1;
    reject([&]{Solver::sample(8,8,8,1,0.25,b,bad);},
           "Deformation");
    bad=b;bad.min_m[1]=-0.1;
    reject([&]{Solver::sample(8,8,8,1,0.25,bad,b);},
           "Outside domain");
    bad=b;bad.max_m[2]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{Solver::sample(8,8,8,1,0.25,b,bad);},
           "Nonfinite geometry");
    check(b.min_m==original.min_m&&b.max_m==original.max_m,
          "Inputs unchanged after failure");
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        for(int n:{8,12,16})for(int axis=0;axis<3;++axis){
            for(int sign:{-1,1}){
                normal(n,axis,sign);
                tangent(n,axis,sign);
            }
            diagonal(n,axis);
            mixed(n,axis);
            stationary(n,axis);
            aligned(n,axis);
        }
        invalid();
        std::cout<<"PASS: moving MAC apertures assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
