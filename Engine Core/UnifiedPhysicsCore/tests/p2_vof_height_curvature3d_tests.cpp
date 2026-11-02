// P2 PARTIAL: native independent 3D height-function geometry verification.
// Manufactured column fractions, not true curved-cell volume integration.
#include "upc/p2_vof_height_curvature3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int checks=0;
void check(bool ok,const char* why){
    ++checks;
    if(!ok)throw std::runtime_error(why);
}
template<class F> void rejects(F f,const char* why){
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    check(threw,why);
}
std::vector<double> manufactured(int n,int axis,bool spherical,double offset=0){
    const double dx=1.2/n;
    std::vector<double> out((std::size_t)n*n*n);
    const int u=(axis+1)%3,v=(axis+2)%3;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        const int c[3]={x,y,z};
        const double U=(c[u]+.5)*dx-.6;
        const double V=(c[v]+.5)*dx-.6;
        const double height=spherical?.05+std::sqrt(1-U*U-V*V):
                                       .63+.1*U-.12*V+offset;
        const double a=std::clamp((height-c[axis]*dx)/dx,0.,1.);
        out[((std::size_t)z*n+y)*n+x]=a;
    }
    return out;
}
void positive(){
    std::array<double,3> error{};
    for(int g=0;g<3;++g){
        const int n=12*(1<<g);
        const double dx=1.2/n;
        for(int axis=0;axis<3;++axis){
            const auto alpha=manufactured(n,axis,true);
            const auto s=upc::P2VofHeightCurvature3D::sample(
                n,n,n,dx,alpha,axis,n/2-1,n/2-1);
            const double e=std::abs(s.curvature_inv_m-2.);
            if(axis==2)error[g]=e;
            check(s.locally_single_valued&&!s.moving_interface_validated&&
                  !s.full_p2_accepted,"Must not claim moving/full validation");
            check(e<.007,"Manufactured spherical cap curvature mismatch");
            check(std::abs(s.interface_coordinate_m-
                (.05+std::sqrt(1-dx*dx/2)))<2e-12,
                "Column volume integral must reconstruct interface height");
            double norm2=0;
            for(double a:s.water_to_air_normal)norm2+=a*a;
            check(std::abs(norm2-1)<1e-12,"Unit 3D interface normal");
            check(s.water_to_air_normal[axis]>0,
                  "Normal points from water toward air");
            std::cout<<std::setprecision(17)
                     <<"HEIGHT_CURVATURE axis="<<axis<<" grid="<<n
                     <<" dx_m="<<dx<<" curvature_inv_m="<<s.curvature_inv_m
                     <<" reference_inv_m=2 error_inv_m="<<e
                     <<" phase_complete=false\n";
        }
    }
    check(error[1]<error[0]/3&&error[2]<error[1]/3,
          "Independent 3-grid sphere curvature must converge");
    for(int axis=0;axis<3;++axis){
        const int n=24;const double dx=1.2/n;
        const auto a=manufactured(n,axis,false);
        const auto s=upc::P2VofHeightCurvature3D::sample(
            n,n,n,dx,a,axis,n/2-1,n/2-1);
        check(std::abs(s.curvature_inv_m)<1e-10,
              "Oblique 3D planar interface must have zero curvature");
        const int u=(axis+1)%3,v=(axis+2)%3;
        check(std::abs(s.water_to_air_normal[axis]-
              1/std::sqrt(1+.1*.1+.12*.12))<1e-12,
              "Oblique plane normal magnitude");
        check(std::abs(s.water_to_air_normal[u]+
              .1/std::sqrt(1+.1*.1+.12*.12))<1e-12,
              "Oblique plane u normal");
        check(std::abs(s.water_to_air_normal[v]-
              .12/std::sqrt(1+.1*.1+.12*.12))<1e-12,
              "Oblique plane v normal");
        std::cout<<std::setprecision(17)
                 <<"HEIGHT_PLANE axis="<<axis<<" grid="<<n
                 <<" curvature_inv_m="<<s.curvature_inv_m
                 <<" phase_complete=false\n";
    }
}
void negative(){
    constexpr int n=12;
    constexpr double dx=.1;
    const auto a=manufactured(n,2,false);
    const auto call=[&](const std::vector<double>& f,int axis=2,
                         int u=5,int v=5,double h=.1){
        return upc::P2VofHeightCurvature3D::sample(
            n,n,n,h,f,axis,u,v);
    };
    rejects([&]{call(a,3);},"Bad normal axis");
    rejects([&]{call(a,2,0,5);},"Stencil at edge");
    rejects([&]{call(a,2,11,5);},"Stencil past edge");
    rejects([&]{call(a,2,5,5,0);},"Zero dx");
    rejects([&]{call(a,2,5,5,std::numeric_limits<double>::infinity());},
            "Infinite dx");
    auto bad=a;bad.pop_back();
    rejects([&]{call(bad);},"Truncated 3D VOF");
    bad=a;bad[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{call(bad);},"Nonfinite alpha");
    bad=a;bad[0]=1.1;
    rejects([&]{call(bad);},"Out-of-range alpha");
    const auto idx=[n](int x,int y,int z){return (z*n+y)*n+x;};
    bad=a;bad[idx(5,5,0)]=.5;
    rejects([&]{call(bad);},"Water not full at lower boundary");
    bad=a;bad[idx(5,5,11)]=.1;
    rejects([&]{call(bad);},"Water at upper boundary");
    bad=a;bad[idx(5,5,5)]=.8;
    rejects([&]{call(bad);},"Two mixed cells in one column");
    bad=a;bad[idx(5,5,4)]=0;
    rejects([&]{call(bad);},"Non-monotone or overhanging column");
    // A height exactly on a cell boundary has no fractional centre cut.
    // Force one exact plane at z=0.6 m for every column.
    bad.assign((std::size_t)n*n*n,0);
    for(int z=0;z<6;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        bad[idx(x,y,z)]=1;
    rejects([&]{call(bad);},"No mixed centre cut must reject");
}
}
int main(){
    try{
        positive();
        negative();
        std::cout<<"PASS P2_VOF_HEIGHT_CURVATURE assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_VOF_HEIGHT_CURVATURE "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
