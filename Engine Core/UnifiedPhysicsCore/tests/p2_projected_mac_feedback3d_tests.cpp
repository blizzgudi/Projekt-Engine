// P2 PARTIAL: opt-in carry-forward of projected OPEN-face MAC velocities.
// This tests feedback of a pressure-corrected field, not Navier-Stokes or FSI.
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
void require(bool ok,const char* message) {
    ++checks;
    if(!ok)throw std::runtime_error(message);
}
void near(double a,double b,double tol,const char* msg) {
    require(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
using Box=upc::P2Box3D;
using State=upc::P2PrescribedBoxMacVof3D;
std::size_t ui(int n,int x,int y,int z){return (z*n+y)*(n+1)+x;}
std::size_t vi(int n,int x,int y,int z){return (z*(n+1)+y)*n+x;}
struct Predictor {
    std::vector<double> u,v,w;
    explicit Predictor(int n):u((n+1)*n*n,0.0),
        v(n*(n+1)*n,0.0),w(n*n*(n+1),0.0){}
};
void verify_feedback(const State& state,
                     const upc::P2PrescribedBoxMacVofStep3D& result,
                     int n,int axis,int sign,double saturation,double dt) {
    const auto& geometry=result.projection.wall.geometry;
    const double area_time=dt; // dx = 1 m for this manufactured test
    const auto verify_faces=[&](const std::vector<double>& q,
                                const std::vector<double>& a,
                                const std::vector<double>& velocity) {
        require(q.size()==a.size()&&a.size()==velocity.size(),
                "corrected MAC shape");
        for(std::size_t i=0;i<q.size();++i) {
            const double expected=a[i]>1e-12?q[i]/(area_time*a[i]):0.0;
            near(velocity[i],expected,1e-12,"MAC aperture inversion");
        }
    };
    verify_faces(result.projection.u_fluid_m3,geometry.u_open,
                 state.corrected_u_m_s());
    verify_faces(result.projection.v_fluid_m3,geometry.v_open,
                 state.corrected_v_m_s());
    verify_faces(result.projection.w_fluid_m3,geometry.w_open,
                 state.corrected_w_m_s());
    double water=0.0,max_uniform=0.0;
    for(std::size_t i=0;i<state.water_fraction().size();++i) {
        const double expected=saturation*state.open_fraction()[i];
        const double actual=state.water_fraction()[i];
        near(actual,expected,5e-10,"uniform fraction preserved");
        water+=actual;
        max_uniform=std::max(max_uniform,std::abs(actual-expected));
    }
    near(water,saturation*(n*n*n-3.375),2e-9,"box analytic water volume");
    near(result.projection.max_continuity_residual_m3,0.0,2e-10,
         "moving cut-cell continuity");
    near(result.vof.max_gcl_residual_cell_volumes,0.0,2e-10,
         "VOF geometric conservation");
    require(state.has_corrected_mac(),"feedback state available");
    require(result.step_index==state.step_count(),"step index matches");
    double circulation=0.0;
    for(double v:state.corrected_u_m_s())circulation+=std::abs(v);
    for(double v:state.corrected_v_m_s())circulation+=std::abs(v);
    require(circulation>0.01,"nonzero projected MAC velocity retained");
    std::cout<<std::setprecision(17)<<"FEEDBACK "<<n<<" "<<axis<<" "<<sign<<" "
             <<result.step_index<<" "<<saturation<<" "<<water<<" "
             <<max_uniform<<" "<<result.projection.max_continuity_residual_m3
             <<" "<<circulation<<"\n";
}
void run(int n,int axis,int sign,double saturation) {
    const Box box{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    State template_state(n,n,n,1.0,1000.0,box);
    auto water=template_state.open_fraction();
    for(double& x:water)x*=saturation;
    State state(n,n,n,1.0,1000.0,box,water);
    Predictor p(n);
    const double dt=0.25;
    // Four-face solenoidal circulation, far from moving box.
    p.u[ui(n,2,1,1)]=0.08;
    p.v[vi(n,2,2,1)]=0.08;
    p.u[ui(n,2,2,1)]=-0.08;
    p.v[vi(n,2,1,1)]=-0.08;
    // An independent non-solenoidal gradient, projected out.
    p.u[ui(n,1,1,1)]-=0.12;
    p.u[ui(n,2,1,1)]+=0.12;
    p.v[vi(n,1,1,1)]-=0.12;
    p.v[vi(n,1,2,1)]+=0.12;
    for(int step=1;step<=3;++step) {
        Box next=state.box();
        next.min_m[axis]+=sign*0.25;
        next.max_m[axis]+=sign*0.25;
        const auto result=step==1?
            state.advance(next,dt,p.u,p.v,p.w):
            state.advance_reusing_projected_mac(next,dt);
        verify_feedback(state,result,n,axis,sign,saturation,dt);
        require(state.step_count()==static_cast<std::size_t>(step),
                "three sequential steps");
    }
}
void negative() {
    const Box box{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    State s(8,8,8,1.0,1000.0,box);
    Box next=box;
    next.min_m[0]+=0.25;next.max_m[0]+=0.25;
    bool threw=false;
    try{s.advance_reusing_projected_mac(next,0.25);}
    catch(const std::logic_error&){threw=true;}
    require(threw&&!s.has_corrected_mac()&&s.step_count()==0,
            "cannot feedback before first projection");
    Predictor p(8);
    s.advance(next,0.25,p.u,p.v,p.w);
    const auto original_box=s.box();
    const auto water=s.water_fraction(),alpha=s.open_fraction();
    const auto u=s.corrected_u_m_s(),v=s.corrected_v_m_s(),
               w=s.corrected_w_m_s();
    auto rollback=[&](const Box& destination,double dt,
                      std::size_t iterations) {
        bool failed=false;
        try{s.advance_reusing_projected_mac(destination,dt,iterations);}
        catch(const std::exception&){failed=true;}
        require(failed,"bad feedback step rejected");
        require(s.water_fraction()==water&&s.open_fraction()==alpha,
                "feedback water and geometry rollback");
        require(s.corrected_u_m_s()==u&&s.corrected_v_m_s()==v&&
                s.corrected_w_m_s()==w,
                "feedback MAC velocity rollback");
        require(s.box().min_m==original_box.min_m&&
                s.box().max_m==original_box.max_m&&s.step_count()==1,
                "feedback box and index rollback");
    };
    rollback(next,0.0,4000);
    rollback(next,0.25,0);
    Box far=next;
    far.min_m[0]+=2.0;far.max_m[0]+=2.0;
    rollback(far,0.25,4000);
}
}
int main() {
    try {
        for(int n:{8,12,16})for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})for(double sat:{0.5,1.0})
                run(n,axis,sign,sat);
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native projected-MAC feedback assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
