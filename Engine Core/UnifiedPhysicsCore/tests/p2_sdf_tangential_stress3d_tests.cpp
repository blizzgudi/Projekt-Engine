// P2 partial: manufactured 3D SDF tangential traction / Marangoni matching.
#include "upc/p2_sdf_tangential_stress3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int assertions=0;
void check(bool ok,const char* why){++assertions;if(!ok)throw std::runtime_error(why);}
template<class F>void rejects(F fn){bool caught=false;try{fn();}catch(const std::invalid_argument&){caught=true;}check(caught,"invalid input accepted");}
constexpr upc::P2GradientTensor3D water={.3,.4,.25,-.1,-.2,.08,.15,-.12,.1};
constexpr upc::P2GradientTensor3D air={-.1,.03,-.2,.06,.05,.1,.02,-.07,-.04};
constexpr double mw=2,ma=.5,sigma=.072,pair=120;
constexpr std::array<double,3> grad_sigma={.11,.6,-.25};
struct Shape {
    int n;double h;std::vector<double> phi;
    Shape(int count,double dx,bool planar):n(count),h(dx),phi((std::size_t)n*n*n) {
        const double c=n*h/2;
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
            const double a=(x+.5)*h-c,b=(y+.5)*h-c,d=(z+.5)*h-c;
            phi[((std::size_t)z*n+y)*n+x]=planar?a-.13:std::sqrt(a*a+b*b+d*d)-.287;
        }
    }
};
double normal_strain(const upc::P2GradientTensor3D& g,const std::array<double,3>& n) {
    double s=0;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)
        s+=n[i]*n[j]*(g[3*i+j]+g[3*j+i])*.5;
    return s;
}
std::array<double,3> independent_tangential_traction(
    const upc::P2GradientTensor3D& w,const upc::P2GradientTensor3D& a,
    const std::array<double,3>& n,double mw_,double ma_) {
    std::array<double,3> t{};
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)
        t[i]+=(mw_*(w[3*i+j]+w[3*j+i])-
               ma_*(a[3*i+j]+a[3*j+i]))*n[j];
    double normal=0;for(int i=0;i<3;++i)normal+=t[i]*n[i];
    for(int i=0;i<3;++i)t[i]-=normal*n[i];
    return t;
}
void planar() {
    Shape s(12,.08,true);
    auto r=upc::p2_sdf_tangential_stress_faces3d(
        s.n,s.n,s.n,s.h,s.phi,sigma,pair,mw,ma,water,air,grad_sigma);
    check(r.faces.size()==144,"planar cut-face count");
    double max_res=0,max_normal_delta=0;
    for(const auto& f:r.faces) {
        const auto& n=f.normal_stress.outward_normal;
        check(std::abs(n[0]-1)<1e-12&&std::abs(n[1])<1e-12&&std::abs(n[2])<1e-12,
              "planar normal");
        check(std::abs(f.surface_gradient_pa[0])<1e-12&&
              std::abs(f.surface_gradient_pa[1]-.6)<1e-12&&
              std::abs(f.surface_gradient_pa[2]+.25)<1e-12,"surface projection");
        const auto before=independent_tangential_traction(water,air,n,mw,ma);
        const auto after=independent_tangential_traction(f.corrected_water_gradient,air,n,mw,ma);
        for(int i=0;i<3;++i){
            check(std::abs(before[i]-f.tangential_traction_before_pa[i])<1e-12,
                  "independent before traction");
            check(std::abs(after[i]-f.surface_gradient_pa[i])<1e-12,
                  "independent corrected traction");
        }
        const double delta=2*mw*(normal_strain(f.corrected_water_gradient,n)-
                                 normal_strain(water,n));
        max_normal_delta=std::max(max_normal_delta,std::abs(delta));
        max_res=std::max(max_res,f.residual_after_pa);
        check(std::abs(f.normal_stress.face.pressure_pa-(pair+1.3))<1e-10,
              "normal pressure unchanged");
    }
    check(max_res<1e-12&&max_normal_delta<1e-12,"planar stress residual");
    std::cout<<"TANGENTIAL_PLANE faces="<<r.faces.size()
             <<" before_pa="<<r.faces[0].residual_before_pa
             <<" after_pa="<<max_res
             <<" normal_delta_pa="<<max_normal_delta<<"\n";
}
void sphere(int n,double h) {
    Shape s(n,h,false);
    const auto r=upc::p2_sdf_tangential_stress_faces3d(
        n,n,n,h,s.phi,sigma,pair,mw,ma,water,air,grad_sigma);
    const auto baseline=upc::p2_sdf_normal_stress_faces3d(
        n,n,n,h,s.phi,sigma,pair,mw,ma,water,air);
    check(r.faces.size()==baseline.faces.size(),"sphere face count");
    double max_res=0,max_normal_delta=0,max_pressure_delta=0;
    for(std::size_t k=0;k<r.faces.size();++k){
        const auto& f=r.faces[k];
        const auto& v=f.normal_stress.outward_normal;
        const auto independently=independent_tangential_traction(
            f.corrected_water_gradient,air,v,mw,ma);
        for(int i=0;i<3;++i){
            check(std::abs(independently[i]-f.surface_gradient_pa[i])<1e-11,
                  "spherical corrected tangential traction");
            check(std::abs(f.tangential_traction_after_pa[i]-independently[i])<1e-11,
                  "spherical traction diagnostic");
        }
        const double normal_delta=2*mw*(normal_strain(f.corrected_water_gradient,v)-
                                        normal_strain(water,v));
        max_normal_delta=std::max(max_normal_delta,std::abs(normal_delta));
        max_res=std::max(max_res,f.residual_after_pa);
        max_pressure_delta=std::max(max_pressure_delta,
            std::abs(f.normal_stress.face.pressure_pa-baseline.faces[k].face.pressure_pa));
        check(f.residual_after_pa<=f.residual_before_pa+1e-12,
              "tangential residual cannot grow");
    }
    check(max_res<1e-11&&max_normal_delta<1e-11&&max_pressure_delta<1e-12,
          "spherical traction/normal-stress separation");
    std::cout<<"TANGENTIAL_SPHERE dx="<<h<<" faces="<<r.faces.size()
             <<" max_after_pa="<<max_res
             <<" max_normal_delta_pa="<<max_normal_delta
             <<" pressure_delta_pa="<<max_pressure_delta<<"\n";
}
void negative() {
    Shape s(12,.08,true);
    auto bad=water;bad[3]=std::numeric_limits<double>::infinity();
    rejects([&]{upc::p2_sdf_tangential_stress_faces3d(
        12,12,12,.08,s.phi,sigma,pair,mw,ma,bad,air,grad_sigma);});
    auto g=grad_sigma;g[1]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{upc::p2_sdf_tangential_stress_faces3d(
        12,12,12,.08,s.phi,sigma,pair,mw,ma,water,air,g);});
    rejects([&]{upc::p2_sdf_tangential_stress_faces3d(
        12,12,12,.08,s.phi,sigma,pair,0,0,water,air,grad_sigma);});
    rejects([&]{upc::p2_sdf_tangential_stress_faces3d(
        12,12,12,.08,s.phi,sigma,pair,-1,ma,water,air,grad_sigma);});
    rejects([&]{upc::p2_sdf_tangential_stress_faces3d(
        12,12,12,.08,std::vector<double>{},sigma,pair,mw,ma,water,air,grad_sigma);});
    upc::P2GradientTensor3D zero{};
    const auto no_shear=upc::p2_sdf_tangential_stress_faces3d(
        12,12,12,.08,s.phi,sigma,pair,0,0,zero,zero,{0,0,0});
    check(no_shear.faces.size()==144,"zero-shear degenerate valid");
    for(const auto& f:no_shear.faces)
        check(f.residual_after_pa==0,"zero shear residual");
    std::cout<<"TANGENTIAL_NEGATIVE rejected=5 zero_shear_faces="
             <<no_shear.faces.size()<<"\n";
}
}
int main(){
    try {
        planar();sphere(12,.08);sphere(24,.04);negative();
        std::cout<<"PASS P2_SDF_TANGENTIAL_STRESS assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_SDF_TANGENTIAL_STRESS "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
