#include "upc/p2_moving_mac_dual_pressure_work3d.hpp"
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
using Audit=upc::P2MovingMacDualPressureWork3D;
std::size_t assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double x,double y,double tol,const char* msg) {
    check(std::isfinite(x)&&std::isfinite(y)&&std::abs(x-y)<=tol,msg);
}
template<class F> void rejects(F f,const char* msg) {
    bool caught=false;
    try{f();}catch(const std::exception&){caught=true;}
    check(caught,msg);
}
Bridge::Faces zeros(int n) {
    Bridge::Faces out;
    for(int a=0;a<3;++a) {
        std::array<int,3> s{{n,n,n}};
        ++s[a];
        out[a].assign(s[0]*s[1]*s[2],0.0);
    }
    return out;
}
void run(int n,int axis,int sign,bool active) {
    const Box old{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    auto next=old;
    if(active) {
        next.min_m[axis]+=0.15*sign;
        next.max_m[axis]+=0.15*sign;
    }
    const auto primal=zeros(n);
    const auto original=primal;
    const auto r=Audit::audit(n,n,n,1.0,0.25,1000.0,old,next,primal);
    check(primal==original,"no caller mutation");
    check(r.primal_gcl_residual_m3<1e-9,"primal GCL");
    double change=0.0,wall=0.0,correction=0.0;
    const auto projected=Bridge::project_primal(
        n,n,n,1.0,0.25,1000.0,old,next,primal);
    const auto predicted=Bridge::reconstruct(
        n,n,n,1.0,0.25,old,next,projected.corrected);
    const auto dual=upc::P2MovingMacDualVolumes3DBuilder::sample(
        n,n,n,1.0,0.25,old,next);
    for(int a=0;a<3;++a) {
        auto lo=old,hi=next;
        lo.min_m[a]-=0.5;lo.max_m[a]-=0.5;
        hi.min_m[a]-=0.5;hi.max_m[a]-=0.5;
        const auto& s=dual.dimensions[a];
        auto q=upc::P2MovingBoxCutCellProjection3DBuilder::sample_with_predictor(
            s[0],s[1],s[2],1.0,0.25,1000.0,lo,hi,
            predicted[a][0],predicted[a][1],predicted[a][2]);
        const auto independent=upc::P2MovingCutCellPressureEnergy3D::audit(
            s[0],s[1],s[2],1.0,0.25,1000.0,
            predicted[a][0],predicted[a][1],predicted[a][2],q);
        const auto& e=r.dual[a];
        near(e.projection_change_j,independent.projection_change_j,1e-9,
             "independent dual projection change");
        near(e.geometric_pressure_work_j,
             independent.geometric_pressure_work_j,1e-9,
             "independent dual wall work");
        near(e.correction_energy_j,independent.correction_energy_j,1e-9,
             "independent dual correction");
        near(e.projection_change_j,
             e.geometric_pressure_work_j-e.correction_energy_j,1e-7,
             "moving dual pressure work identity");
        check(e.max_continuity_residual_m3<1e-9,"dual GCL");
        check(e.max_pressure_gradient_residual_m_s<1e-9,
              "dual pressure gradient");
        // Closed-domain pressure gauge shift cannot change work identity.
        for(double& p:q.pressure_pa)p+=17.0;
        const auto gauge=upc::P2MovingCutCellPressureEnergy3D::audit(
            s[0],s[1],s[2],1.0,0.25,1000.0,
            predicted[a][0],predicted[a][1],predicted[a][2],q);
        near(gauge.geometric_pressure_work_j,
             e.geometric_pressure_work_j,1e-7,"pressure gauge wall work");
        near(gauge.projection_change_j,
             e.projection_change_j,1e-7,"pressure gauge kinetic change");
        // An inconsistent pressure gradient must not pass as evidence.
        if(active) {
            q.pressure_pa[0]+=100.0;
            rejects([&]{
                upc::P2MovingCutCellPressureEnergy3D::audit(
                    s[0],s[1],s[2],1.0,0.25,1000.0,
                    predicted[a][0],predicted[a][1],predicted[a][2],q);
            },"corrupt pressure rejected");
        }
        change+=e.projection_change_j;
        wall+=e.geometric_pressure_work_j;
        correction+=e.correction_energy_j;
    }
    near(change,r.sum_projection_change_j,1e-8,"sum projection change");
    near(wall,r.sum_geometric_pressure_work_j,1e-8,"sum wall work");
    near(correction,r.sum_correction_energy_j,1e-8,"sum correction");
    near(change,wall-correction,1e-7,"summed dual identity");
    check(r.max_identity_residual_j<1e-7,"max residual");
    if(!active) {
        near(change,0.0,1e-8,"static zero velocity no pressure work");
        near(wall,0.0,1e-8,"static zero wall work");
    }else check(correction>1e-5,"moving dual nonzero pressure correction");
    std::cout<<"DUAL_PRESSURE "<<n<<" "<<axis<<" "<<sign<<" "<<(active?1:0)
             <<" "<<std::setprecision(17)<<change<<" "<<wall<<" "
             <<correction<<" "<<r.max_identity_residual_j<<"\n";
}
}
int main() {
    try {
        for(int n:{8,12}) {
            run(n,0,1,false);
            for(int axis=0;axis<3;++axis)
                for(int sign:{-1,1})run(n,axis,sign,true);
        }
        const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
        auto input=zeros(8);
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,0.0,b,b,input);},
                "zero density rejected");
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,1000.0,b,b,Bridge::Faces{});},
                "bad shape rejected");
        input[0][0]=1.0;
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,1000.0,b,b,input);},
                "closed exterior face rejected");
        input=zeros(8);
        input[0][0]=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,1000.0,b,b,input);},
                "NaN predictor rejected");
        std::cout<<"PASS: "<<assertions
                 <<" native moving dual pressure work assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
