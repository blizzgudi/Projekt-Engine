#include "upc/p2_hydrostatic_gravity3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Model=upc::P2HydrostaticGravity3D;
std::size_t checks=0;
void require(bool ok,const char* msg) {
    ++checks;
    if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,msg);
}
template<class F>void rejects(F f,const char* msg) {
    bool threw=false;try{f();}catch(const std::exception&){threw=true;}
    require(threw,msg);
}
std::vector<double> water(int n,double h) {
    std::vector<double> f(n*n*n,0.0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        f[(z*n+y)*n+x]=std::clamp(h-z,0.0,1.0);
    return f;
}
// Independent piecewise integration of -F_b(z) over a translation.
// U(z)=rho*g*A*(H*(h-zc) for fully wet, 0.5*(h-b)^2 for partial)
// with continuity offset H^2/2 at the full-wet boundary.
double potential(double z,double h,double rho,double g) {
    constexpr double H=1.6,A=2.56;
    const double bottom=z-0.8,top=z+0.8;
    if(bottom>=h)return 0.0;
    if(top<=h)return rho*g*A*(H*(h-top)+0.5*H*H);
    const double d=h-bottom;
    return 0.5*rho*g*A*d*d;
}
void scenario(int n,double h,double g,double mass,double initial_v=0.0,
              double dt=0.01) {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=mass;body.centre_m={{4,4,4}};
    body.velocity_m_s[2]=initial_v;
    const auto f=water(n,h),old=f;
    const auto out=Model::step(n,n,n,1.0,dt,1000.0,g,box,body,f);
    const double submerged=std::clamp(h-3.2,0.0,1.6);
    const double buoyancy=1000.0*g*2.56*submerged;
    const double weight=mass*g,net=buoyancy-weight;
    const double vz=initial_v+net*dt/mass;
    const double dz=0.5*(initial_v+vz)*dt;
    const double deltaK=0.5*mass*(vz*vz-initial_v*initial_v);
    const double deltaUb=potential(4.0+dz,h,1000.0,g)-
                         potential(4.0,h,1000.0,g);
    near(out.water_level_m,h,1e-12,"horizontal level");
    near(out.buoyancy_force_N,buoyancy,1e-8,"Archimedes");
    near(out.gravity_force_N,-weight,1e-8,"weight");
    near(out.net_vertical_force_N,net,1e-8,"net force");
    near(out.net_rigid.after.velocity_m_s[2],vz,1e-11,"velocity");
    near(out.net_rigid.after.centre_m[2],4+dz,1e-11,"position");
    near(out.box_after.min_m[2],3.2+dz,1e-11,"moving cut geometry");
    near(out.net_rigid.body_linear_impulse_Ns[2],net*dt,1e-8,
         "net body impulse");
    near(out.buoyancy_reaction_unapplied_Ns,-buoyancy*dt,1e-8,
         "fluid reaction excludes weight");
    near(out.net_rigid.fluid_reaction_impulse_Ns[2],-buoyancy*dt,1e-8,
         "generic rigid reaction corrected to pressure-only");
    near(out.gravity_impulse_Ns,-weight*dt,1e-8,"external gravity impulse");
    near(out.gravity_potential_change_J,weight*dz,1e-8,
         "independent gravitational potential");
    near(out.buoyancy_potential_change_J,deltaUb,1e-8,
         "independent hydrostatic potential");
    near(out.kinetic_energy_change_J,deltaK,1e-8,"kinetic energy");
    near(out.fixed_level_mechanical_defect_J,
         deltaK+weight*dz+deltaUb,1e-8,
         "exact fixed-level energy quadrature audit");
    near(out.net_rigid.energy_residual_J,0.0,1e-8,
         "frozen net-force kinetic work");
    require(f==old,"input water unchanged");
    near(body.centre_m[2],4.0,0.0,"input body unchanged");
    for(int a=0;a<2;++a) {
        near(out.net_rigid.after.velocity_m_s[a],0.0,0.0,
             "no lateral drift");
        near(out.net_rigid.body_angular_impulse_Nms[a],0.0,0.0,
             "no spurious torque");
    }
    std::cout<<"GRAV "<<n<<" "<<std::setprecision(17)<<h<<" "<<g
             <<" "<<mass<<" "<<buoyancy<<" "<<net<<" "<<dz<<" "
             <<out.fixed_level_mechanical_defect_J<<"\n";
}
void equilibrium() {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=2048.0;body.centre_m={{4,4,4}};
    const auto f=water(8,4.0);
    const auto out=Model::step(8,8,8,1,0.1,1000,9.81,box,body,f);
    near(out.net_vertical_force_N,0.0,1e-9,"neutral buoyancy equilibrium");
    near(out.net_rigid.after.centre_m[2],4.0,0.0,"equilibrium no drift");
    near(out.fixed_level_mechanical_defect_J,0.0,1e-10,
         "equilibrium no mechanical defect");
    near(out.buoyancy_reaction_unapplied_Ns,-2009.088,1e-8,
         "buoyancy reaction is not net gravity reaction");
}
void interface_crossing() {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=2048;body.centre_m={{4,4,4}};
    body.velocity_m_s[2]=2.0;
    const double h=4.35,dt=0.2;
    const auto out=Model::step(8,8,8,1,dt,1000,9.81,box,body,water(8,h));
    const double dz=out.net_rigid.after.centre_m[2]-4.0;
    require(dz>0.0&&dz<1.0,"interface movement within cell CFL");
    // For a partially submerged box remaining partially submerged:
    // frozen initial buoyancy overestimates exact work by 0.5*rho*g*A*dz^2.
    near(out.fixed_level_mechanical_defect_J,
         0.5*1000*9.81*2.56*dz*dz,1e-7,
         "nonzero interface crossing quadrature defect");
    require(out.fixed_level_mechanical_defect_J>100.0,
            "no false global-energy PASS at moving interface");
}
void negative() {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=2048;body.centre_m={{4,4,4}};
    auto f=water(8,4);
    auto bad=f;bad[0]=0.5;
    rejects([&]{Model::step(8,8,8,1,.01,1000,9.81,box,body,bad);},
            "nonhorizontal surface rejected");
    bad=f;bad[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Model::step(8,8,8,1,.01,1000,9.81,box,body,bad);},
            "NaN fraction rejected");
    rejects([&]{Model::step(8,8,8,1,-.01,1000,9.81,box,body,f);},
            "invalid dt rejected");
    rejects([&]{Model::step(8,8,8,1,.01,1000,-9.81,box,body,f);},
            "negative gravity rejected");
    auto shifted=body;shifted.centre_m[0]=4.1;
    rejects([&]{Model::step(8,8,8,1,.01,1000,9.81,box,shifted,f);},
            "off-centre body rejected");
    auto too_fast=body;too_fast.velocity_m_s[2]=1000;
    rejects([&]{Model::step(8,8,8,1,.01,1000,9.81,box,too_fast,f);},
            "moving box CFL rejected");
    require(f==water(8,4),"failure leaves input intact");
}
}
int main() {
    try {
        for(int n:{8,12})for(double h:{0.0,3.125,4.0,4.35,5.75,
                                      double(n)})
        for(double g:{0.0,9.81})
            scenario(n,h,g,10000.0);
        equilibrium();
        interface_crossing();
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native hydrostatic gravity energy assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
