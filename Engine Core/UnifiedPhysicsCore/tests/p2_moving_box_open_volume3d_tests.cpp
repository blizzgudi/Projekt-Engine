// P2 PARTIAL: analytic 3D box/voxel intersections, no fluid FSI claim.
#include "upc/p2_moving_box_open_volume3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
std::size_t checks=0;
void require(bool v,const char* message) {
    ++checks; if(!v)throw std::runtime_error(message);
}
void eq(double actual,double expected,double tol,const char* msg) {
    require(std::isfinite(actual)&&std::abs(actual-expected)<=tol,msg);
}
template<class F> void invalid(F&& fn,const char* msg) {
    bool thrown=false;
    try {fn();}catch(const std::invalid_argument&) {thrown=true;}
    require(thrown,msg);
}
using Box=upc::P2Box3D;
using Solver=upc::P2MovingBoxOpenVolumes3D;
std::size_t cell(int n,int x,int y,int z) {
    return (static_cast<std::size_t>(z)*n+y)*n+x;
}
Box translate(Box b,int axis,double delta) {
    b.min_m[axis]+=delta;b.max_m[axis]+=delta;return b;
}
void aligned(int n,int axis,int sign,double fraction) {
    const double dx=2.0,dt=0.25;
    Box b{{6.0,6.0,6.0},{10.0,8.0,8.0}};
    const auto moved=translate(b,axis,sign*fraction*dx);
    const auto old=b;
    const auto result=Solver::sample(n,n,n,dx,dt,b,moved);
    const double expected_l1=2.0*(axis==0?1.0:2.0)*fraction;
    eq(result.solid_volume_m3,16.0,1e-12,"Rigid box volume");
    eq(result.old_open_m3,(n*n*n-2.0)*8.0,1e-9,"Old open volume");
    eq(result.new_open_m3,(n*n*n-2.0)*8.0,1e-9,"New open volume");
    eq(result.solid_volume_residual_m3,0.0,1e-11,"Volume residual");
    eq(result.changed_open_cell_volumes_l1,expected_l1,1e-11,
       "Analytic swept open-volume L1");
    for(int a=0;a<3;++a)
        eq(result.translation_m[a],a==axis?sign*fraction*dx:0.0,
           1e-12,"Translation vector");
    const int x=3,y=3,z=3;
    eq(result.alpha_old[cell(n,x,y,z)],0.0,1e-12,
       "Initial full solid voxel");
    for(double alpha:result.alpha_new) {
        require(alpha>=0.0&&alpha<=1.0,"Valid open fraction");
    }
    require(b.min_m==old.min_m&&b.max_m==old.max_m,
            "Caller geometry unchanged");
    std::cout<<"aligned,"<<n<<","<<axis<<","<<sign<<","
             <<fraction<<","<<result.solid_volume_m3<<","
             <<result.changed_open_cell_volumes_l1<<","
             <<result.solid_volume_residual_m3<<"\n";
}
void subcell(int n) {
    const double dx=2.0;
    Box old{{4.5,5.0,5.5},{8.5,7.0,7.5}};
    Box moved=translate(old,0,0.5);
    const auto r=Solver::sample(n,n,n,dx,0.25,old,moved);
    // Voxel (2,2,2) overlaps (0.75,0.5,0.25) in old box.
    eq(r.alpha_old[cell(n,2,2,2)],1.0-0.75*0.5*0.25,
       1e-12,"Independent old subcell overlap");
    eq(r.alpha_new[cell(n,2,2,2)],1.0-0.5*0.5*0.25,
       1e-12,"Independent translated subcell overlap");
    eq(r.alpha_old[cell(n,3,3,3)],1.0-1.0*0.5*0.75,
       1e-12,"Independent interior partial voxel");
    eq(r.solid_volume_m3,16.0,1e-12,"Subcell box physical volume");
    eq(r.old_open_m3,r.new_open_m3,1e-10,"Subcell conserved volume");
    std::cout<<"subcell,"<<n<<","
             <<r.alpha_old[cell(n,2,2,2)]<<","
             <<r.alpha_new[cell(n,2,2,2)]<<","
             <<r.alpha_old[cell(n,3,3,3)]<<","
             <<r.solid_volume_residual_m3<<"\n";
}
void invalid_cases() {
    Box box{{6.0,6.0,6.0},{10.0,8.0,8.0}};
    auto run=[&](Box a,Box b,double dx=2.0,double dt=0.25) {
        return Solver::sample(8,8,8,dx,dt,a,b);
    };
    eq(run(box,box).changed_open_cell_volumes_l1,0.0,0.0,
       "Stationary body unchanged");
    invalid([&]{run(box,translate(box,0,2.1));},
            "Translation above one cell rejected");
    Box bad=box;bad.max_m[1]+=0.1;
    invalid([&]{run(box,bad);},"Deformation rejected");
    bad=box;bad.min_m[0]=bad.max_m[0];
    invalid([&]{run(bad,box);},"Degenerate box rejected");
    bad=box;bad.min_m[0]=-0.1;
    invalid([&]{run(bad,box);},"Negative boundary rejected");
    bad=box;bad.max_m[0]=17.0;
    invalid([&]{run(box,bad);},"Outside domain rejected");
    bad=box;bad.max_m[0]=std::numeric_limits<double>::quiet_NaN();
    invalid([&]{run(box,bad);},"Nonfinite box rejected");
    invalid([&]{run(box,box,0.0);},"Invalid dx rejected");
    invalid([&]{run(box,box,2.0,0.0);},"Invalid dt rejected");
    invalid([&]{Solver::sample(0,8,8,2,0.25,box,box);},
            "Zero grid rejected");
    invalid([&]{Solver::sample(2000001,1,1,2,0.25,box,box);},
            "Oversized grid rejected");
    require(box.min_m[0]==6.0&&box.max_m[0]==10.0,
            "Input unchanged after exceptions");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        for(int n:{8,12,16}) {
            for(int axis=0;axis<3;++axis)
                for(int sign:{-1,1})
                    for(double fraction:{0.25,0.5})
                        aligned(n,axis,sign,fraction);
            subcell(n);
        }
        invalid_cases();
        std::cout<<"PASS: moving box geometry assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<checks<<"\n";
        return 1;
    }
}
