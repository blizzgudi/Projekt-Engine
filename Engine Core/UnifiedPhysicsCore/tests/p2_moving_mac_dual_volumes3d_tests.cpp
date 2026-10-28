#include "upc/p2_moving_mac_dual_volumes3d.hpp"
#include "upc/p2_moving_box_open_volume3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Box=upc::P2Box3D;
using Builder=upc::P2MovingMacDualVolumes3DBuilder;
std::size_t checks=0;
void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
void near(double a,double b,double eps,const char* m) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=eps,m);
}
template<class F>void rejects(F fn,const char* m) {
    bool threw=false;try{fn();}catch(const std::exception&){threw=true;}
    check(threw,m);
}
Box shift(Box b,int axis,double d){
    b.min_m[axis]+=d;b.max_m[axis]+=d;return b;
}
// Independent direct physical-coordinate box intersection. Never calls
// the production voxel-overlap routine.
double analytic_open(const Box& b,int component,int x,int y,int z) {
    const std::array<int,3> ijk{{x,y,z}};
    double solid=1.0;
    for(int a=0;a<3;++a) {
        const double lo=ijk[a]+(a==component?0.5:0.0);
        const double hi=lo+1.0;
        solid*=std::max(0.0,std::min(hi,b.max_m[a])-
                                std::max(lo,b.min_m[a]));
    }
    return 1.0-solid;
}
std::size_t cell(int x,int y,int z,const std::array<std::size_t,3>& s) {
    return (static_cast<std::size_t>(z)*s[1]+y)*s[0]+x;
}
void run(int n,int move_axis,int sign,double fraction) {
    Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    for(int step=0;step<2;++step) {
        const auto next=shift(b,move_axis,sign*fraction);
        const auto dual=Builder::sample(n,n,n,1.0,0.25,b,next);
        near(dual.max_local_gcl_residual_m3,0.0,5e-10,"dual GCL");
        for(int component=0;component<3;++component) {
            const auto& s=dual.dimensions[component];
            const auto& w=dual.component[component];
            const auto& g=w.geometry.cell_geometry;
            check(s[component]==static_cast<std::size_t>(n-1),
                  "staggered shape");
            near(g.solid_volume_m3,3.375,1e-10,"dual solid volume");
            near(g.old_open_m3,g.new_open_m3,1e-10,"dual volume conservation");
            near(w.boundary_solid_flux_m3,0.0,5e-10,"no exterior solid flux");
            near(w.signed_open_volume_change_m3,0.0,5e-10,"global dual GCL");
            check(g.changed_open_cell_volumes_l1>0.0,"moving dual cut cells");
            for(int z=0;z<static_cast<int>(s[2]);++z)
            for(int y=0;y<static_cast<int>(s[1]);++y)
            for(int x=0;x<static_cast<int>(s[0]);++x) {
                const auto i=cell(x,y,z,s);
                near(g.alpha_old[i],analytic_open(b,component,x,y,z),
                     2e-12,"independent old dual intersection");
                near(g.alpha_new[i],analytic_open(next,component,x,y,z),
                     2e-12,"independent new dual intersection");
                check(g.alpha_old[i]>=0.0&&g.alpha_old[i]<=1.0&&
                      g.alpha_new[i]>=0.0&&g.alpha_new[i]<=1.0,
                      "dual open fraction bounds");
            }
            const auto i=cell(3,3,3,s);
            std::cout<<"DUAL "<<n<<" "<<move_axis<<" "<<sign<<" "
                     <<fraction<<" "<<step<<" "<<component<<" "
                     <<std::setprecision(17)<<g.alpha_old[i]<<" "
                     <<g.alpha_new[i]<<" "<<w.max_local_gcl_residual_m3
                     <<" "<<g.solid_volume_m3<<"\n";
        }
        // Exact dual occupancy differs from averaging primal cell fractions
        // for a partially immersed box. The dual geometry must not be
        // replaced with that incorrect shortcut.
        if(step==0&&move_axis==0&&sign==1) {
            const auto primal=upc::P2MovingBoxOpenVolumes3D::sample(
                n,n,n,1.0,0.25,b,next);
            const auto& u=dual.component[0].geometry.cell_geometry.alpha_old;
            const auto& s=dual.dimensions[0];
            const double naive=0.5*(primal.alpha_old[cell(3,3,3,
                {{static_cast<std::size_t>(n),static_cast<std::size_t>(n),
                  static_cast<std::size_t>(n)}})]+
                primal.alpha_old[cell(4,3,3,
                {{static_cast<std::size_t>(n),static_cast<std::size_t>(n),
                  static_cast<std::size_t>(n)}})]);
            check(std::abs(u[cell(3,3,3,s)]-naive)>0.05,
                  "dual volume is not primal alpha average");
        }
        b=next;
    }
}
void invalid() {
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    rejects([&]{Builder::sample(2,8,8,1.0,0.25,b,b);},"thin grid");
    rejects([&]{Builder::sample(8,8,8,0.0,0.25,b,b);},"zero dx");
    rejects([&]{Builder::sample(8,8,8,1.0,0.0,b,b);},"zero dt");
    rejects([&]{Builder::sample(8,8,8,1.0,0.25,b,shift(b,0,1.01));},
            "translation CFL");
    Box edge=b;edge.min_m[0]=0.2;
    rejects([&]{Builder::sample(8,8,8,1.0,0.25,edge,edge);},
            "shifted dual exterior clearance");
    Box deformed=b;deformed.max_m[0]+=0.1;
    rejects([&]{Builder::sample(8,8,8,1.0,0.25,b,deformed);},
            "deformed box");
    Box nan=b;nan.min_m[1]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Builder::sample(8,8,8,1.0,0.25,nan,nan);},
            "nonfinite box");
    const auto still=Builder::sample(8,8,8,1.0,0.25,b,b);
    for(int c=0;c<3;++c)
        near(still.component[c].geometry.cell_geometry.changed_open_cell_volumes_l1,
             0.0,0.0,"stationary dual geometry");
    check(b.min_m[0]==3.2&&b.max_m[0]==4.7,
          "caller geometry immutable");
}
}
int main() {
    try {
        for(int n:{8,12,16})for(int a=0;a<3;++a)
            for(int sign:{-1,1})for(double f:{0.25,0.5})
                run(n,a,sign,f);
        invalid();
        std::cout<<"PASS: "<<checks<<" native exact moving MAC dual geometry assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" checks\n";
        return 1;
    }
}
