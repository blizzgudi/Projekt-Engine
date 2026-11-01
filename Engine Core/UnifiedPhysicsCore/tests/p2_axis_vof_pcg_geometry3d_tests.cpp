// Native P2 partial: VOF fractions reconstructed to actual 3D MAC
// pressure boundary distance for strictly axis-aligned planar cuts.
#include "upc/p2_axis_vof_pcg_geometry3d.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int checks=0;
void check(bool ok,const char* message) {
    ++checks;
    if(!ok)throw std::runtime_error(message);
}
void close(double a,double b,double tol,const char* message) {
    if(!std::isfinite(a)||std::abs(a-b)>tol)
        std::cerr<<"MISMATCH "<<message<<" actual="<<a<<" expected="<<b<<"\n";
    check(std::isfinite(a)&&std::abs(a-b)<=tol,message);
}
template<class F>void reject(F fn) {
    bool caught=false;
    try{fn();}catch(const std::exception&){caught=true;}
    check(caught,"unsupported VOF geometry must reject");
}
void slab(int axis,int n,bool reverse,bool partial_wet) {
    const double dx=1,dt=.1,rho=1000;
    upc::MacProjection3D mac(n,n,n,dx,rho);
    std::vector<double> fill(n*n*n,0);
    std::vector<std::uint8_t> solid(n*n*n,0);
    const int boundary=reverse?n-2:2;
    const int wet_partial=reverse?boundary:boundary-1;
    const int dry_partial=reverse?boundary-1:boundary;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const int a=axis==0?x:axis==1?y:z;
        double f=reverse?(a>=boundary?1.0:0.0):(a<boundary?1.0:0.0);
        if(partial_wet&&a==wet_partial)f=.75;
        if(!partial_wet&&a==dry_partial)f=.25;
        const auto i=(z*n+y)*n+x;
        fill[i]=f;
        mac.set_cell(x,y,z,f>.5);
    }
    auto geometry=upc::p2_axis_vof_pcg_geometry3d(fill,solid,n,n,n,dx);
    check(geometry.resolved.size()==std::size_t(n*n),"resolved 3D face count");
    check(geometry.unresolved_pure_faces==0,"all interface faces geometrically located");
    const double theta=partial_wet?.25:.75;
    for(const auto& face:geometry.resolved) {
        check(face.axis==axis,"correct face axis");
        close(face.theta,theta,1e-14,"geometric theta");
    }
    upc::p2_apply_axis_vof_pcg_geometry3d(mac,geometry);
    for(int b=0;b<n;++b)for(int a=0;a<n;++a) {
        const double u=reverse?-1:1;
        if(axis==0)mac.set_face_velocity_x(boundary,a,b,u);
        else if(axis==1)mac.set_face_velocity_y(a,boundary,b,u);
        else mac.set_face_velocity_z(a,b,boundary,u);
    }
    const auto audit=mac.project_pcg_verified(dt,1000,1e-9,1e-12);
    close(audit.max_divergence_s_inv,0,1e-9,"projected wet divergence");
    const double expected=-rho*dx/dt*theta;
    const int wet_cell=reverse?boundary:boundary-1;
    for(int b=0;b<n;++b)for(int a=0;a<n;++a) {
        if(axis==0) {
            close(mac.pressure_pa(wet_cell,a,b),expected,1e-6,"x pressure");
            close(mac.face_velocity_x(boundary,a,b),0,1e-10,"x face");
        } else if(axis==1) {
            close(mac.pressure_pa(a,wet_cell,b),expected,1e-6,"y pressure");
            close(mac.face_velocity_y(a,boundary,b),0,1e-10,"y face");
        } else {
            close(mac.pressure_pa(a,b,wet_cell),expected,1e-6,"z pressure");
            close(mac.face_velocity_z(a,b,boundary),0,1e-10,"z face");
        }
    }
    std::cout<<"VOF_PCG axis="<<axis<<" n="<<n
             <<" reverse="<<int(reverse)<<" wet_partial="<<int(partial_wet)
             <<" theta="<<theta<<" pressure_pa="<<expected
             <<" iterations="<<audit.iterations<<"\n";
}
void negative_cases() {
    constexpr int n=5;
    std::vector<double> f(n*n*n,0);
    std::vector<std::uint8_t> solid(f.size(),0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        f[(z*n+y)*n+x]=x<2?1:0;
    const auto pure=upc::p2_axis_vof_pcg_geometry3d(f,solid,n,n,n,1);
    check(pure.resolved.empty(),"pure step has no known interface location");
    check(pure.unresolved_pure_faces==n*n,"pure step unresolved count");
    reject([&]{upc::p2_axis_vof_pcg_geometry3d(f,solid,n,n,n,1,.1);});
    for(int z=0;z<n;++z)for(int y=0;y<n;++y) {
        f[(z*n+y)*n+1]=.75;
        f[(z*n+y)*n+2]=.25;
    }
    reject([&]{upc::p2_axis_vof_pcg_geometry3d(f,solid,n,n,n,1);});
    f[1]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{upc::p2_axis_vof_pcg_geometry3d(f,solid,n,n,n,1);});
    // Oblique local VOF gradient must not masquerade as an axis cut.
    std::fill(f.begin(),f.end(),0.0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        f[(z*n+y)*n+x]=x==0?1.0:(x==1?(y>=3?.9:.75):0.0);
    reject([&]{upc::p2_axis_vof_pcg_geometry3d(f,solid,n,n,n,1);});
    // Reuse the SAME MAC across frames. A pure/ambiguous frame must
    // clear old .25 distances and restore the default half-cell.
    upc::MacProjection3D mac(n,n,n,1,1000);
    upc::P2AxisVofPcgGeometry3D geom;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y) {
        for(int x=0;x<n;++x)mac.set_cell(x,y,z,x<2);
        mac.set_face_velocity_x(2,y,z,1);
        geom.resolved.push_back({0,2,(std::size_t)y,(std::size_t)z,.25});
    }
    upc::p2_apply_axis_vof_pcg_geometry3d(mac,geom);
    geom.resolved.front().theta=std::numeric_limits<double>::quiet_NaN();
    reject([&]{upc::p2_apply_axis_vof_pcg_geometry3d(mac,geom);});
    const auto first=mac.project_pcg_verified(.1,1000,1e-9,1e-12);
    close(first.max_divergence_s_inv,0,1e-9,"first-frame divergence");
    close(mac.pressure_pa(1,2,2),-2500,1e-6,"invalid geometry leaves previous frame intact");
    upc::p2_apply_axis_vof_pcg_geometry3d(mac,{});
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
        mac.set_face_velocity_x(2,y,z,1);
    const auto second=mac.project_pcg_verified(.1,1000,1e-9,1e-12);
    close(second.max_divergence_s_inv,0,1e-9,"second-frame divergence");
    close(mac.pressure_pa(1,2,2),-5000,1e-6,"old fractional geometry cleared");
    std::cout<<"VOF_PCG_RESET first_pa=-2500 second_pa=-5000\n";
}
}
int main(){
    try {
        for(int axis=0;axis<3;++axis)
            for(int n:{5,9})
                for(bool reverse:{false,true})
                    for(bool partial_wet:{false,true})
                        slab(axis,n,reverse,partial_wet);
        negative_cases();
        std::cout<<"PASS P2_AXIS_VOF_PCG assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_AXIS_VOF_PCG "<<e.what()<<"\n";
        return 1;
    }
}
