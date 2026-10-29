#include "upc/p2_closed_basin_kinematic_flux3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::size_t checks=0;
void require(bool b,const char* msg) {
    ++checks;
    if(!b)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
upc::P2Box3D box() {return {{3.2,3.2,3.2},{4.8,4.8,4.8}};}
upc::RigidPressureState3D body(double velocity) {
    upc::RigidPressureState3D b;
    b.centre_m={{4,4,4}};
    b.velocity_m_s[2]=velocity;
    b.mass_kg=10000;
    return b;
}
double water_volume(int n,double h) {
    return n*n*h-2.56*std::clamp(h-3.2,0.0,1.6);
}
template<class F>void rejects(F fn,const char* msg) {
    bool caught=false;
    try {fn();} catch(const std::exception&) {caught=true;}
    require(caught,msg);
}
void run(int n,double level,double g,double v) {
    const double dx=1,dt=.01,rho=1000;
    const auto b=box();
    const auto hydro=upc::P2ClosedBasinHydrostatic3D::step(
        n,n,n,dx,dt,rho,g,water_volume(n,level),b,body(v));
    const auto r=upc::P2ClosedBasinKinematicFlux3D::reconstruct(
        n,n,n,dx,dt,b,hydro);
    const auto geometry=upc::P2MovingBoxFaceApertures3D::sample(
        n,n,n,dx,dt,b,hydro.box_after);
    near(r.total_water_change_m3,0,2e-10,"global water mass");
    require(r.cg_iterations<20000,"CG finite iteration count");
    require(r.cg_residual_l2_m3<1e-8,"CG residual");
    require(r.max_cell_continuity_residual_m3<1e-8,"cell continuity");
    require(r.u_m3.size()==std::size_t((n+1)*n*n),"u dimensions");
    require(r.v_m3.size()==std::size_t(n*(n+1)*n),"v dimensions");
    require(r.w_m3.size()==std::size_t(n*n*(n+1)),"w dimensions");
    const auto id=[&](int x,int y,int z){return (z*n+y)*n+x;};
    const auto u=[&](int x,int y,int z){return (z*n+y)*(n+1)+x;};
    const auto vv=[&](int x,int y,int z){return (z*(n+1)+y)*n+x;};
    const auto w=[&](int x,int y,int z){return (z*n+y)*n+x;};
    double max_err=0,max_flux=0,net_div=0,max_inventory_delta=0;
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=id(x,y,z);
        const double divergence=
            r.u_m3[u(x+1,y,z)]-r.u_m3[u(x,y,z)]+
            r.v_m3[vv(x,y+1,z)]-r.v_m3[vv(x,y,z)]+
            r.w_m3[w(x,y,z+1)]-r.w_m3[w(x,y,z)];
        const double before=geometry.cell_geometry.alpha_old[i]*
                            hydro.water_fraction_before[i];
        const double after=geometry.cell_geometry.alpha_new[i]*
                           hydro.water_fraction_after[i];
        max_inventory_delta=std::max(max_inventory_delta,std::abs(before-after));
        const double error=std::abs(before-divergence-after);
        max_err=std::max(max_err,error);
        net_div+=divergence;
        near(before-divergence,after,2e-8,"independent cell water continuity");
        if(x==0)near(r.u_m3[u(x,y,z)],0,0,"left wall sealed");
        if(x==n-1)near(r.u_m3[u(x+1,y,z)],0,0,"right wall sealed");
        if(y==0)near(r.v_m3[vv(x,y,z)],0,0,"front wall sealed");
        if(y==n-1)near(r.v_m3[vv(x,y+1,z)],0,0,"back wall sealed");
        if(z==0)near(r.w_m3[w(x,y,z)],0,0,"floor sealed");
        if(z==n-1)near(r.w_m3[w(x,y,z+1)],0,0,"ceiling sealed");
    }
    for(std::size_t i=0;i<r.u_m3.size();++i) {
        require(std::isfinite(r.u_m3[i]),"finite u");
        if(geometry.u_open[i]==0)near(r.u_m3[i],0,0,"blocked u");
        max_flux=std::max(max_flux,std::abs(r.u_m3[i]));
    }
    for(std::size_t i=0;i<r.v_m3.size();++i) {
        require(std::isfinite(r.v_m3[i]),"finite v");
        if(geometry.v_open[i]==0)near(r.v_m3[i],0,0,"blocked v");
        max_flux=std::max(max_flux,std::abs(r.v_m3[i]));
    }
    for(std::size_t i=0;i<r.w_m3.size();++i) {
        require(std::isfinite(r.w_m3[i]),"finite w");
        if(geometry.w_open[i]==0)near(r.w_m3[i],0,0,"blocked w");
        max_flux=std::max(max_flux,std::abs(r.w_m3[i]));
    }
    near(net_div,0,1e-10,"independent global divergence cancellation");
    if(max_inventory_delta>1e-10)require(max_flux>1e-10,"water inventory change requires flux");
    std::cout<<"FLUX "<<n<<" "<<std::setprecision(17)<<level<<" "<<g
             <<" "<<v<<" "<<max_err<<" "<<r.cg_residual_l2_m3
             <<" "<<r.cg_iterations<<" "<<max_flux<<" "
             <<r.total_water_change_m3<<"\n";
}
void disconnected_components() {
    // Two isolated wet islands, each surrounded by one-cell dry halos.
    // The graph must solve independent balanced transfers but reject a
    // globally balanced transfer BETWEEN disconnected islands.
    constexpr int nx=12,ny=8,nz=8;
    const auto b=box();
    upc::P2ClosedBasinHydrostaticResult3D state;
    state.box_after=b;
    state.water_fraction_before.assign(nx*ny*nz,0.0);
    state.water_fraction_after.assign(nx*ny*nz,0.0);
    const auto id=[=](int x) {return std::size_t(nx+x);}; // y=1,z=0
    for(int x:{1,2,8,9}) {
        state.water_fraction_before[id(x)]=0.5;
        state.water_fraction_after[id(x)]=0.5;
    }
    state.water_fraction_after[id(1)]=0.25;
    state.water_fraction_after[id(2)]=0.75;
    state.water_fraction_after[id(8)]=0.75;
    state.water_fraction_after[id(9)]=0.25;
    const auto result=upc::P2ClosedBasinKinematicFlux3D::reconstruct(
        nx,ny,nz,1.0,0.01,b,state);
    require(result.max_cell_continuity_residual_m3<1e-9,
            "balanced disconnected components conserve independently");
    near(result.total_water_change_m3,0.0,1e-12,
         "balanced components conserve total water");
    // Each island is individually unbalanced, while the global total is 0.
    auto invalid=state;
    invalid.water_fraction_after[id(1)]+=0.1;
    invalid.water_fraction_after[id(8)]-=0.1;
    bool rejected=false;
    try {
        (void)upc::P2ClosedBasinKinematicFlux3D::reconstruct(
            nx,ny,nz,1.0,0.01,b,invalid);
    } catch(const std::invalid_argument& e) {
        rejected=std::string(e.what()).find("Disconnected wet component")
                 !=std::string::npos;
    }
    require(rejected,"globally balanced cross-island transfer rejected");
    // An isolated dry cell cannot gain liquid without an open wet path.
    invalid=state;
    invalid.water_fraction_after[id(5)]=0.1;
    invalid.water_fraction_after[id(1)]-=0.1;
    rejects([&]{(void)upc::P2ClosedBasinKinematicFlux3D::reconstruct(
        nx,ny,nz,1.0,0.01,b,invalid);},
        "isolated dry cell cannot receive water");
    std::cout<<"COMPONENT 2 1 1 "
             <<result.max_cell_continuity_residual_m3<<"\n";
}
void negative() {
    const auto b=box();
    auto state=upc::P2ClosedBasinHydrostatic3D::step(
        8,8,8,1,.01,1000,9.81,water_volume(8,4),b,body(.4));
    auto invalid=state;
    invalid.water_fraction_before.pop_back();
    rejects([&]{upc::P2ClosedBasinKinematicFlux3D::reconstruct(
        8,8,8,1,.01,b,invalid);},"wrong inventory length");
    invalid=state;
    invalid.water_fraction_after[0]=1.5;
    rejects([&]{upc::P2ClosedBasinKinematicFlux3D::reconstruct(
        8,8,8,1,.01,b,invalid);},"invalid fraction");
    invalid=state;
    invalid.water_fraction_after[0]=0;
    invalid.water_fraction_after[1]=0;
    rejects([&]{upc::P2ClosedBasinKinematicFlux3D::reconstruct(
        8,8,8,1,.01,b,invalid);},"nonconservative inventory");
    rejects([&]{upc::P2ClosedBasinKinematicFlux3D::reconstruct(
        8,8,8,1,0,b,state);},"zero dt");
}
}
int main() {
    try {
        for(int n:{8,12})
        for(double level:{0.0,3.2,4.0,5.75})
        for(double g:{0.0,9.81})
        for(double v:{-0.4,0.4})run(n,level,g,v);
        disconnected_components();
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native closed-basin 3D kinematic flux assertions\n";
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" checks\n";
        return 1;
    }
}
