// P2 PARTIAL: independent water-weighted MAC momentum/kinetic quadrature.
#include "upc/p2_water_mac_inventory3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
constexpr double pi=3.14159265358979323846;
int assertions=0;
void check(bool ok,const char* name) {
    ++assertions;
    if(!ok)throw std::runtime_error(name);
}
void near(double a,double b,double tol,const char* name) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,name);
}
using State=upc::P2NonlinearMacPlicState3D;
State make(int n) {
    State s;
    s.water_fill.assign(n*n*n,0);
    s.staggered_velocity={{
        std::vector<double>((n+1)*n*n,0),
        std::vector<double>(n*(n+1)*n,0),
        std::vector<double>(n*n*(n+1),0)}};
    return s;
}
void uniform(int n) {
    auto s=make(n);
    const double a=.2,b=-.1,c=.05,f=.25;
    std::fill(s.water_fill.begin(),s.water_fill.end(),f);
    std::fill(s.staggered_velocity[0].begin(),s.staggered_velocity[0].end(),a);
    std::fill(s.staggered_velocity[1].begin(),s.staggered_velocity[1].end(),b);
    std::fill(s.staggered_velocity[2].begin(),s.staggered_velocity[2].end(),c);
    const auto q=upc::p2_water_mac_inventory3d(s,n,n,n,.1,1000);
    const double mass=static_cast<double>(n)*n*n*.25;
    near(q.water_mass_kg,mass,2e-10,"uniform water mass");
    near(q.water_volume_m3,mass/1000,2e-13,"uniform water volume");
    near(q.water_momentum_kg_m_s[0],mass*a,2e-10,"uniform x momentum");
    near(q.water_momentum_kg_m_s[1],mass*b,2e-10,"uniform y momentum");
    near(q.water_momentum_kg_m_s[2],mass*c,2e-10,"uniform z momentum");
    near(q.water_kinetic_energy_j,.5*mass*(a*a+b*b+c*c),
         2e-10,"uniform kinetic energy");
    std::cout<<"UNIFORM "<<n<<" "<<q.water_mass_kg<<" "
             <<q.water_momentum_kg_m_s[0]<<" "<<q.water_kinetic_energy_j<<"\n";
}
void shear(int n) {
    auto s=make(n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i)
        s.staggered_velocity[0][(k*n+j)*(n+1)+i]=
            .03+.04*std::sin(2*pi*j/n);
    double mass=0,momentum=0,energy=0;
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i) {
        const double f=(j<n/2)?0.5:0.0;
        s.water_fill[(k*n+j)*n+i]=f;
        const double u=.03+.04*std::sin(2*pi*j/n);
        mass+=f;
        momentum+=f*u;
        energy+=.5*f*u*u;
    }
    const auto q=upc::p2_water_mac_inventory3d(s,n,n,n,.1,1000);
    near(q.water_mass_kg,mass,2e-10,"shear water mass");
    near(q.water_momentum_kg_m_s[0],momentum,2e-10,"shear water momentum");
    near(q.water_kinetic_energy_j,energy,2e-10,"shear water energy");
    near(q.water_momentum_kg_m_s[1],0,1e-12,"shear v zero");
    near(q.water_momentum_kg_m_s[2],0,1e-12,"shear w zero");
    std::cout<<"SHEAR "<<n<<" "<<q.water_mass_kg<<" "
             <<q.water_momentum_kg_m_s[0]<<" "<<q.water_kinetic_energy_j<<"\n";
}
void mixed_curl(int n) {
    auto s=make(n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i)
        s.staggered_velocity[0][(k*n+j)*(n+1)+i]=
            .02+.04*std::sin(2*pi*j/n);
    for(int k=0;k<n;++k)for(int j=0;j<=n;++j)
    for(int i=0;i<n;++i)
        s.staggered_velocity[1][(k*(n+1)+j)*n+i]=
            .01*std::cos(2*pi*k/n);
    for(int k=0;k<=n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i)
        s.staggered_velocity[2][(k*n+j)*n+i]=
            .03*std::sin(2*pi*i/n);
    double mass=0,kinetic=0;
    double momentum[3]={0,0,0};
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<n;++i) {
        const double f=((i+2*j+3*k)%5)/4.0;
        s.water_fill[(k*n+j)*n+i]=f;
        const double vel[3]={
            .02+.04*std::sin(2*pi*j/n),
            .01*std::cos(2*pi*k/n),
            .03*std::sin(2*pi*i/n)};
        mass+=f;
        for(int c=0;c<3;++c) {
            momentum[c]+=f*vel[c];
            kinetic+=.5*f*vel[c]*vel[c];
        }
    }
    const auto q=upc::p2_water_mac_inventory3d(s,n,n,n,.1,1000);
    near(q.water_mass_kg,mass,3e-10,"mixed water mass");
    for(int c=0;c<3;++c)
        near(q.water_momentum_kg_m_s[c],momentum[c],3e-10,
             "mixed curl momentum");
    near(q.water_kinetic_energy_j,kinetic,3e-10,"mixed curl kinetic");
    std::cout<<"CURL "<<n<<" "<<q.water_mass_kg<<" "
             <<q.water_momentum_kg_m_s[0]<<" "
             <<q.water_momentum_kg_m_s[1]<<" "
             <<q.water_momentum_kg_m_s[2]<<" "
             <<q.water_kinetic_energy_j<<"\n";
}
void sequence() {
    const int n=12;
    auto s=make(n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i)
        s.staggered_velocity[0][(k*n+j)*(n+1)+i]=
            .02+.04*std::sin(2*pi*j/n);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=2;i<5;++i)s.water_fill[(k*n+j)*n+i]=1;
    const auto original=s;
    const auto seq=upc::p2_viscous_mac_plic_sequence3d(
        s,{{.04,.005},{.03,.002},{.02,0}},n,n,n,.1,1000);
    const auto q=upc::p2_water_mac_sequence_endpoints3d(
        s,seq,n,n,n,.1,1000);
    near(q.before.water_mass_kg,432,1e-8,"sequence initial mass");
    near(q.after.water_mass_kg,432,1e-8,"sequence final mass");
    near(q.water_mass_residual_kg,0,1e-8,"sequence water residual");
    check(q.before.water_kinetic_energy_j<
          seq.bulk_initial.kinetic_energy_j,"water is not bulk kinetic");
    check(q.after.water_kinetic_energy_j<
          seq.bulk_final.kinetic_energy_j,"final water is not bulk kinetic");
    check(s.water_fill==original.water_fill&&
          s.staggered_velocity==original.staggered_velocity,
          "sequence input unchanged");
    const auto direct=upc::p2_water_mac_inventory3d(
        seq.state,n,n,n,.1,1000);
    near(direct.water_kinetic_energy_j,q.after.water_kinetic_energy_j,
         1e-12,"sequence endpoint direct parity");
    std::cout<<"SEQUENCE "<<q.before.water_mass_kg<<" "
             <<q.after.water_mass_kg<<" "
             <<q.before.water_kinetic_energy_j<<" "
             <<q.after.water_kinetic_energy_j<<" "
             <<q.kinetic_energy_change_j<<" "
             <<q.water_mass_residual_kg<<"\n";
}
void negatives() {
    int rejected=0;
    auto s=make(8);
    auto reject=[&](auto fn) {
        try {fn();}
        catch(const std::invalid_argument&) {++rejected;return;}
        throw std::runtime_error("invalid water MAC input accepted");
    };
    auto audit=[&](double dx=.1,double rho=1000) {
        return upc::p2_water_mac_inventory3d(s,8,8,8,dx,rho);
    };
    reject([&]{audit(0);});
    reject([&]{audit(.1,-1);});
    reject([&]{audit(std::numeric_limits<double>::infinity());});
    reject([&]{upc::p2_water_mac_inventory3d(s,2,8,8,.1,1000);});
    s.water_fill[0]=-0.1;reject([&]{audit();});
    s.water_fill[0]=1.1;reject([&]{audit();});
    s.water_fill[0]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{audit();});
    s=make(8);s.staggered_velocity[0][0]=
        std::numeric_limits<double>::infinity();reject([&]{audit();});
    s=make(8);s.staggered_velocity[1].pop_back();reject([&]{audit();});
    s=make(8);s.staggered_velocity[0][8]=.1;reject([&]{audit();});
    s=make(8);s.staggered_velocity[1][8*8]=.1;reject([&]{audit();});
    s=make(8);s.staggered_velocity[2][8*8*8]=.1;reject([&]{audit();});
    check(rejected==12,"all 12 invalid cases rejected");
    std::cout<<"NEGATIVE "<<rejected<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        uniform(8);uniform(12);
        shear(8);shear(12);
        mixed_curl(8);mixed_curl(12);
        sequence();negatives();
        std::cout<<"PASS: water MAC inventory assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
