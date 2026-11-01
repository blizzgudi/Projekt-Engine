// P2 partial: geometrically located p=0 liquid/air interface in real
// 3D MAC-PCG projection. Does not model air stress or surface tension.
#include "upc/mac3d.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace {
int assertions=0;
void require(bool ok,const char* msg){
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double eps,const char* msg){
    if(!std::isfinite(a)||std::abs(a-b)>eps){
        std::cerr<<"DIAGNOSTIC "<<msg<<" actual="<<a<<" expected="<<b<<" eps="<<eps<<"\n";
    }
    require(std::isfinite(a)&&std::abs(a-b)<=eps,msg);
}
template<class F>void rejects(F f){
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    require(threw,"invalid interface geometry must reject");
}
void slab(int axis,int n,double theta,bool reverse=false){
    constexpr double rho=1000,dt=0.1,dx=1;
    upc::MacProjection3D fluid(n,n,n,dx,rho);
    // Two-cell-deep liquid slab, adjacent to an internal air interface.
    const int first=reverse?n-2:0, last=reverse?n:2;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        int a=axis==0?x:axis==1?y:z;
        fluid.set_cell(x,y,z,a>=first&&a<last);
    }
    const int face=reverse?n-2:2;
    const double velocity=reverse?-1.0:1.0;
    for(int b=0;b<n;++b)for(int a=0;a<n;++a){
        if(axis==0){
            fluid.set_face_velocity_x(face,a,b,velocity);
            fluid.set_interface_distance_x(face,a,b,theta);
        }else if(axis==1){
            fluid.set_face_velocity_y(a,face,b,velocity);
            fluid.set_interface_distance_y(a,face,b,theta);
        }else{
            fluid.set_face_velocity_z(a,b,face,velocity);
            fluid.set_interface_distance_z(a,b,face,theta);
        }
    }
    const auto audit=fluid.project_pcg_verified(dt,1000,1e-9,1e-12);
    require(audit.liquid_cells==(std::size_t)(2*n*n),"two wet slabs");
    require(audit.closed_neumann_components==0,"air-Dirichlet interface");
    near(audit.max_divergence_s_inv,0,1e-9,"actual wet divergence");
    const double expected=-rho*dx/dt*theta;
    for(int b=0;b<n;++b)for(int a=0;a<n;++a){
        const int inner=reverse?n-2:1;
        const int outer=reverse?n-1:0;
        if(axis==0){
            near(fluid.pressure_pa(inner,a,b),expected,1e-6,"x surface pressure");
            near(fluid.pressure_pa(outer,a,b),expected,1e-6,"x inner pressure");
            near(fluid.face_velocity_x(face,a,b),0,1e-10,"x interface flux");
        }else if(axis==1){
            near(fluid.pressure_pa(a,inner,b),expected,1e-6,"y surface pressure");
            near(fluid.pressure_pa(a,outer,b),expected,1e-6,"y inner pressure");
            near(fluid.face_velocity_y(a,face,b),0,1e-10,"y interface flux");
        }else{
            near(fluid.pressure_pa(a,b,inner),expected,1e-6,"z surface pressure");
            near(fluid.pressure_pa(a,b,outer),expected,1e-6,"z inner pressure");
            near(fluid.face_velocity_z(a,b,face),0,1e-10,"z interface flux");
        }
    }
    std::cout<<"FRACTIONAL_INTERFACE axis="<<axis<<" n="<<n
             <<" theta="<<theta<<" reverse="<<reverse
             <<" p_Pa="<<expected<<" iterations="<<audit.iterations<<"\n";
}
void legacy_and_validation(){
    upc::MacProjection3D a(5,5,5,1),b(5,5,5,1);
    for(int z=0;z<5;++z)for(int y=0;y<5;++y)for(int x=0;x<5;++x){
        const bool wet=x<2;
        a.set_cell(x,y,z,wet);
        b.set_cell(x,y,z,wet);
    }
    for(int z=0;z<5;++z)for(int y=0;y<5;++y){
        a.set_face_velocity_x(2,y,z,1);
        b.set_face_velocity_x(2,y,z,1);
        b.set_interface_distance_x(2,y,z,.5);
        b.set_interface_distance_x(1,y,z,.3); // liquid/liquid: ignored
    }
    a.project_pcg_verified(.1,1000,1e-9,1e-12);
    b.project_pcg_verified(.1,1000,1e-9,1e-12);
    for(int z=0;z<5;++z)for(int y=0;y<5;++y){
        near(a.pressure_pa(1,y,z),b.pressure_pa(1,y,z),0,
             "default half-cell equals explicit half-cell bitwise");
        near(a.face_velocity_x(2,y,z),b.face_velocity_x(2,y,z),0,
             "internal liquid-liquid distance ignored");
    }
    rejects([&]{b.set_interface_distance_x(0,0,0,.5);});
    rejects([&]{b.set_interface_distance_x(5,0,0,.5);});
    rejects([&]{b.set_interface_distance_y(0,0,0,.5);});
    rejects([&]{b.set_interface_distance_z(0,0,0,.5);});
    rejects([&]{b.set_interface_distance_x(2,0,0,0);});
    rejects([&]{b.set_interface_distance_y(0,2,0,-.2);});
    rejects([&]{b.set_interface_distance_z(0,0,2,1.01);});
    rejects([&]{b.set_interface_distance_x(2,0,0,
                std::numeric_limits<double>::quiet_NaN());});
    rejects([&]{b.set_interface_distance_y(0,2,0,
                std::numeric_limits<double>::infinity());});
}
}
int main(){
    try{
        for(int axis=0;axis<3;++axis){
            for(int n:{5,9}){
                for(double theta:{.25,.5,.75})slab(axis,n,theta);
            }
            slab(axis,5,.25,true);
        }
        legacy_and_validation();
        std::cout<<"PASS P2_FRACTIONAL_FREE_SURFACE_PCG assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_FRACTIONAL_FREE_SURFACE_PCG "<<e.what()<<"\n";
        return 1;
    }
}
