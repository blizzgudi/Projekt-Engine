#include "upc/p2_moving_box_projected_reaction3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Step=upc::P2MovingBoxProjectedReaction3D;
using Faces=Step::Faces;
std::size_t checks=0;
void require(bool v,const char* name) {++checks;if(!v)throw std::runtime_error(name);}
void near(double a,double b,double tol,const char* name) {
    require(std::isfinite(a)&&std::abs(a-b)<=tol,name);
}
Faces zero(int n) {return {{std::vector<double>((n+1)*n*n,0.0),
    std::vector<double>(n*(n+1)*n,0.0),
    std::vector<double>(n*n*(n+1),0.0)}};}
std::vector<double> gradient(int n,int axis,int sign) {
    std::vector<double> p(n*n*n);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const int q[3]={x,y,z};
        p[(z*n+y)*n+x]=100+10*sign*(q[axis]+0.5);
    }
    return p;
}
void scenario(int n,int axis,int sign) {
    upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=10;body.centre_m={{4,4,4}};
    auto p=gradient(n,axis,sign);
    auto input=zero(n);
    const auto snapshot=input;
    auto r=Step::step(n,n,n,1.0,0.1,1000,box,body,p,input);
    near(r.unprojected.body.rigid.body_linear_impulse_Ns[axis],
         -2.56*sign,1e-10,"independent analytic body impulse");
    near(r.unprojected.applied_fluid_impulse_Ns[axis],
         2.56*sign,1e-10,"independent analytic fluid impulse");
    require(r.unprojected.max_geometric_continuity_residual_m3>1e-8,
            "unprojected residual must be visible");
    require(r.projection.max_continuity_residual_m3<1e-10,
            "projected continuity");
    require(r.projection.max_continuity_residual_m3<
            r.unprojected.max_geometric_continuity_residual_m3*1e-4,
            "projection reduces residual by four orders");
    require(r.projection.cg_iterations>0,"nontrivial pressure projection");
    for(int a=0;a<3;++a) {
        require(r.projected_velocity_m_s[a].size()==input[a].size(),
                "MAC shape retained");
        near(r.combined_impulse_residual_proxy_Ns[a],
             r.projection_momentum_change_proxy_Ns[a]+
             r.unprojected.total_impulse_residual_Ns[a],1e-10,
             "pressure correction explicitly accounted");
        require(std::isfinite(r.projection_momentum_change_proxy_Ns[a]),
                "finite projection momentum");
        require(std::isfinite(r.combined_impulse_residual_proxy_Ns[a]),
                "finite residual");
    }
    // Independent finite-volume GCL: reconstructed MAC velocities and
    // aperture fractions must close every moving primal cut-cell.
    const auto& geom=r.projection.wall.geometry;
    const auto& u=r.projected_velocity_m_s[0];
    const auto& v=r.projected_velocity_m_s[1];
    const auto& w=r.projected_velocity_m_s[2];
    double max_manual=0.0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const std::size_t ci=(z*n+y)*n+x;
        const auto ui=[&](int i,int j,int k) {
            return (k*n+j)*(n+1)+i;};
        const auto vi=[&](int i,int j,int k) {
            return (k*(n+1)+j)*n+i;};
        const auto wi=[&](int i,int j,int k) {
            return (k*n+j)*n+i;};
        const double residual=geom.cell_geometry.alpha_new[ci]-
            geom.cell_geometry.alpha_old[ci]+0.1*(
            u[ui(x+1,y,z)]*geom.u_open[ui(x+1,y,z)]-
            u[ui(x,y,z)]*geom.u_open[ui(x,y,z)]+
            v[vi(x,y+1,z)]*geom.v_open[vi(x,y+1,z)]-
            v[vi(x,y,z)]*geom.v_open[vi(x,y,z)]+
            w[wi(x,y,z+1)]*geom.w_open[wi(x,y,z+1)]-
            w[wi(x,y,z)]*geom.w_open[wi(x,y,z)]);
        max_manual=std::max(max_manual,std::abs(residual));
    }
    require(max_manual<1e-10,"independent reconstructed MAC GCL");
    near(max_manual,r.projection.max_continuity_residual_m3,1e-11,
         "independent versus internal GCL");
    require(std::abs(r.projection_momentum_change_proxy_Ns[axis])>1e-4,
            "projection materially alters proxy impulse");
    require(input==snapshot,"input faces unchanged");
    require(p==gradient(n,axis,sign),"input pressure unchanged");
    std::cout<<"PROJECTED "<<n<<" "<<axis<<" "<<sign<<" "
             <<std::setprecision(17)
             <<r.unprojected.max_geometric_continuity_residual_m3<<" "
             <<r.projection.max_continuity_residual_m3<<" "
             <<r.projection_momentum_change_proxy_Ns[axis]<<" "
             <<r.combined_impulse_residual_proxy_Ns[axis]<<"\n";
}
void gauge() {
    upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;body.mass_kg=10;body.centre_m={{4,4,4}};
    auto p=gradient(8,0,1);
    const auto a=Step::step(8,8,8,1,0.1,1000,box,body,p,zero(8));
    for(auto& x:p)x+=61;
    const auto b=Step::step(8,8,8,1,0.1,1000,box,body,p,zero(8));
    for(int axis=0;axis<3;++axis) {
        near(a.projection_momentum_change_proxy_Ns[axis],
             b.projection_momentum_change_proxy_Ns[axis],1e-9,
             "gauge-invariant pressure correction");
        for(std::size_t i=0;i<a.projected_velocity_m_s[axis].size();++i)
            near(a.projected_velocity_m_s[axis][i],
                 b.projected_velocity_m_s[axis][i],1e-11,
                 "gauge-invariant MAC face");
    }
}
void negative() {
    upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;body.mass_kg=10;body.centre_m={{4,4,4}};
    auto p=gradient(8,0,1);
    auto v=zero(8);
    const auto snapshot=v;
    bool rejected=false;
    try {Step::step(8,8,8,1,0.1,0,box,body,p,v);}
    catch(const std::exception&) {rejected=true;}
    require(rejected,"zero density rejected");
    v[0][0]=0.1;rejected=false;
    try {Step::step(8,8,8,1,0.1,1000,box,body,p,v);}
    catch(const std::exception&) {rejected=true;}
    require(rejected,"nonzero outer boundary rejected");
    v=snapshot;
    p[0]=std::numeric_limits<double>::quiet_NaN();rejected=false;
    try {Step::step(8,8,8,1,0.1,1000,box,body,p,v);}
    catch(const std::exception&) {rejected=true;}
    require(rejected,"nonfinite pressure rejected");
    require(v==snapshot,"failure did not mutate caller");
}
}
int main() {
    try {
        for(int n:{8,12})for(int a=0;a<3;++a)
            for(int sign:{-1,1})scenario(n,a,sign);
        gauge();negative();
        std::cout<<"PASS: "<<checks<<" native projected reaction assertions\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
