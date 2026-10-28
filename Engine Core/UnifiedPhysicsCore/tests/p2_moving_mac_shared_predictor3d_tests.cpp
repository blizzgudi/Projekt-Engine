#include "upc/p2_moving_mac_shared_predictor3d.hpp"
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
using Bridge=upc::P2MovingMacSharedPredictor3D;
using Faces=Bridge::Faces;
using Builder=upc::P2MovingMacDualVolumes3DBuilder;
std::size_t checks=0;
void check(bool ok,const char* msg){++checks;if(!ok)throw std::runtime_error(msg);}
void near(double a,double b,double tol,const char* msg){
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
template<class F> void rejects(F fn,const char* msg){
    bool rejected=false;
    try{fn();}catch(const std::exception&){rejected=true;}
    check(rejected,msg);
}
Box shift(Box b,int axis,double d){
    b.min_m[axis]+=d;b.max_m[axis]+=d;return b;
}
Faces primal_field(int n,double amplitude){
    Faces out;
    for(int f=0;f<3;++f){
        std::array<int,3> dims{{n,n,n}};
        ++dims[f];
        out[f].resize(dims[0]*dims[1]*dims[2]);
        for(int z=0;z<dims[2];++z)
        for(int y=0;y<dims[1];++y)
        for(int x=0;x<dims[0];++x){
            const std::array<int,3> p{{x,y,z}};
            const auto i=(z*dims[1]+y)*dims[0]+x;
            if(p[f]==0||p[f]==n){out[f][i]=0.0;continue;}
            // Smooth three-component field, nonzero curl and a nontrivial
            // pressure correction. Exactly zero normal boundary flux.
            out[f][i]=amplitude*std::sin(3.141592653589793*p[f]/n)*
                (0.4*std::cos(0.3*x+0.5*y-0.2*z+f)+
                 0.6*std::sin(0.2*x-0.3*y+0.4*z+f));
        }
    }
    return out;
}
Faces values(int n,const Box& box){
    const auto dual=Builder::sample(n,n,n,1.0,0.25,box,box);
    Faces out;
    for(int c=0;c<3;++c){
        const auto& s=dual.dimensions[c];
        const auto& a=dual.component[c].geometry.cell_geometry.alpha_old;
        out[c].resize(a.size());
        for(std::size_t z=0;z<s[2];++z)
        for(std::size_t y=0;y<s[1];++y)
        for(std::size_t x=0;x<s[0];++x){
            const auto i=(z*s[1]+y)*s[0]+x;
            out[c][i]=a[i]>0.0?
                0.2*(c+1)+0.07*std::sin(0.4*x+0.2*y-0.3*z+c):0.0;
        }
    }
    return out;
}
void midpoint_and_errors(){
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    for(int n:{8,12,16}){
        const auto primal=primal_field(n,0.07);
        const auto p=Bridge::reconstruct(n,n,n,1.0,0.25,b,b,primal);
        const auto dual=Builder::sample(n,n,n,1.0,0.25,b,b);
        for(int a=0;a<3;++a)for(int f=0;f<3;++f){
            auto fs=dual.dimensions[a];
            ++fs[f];
            std::array<std::size_t,3> ps{{std::size_t(n),std::size_t(n),std::size_t(n)}};
            ++ps[f];
            const auto& geom=dual.component[a].geometry;
            const auto& aperture=f==0?geom.u_open:
                                 f==1?geom.v_open:geom.w_open;
            for(std::size_t z=0;z<fs[2];++z)
            for(std::size_t y=0;y<fs[1];++y)
            for(std::size_t x=0;x<fs[0];++x){
                const std::array<std::size_t,3> idx{{x,y,z}};
                const auto i=(z*fs[1]+y)*fs[0]+x;
                if(idx[f]==0||idx[f]==dual.dimensions[a][f]||
                   aperture[i]<=1e-12){
                    near(p[a][f][i],0.0,0.0,"blocked or exterior dual face");
                }else{
                    auto next=idx;++next[a];
                    const auto j=(z*ps[1]+y)*ps[0]+x;
                    const auto k=(next[2]*ps[1]+next[1])*ps[0]+next[0];
                    near(p[a][f][i],0.5*(primal[f][j]+primal[f][k]),
                         1e-15,"independent midpoint interpolation");
                }
            }
        }
    }
    auto primal=primal_field(8,0.07);
    const auto original=primal;
    auto wrong=primal;wrong[0].pop_back();
    rejects([&]{Bridge::reconstruct(8,8,8,1,0.25,b,b,wrong);},
            "primal shape rejected");
    auto nan=primal;nan[1][10]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Bridge::reconstruct(8,8,8,1,0.25,b,b,nan);},
            "nonfinite primal rejected");
    auto boundary=primal;boundary[0][0]=0.1;
    rejects([&]{Bridge::reconstruct(8,8,8,1,0.25,b,b,boundary);},
            "normal exterior flow rejected");
    rejects([&]{Bridge::reconstruct(2,8,8,1,0.25,b,b,primal);},
            "thin grid rejected");
    check(primal==original,"no input mutation after failure");
}
void run(int n,int axis,int sign){
    Box box{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    const auto primal=primal_field(n,0.07);
    auto state=values(n,box);
    for(int step=0;step<2;++step){
        const auto next=shift(box,axis,sign*0.15);
        const auto result=Bridge::step(n,n,n,1.0,0.25,1000.0,
                                        box,next,primal,state);
        const auto dual=Builder::sample(n,n,n,1.0,0.25,box,next);
        for(int c=0;c<3;++c){
            const auto& old_a=dual.component[c].geometry.cell_geometry.alpha_old;
            const auto& new_a=dual.component[c].geometry.cell_geometry.alpha_new;
            double before=0.0,after=0.0;
            for(std::size_t i=0;i<state[c].size();++i){
                before+=old_a[i]*state[c][i];
                after+=new_a[i]*result.component_new[c][i];
                check(std::isfinite(result.component_new[c][i]),
                      "finite shared transport value");
            }
            near(result.integral_before[c],before,1e-9,"independent before");
            near(result.integral_after[c],after,1e-9,"independent after");
            near(after,before,1e-8,"conservative shared transport");
            check(result.active_faces[c]>0,"nonzero shared projected flux");
            std::cout<<"SHAREDMAC "<<n<<" "<<axis<<" "<<sign<<" "<<step
                     <<" "<<c<<" "<<std::setprecision(17)<<before<<" "
                     <<after<<" "<<result.max_gcl_residual<<" "
                     <<result.max_donor_courant<<" "
                     <<result.active_faces[c]<<"\n";
        }
        check(result.max_gcl_residual<=1e-10,"shared predictor GCL");
        check(result.max_donor_courant<=1.0+1e-11,"shared donor CFL");
        state=result.component_new;
        box=next;
    }
}
void zero_and_nonzero(){
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    const auto original=values(12,b);
    const auto zero=primal_field(12,0.0);
    const auto reference=upc::P2MovingMacDualTransport3D::step(
        12,12,12,1.0,0.25,1000,b,shift(b,0,0.15),original);
    const auto reconstructed=Bridge::step(
        12,12,12,1.0,0.25,1000,b,shift(b,0,0.15),zero,original);
    for(int c=0;c<3;++c){
        check(reference.component_new[c]==reconstructed.component_new[c],
              "zero shared predictor reproduces legacy exact");
    }
    const auto nonzero=Bridge::step(
        12,12,12,1.0,0.25,1000,b,b,primal_field(12,0.07),original);
    const auto stationary=upc::P2MovingMacDualTransport3D::step(
        12,12,12,1.0,0.25,1000,b,b,original);
    double difference=0.0;
    for(int c=0;c<3;++c)
        for(std::size_t i=0;i<original[c].size();++i)
            difference+=std::abs(nonzero.component_new[c][i]-
                                 stationary.component_new[c][i]);
    check(difference>1e-6,"nonzero primal velocity changes dual transport");
    std::cout<<"NONZERO_DIFFERENCE "<<std::setprecision(17)<<difference<<"\n";
    auto wrong=original;wrong[2].pop_back();
    rejects([&]{Bridge::step(12,12,12,1,.25,1000,b,b,zero,wrong);},
            "bad dual component rejected");
    rejects([&]{Bridge::step(12,12,12,1,.25,0,b,b,zero,original);},
            "bad density rejected");
    check(original==values(12,b),"caller state not mutated");
}
}
int main(){
    try{
        midpoint_and_errors();
        for(int n:{8,12,16})for(int a=0;a<3;++a)
            for(int sign:{-1,1})run(n,a,sign);
        zero_and_nonzero();
        std::cout<<"PASS: "<<checks<<" native shared MAC predictor assertions\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" checks\n";
        return 1;
    }
}
