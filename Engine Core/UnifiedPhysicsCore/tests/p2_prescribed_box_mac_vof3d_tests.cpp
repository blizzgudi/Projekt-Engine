// P2 PARTIAL: sequential moving-box pressure/VOF integration with
// independent uniform-saturation and volume references. No two-way FSI.
#include "upc/p2_prescribed_box_mac_vof3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
std::size_t checks=0;
void check(bool ok,const char* message) {
    ++checks;
    if(!ok)throw std::runtime_error(message);
}
void near(double a,double b,double tol,const char* msg) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
using Box=upc::P2Box3D;
using State=upc::P2PrescribedBoxMacVof3D;
std::size_t ui(int n,int x,int y,int z){return (z*n+y)*(n+1)+x;}
std::size_t vi(int n,int x,int y,int z){return (z*(n+1)+y)*n+x;}
std::size_t wi(int n,int x,int y,int z){return (z*n+y)*n+x;}
struct Predictor {
    std::vector<double> u,v,w;
    explicit Predictor(int n):u((n+1)*n*n,0.0),
        v(n*(n+1)*n,0.0),w(n*n*(n+1),0.0){}
};
void run(int n,int axis,int sign,double saturation) {
    Box initial{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    State full(n,n,n,1.0,1000.0,initial);
    std::vector<double> water=full.open_fraction();
    for(double& f:water)f*=saturation;
    State state(n,n,n,1.0,1000.0,initial,water);
    const double expected_water=saturation*(n*n*n-3.375);
    Predictor predictor(n);
    // Manufactured 4-face solenoidal loop and 6-face pressure gradient.
    // Both are far from the moving box for every test resolution.
    const double dt=0.25,chi=0.03,curl=0.02;
    predictor.u[ui(n,2,1,1)]+=curl/dt;
    predictor.v[vi(n,2,2,1)]+=curl/dt;
    predictor.u[ui(n,2,2,1)]-=curl/dt;
    predictor.v[vi(n,1,2,1)]-=curl/dt;
    predictor.u[ui(n,1,1,1)]-=chi/dt;
    predictor.u[ui(n,2,1,1)]+=chi/dt;
    predictor.v[vi(n,1,1,1)]-=chi/dt;
    predictor.v[vi(n,1,2,1)]+=chi/dt;
    predictor.w[wi(n,1,1,1)]-=chi/dt;
    predictor.w[wi(n,1,1,2)]+=chi/dt;
    for(int step=1;step<=2;++step) {
        Box next=state.box();
        next.min_m[axis]+=sign*0.25;
        next.max_m[axis]+=sign*0.25;
        const auto result=state.advance(
            next,dt,predictor.u,predictor.v,predictor.w);
        check(state.step_count()==static_cast<std::size_t>(step),
              "time step count");
        check(result.step_index==static_cast<std::size_t>(step),
              "result step index");
        check(state.box().min_m==next.min_m,"box state updated");
        double actual=0.0,max_uniform=0.0;
        for(std::size_t i=0;i<state.water_fraction().size();++i) {
            const double a=state.open_fraction()[i];
            const double f=state.water_fraction()[i];
            actual+=f;
            max_uniform=std::max(max_uniform,std::abs(f-saturation*a));
            near(f,saturation*a,5e-10,"uniform saturation preserved");
            near(result.vof.water_cell_fraction[i],f,0.0,
                 "result and committed state match");
        }
        near(actual,expected_water,2e-9,"independent box-volume water reference");
        near(result.vof.mass_residual_m3,0.0,2e-9,"VOF mass");
        near(result.projection.max_continuity_residual_m3,0.0,2e-10,
             "cut-cell pressure continuity");
        near(result.vof.max_gcl_residual_cell_volumes,0.0,2e-10,
             "VOF geometric conservation");
        std::cout<<std::setprecision(17)<<"SEQ "<<n<<" "<<axis<<" "<<sign
                 <<" "<<saturation<<" "<<step<<" "<<actual<<" "
                 <<max_uniform<<" "
                 <<result.vof.max_gcl_residual_cell_volumes<<"\n";
    }
}
void negative() {
    Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    State s(8,8,8,1.0,1000.0,b);
    Predictor p(8);
    auto reject=[&](const Box& next,const Predictor& x,double dt) {
        const auto water=s.water_fraction();
        const auto alpha=s.open_fraction();
        const auto original=s.box();
        const auto count=s.step_count();
        bool threw=false;
        try{s.advance(next,dt,x.u,x.v,x.w);}
        catch(const std::exception&){threw=true;}
        check(threw,"bad step rejected");
        check(s.water_fraction()==water&&s.open_fraction()==alpha,
              "rollback water and geometry");
        check(s.box().min_m==original.min_m&&
              s.box().max_m==original.max_m&&s.step_count()==count,
              "rollback box and step index");
    };
    Box next=b;
    next.min_m[0]+=0.25;next.max_m[0]+=0.25;
    p.u[ui(8,0,1,1)]=1.0;
    reject(next,p,0.25); // exterior boundary
    p.u[ui(8,0,1,1)]=0.0;
    p.v[vi(8,1,1,1)]=std::numeric_limits<double>::quiet_NaN();
    reject(next,p,0.25);
    p.v[vi(8,1,1,1)]=0.0;
    p.u.pop_back();
    reject(next,p,0.25);
    p.u.push_back(0.0);
    reject(next,p,0.0);
    Box too_far=b;
    too_far.min_m[0]+=1.25;too_far.max_m[0]+=1.25;
    reject(too_far,p,0.25);
    // Grossly excessive internal face flux violates the donor CFL.
    p.u[ui(8,2,1,1)]=100.0;
    reject(next,p,0.25);
    check(s.step_count()==0,"all failures transactional");
}
}
int main(){
    try{
        for(int n:{8,12,16})for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})for(double saturation:{0.5,1.0})
                run(n,axis,sign,saturation);
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native sequential cut-cell MAC/VOF assertions\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
