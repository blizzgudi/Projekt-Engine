// P2 partial: independently manufactured sphere/cylinder/plane SDF
// capillary pressure on a native 3D MAC-PCG projection.
#include "upc/p2_sdf_capillary3d.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int checks=0;
void check(bool b,const char* msg){++checks;if(!b)throw std::runtime_error(msg);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"invalid data must reject");}
struct Case {
    upc::MacProjection3D mac;
    std::vector<double> phi;
    Case(int n,double h,double radius,int shape):mac(n,n,n,h),phi((std::size_t)n*n*n){
        const double c=n*h/2;
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
            const double a=(x+.5)*h-c,b=(y+.5)*h-c,d=(z+.5)*h-c;
            const double r=shape==0?std::sqrt(a*a+b*b+d*d):
                           shape==1?std::sqrt(a*a+b*b):a;
            const double v=r-radius;
            phi[((std::size_t)z*n+y)*n+x]=v;
            mac.set_cell(x,y,z,v<0);
        }
    }
};
void manufactured(int n,double h,int shape,double reference){
    Case a(n,h,.287,shape);
    auto out=upc::p2_apply_sdf_capillary3d(a.mac,n,n,n,h,a.phi,.072,120);
    check(out.faces.size()>20,"nontrivial 3D capillary surface");
    double maxerr=0,mean=0;
    for(const auto& f:out.faces){
        check(f.theta>0&&f.theta<1,"valid fractional distance");
        const double rel=std::abs(f.curvature_inv_m-reference)/reference;
        maxerr=std::max(maxerr,rel);mean+=f.curvature_inv_m;
        check(std::abs(f.pressure_pa-(120+.072*f.curvature_inv_m))<1e-11,
              "Young-Laplace SI pressure jump");
    }
    mean/=out.faces.size();
    std::cout<<"CAPILLARY_GEOMETRY shape="<<(shape==0?"sphere":"cylinder")
             <<" dx="<<h<<" faces="<<out.faces.size()
             <<" mean_curvature="<<mean<<" expected="<<reference
             <<" max_rel_error="<<maxerr<<"\n";
    check(maxerr<(h>.05?.2:.06),"manufactured curvature error bound");
    if(shape==0&&n==12){
        auto audit=a.mac.project_pcg_verified(.002,3000,1e-7,1e-10);
        check(audit.max_divergence_s_inv<1e-7,"capillary MAC divergence");
        double maxu=0;
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
            maxu=std::max(maxu,std::abs(a.mac.face_velocity_x(x,y,z)));
        std::cout<<"CAPILLARY_PCG divergence_s_inv="<<audit.max_divergence_s_inv
                 <<" max_x_face_speed_m_s="<<maxu
                 <<" iterations="<<audit.iterations<<"\n";
        check(std::isfinite(maxu),"finite measured parasitic velocity");
    }
}
void plane_and_rejections(){
    constexpr int n=12;constexpr double h=.08;
    Case a(n,h,.13,2);
    auto out=upc::p2_apply_sdf_capillary3d(a.mac,n,n,n,h,a.phi,.072,120);
    check(out.faces.size()==(std::size_t)n*n,"planar interface count");
    for(const auto& f:out.faces){
        check(f.axis==0&&std::abs(f.curvature_inv_m)<1e-10,"planar zero curvature");
        check(std::abs(f.pressure_pa-120)<1e-10,"planar pressure");
    }
    auto audit=a.mac.project_pcg_verified(.01,500,1e-9,1e-12);
    check(audit.max_divergence_s_inv<1e-9,"planar divergence");
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        if(a.phi[((std::size_t)z*n+y)*n+x]<0)
            check(std::abs(a.mac.pressure_pa(x,y,z)-120)<1e-7,
                  "planar constant liquid pressure");
        check(std::abs(a.mac.face_velocity_x(x,y,z))<1e-9,
              "planar zero parasitic velocity");
    }
    std::cout<<"CAPILLARY_PLANE faces="<<out.faces.size()
             <<" divergence_s_inv="<<audit.max_divergence_s_inv<<"\n";
    rejects([&]{upc::p2_sdf_capillary_faces3d(n,n,n,h,a.phi,-.072);});
    rejects([&]{upc::p2_sdf_capillary_faces3d(n,n,n,h,a.phi,.072,
                      std::numeric_limits<double>::infinity());});
    auto bad=a.phi;bad[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{upc::p2_apply_sdf_capillary3d(a.mac,n,n,n,h,bad,.072);});
    bad=a.phi;bad[0]=0;
    rejects([&]{upc::p2_apply_sdf_capillary3d(a.mac,n,n,n,h,bad,.072);});
    bad=a.phi;bad.pop_back();
    rejects([&]{upc::p2_apply_sdf_capillary3d(a.mac,n,n,n,h,bad,.072);});
    rejects([&]{upc::p2_apply_sdf_capillary3d(a.mac,n,n,n,h*2,a.phi,.072);});
    auto mismatch=a.mac;
    mismatch.set_cell(0,0,0,false);
    rejects([&]{upc::p2_apply_sdf_capillary3d(mismatch,n,n,n,h,a.phi,.072);});
    mismatch=a.mac;mismatch.set_cell(0,0,0,false,true);
    rejects([&]{upc::p2_apply_sdf_capillary3d(mismatch,n,n,n,h,a.phi,.072);});
    std::vector<double> allwater(a.phi.size(),-1);
    rejects([&]{upc::p2_sdf_capillary_faces3d(n,n,n,h,allwater,.072);});
    check(std::abs(a.mac.pressure_pa(0,0,0)-120)<1e-7,
          "failed updates leave existing MAC pressure untouched");
}
}
int main(){
    try{
        manufactured(12,.08,0,2/.287);
        manufactured(24,.04,0,2/.287);
        manufactured(12,.08,1,1/.287);
        manufactured(24,.04,1,1/.287);
        plane_and_rejections();
        std::cout<<"PASS P2_SDF_CAPILLARY assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_SDF_CAPILLARY "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
