// P2 PARTIAL: independent exact plane fractions and strict nonplanar rejection.
#include "upc/p2_local_planar_normal_fit3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <iomanip>
using V=std::array<double,3>;
static int assertions=0;
static void check(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
static double dot(V a,V b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
static void near(double got,double want,double tol,const char* why) {
    if(!std::isfinite(got)||std::abs(got-want)>tol)
        std::cerr<<"MISMATCH "<<why<<" got="<<got<<" want="<<want<<"\n";
    check(std::isfinite(got)&&std::abs(got-want)<=tol,why);
}
struct Slab {
    std::vector<double> fill;
    std::vector<std::uint8_t> solid;
};
static Slab make(int n,V normal,double cut) {
    Slab s;
    s.fill.resize(n*n*n);
    s.solid.resize(s.fill.size(),0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=(z*n+y)*n+x;
        const double c=cut-dot(normal,{double(x),double(y),double(z)});
        const double f=std::clamp(upc::p2_oblique_detail::clip_cube(normal,c).volume,0.,1.);
        s.fill[i]=f<1e-12?0.:f>1-1e-12?1.:f;
    }
    return s;
}
template<class F>static void reject(F fn,const char* why) {
    bool caught=false;
    try{fn();}catch(const std::exception&){caught=true;}
    check(caught,why);
}
static void exact(int n,V normal,double cut) {
    auto s=make(n,normal,cut);
    const auto before=s.fill;
    const auto fit=upc::p2_fit_local_planar_vof_normals3d(
        s.fill,s.solid,n,n,n,1.,1e-9);
    check(fit.fitted_cells>n*n/2,"planar partial cell count");
    check(fit.worst_patch_residual<1e-9,"independent clipped-volume fit");
    double min_alignment=1.;
    for(std::size_t i=0;i<s.fill.size();++i) {
        if(s.fill[i]<=0||s.fill[i]>=1)continue;
        const double alignment=dot(fit.normals[i],normal);
        min_alignment=std::min(min_alignment,alignment);
        check(alignment>1.-2e-7,"planar normal orientation recovered");
    }
    const auto geo=upc::p2_planar_vof_pcg_geometry3d(
        s.fill,s.solid,fit.normals,n,n,n,1.);
    check(!geo.resolved.empty(),"actual fitted geometry resolved");
    check(geo.oblique_faces==geo.resolved.size(),"oblique fitted geometry");
    for(const auto& f:geo.resolved) {
        const std::array<std::size_t,3> right{f.x,f.y,f.z};
        auto left=right;--left[f.axis];
        const auto i=(left[2]*n+left[1])*n+left[0];
        const auto j=(right[2]*n+right[1])*n+right[0];
        const bool leftwet=s.fill[i]>.5;
        const int direction=leftwet?1:-1;
        const V wet=leftwet?
            V{double(left[0])+.5,double(left[1])+.5,double(left[2])+.5}:
            V{double(right[0])+.5,double(right[1])+.5,double(right[2])+.5};
        const double expected=(cut-dot(normal,wet))/(normal[f.axis]*direction);
        near(f.theta,expected,2e-7,"independent analytic pressure cut");
    }
    upc::MacProjection3D mac(n,n,n,1.,1000.);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        mac.set_cell(x,y,z,s.fill[(z*n+y)*n+x]>.5);
    upc::p2_apply_planar_vof_pcg_geometry3d(mac,geo);
    for(const auto& f:geo.resolved) {
        if(f.axis==0)mac.set_face_velocity_x(f.x,f.y,f.z,.05);
        else if(f.axis==1)mac.set_face_velocity_y(f.x,f.y,f.z,.05);
        else mac.set_face_velocity_z(f.x,f.y,f.z,.05);
    }
    const auto pressure=mac.project_pcg_verified(.1,3000,1e-7,1e-10);
    check(pressure.max_divergence_s_inv<1e-7,
          "fitted 3D PLIC geometry in native MAC PCG");
    check(pressure.liquid_cells>0,"fitted pressure wet cells");
    check(s.fill==before,"read-only VOF fit");
    std::cout<<"PLANE n="<<n<<" cells="<<fit.fitted_cells
             <<" faces="<<geo.resolved.size()
             <<" min_alignment="<<min_alignment
             <<" max_fraction_error="<<fit.worst_patch_residual
             <<" iterations="<<fit.iterations<<"\n";
}
static void negatives() {
    const int n=8;
    const V v{1./std::sqrt(2.),1./std::sqrt(2.),0};
    auto s=make(n,v,6.17/std::sqrt(2.));
    // Perturb a real partial cell; the volume fractions can no longer
    // describe the same plane in the local neighborhood.
    for(std::size_t i=0;i<s.fill.size();++i)if(s.fill[i]>.2&&s.fill[i]<.8) {
        s.fill[i]=std::min(1.,s.fill[i]+.08);break;
    }
    reject([&]{(void)upc::p2_fit_local_planar_vof_normals3d(
        s.fill,s.solid,n,n,n,1.,1e-9);},"nonplanar measured patch rejected");
    s=make(n,v,6.17/std::sqrt(2.));
    reject([&]{(void)upc::p2_fit_local_planar_vof_normals3d(
        s.fill,s.solid,n,n,n,1.,0);},"zero tolerance rejected");
    reject([&]{(void)upc::p2_fit_local_planar_vof_normals3d(
        s.fill,s.solid,n,n,n,1.,std::numeric_limits<double>::quiet_NaN());},
        "nan tolerance rejected");
    s.fill[0]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{(void)upc::p2_fit_local_planar_vof_normals3d(
        s.fill,s.solid,n,n,n,1.);},"nan fraction rejected");
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        for(int n:{8,12}){
            exact(n,{1./std::sqrt(2.),1./std::sqrt(2.),0.},
                  (.75*n+.17)/std::sqrt(2.));
            exact(n,{1./std::sqrt(3.),1./std::sqrt(3.),1./std::sqrt(3.)},
                  (.9*n+.01)/std::sqrt(3.));
            exact(n,{.8/std::sqrt(.98),.5/std::sqrt(.98),-.3/std::sqrt(.98)},
                  (double(n)*.5+.17)/std::sqrt(.98));
        }
        negatives();
        std::cout<<"PASS P2_LOCAL_PLANAR_NORMAL_FIT assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_LOCAL_PLANAR_NORMAL_FIT assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
