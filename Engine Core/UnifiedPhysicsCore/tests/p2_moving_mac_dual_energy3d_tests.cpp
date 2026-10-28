#include "upc/p2_moving_mac_dual_energy3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Box=upc::P2Box3D;
using Bridge=upc::P2MovingMacSharedPredictor3D;
using Energy=upc::P2MovingMacDualEnergy3D;
using Builder=upc::P2MovingMacDualVolumes3DBuilder;
std::size_t assertions=0;
void check(bool b,const char* msg) {
    ++assertions;
    if(!b)throw std::runtime_error(msg);
}
void near(double x,double y,double tol,const char* msg) {
    check(std::isfinite(x)&&std::isfinite(y)&&std::abs(x-y)<=tol,msg);
}
template<class F> void rejects(F f,const char* msg) {
    bool caught=false;
    try{f();}catch(const std::exception&){caught=true;}
    check(caught,msg);
}
Bridge::Faces primal(int n,double amplitude) {
    Bridge::Faces result;
    constexpr double pi=3.14159265358979323846;
    for(int a=0;a<3;++a) {
        std::array<int,3> s{{n,n,n}};
        ++s[a];
        result[a].assign(s[0]*s[1]*s[2],0.0);
        for(int z=0;z<s[2];++z)for(int y=0;y<s[1];++y)
        for(int x=0;x<s[0];++x) {
            const std::array<int,3> p{{x,y,z}};
            if(p[a]==0||p[a]==n)continue;
            const auto i=(z*s[1]+y)*s[0]+x;
            result[a][i]=amplitude*std::sin(pi*p[a]/n)*
                (0.4*std::sin(0.4*x+0.2*y-0.1*z+a)+
                 0.6*std::cos(0.2*x-0.3*y+0.3*z+a));
        }
    }
    return result;
}
Bridge::Faces initial(int n,const Box& b,bool uniform) {
    const auto dual=Builder::sample(n,n,n,1.0,0.25,b,b);
    Bridge::Faces result;
    for(int a=0;a<3;++a) {
        const auto& s=dual.dimensions[a];
        const auto& open=dual.component[a].geometry.cell_geometry.alpha_old;
        result[a].assign(open.size(),0.0);
        for(std::size_t z=0;z<s[2];++z)
        for(std::size_t y=0;y<s[1];++y)
        for(std::size_t x=0;x<s[0];++x) {
            const auto i=(z*s[1]+y)*s[0]+x;
            if(open[i]>0.0)
                result[a][i]=-0.2*(a+1)+
                    (uniform?0.0:0.045*std::sin(0.2*x+0.4*y-0.3*z+a));
        }
    }
    return result;
}
void run(int n,int axis,int sign,int mode) {
    Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    auto values=initial(n,b,mode==0);
    const auto velocity=primal(n,mode==0?0.0:0.04);
    const auto original_velocity=velocity;
    for(int step=0;step<2;++step) {
        Box next=b;
        next.min_m[axis]+=0.15*sign;
        next.max_m[axis]+=0.15*sign;
        const auto original=values;
        const auto r=Energy::step(n,n,n,1.0,0.25,1000.0,
                                  b,next,velocity,values);
        check(values==original&&velocity==original_velocity,
              "energy audit does not mutate inputs");
        const auto dual=Builder::sample(n,n,n,1.0,0.25,b,next);
        double total_before=0.0,total_after=0.0;
        for(int a=0;a<3;++a) {
            const auto& g=dual.component[a].geometry.cell_geometry;
            double before=0.0,after=0.0,p0=0.0,p1=0.0;
            for(std::size_t i=0;i<values[a].size();++i) {
                const double m0=1000.0*g.alpha_old[i];
                const double m1=1000.0*g.alpha_new[i];
                before+=0.5*m0*values[a][i]*values[a][i];
                after+=0.5*m1*r.transport.component_new[a][i]*
                                    r.transport.component_new[a][i];
                p0+=m0*values[a][i];
                p1+=m1*r.transport.component_new[a][i];
                ++assertions;
            }
            near(r.energy_before_j[a],before,1e-8,
                 "independent old dual kinetic energy");
            near(r.energy_after_j[a],after,1e-8,
                 "independent new dual kinetic energy");
            near(r.upwind_loss_j[a],before-after,1e-8,
                 "independent upwind loss");
            near(r.momentum_before_kg_m_s[a],p0,1e-8,
                 "independent old signed momentum");
            near(r.momentum_after_kg_m_s[a],p1,1e-8,
                 "independent new signed momentum");
            near(p0,p1,1e-7,"signed dual momentum conservation");
            check(after<=before+1e-7,"nonincreasing dual L2 energy");
            near(r.transport.integral_residual[a],0.0,1e-8,
                 "transport signed conservation");
            if(mode==0)near(after,before,1e-7,
                            "uniform state preserves dual energy");
            total_before+=before;total_after+=after;
        }
        near(total_before,r.total_energy_before_j,1e-8,
             "independent total old energy");
        near(total_after,r.total_energy_after_j,1e-8,
             "independent total new energy");
        near(total_before-total_after,r.total_upwind_loss_j,1e-8,
             "independent total dissipation");
        check(r.transport.max_gcl_residual<1e-9,"dual GCL");
        check(r.transport.max_donor_courant<=1.0+1e-10,"dual CFL");
        std::cout<<"DUAL_ENERGY "<<n<<" "<<axis<<" "<<sign<<" "
                 <<mode<<" "<<step<<" "<<std::setprecision(17)
                 <<r.total_energy_before_j<<" "<<r.total_energy_after_j
                 <<" "<<r.total_upwind_loss_j<<"\n";
        values=r.transport.component_new;
        b=next;
    }
}
void stationary_and_invalid() {
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    const auto values=initial(8,b,false);
    const auto zeros=primal(8,0.0);
    const auto result=Energy::step(8,8,8,1.0,0.25,1000.0,
                                   b,b,zeros,values);
    near(result.total_upwind_loss_j,0.0,1e-10,
         "stationary zero-flux exact energy");
    for(int a=0;a<3;++a)
        for(std::size_t i=0;i<values[a].size();++i)
            near(result.transport.component_new[a][i],values[a][i],1e-10,
                 "stationary zero-flux within projection tolerance");
    rejects([&]{Energy::step(8,8,8,1.0,0.25,0.0,b,b,zeros,values);},
            "invalid density");
    auto broken=values;broken[0].pop_back();
    rejects([&]{Energy::step(8,8,8,1.0,0.25,1000.0,b,b,zeros,broken);},
            "invalid dual shape");
    broken=values;
    broken[0][0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Energy::step(8,8,8,1.0,0.25,1000.0,b,b,zeros,broken);},
            "nonfinite dual state");
    auto bad=primal(8,0.0);bad[0][0]=1.0;
    rejects([&]{Energy::step(8,8,8,1.0,0.25,1000.0,b,b,bad,values);},
            "exterior predictor");
}
}
int main() {
    try {
        stationary_and_invalid();
        for(int n:{8,12})for(int axis=0;axis<3;++axis)
        for(int sign:{-1,1})for(int mode:{0,1})
            run(n,axis,sign,mode);
        std::cout<<"PASS: "<<assertions
                 <<" native moving dual energy assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
