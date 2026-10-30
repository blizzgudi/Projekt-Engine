// P2 partial: opt-in transactional MAC projection, independent two-cell
// manufactured pressure reference and under-convergence rollback.
#include "upc/mac3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t assertions=0;
void check(bool value,const char* msg) {
    ++assertions;
    if(!value)throw std::runtime_error(msg);
}
void near(double got,double expected,double tolerance,const char* msg) {
    check(std::isfinite(got)&&std::abs(got-expected)<=tolerance,msg);
}
void same(const upc::MacProjection3D& a,const upc::MacProjection3D& b,int n) {
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        near(a.pressure_pa(x,y,z),b.pressure_pa(x,y,z),0,"atomic pressure");
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<=n;++x)
        near(a.face_velocity_x(x,y,z),b.face_velocity_x(x,y,z),0,"atomic x");
    for(int z=0;z<n;++z)for(int y=0;y<=n;++y)for(int x=0;x<n;++x)
        near(a.face_velocity_y(x,y,z),b.face_velocity_y(x,y,z),0,"atomic y");
    for(int z=0;z<=n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        near(a.face_velocity_z(x,y,z),b.face_velocity_z(x,y,z),0,"atomic z");
}
void run(int n,double dx,double rho,double dt) {
    const int c=n/2;
    upc::MacProjection3D mac(n,n,n,dx,rho);
    mac.set_cell(c,c,c,true);
    mac.set_cell(c+1,c,c,true);
    mac.set_face_velocity_x(c+1,c,c,0.12);
    mac.set_face_velocity_x(0,c,c,0.4); // must be rolled back on failure
    const auto original=mac;
    bool rejected=false;
    try { (void)mac.project_verified(dt,1,1e-8); }
    catch(const std::runtime_error&) { rejected=true; }
    check(rejected,"one Gauss-Seidel iteration must not be accepted");
    same(mac,original,n);
    rejected=false;
    try { (void)mac.project_verified(dt,4000,0); }
    catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"zero tolerance must be rejected");
    same(mac,original,n);
    rejected=false;
    try { (void)mac.project_verified(dt,4000,
                       std::numeric_limits<double>::quiet_NaN()); }
    catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"NaN tolerance must be rejected");
    same(mac,original,n);
    rejected=false;
    try { (void)mac.project_verified(-dt,4000,1e-8); }
    catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"invalid time step must be rejected");
    same(mac,original,n);
    const double max_div=mac.project_verified(dt,4000,1e-8);
    const double R=rho*dx*0.12/dt;
    near(mac.pressure_pa(c,c,c),-R/12,1e-7,"left manufactured pressure");
    near(mac.pressure_pa(c+1,c,c),R/12,1e-7,"right manufactured pressure");
    near(mac.face_velocity_x(c+1,c,c),0.1,1e-10,"two-cell internal flux");
    near(mac.face_velocity_x(0,c,c),0,0,"external wall clamped");
    check(max_div<=1e-8&&max_div>=0,"verified divergence tolerance");
    near(mac.max_divergence(),max_div,0,"returned actual divergence");
    // A one-cell Poisson problem is solved exactly in one iteration.
    upc::MacProjection3D isolated(n,n,n,dx,rho);
    isolated.set_cell(c,c,c,true);
    isolated.set_face_velocity_x(c+1,c,c,0.12);
    const double one_div=isolated.project_verified(dt,1,1e-8);
    near(isolated.pressure_pa(c,c,c),-R/12,1e-8,
         "single wet cell six-air-face pressure");
    check(one_div<=1e-8,"one-iteration converged case accepted");
    std::cout<<"case,"<<n<<","<<dx<<","<<rho<<","<<dt
             <<","<<mac.pressure_pa(c,c,c)
             <<","<<mac.pressure_pa(c+1,c,c)
             <<","<<mac.face_velocity_x(c+1,c,c)
             <<","<<max_div<<","<<one_div<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,dx,rho,dt,p_left,p_right,u_internal,divergence,one_cell_divergence\n";
        run(8,0.1,1000,0.02);
        run(12,0.08,1100,0.015);
        std::cout<<"PASS: P2 verified MAC projection assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" after "<<assertions<<" assertions\n";
        return 1;
    }
}
