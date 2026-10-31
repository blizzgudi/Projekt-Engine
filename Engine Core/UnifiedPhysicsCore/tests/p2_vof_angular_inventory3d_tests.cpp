// P2 partial: independent analytic VOF angular momentum / inertia checks.
#include "upc/p2_vof_angular_inventory3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int assertions=0;
void need(bool ok,const char* message) {
    ++assertions;
    if(!ok)throw std::runtime_error(message);
}
void near(double got,double expected,double tol,const char* message) {
    need(std::isfinite(got)&&std::abs(got-expected)<=tol,message);
}
template<class F> void rejects(F fn,const char* why) {
    bool caught=false;
    try{fn();}catch(const std::invalid_argument&){caught=true;}
    need(caught,why);
}
void run(int n) {
    constexpr double dx=.1,rho=1000;
    upc::MacProjection3D mac(n,n,n,dx,rho);
    std::vector<double> fill(n*n*n,0);
    std::vector<std::uint8_t> solid(n*n*n,0);
    auto idx=[&](int x,int y,int z){return (z*n+y)*n+x;};
    auto velocity=[&](int x,int y,int z,double u,double v,double w) {
        mac.set_face_velocity_x(x,y,z,u);
        mac.set_face_velocity_x(x+1,y,z,u);
        mac.set_face_velocity_y(x,y,z,v);
        mac.set_face_velocity_y(x,y+1,z,v);
        mac.set_face_velocity_z(x,y,z,w);
        mac.set_face_velocity_z(x,y,z+1,w);
    };
    fill[idx(2,2,2)]=.25;fill[idx(5,5,5)]=.75;
    solid[idx(4,4,4)]=1;
    velocity(2,2,2,2,-1,.5);
    velocity(5,5,5,-1,0,-2);
    mac.set_face_velocity_x(0,0,0,1000);
    const auto a=upc::vof_angular_inventory3d(mac,fill,solid,n,n,n,dx,rho);
    near(a.mass_kg,1,1e-13,"mass");
    const double p[3]={-.25,-.25,-1.375};
    const double L[3]={-.73125,.50625,.225};
    for(int k=0;k<3;++k) {
        near(a.centre_of_mass_m[k],.475,1e-13,"centroid");
        near(a.linear_momentum_kg_m_s[k],p[k],1e-13,"linear momentum");
        near(a.angular_momentum_kg_m2_s[k],L[k],1e-13,"angular momentum");
        for(int j=0;j<3;++j) {
            near(a.inertia_about_origin_kg_m2[k][j],k==j?.485:-.2425,
                 1e-13,"inertia about origin");
            near(a.inertia_about_com_kg_m2[k][j],k==j?.03375:-.016875,
                 1e-13,"parallel axis theorem");
        }
    }
    need(a.occupied_cells==2,"two wet cells");
    mac.set_face_velocity_x(0,0,0,-5000);
    const auto dry=upc::vof_angular_inventory3d(mac,fill,solid,n,n,n,dx,rho);
    for(int k=0;k<3;++k)
        near(dry.angular_momentum_kg_m2_s[k],a.angular_momentum_kg_m2_s[k],0,"dry ghost ignored");
    const std::array<double,3> shifted{.1,-.2,.3};
    const auto b=upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho,shifted);
    const double translated[3]={-1.08125,.44375,.3};
    for(int k=0;k<3;++k) {
        near(b.angular_momentum_kg_m2_s[k],translated[k],1e-13,
             "origin shift L'=L-a cross P");
        near(b.centre_of_mass_m[k],.475,1e-13,"centroid origin invariant");
        for(int j=0;j<3;++j)
            near(b.inertia_about_com_kg_m2[k][j],
                 a.inertia_about_com_kg_m2[k][j],1e-13,
                 "COM inertia origin invariant");
    }
    fill[idx(2,2,2)]=.125;fill[idx(5,5,5)]=.875;
    const auto moved=upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho);
    const auto delta=upc::vof_angular_delta3d(a,moved);
    const double expected_delta[3]={-.184375,.021875,.1625};
    for(int k=0;k<3;++k)
        near(delta[k],expected_delta[k],1e-13,"analytic VOF transfer angular delta");
    std::cout<<"analytic,"<<n<<","<<a.mass_kg<<","
             <<a.angular_momentum_kg_m2_s[0]<<","
             <<a.angular_momentum_kg_m2_s[1]<<","
             <<a.angular_momentum_kg_m2_s[2]<<","
             <<a.inertia_about_com_kg_m2[0][0]<<","
             <<a.inertia_about_com_kg_m2[0][1]<<","
             <<delta[0]<<","<<delta[1]<<","<<delta[2]<<"\n";
    // Exercise a real pressure-projection update on a nonempty 3D mask.
    fill[idx(2,2,2)]=.25;fill[idx(5,5,5)]=.75;
    fill[idx(3,2,2)]=.5; // Unequal partial fill on the adjacent projected cell.
    mac.set_cell(2,2,2,true);mac.set_cell(3,2,2,true);mac.set_cell(5,5,5,true);
    mac.set_face_velocity_x(3,2,2,2.2); // Nonzero divergence to exercise projection.
    const auto before=upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho);
    const double divergence=mac.project_verified(.01,4000,1e-8);
    const auto after=upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho);
    const auto projected_delta=upc::vof_angular_delta3d(before,after);
    need(divergence<=1e-8,"verified 3D projection");
    need(std::abs(projected_delta[0])+std::abs(projected_delta[1])+
         std::abs(projected_delta[2])>1e-10,"projection must change angular inventory");
    near(before.mass_kg,after.mass_kg,0,"read-only mass across projection");
    for(double v:projected_delta)need(std::isfinite(v),"finite projected angular delta");
    std::cout<<"projected,"<<n<<","<<after.mass_kg<<","
             <<after.angular_momentum_kg_m2_s[0]<<","
             <<after.angular_momentum_kg_m2_s[1]<<","
             <<after.angular_momentum_kg_m2_s[2]<<","
             <<after.inertia_about_com_kg_m2[0][0]<<","
             <<after.inertia_about_com_kg_m2[0][1]<<","
             <<projected_delta[0]<<","<<projected_delta[1]<<","
             <<projected_delta[2]<<"\n";
    fill[idx(2,2,2)]=-1;
    rejects([&]{(void)upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho);},"negative fill rejected");
    fill[idx(2,2,2)]=.25;fill[idx(4,4,4)]=.1;
    rejects([&]{(void)upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho);},"solid overlap rejected");
    fill[idx(4,4,4)]=0;
    rejects([&]{(void)upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho,
        {std::numeric_limits<double>::quiet_NaN(),0,0});},
        "nonfinite origin rejected");
    auto bad=before;bad.angular_momentum_kg_m2_s[0]=
        std::numeric_limits<double>::infinity();
    rejects([&]{(void)upc::vof_angular_delta3d(bad,after);},
        "nonfinite delta rejected");
    std::fill(fill.begin(),fill.end(),0);
    const auto empty=upc::vof_angular_inventory3d(
        mac,fill,solid,n,n,n,dx,rho,shifted);
    near(empty.mass_kg,0,0,"empty mass");
    near(empty.centre_of_mass_m[0],shifted[0],0,"empty origin convention");
    near(empty.inertia_about_com_kg_m2[0][0],0,0,"empty inertia");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,mass,Lx,Ly,Lz,Ixx,Ixy,dLx,dLy,dLz\n";
        run(8);run(12);
        std::cout<<"PASS: vof angular inventory assertions="<<assertions
                 <<" phase_complete=false\n";
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
    return 0;
}
