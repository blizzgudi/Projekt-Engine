#include "upc/p2_frozen_wrench_mac_reaction3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t checks=0;
void check(bool v,const char* why) {
    ++checks;
    if(!v)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
template<class F>void rejects(F f,const char* why) {
    bool bad=false;
    try {f();}catch(const std::exception&){bad=true;}
    check(bad,why);
}
std::vector<double> pressure(int n) {
    std::vector<double> p(static_cast<std::size_t>(n)*n*n);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const double X=x+0.5,Y=y+0.5,Z=z+0.5;
        p[(static_cast<std::size_t>(z)*n+y)*n+x]=
            100+3*X-2*Y+4*Z+0.25*std::pow(X-4.5,3)*(Y-4.5);
    }
    return p;
}
struct Scene {
    upc::MacProjection3D mac;
    std::vector<std::uint8_t> wet,solid,body;
    Scene(int n,double dx,bool seeded):mac(n,n,n,dx,1000),
      wet(static_cast<std::size_t>(n)*n*n,0),
      solid(wet.size(),0),body(wet.size(),0) {
        const auto idx=[&](int x,int y,int z){
            return (static_cast<std::size_t>(z)*n+y)*n+x;};
        for(int z=0;z<n;++z)
        for(int y=0;y<n;++y)
        for(int x=0;x<n;++x) {
            const bool b=x>=4&&x<=5&&y>=4&&y<=5&&z>=4&&z<=5;
            body[idx(x,y,z)]=b;
            solid[idx(x,y,z)]=b;
            wet[idx(x,y,z)]=!b;
            mac.set_cell(x,y,z,!b,b);
        }
        for(int a=0;a<3;++a)
        for(int z=0;z<n+(a==2);++z)
        for(int y=0;y<n+(a==1);++y)
        for(int x=0;x<n+(a==0);++x) {
            if((a==0&&(x==0||x==n))||
               (a==1&&(y==0||y==n))||
               (a==2&&(z==0||z==n)))continue;
            const int ax=x-(a==0),ay=y-(a==1),az=z-(a==2);
            if(!wet[idx(ax,ay,az)]||!wet[idx(x,y,z)])continue;
            const double v=seeded?(0.01*(a+1)+0.0005*(x-2*y+z)):0.0;
            if(a==0)mac.set_face_velocity_x(x,y,z,v);
            if(a==1)mac.set_face_velocity_y(x,y,z,v);
            if(a==2)mac.set_face_velocity_z(x,y,z,v);
        }
    }
};
double face(const upc::MacProjection3D& mac,int a,int x,int y,int z) {
    return a==0?mac.face_velocity_x(x,y,z):
           a==1?mac.face_velocity_y(x,y,z):
                mac.face_velocity_z(x,y,z);
}
void run(int n,double dx,int sign,bool seeded) {
    using Step=upc::P2FrozenWrenchMacReaction3D;
    using Box=upc::P2Box3D;
    const double dt=0.05,rho=1000.0,mass=rho*dx*dx*dx;
    const Box old{{3.2*dx,3.1*dx,3.3*dx},{5.7*dx,5.6*dx,5.8*dx}};
    Box next=old;
    for(int a=0;a<3;++a) {
        const double d=sign*(a==0?0.11:(a==1?-0.08:0.09))*dx;
        next.min_m[a]+=d;next.max_m[a]+=d;
    }
    upc::RigidPressureState3D body;
    body.mass_kg=12;
    body.inertia_world_kg_m2={{0.45,0.63,0.78}};
    body.velocity_m_s={{0.15,-0.07,0.08}};
    body.angular_velocity_rad_s={{0.2,-0.1,0.15}};
    for(int a=0;a<3;++a)
        body.centre_m[a]=0.5*(old.min_m[a]+old.max_m[a]);
    const std::array<double,3> ref{{3.4*dx,4.9*dx,4.1*dx}};
    auto p=pressure(n);
    Scene scene(n,dx,seeded);
    const auto before=scene.mac;
    const auto result=Step::step(
        scene.mac,n,n,n,dx,dt,rho,old,next,p,ref,body,
        scene.wet,scene.solid,scene.body);
    std::array<double,6> impulse{};
    double energy=0,max_delta=0;
    std::array<std::size_t,3> changed{};
    const auto idx=[&](int x,int y,int z){
        return (static_cast<std::size_t>(z)*n+y)*n+x;};
    for(int a=0;a<3;++a)
    for(int z=0;z<n+(a==2);++z)
    for(int y=0;y<n+(a==1);++y)
    for(int x=0;x<n+(a==0);++x) {
        const double v0=face(before,a,x,y,z);
        const double v1=face(scene.mac,a,x,y,z);
        const double dv=v1-v0;
        const bool interior=(a==0?x>0&&x<n:a==1?y>0&&y<n:z>0&&z<n);
        const int ax=x-(a==0),ay=y-(a==1),az=z-(a==2);
        const bool allowed=interior&&scene.wet[idx(ax,ay,az)]&&
                           scene.wet[idx(x,y,z)];
        if(!allowed) {
            near(dv,0,0,"unchanged solid/exterior MAC face");
            continue;
        }
        if(dv!=0)++changed[a];
        const double j=mass*dv;
        impulse[a]+=j;
        const std::array<double,3> r{{
            (x+(a!=0?0.5:0))*dx-body.centre_m[0],
            (y+(a!=1?0.5:0))*dx-body.centre_m[1],
            (z+(a!=2?0.5:0))*dx-body.centre_m[2]}};
        impulse[3+(a+1)%3]+=r[(a+2)%3]*j;
        impulse[3+(a+2)%3]-=r[(a+1)%3]*j;
        energy+=0.5*mass*(v1*v1-v0*v0);
        max_delta=std::max(max_delta,std::abs(dv));
    }
    for(int a=0;a<3;++a) {
        near(result.applied_fluid_linear_Ns[a],impulse[a],1e-11,
             "independent linear face impulse");
        near(result.applied_fluid_angular_Nms[a],impulse[a+3],1e-11,
             "independent angular face impulse");
        near(result.body_plus_fluid_linear_residual_Ns[a],0,1e-10,
             "linear body-fluid reaction closes");
        near(result.body_plus_fluid_angular_residual_Nms[a],0,1e-10,
             "angular body-fluid reaction closes");
        check(result.selected_faces[a]>0,"selected wet-wet faces");
        check(changed[a]==result.selected_faces[a],
              "all selected MAC face velocities modified");
    }
    near(result.fluid_face_energy_delta_proxy_J,energy,1e-10,
         "independent face kinetic energy change");
    near(result.max_abs_face_delta_m_s,max_delta,1e-11,
         "independent largest MAC update");
    near(result.max_divergence_after_s_inv,scene.mac.max_divergence(),1e-12,
         "divergence is measured not silently projected");
    check(result.max_divergence_after_s_inv>1e-6,
          "unprojected reaction has nonzero divergence");
    check(std::abs(result.rigid.response.body_angular_impulse_Nms[2])>1e-8,
          "nontrivial body angular impulse");
    const auto saved=scene.mac;
    Scene replay(n,dx,seeded);
    const auto rr=Step::step(replay.mac,n,n,n,dx,dt,rho,old,next,p,ref,body,
                            replay.wet,replay.solid,replay.body);
    for(int a=0;a<3;++a)
    for(int z=0;z<n+(a==2);++z)
    for(int y=0;y<n+(a==1);++y)
    for(int x=0;x<n+(a==0);++x)
        near(face(replay.mac,a,x,y,z),face(saved,a,x,y,z),0,
             "deterministic MAC replay");
    near(rr.fluid_face_energy_delta_proxy_J,
         result.fluid_face_energy_delta_proxy_J,0,"deterministic energy");
    // P2 partial: actually re-project the frozen 6-DOF MAC reaction.
    // Audit the pressure-projection correction separately: the original
    // six-DOF face impulse ledger is NOT a post-projection conservation proof.
    auto projected=saved;
    projected.project(dt,3000);
    const double postdiv=projected.max_divergence();
    check(std::isfinite(postdiv)&&postdiv<result.max_divergence_after_s_inv*0.01,
          "pressure projection must reduce reaction divergence by >=100x");
    std::array<double,6> projection_impulse{},total_impulse{};
    double projection_energy=0.0;
    double independent_postdiv=0.0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        if(!scene.wet[idx(x,y,z)])continue;
        const double d=(face(projected,0,x+1,y,z)-face(projected,0,x,y,z)
                       +face(projected,1,x,y+1,z)-face(projected,1,x,y,z)
                       +face(projected,2,x,y,z+1)-face(projected,2,x,y,z))/dx;
        independent_postdiv=std::max(independent_postdiv,std::abs(d));
    }
    near(postdiv,independent_postdiv,1e-12,"independent post-projection divergence");
    for(int a=0;a<3;++a)
    for(int z=0;z<n+(a==2);++z)
    for(int y=0;y<n+(a==1);++y)
    for(int x=0;x<n+(a==0);++x) {
        const double v0=face(before,a,x,y,z);
        const double v1=face(saved,a,x,y,z);
        const double v2=face(projected,a,x,y,z);
        const double jp=mass*(v2-v1),jt=mass*(v2-v0);
        projection_impulse[a]+=jp;
        total_impulse[a]+=jt;
        const std::array<double,3> r{{
            (x+(a!=0?0.5:0))*dx-body.centre_m[0],
            (y+(a!=1?0.5:0))*dx-body.centre_m[1],
            (z+(a!=2?0.5:0))*dx-body.centre_m[2]}};
        projection_impulse[3+(a+1)%3]+=r[(a+2)%3]*jp;
        projection_impulse[3+(a+2)%3]-=r[(a+1)%3]*jp;
        total_impulse[3+(a+1)%3]+=r[(a+2)%3]*jt;
        total_impulse[3+(a+2)%3]-=r[(a+1)%3]*jt;
        projection_energy+=0.5*mass*(v2*v2-v1*v1);
        check(std::isfinite(v2),"finite projected MAC face");
    }
    for(int i=0;i<6;++i) {
        const double original=i<3?result.applied_fluid_linear_Ns[i]:
                                  result.applied_fluid_angular_Nms[i-3];
        near(total_impulse[i],original+projection_impulse[i],1e-10,
             "projection changes body-plus-fluid impulse ledger by measured correction");
    }
    check(std::isfinite(projection_energy),"finite pressure projection face energy");
    double correction_norm=0.0;
    for(double j:projection_impulse)correction_norm+=j*j;
    check(correction_norm>1e-14,"projection must modify frozen face wrench");
    if(n==8&&sign==1&&!seeded) {
        // Rejection must be atomic, even for rank-deficient geometry.
        auto empty=scene.body;
        std::fill(empty.begin(),empty.end(),0);
        auto oldmac=before;
        rejects([&]{Step::step(oldmac,n,n,n,dx,dt,rho,old,next,p,ref,body,
                               scene.wet,scene.solid,empty);},
                "missing body rejected");
        near(oldmac.face_velocity_x(3,3,3),before.face_velocity_x(3,3,3),0,
             "missing body rollback");
        auto isolated_wet=scene.wet;
        std::fill(isolated_wet.begin(),isolated_wet.end(),0);
        isolated_wet[idx(3,4,4)]=1;
        oldmac=before;
        rejects([&]{Step::step(oldmac,n,n,n,dx,dt,rho,old,next,p,ref,body,
                               isolated_wet,scene.solid,scene.body);},
                "rank-deficient MAC transfer rejected");
        near(oldmac.face_velocity_x(3,4,4),before.face_velocity_x(3,4,4),0,
             "rank-deficient rollback");
        oldmac=before;
        rejects([&]{Step::step(oldmac,n,n,n,dx,dt,-rho,old,next,p,ref,body,
                               scene.wet,scene.solid,scene.body);},
                "negative density rejected");
        near(oldmac.face_velocity_x(3,3,3),before.face_velocity_x(3,3,3),0,
             "invalid density rollback");
    }
    std::cout<<std::setprecision(17)<<"CASE n="<<n<<" dx="<<dx
             <<" sign="<<sign<<" seeded="<<(seeded?1:0);
    for(int a=0;a<3;++a)
        std::cout<<" count"<<a<<"="<<result.selected_faces[a];
    for(int a=0;a<3;++a)
        std::cout<<" lin"<<a<<"="<<result.applied_fluid_linear_Ns[a];
    for(int a=0;a<3;++a)
        std::cout<<" ang"<<a<<"="<<result.applied_fluid_angular_Nms[a];
    std::cout<<" energy="<<result.fluid_face_energy_delta_proxy_J
             <<" maxdv="<<result.max_abs_face_delta_m_s
             <<" divergence="<<result.max_divergence_after_s_inv
             <<" postdiv="<<postdiv
             <<" projenergy="<<projection_energy
             <<" projimp0="<<projection_impulse[0]
             <<" projimp1="<<projection_impulse[1]
             <<" projimp2="<<projection_impulse[2]<<"\n";
}
}
int main() {
    try {
        for(int n:{8,12})
            for(int sign:{-1,1})
                for(bool seeded:{false,true})
                    run(n,n==8?0.1:0.125,sign,seeded);
        std::cout<<"PASS: "<<checks
                 <<" native frozen-wrench MAC reaction assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
