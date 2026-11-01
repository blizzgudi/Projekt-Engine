// P2 partial: manufactured 3D viscous normal-stress jump at SDF/MAC-PCG.
#include "upc/p2_sdf_normal_stress3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}check(caught,"invalid input accepted");}
constexpr upc::P2GradientTensor3D water={.3,.4,.25,-.1,-.2,.08,.15,-.12,.1};
constexpr upc::P2GradientTensor3D air={-.1,.03,-.2,.06,.05,.1,.02,-.07,-.04};
constexpr double mu_water=2,mu_air=.5,sigma=.072,p_air=120;
double strain(const upc::P2GradientTensor3D& g,const std::array<double,3>& n){
    double s=0;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)
        s+=n[i]*n[j]*(g[i*3+j]+g[j*3+i])*.5;
    return s;
}
struct Sphere {
    upc::MacProjection3D mac;
    std::vector<double> phi;
    Sphere(int n,double h,bool plane):mac(n,n,n,h),phi((std::size_t)n*n*n){
        double c=n*h/2;
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
            const double a=(x+.5)*h-c,b=(y+.5)*h-c,d=(z+.5)*h-c;
            const double signed_distance=(plane?a-.13:std::sqrt(a*a+b*b+d*d)-.287);
            phi[((std::size_t)z*n+y)*n+x]=signed_distance;
            mac.set_cell(x,y,z,signed_distance<0);
        }
    }
};
void plane(){
    constexpr int n=12;constexpr double h=.08;
    Sphere s(n,h,true);
    const auto result=upc::p2_apply_sdf_normal_stress3d(
        s.mac,n,n,n,h,s.phi,sigma,p_air,mu_water,mu_air,water,air);
    check(result.faces.size()==144,"planar interface face count");
    const double expected_jump=2*(mu_water*.3-mu_air*(-.1));
    for(const auto& f:result.faces){
        check(f.face.axis==0,"planar normal axis");
        check(std::abs(f.outward_normal[0]-1)<1e-12,"outward normal x");
        check(std::abs(f.outward_normal[1])<1e-12&&
              std::abs(f.outward_normal[2])<1e-12,"planar transverse normals");
        check(std::abs(f.viscous_normal_jump_pa-expected_jump)<1e-12,
              "normal viscous jump independent of tangential shear");
        check(std::abs(f.face.pressure_pa-(p_air+expected_jump))<1e-10,
              "planar pressure jump");
    }
    const auto audit=s.mac.project_pcg_verified(.01,1000,1e-9,1e-12);
    check(audit.max_divergence_s_inv<1e-9,"normal-stress PCG divergence");
    double max_speed=0,max_pressure_error=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        max_speed=std::max(max_speed,std::abs(s.mac.face_velocity_x(x,y,z)));
        if(s.phi[((std::size_t)z*n+y)*n+x]<0)
            max_pressure_error=std::max(max_pressure_error,
                std::abs(s.mac.pressure_pa(x,y,z)-(p_air+expected_jump)));
    }
    check(max_speed<1e-9&&max_pressure_error<1e-7,"static planar equilibrium");
    std::cout<<"NORMAL_STRESS_PLANE faces="<<result.faces.size()
             <<" viscous_jump_pa="<<expected_jump
             <<" max_speed_m_s="<<max_speed
             <<" max_pressure_error_pa="<<max_pressure_error
             <<" divergence_s_inv="<<audit.max_divergence_s_inv<<"\n";
}
void sphere(int n,double h){
    Sphere s(n,h,false);
    const auto baseline=upc::p2_sdf_capillary_faces3d(n,n,n,h,s.phi,sigma,p_air);
    const auto result=upc::p2_apply_sdf_normal_stress3d(
        s.mac,n,n,n,h,s.phi,sigma,p_air,mu_water,mu_air,water,air);
    check(result.faces.size()==baseline.faces.size(),"same SDF cut faces");
    auto skew=water;
    skew[1]+=.8;skew[3]-=.8;skew[2]-=.5;skew[6]+=.5;
    const auto skew_result=upc::p2_sdf_normal_stress_faces3d(
        n,n,n,h,s.phi,sigma,p_air,mu_water,mu_air,skew,air);
    const auto shifted=upc::p2_sdf_normal_stress_faces3d(
        n,n,n,h,s.phi,sigma,p_air+100,mu_water,mu_air,water,air);
    double max_analytic_error=0,max_skew_error=0;
    for(std::size_t i=0;i<result.faces.size();++i){
        const auto& f=result.faces[i];
        const auto& b=baseline.faces[i];
        const auto& nvec=f.outward_normal;
        double norm=0;
        for(double a:nvec)norm+=a*a;
        check(std::abs(norm-1)<1e-12,"unit normal");
        const double independent_jump=2*(mu_water*strain(water,nvec)-
                                         mu_air*strain(air,nvec));
        check(std::abs(f.viscous_normal_jump_pa-independent_jump)<1e-10,
              "symmetric 3D normal traction");
        check(std::abs(f.face.pressure_pa-b.pressure_pa-independent_jump)<1e-10,
              "Young-Laplace plus viscous normal stress");
        max_skew_error=std::max(max_skew_error,
            std::abs(f.face.pressure_pa-skew_result.faces[i].face.pressure_pa));
        check(std::abs(shifted.faces[i].face.pressure_pa-f.face.pressure_pa-100)<1e-10,
              "air pressure gauge shift");
        const auto& face=f.face;
        double xyz[3]={(face.x+.5)*h-n*h/2,
                       (face.y+.5)*h-n*h/2,
                       (face.z+.5)*h-n*h/2};
        const int axis=face.axis;
        const std::size_t lidx=(axis==0?face.x-1:face.x);
        const std::size_t lidy=(axis==1?face.y-1:face.y);
        const std::size_t lidz=(axis==2?face.z-1:face.z);
        const bool leftwet=s.phi[(lidz*n+lidy)*n+lidx]<0;
        xyz[axis]=(face.axis==0?face.x-.5:face.axis==1?face.y-.5:face.z-.5)*h-
                  n*h/2+(leftwet?face.theta:1-face.theta)*h;
        double r=std::sqrt(xyz[0]*xyz[0]+xyz[1]*xyz[1]+xyz[2]*xyz[2]);
        std::array<double,3> exact={xyz[0]/r,xyz[1]/r,xyz[2]/r};
        const double expected=2*(mu_water*strain(water,exact)-
                                 mu_air*strain(air,exact));
        max_analytic_error=std::max(max_analytic_error,
                                    std::abs(expected-f.viscous_normal_jump_pa));
    }
    check(max_skew_error<1e-11,"antisymmetric rotation cannot change normal stress");
    check(max_analytic_error<.07,"sphere normal stress agrees with radial reference");
    std::cout<<"NORMAL_STRESS_SPHERE dx="<<h<<" faces="<<result.faces.size()
             <<" max_radial_jump_error_pa="<<max_analytic_error
             <<" max_skew_error_pa="<<max_skew_error<<"\n";
}
void negative(){
    constexpr int n=12;constexpr double h=.08;
    Sphere s(n,h,true);
    auto bad=water;bad[0]=std::numeric_limits<double>::infinity();
    rejects([&]{upc::p2_apply_sdf_normal_stress3d(s.mac,n,n,n,h,s.phi,
                     sigma,p_air,mu_water,mu_air,bad,air);});
    rejects([&]{upc::p2_apply_sdf_normal_stress3d(s.mac,n,n,n,h,s.phi,
                     sigma,p_air,-1,mu_air,water,air);});
    rejects([&]{upc::p2_apply_sdf_normal_stress3d(s.mac,n,n,n,h,s.phi,
                     sigma,p_air,mu_water,std::numeric_limits<double>::quiet_NaN(),water,air);});
    rejects([&]{upc::p2_apply_sdf_normal_stress3d(s.mac,n,n,n,h*2,s.phi,
                     sigma,p_air,mu_water,mu_air,water,air);});
    auto mismatch=s.mac;mismatch.set_cell(0,0,0,false);
    rejects([&]{upc::p2_apply_sdf_normal_stress3d(mismatch,n,n,n,h,s.phi,
                     sigma,p_air,mu_water,mu_air,water,air);});
    check(s.mac.pressure_pa(0,0,0)==0,"rejected updates preserve MAC pressure");
    const auto base=upc::p2_sdf_capillary_faces3d(n,n,n,h,s.phi,sigma,p_air);
    const auto zero=upc::p2_sdf_normal_stress_faces3d(n,n,n,h,s.phi,
                     sigma,p_air,0,0,water,air);
    for(std::size_t i=0;i<base.faces.size();++i)
        check(zero.faces[i].face.pressure_pa==base.faces[i].pressure_pa,
              "zero-viscosity reduces exactly to capillary pressure");
    std::cout<<"NORMAL_STRESS_NEGATIVE rejected=5 zero_viscosity_faces="
             <<base.faces.size()<<"\n";
}
}
int main(){
    try{
        plane();sphere(12,.08);sphere(24,.04);negative();
        std::cout<<"PASS P2_SDF_NORMAL_STRESS assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_SDF_NORMAL_STRESS "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
