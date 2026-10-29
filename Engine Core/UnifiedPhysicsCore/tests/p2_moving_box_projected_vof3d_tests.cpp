#include "upc/p2_moving_box_projected_vof3d.hpp"
#include "upc/p2_moving_box_open_volume3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Solver=upc::P2MovingBoxProjectedVof3D;
using Faces=Solver::Faces;
std::size_t checks=0;
void check(bool ok,const char* msg) {
    ++checks;
    if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
template<class F>void rejects(F f,const char* msg) {
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    check(threw,msg);
}
Faces zero(int n) {
    return {{std::vector<double>((n+1)*n*n),
             std::vector<double>(n*(n+1)*n),
             std::vector<double>(n*n*(n+1))}};
}
std::vector<double> gradient(int n,int axis,int sign) {
    std::vector<double> p(n*n*n);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const int q[3]={x,y,z};
        p[(z*n+y)*n+x]=100.0+10.0*sign*(q[axis]+0.5);
    }
    return p;
}
std::vector<double> fill(int n,const std::vector<double>& alpha,bool uniform) {
    std::vector<double> f(alpha.size(),0.0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const std::size_t i=(z*n+y)*n+x;
        if(alpha[i]>0.0)
            f[i]=uniform?1.0:0.2+0.01*x+0.012*y+0.008*z;
    }
    return f;
}
// Independent finite-volume water mass reference, in kg (dx=1).
std::vector<double> reference(
    int n,const upc::P2MovingBoxCutCellProjection3D& p,
    const std::vector<double>& old) {
    const auto& g=p.wall.geometry.cell_geometry;
    std::vector<double> mass(old.size());
    for(std::size_t i=0;i<mass.size();++i)mass[i]=g.alpha_old[i]*old[i];
    const auto ci=[&](int x,int y,int z){return (z*n+y)*n+x;};
    const auto flux=[&](int a,int x,int y,int z,double q){
        if(q==0.0)return;
        const int lx=x-(a==0),ly=y-(a==1),lz=z-(a==2);
        const auto l=ci(lx,ly,lz),r=ci(x,y,z);
        const auto donor=q>0?l:r;
        const double transported=q*old[donor];
        mass[l]-=transported;
        mass[r]+=transported;
    };
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=1;x<n;++x)
        flux(0,x,y,z,p.u_fluid_m3[(z*n+y)*(n+1)+x]);
    for(int z=0;z<n;++z)for(int y=1;y<n;++y)
    for(int x=0;x<n;++x)
        flux(1,x,y,z,p.v_fluid_m3[(z*(n+1)+y)*n+x]);
    for(int z=1;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        flux(2,x,y,z,p.w_fluid_m3[(z*n+y)*n+x]);
    for(std::size_t i=0;i<mass.size();++i)
        mass[i]=g.alpha_new[i]>1e-12?mass[i]/g.alpha_new[i]:0.0;
    return mass;
}
void scenario(int n,int axis,int sign,bool uniform) {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=10.0;body.centre_m={{4.0,4.0,4.0}};
    const auto p=gradient(n,axis,sign);
    const auto velocity=zero(n);
    const auto g=upc::P2MovingBoxOpenVolumes3D::sample(
        n,n,n,1.0,0.1,box,box);
    const auto water=fill(n,g.alpha_old,uniform);
    const auto snapshot=water;
    const auto result=Solver::step(
        n,n,n,1.0,0.1,1000.0,box,body,p,velocity,water);
    const auto& transport=result.transport;
    const auto& reaction=result.reaction;
    const auto expected=reference(n,reaction.projection,water);
    check(expected.size()==transport.value_new.size(),"water shape");
    for(std::size_t i=0;i<expected.size();++i) {
        near(transport.value_new[i],expected[i],2e-10,
             "independent face-by-face water remap");
        check(transport.value_new[i]>=-1e-11&&
              transport.value_new[i]<=1.0+1e-11,"water bounded");
        if(uniform)
            near(transport.value_new[i],
                 reaction.projection.wall.geometry.cell_geometry.alpha_new[i]>1e-12
                    ?1.0:0.0,2e-10,"full water constant-state remap");
    }
    near(result.water_mass_before_kg,result.water_mass_after_kg,1e-7,
         "water mass closed");
    near(result.water_mass_residual_kg,0.0,1e-7,
         "water mass residual");
    check(transport.active_faces>0,"projected water flux active");
    check(transport.max_gcl_residual<1e-10,"water remap geometric GCL");
    check(transport.max_donor_courant<=1.0+1e-10,"water donor CFL");
    check(reaction.projection.max_continuity_residual_m3<1e-10,
          "MAC projected continuity");
    check(std::abs(reaction.combined_impulse_residual_proxy_Ns[axis])>1e-4,
          "physical FSI impulse NOT falsely claimed");
    check(water==snapshot,"water input unchanged");
    check(velocity==zero(n),"velocity input unchanged");
    std::cout<<"VOF "<<n<<" "<<axis<<" "<<sign<<" "<<int(uniform)<<" "
             <<std::setprecision(17)<<result.water_mass_before_kg<<" "
             <<result.water_mass_residual_kg<<" "
             <<transport.max_donor_courant<<" "
             <<reaction.combined_impulse_residual_proxy_Ns[axis]<<"\n";
}
void sequential(int n,int axis,int sign) {
    upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=10;body.centre_m={{4,4,4}};
    const auto p=gradient(n,axis,sign);
    auto velocities=zero(n);
    const auto g=upc::P2MovingBoxOpenVolumes3D::sample(
        n,n,n,1.0,0.1,box,box);
    auto water=fill(n,g.alpha_old,false);
    double initial_mass=0.0;
    for(int i=0;i<2;++i) {
        const auto out=Solver::step(
            n,n,n,1.0,0.1,1000,box,body,p,velocities,water);
        if(i==0)initial_mass=out.water_mass_before_kg;
        near(out.water_mass_after_kg,initial_mass,2e-7,
             "sequential water mass");
        check(out.transport.max_gcl_residual<1e-10,
              "sequential moving box GCL");
        check(out.transport.max_donor_courant<=1.0+1e-10,
              "sequential donor CFL");
        check(out.reaction.projection.max_continuity_residual_m3<1e-10,
              "sequential projected MAC");
        water=out.transport.value_new;
        velocities=out.reaction.projected_velocity_m_s;
        body=out.reaction.unprojected.body.rigid.after;
        box=out.reaction.unprojected.body.box_after;
    }
    std::cout<<"SEQUENTIAL "<<n<<" "<<axis<<" "<<sign<<" "
             <<std::setprecision(17)<<initial_mass<<"\n";
}
void negative() {
    constexpr int n=8;
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=10;body.centre_m={{4,4,4}};
    auto p=gradient(n,0,1);
    auto v=zero(n);
    const auto g=upc::P2MovingBoxOpenVolumes3D::sample(n,n,n,1,0.1,box,box);
    const auto f=fill(n,g.alpha_old,false);
    auto bad=f;bad[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Solver::step(n,n,n,1,0.1,1000,box,body,p,v,bad);},
            "NaN fill rejected");
    bad=f;bad[0]=1.01;
    rejects([&]{Solver::step(n,n,n,1,0.1,1000,box,body,p,v,bad);},
            "overfilled water rejected");
    bad=f;bad.pop_back();
    rejects([&]{Solver::step(n,n,n,1,0.1,1000,box,body,p,v,bad);},
            "water shape rejected");
    const upc::P2Box3D blocked{{3.0,3.0,3.0},{5.0,5.0,5.0}};
    const auto bg=upc::P2MovingBoxOpenVolumes3D::sample(
        n,n,n,1,0.1,blocked,blocked);
    auto blocked_fill=fill(n,bg.alpha_old,false);
    blocked_fill[(3*n+3)*n+3]=0.1;
    rejects([&]{Solver::step(n,n,n,1,0.1,1000,blocked,body,p,v,blocked_fill);},
            "water in blocked old cell rejected");
    rejects([&]{Solver::step(n,n,n,1,0.1,0,box,body,p,v,f);},
            "invalid density rejected");
    check(f==fill(n,g.alpha_old,false),"inputs remain unchanged");
}
}
int main() {
    try {
        for(int n:{8,12})for(int axis=0;axis<3;++axis)
        for(int sign:{-1,1})for(bool uniform:{false,true})
            scenario(n,axis,sign,uniform);
        for(int n:{8,12})for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})sequential(n,axis,sign);
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native projected moving-box VOF assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
