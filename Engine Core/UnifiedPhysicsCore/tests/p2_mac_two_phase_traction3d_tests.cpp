// P2 partial: independent manufactured 3D MAC phase gradients and stress jump.
#include "upc/p2_mac_two_phase_traction3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
int assertions=0;
void check(bool ok,const char* msg){++assertions;if(!ok)throw std::runtime_error(msg);}
template<class F>void rejects(F fn){bool caught=false;try{fn();}catch(const std::invalid_argument&){caught=true;}check(caught,"invalid stencil/mask accepted");}
struct Fixture {
    int n;double h;upc::MacProjection3D mac;std::vector<double> phi;
    Fixture(int count,double dx,int interface_cell):n(count),h(dx),mac(n,n,n,h),phi((std::size_t)n*n*n) {
        const double xi=interface_cell*h;
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
            const double p=(x+.5)*h-xi;
            phi[((std::size_t)z*n+y)*n+x]=p;
            mac.set_cell(x,y,z,p<0);
        }
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<=n;++x){
            const double yy=(y+.5)*h,zz=(z+.5)*h;
            mac.set_face_velocity_x(x,y,z,.03*yy+.04*zz);
        }
        for(int z=0;z<n;++z)for(int y=0;y<=n;++y)for(int x=0;x<n;++x){
            const double xx=(x+.5)*h,yy=y*h,zz=(z+.5)*h;
            const double slope=xx<xi?1.5:-.5;
            mac.set_face_velocity_y(x,y,z,slope*(xx-xi)+.1*yy+.2*zz);
        }
        for(int z=0;z<=n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
            const double xx=(x+.5)*h,yy=(y+.5)*h,zz=z*h;
            const double slope=xx<xi?-.4:.8;
            mac.set_face_velocity_z(x,y,z,slope*(xx-xi)+.07*yy-.1*zz);
        }
    }
};
constexpr std::array<double,3> target{0,3.295,-1.14};
void run(int n,double h){
    Fixture f(n,h,n/2);
    const auto r=upc::p2_mac_two_phase_traction3d(f.mac,f.phi,0,120,2,.5,target);
    check(r.faces.size()==(std::size_t)n*n,"planar cut face count");
    double max_res=0,max_grad=0,max_pressure=0;
    const std::array<double,9> gw{0,.03,.04,1.5,.1,.2,-.4,.07,-.1};
    const std::array<double,9> ga{0,.03,.04,-.5,.1,.2,.8,.07,-.1};
    for(const auto& face:r.faces){
        check(face.interface.face.axis==0,"interface x normal");
        check(std::abs(face.interface.outward_normal[0]-1)<1e-12,"normal orientation");
        for(int k=0;k<9;++k){
            max_grad=std::max(max_grad,std::abs(face.water_gradient[k]-gw[k]));
            max_grad=std::max(max_grad,std::abs(face.air_gradient[k]-ga[k]));
        }
        max_res=std::max(max_res,face.tangential_residual_norm_pa);
        max_pressure=std::max(max_pressure,std::abs(face.required_liquid_pressure_pa-120));
        check(std::abs(face.traction_jump_pa[1]-target[1])<1e-11,"y stress jump");
        check(std::abs(face.traction_jump_pa[2]-target[2])<1e-11,"z stress jump");
        check(std::abs(face.normal_viscous_jump_pa)<1e-11,"normal stress jump");
    }
    check(max_grad<1e-11,"3D gradient reference");
    check(max_res<1e-11,"two-phase Marangoni residual");
    check(max_pressure<1e-11,"normal pressure reference");
    const auto mismatch=upc::p2_mac_two_phase_traction3d(f.mac,f.phi,0,120,2,.5,{0,0,0});
    check(mismatch.faces.front().tangential_residual_norm_pa>3.0,"negative stress mismatch detected");
    auto invalid=f;
    invalid.mac.set_cell(n/2,0,0,true);
    rejects([&]{upc::p2_mac_two_phase_traction3d(invalid.mac,invalid.phi,0,120,2,.5,target);});
    auto thin=Fixture(n,h,1);
    rejects([&]{upc::p2_mac_two_phase_traction3d(thin.mac,thin.phi,0,120,2,.5,target);});
    rejects([&]{upc::p2_mac_two_phase_traction3d(f.mac,f.phi,0,120,2,.5,{0,INFINITY,0});});
    std::cout<<"MAC_TWO_PHASE n="<<n<<" faces="<<r.faces.size()
             <<" max_gradient_error="<<max_grad
             <<" max_tangential_residual_pa="<<max_res
             <<" max_normal_pressure_error_pa="<<max_pressure<<"\n";
}
}
int main(){
    try{run(12,.08);run(24,.04);
        std::cout<<"PASS: MAC_TWO_PHASE assertions="<<assertions<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
