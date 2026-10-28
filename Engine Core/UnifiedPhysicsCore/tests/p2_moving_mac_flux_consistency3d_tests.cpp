#include "upc/p2_moving_mac_flux_consistency3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Audit=upc::P2MovingMacFluxConsistency3D;
using Bridge=upc::P2MovingMacSharedPredictor3D;
using Box=upc::P2Box3D;
std::size_t assertions=0;
void require(bool b,const char* msg) {
    ++assertions;
    if(!b)throw std::runtime_error(msg);
}
void near(double a,double b,double eps,const char* msg) {
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=eps,msg);
}
Bridge::Faces zero(int n) {
    Bridge::Faces out;
    for(int f=0;f<3;++f) {
        std::array<int,3> s{{n,n,n}};
        ++s[f];
        out[f].assign(s[0]*s[1]*s[2],0.0);
    }
    return out;
}
void verify_case(int n,int axis,int direction) {
    const Box old_box{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    Box next=old_box;
    next.min_m[axis]+=0.15*direction;
    next.max_m[axis]+=0.15*direction;
    const auto primal=zero(n);
    const auto original=primal;
    const auto result=Audit::audit(n,n,n,1.0,0.25,1000.0,
                                   old_box,next,primal);
    require(primal==original,"audit does not mutate caller");
    require(result.primal_gcl_residual_m3<1e-9,"primal GCL");
    require(result.max_dual_gcl_residual_m3<1e-9,"dual GCL");
    require(result.sum_abs_flux_correction_m3>1e-6,
            "moving box requires nonzero dual correction");
    require(result.sum_abs_predicted_flux_m3>1e-6,
            "moving-box primal pressure projection generates dual input flux");
    double sum=0.0;
    for(int a=0;a<3;++a) {
        sum+=result.component_abs_correction_m3[a];
        require(result.active_dual_faces[a]>0,"active dual faces");
        require(result.component_abs_correction_m3[a]>0,
                "each moving dual needs GCL correction");
    }
    near(sum,result.sum_abs_flux_correction_m3,1e-9,
         "component correction decomposition");
    require(result.max_flux_correction_m3>0.0,"max correction positive");
    // Independently reconstruct every dual projection and sum the
    // absolute difference between projected and reconstructed fluxes.
    const auto p=Bridge::project_primal(n,n,n,1.0,0.25,1000.0,
                                        old_box,next,primal);
    const auto v=Bridge::reconstruct(n,n,n,1.0,0.25,
                                      old_box,next,p.corrected);
    const auto d=upc::P2MovingMacDualVolumes3DBuilder::sample(
        n,n,n,1.0,0.25,old_box,next);
    double manual=0.0,maximum=0.0;
    for(int a=0;a<3;++a) {
        Box old_shift=old_box,new_shift=next;
        old_shift.min_m[a]-=0.5;old_shift.max_m[a]-=0.5;
        new_shift.min_m[a]-=0.5;new_shift.max_m[a]-=0.5;
        const auto& s=d.dimensions[a];
        const auto q=upc::P2MovingBoxCutCellProjection3DBuilder::
            sample_with_predictor(s[0],s[1],s[2],1.0,0.25,1000.0,
                                  old_shift,new_shift,
                                  v[a][0],v[a][1],v[a][2]);
        const std::array<const std::vector<double>*,3> flux{{
            &q.u_fluid_m3,&q.v_fluid_m3,&q.w_fluid_m3}};
        const std::array<const std::vector<double>*,3> aperture{{
            &q.wall.geometry.u_open,&q.wall.geometry.v_open,
            &q.wall.geometry.w_open}};
        double per_axis=0.0;
        for(int f=0;f<3;++f)for(std::size_t i=0;i<flux[f]->size();++i) {
            const double difference=std::abs(
                (*flux[f])[i]-0.25*(*aperture[f])[i]*v[a][f][i]);
            per_axis+=difference;
            maximum=std::max(maximum,difference);
            ++assertions;
        }
        near(per_axis,result.component_abs_correction_m3[a],1e-9,
             "independent per-axis flux sum");
        manual+=per_axis;
    }
    near(manual,result.sum_abs_flux_correction_m3,1e-9,
         "independent global correction sum");
    near(maximum,result.max_flux_correction_m3,1e-12,
         "independent maximum correction");
    std::cout<<"FLUX_AUDIT "<<n<<" "<<axis<<" "<<direction<<" "
             <<std::setprecision(17)<<result.primal_gcl_residual_m3<<" "
             <<result.max_dual_gcl_residual_m3<<" "
             <<result.sum_abs_flux_correction_m3<<" "
             <<result.max_flux_correction_m3<<"\n";
}
void static_zero(int n) {
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    const auto a=Audit::audit(n,n,n,1.0,0.25,1000.0,b,b,zero(n));
    near(a.primal_gcl_residual_m3,0.0,1e-12,"stationary primal");
    near(a.max_dual_gcl_residual_m3,0.0,1e-12,"stationary dual");
    near(a.sum_abs_flux_correction_m3,0.0,1e-12,"stationary zero correction");
    near(a.sum_abs_predicted_flux_m3,0.0,1e-12,"stationary zero predictor");
}
template<class F> void rejects(F f,const char* msg) {
    bool thrown=false;
    try{f();}catch(const std::exception&){thrown=true;}
    require(thrown,msg);
}
}
int main() {
    try {
        for(int n:{8,12}) {
            static_zero(n);
            for(int axis=0;axis<3;++axis)
                for(int sign:{-1,1})verify_case(n,axis,sign);
        }
        const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
        auto input=zero(8);
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,0.0,b,b,input);},
                "zero density rejected");
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,1000.0,b,b,
                                Bridge::Faces{});},
                "wrong predictor shape rejected");
        input[0][0]=1.0;
        rejects([&]{Audit::audit(8,8,8,1.0,0.25,1000.0,b,b,input);},
                "exterior nonzero face rejected");
        std::cout<<"PASS: "<<assertions
                 <<" native flux consistency audit assertions\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
