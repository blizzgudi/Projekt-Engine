#include "upc/p2_moving_mac_dual_transport3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Box=upc::P2Box3D;
using Builder=upc::P2MovingMacDualVolumes3DBuilder;
using Transport=upc::P2MovingMacDualTransport3D;
std::size_t checks=0;
void check(bool b,const char* msg){++checks;if(!b)throw std::runtime_error(msg);}
void near(double a,double b,double tol,const char* msg){
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
template<class F> void rejects(F fn,const char* msg){
    bool threw=false;try{fn();}catch(const std::exception&){threw=true;}
    check(threw,msg);
}
Box shift(Box b,int a,double d){
    b.min_m[a]+=d;b.max_m[a]+=d;return b;
}
std::array<std::vector<double>,3> values(
    const upc::P2MovingMacDualVolumes3D& dual,bool uniform) {
    std::array<std::vector<double>,3> out;
    for(int c=0;c<3;++c){
        const auto& s=dual.dimensions[c];
        const auto& alpha=dual.component[c].geometry.cell_geometry.alpha_old;
        for(std::size_t z=0;z<s[2];++z)
        for(std::size_t y=0;y<s[1];++y)
        for(std::size_t x=0;x<s[0];++x){
            const auto i=(z*s[1]+y)*s[0]+x;
            const double v=uniform?0.4*(c+1):
                (c+1)*0.15+0.08*std::sin(0.4*x+0.2*y-0.3*z+c)
                -0.13*std::cos(0.3*x-0.2*y+0.1*z);
            out[c].push_back(alpha[i]>0.0?v:0.0);
        }
    }
    return out;
}
void run(int n,int axis,int sign,double fraction){
    Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    const auto start=Builder::sample(n,n,n,1.0,0.25,b,b);
    auto field=values(start,false);
    for(int step=0;step<2;++step){
        const auto next=shift(b,axis,sign*fraction);
        const auto result=Transport::step(n,n,n,1.0,0.25,1000.0,b,next,field);
        const auto dual=Builder::sample(n,n,n,1.0,0.25,b,next);
        for(int c=0;c<3;++c){
            const auto& s=dual.dimensions[c];
            const auto& old_a=dual.component[c].geometry.cell_geometry.alpha_old;
            const auto& new_a=dual.component[c].geometry.cell_geometry.alpha_new;
            check(result.component_new[c].size()==field[c].size(),"shape");
            double before=0.0,after=0.0;
            for(std::size_t i=0;i<field[c].size();++i){
                before+=old_a[i]*field[c][i];
                after+=new_a[i]*result.component_new[c][i];
                check(std::isfinite(result.component_new[c][i]),"finite dual value");
            }
            near(result.integral_before[c],before,1e-10,"independent integral before");
            near(result.integral_after[c],after,1e-10,"independent integral after");
            near(after,before,1e-9,"signed dual conservation");
            near(result.integral_residual[c],0.0,1e-9,"reported residual");
            check(result.active_faces[c]>0,"moving dual flux");
            std::cout<<"DUALTRANSPORT "<<n<<" "<<axis<<" "<<sign<<" "
                     <<fraction<<" "<<step<<" "<<c<<" "
                     <<std::setprecision(17)<<before<<" "<<after<<" "
                     <<result.max_gcl_residual<<" "
                     <<result.max_donor_courant<<" "
                     <<result.active_faces[c]<<"\n";
        }
        check(result.max_gcl_residual<=1e-10,"GCL closure");
        check(result.max_donor_courant<=1.0+1e-11,"donor CFL");
        field=result.component_new;
        b=next;
    }
}
void constant_and_errors(){
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    for(int n:{8,12,16}){
        const auto dual=Builder::sample(n,n,n,1.0,0.25,b,b);
        auto constant=values(dual,true);
        auto stationary=Transport::step(n,n,n,1.0,0.25,1000,b,b,constant);
        for(int c=0;c<3;++c){
            check(stationary.active_faces[c]==0,"stationary zero transport");
            for(std::size_t i=0;i<constant[c].size();++i)
                near(stationary.component_new[c][i],constant[c][i],1e-14,
                     "stationary roundoff identity");
        }
        auto next=shift(b,0,0.25);
        auto moved=Transport::step(n,n,n,1.0,0.25,1000,b,next,constant);
        const auto geometry=Builder::sample(n,n,n,1.0,0.25,b,next);
        for(int c=0;c<3;++c){
            const auto& alpha=geometry.component[c].geometry.cell_geometry.alpha_new;
            std::cout<<"CONST "<<n<<" "<<c<<" "<<std::setprecision(17)
                     <<moved.integral_before[c]<<" "
                     <<moved.integral_after[c]<<"\n";
            for(std::size_t i=0;i<alpha.size();++i)
                if(alpha[i]>1e-12)
                    near(moved.component_new[c][i],0.4*(c+1),1e-8,
                         "moving uniform dual state");
        }
    }
    auto field=values(Builder::sample(8,8,8,1.0,0.25,b,b),false);
    const auto original=field;
    rejects([&]{Transport::step(2,8,8,1,0.25,1000,b,b,field);},"thin dual grid");
    rejects([&]{Transport::step(8,8,8,1,0.25,0,b,b,field);},"zero density");
    rejects([&]{Transport::step(8,8,8,1,0.25,
        std::numeric_limits<double>::quiet_NaN(),b,b,field);},"nan density");
    rejects([&]{Transport::step(8,8,8,1,0.25,1000,b,shift(b,0,1.1),field);},
            "moving box CFL");
    auto wrong=field;wrong[1].pop_back();
    rejects([&]{Transport::step(8,8,8,1,0.25,1000,b,b,wrong);},
            "wrong dual shape");
    auto nan=field;nan[2][0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Transport::step(8,8,8,1,0.25,1000,b,b,nan);},
            "nonfinite dual value");
    check(field==original,"transactional caller state");
}
}
int main(){
    try{
        for(int n:{8,12,16})for(int a=0;a<3;++a)
        for(int sign:{-1,1})for(double f:{0.25,0.5})run(n,a,sign,f);
        constant_and_errors();
        std::cout<<"PASS: "<<checks<<" native dual conservative transport assertions\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" checks\n";
        return 1;
    }
}
