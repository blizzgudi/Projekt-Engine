// P2 PARTIAL: independent SI-unit periodic MAC bulk momentum/energy audit.
// Full-domain constant-density quadrature, NOT water-only energy or FSI.
#include "upc/p2_viscous_mac_plic_sequence3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
constexpr double pi=3.14159265358979323846;
std::size_t checks=0;
void check(bool value,const char* label) {
    ++checks;
    if(!value)throw std::runtime_error(label);
}
void near(double a,double b,double tol,const char* label) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,label);
}
using State=upc::P2NonlinearMacPlicState3D;
using Interval=upc::P2ViscousMacPlicInterval3D;
State setup(int n,bool full) {
    State s;
    s.staggered_velocity={
        std::vector<double>((n+1)*n*n,0),
        std::vector<double>(n*(n+1)*n,0),
        std::vector<double>(n*n*(n+1),0)};
    s.water_fill.assign(n*n*n,full?1.0:0.0);
    for(int k=0;k<n;++k)for(int j=0;j<n;++j)
    for(int i=0;i<=n;++i)
        s.staggered_velocity[0][(k*n+j)*(n+1)+i]=
            .02+.04*std::sin(2*pi*j/n);
    if(!full)
        for(int k=0;k<n;++k)for(int j=0;j<n;++j)
        for(int i=0;i<n;++i)
            s.water_fill[(k*n+j)*n+i]=(i>=2&&i<5)?1.0:0.0;
    return s;
}
const std::vector<Interval> schedule{{.2,.002},{.4,.004},{.15,0.0}};
void eigen(int n) {
    const auto s=setup(n,true);
    const auto initial=s;
    const auto result=upc::p2_viscous_mac_plic_sequence3d(
        s,schedule,n,n,n,.1,1000,.4,128,128);
    const double cell_mass=1000*.1*.1*.1;
    const double n3=static_cast<double>(n)*n*n;
    const double expected_momentum=n3*cell_mass*.02;
    double factor=1.0, max_error=0.0;
    for(std::size_t t=0;t<schedule.size();++t) {
        const auto& interval=schedule[t];
        const auto& row=result.intervals[t];
        const double before_energy=.5*n3*cell_mass*
            (.02*.02+.5*.04*.04*factor*factor);
        near(row.bulk_before.kinetic_energy_j,before_energy,3e-9,
             "independent pre-step kinetic Fourier reference");
        const double a=interval.nu_m2_s*interval.dt_s/(.1*.1);
        if(a>0) {
            const auto count=static_cast<int>(std::ceil(a*8-1e-14));
            factor*=std::pow(1-4*(a/count)*std::pow(std::sin(pi/n),2),count);
        }
        const double after_energy=.5*n3*cell_mass*
            (.02*.02+.5*.04*.04*factor*factor);
        max_error=std::max(max_error,
            std::abs(row.bulk_after.kinetic_energy_j-after_energy));
        near(row.bulk_after.kinetic_energy_j,after_energy,3e-9,
             "independent post-step kinetic Fourier reference");
        near(row.bulk_kinetic_delta_j,after_energy-before_energy,3e-9,
             "independent kinetic energy delta");
        for(int c=0;c<3;++c) {
            const double expected=(c==0)?expected_momentum:0.0;
            near(row.bulk_before.momentum_kg_m_s[c],expected,3e-9,
                 "periodic momentum before");
            near(row.bulk_after.momentum_kg_m_s[c],expected,3e-9,
                 "periodic momentum after");
            near(row.bulk_momentum_delta_kg_m_s[c],0,3e-9,
                 "periodic momentum delta");
        }
        if(interval.nu_m2_s>0)
            check(row.bulk_kinetic_delta_j<0,"positive viscosity dissipates");
        else
            near(row.bulk_kinetic_delta_j,0,3e-9,"inviscid shear invariant");
    }
    near(result.bulk_initial.kinetic_energy_j,
         .5*n3*cell_mass*(.02*.02+.5*.04*.04),3e-9,
         "initial total bulk kinetic energy");
    near(result.bulk_final.kinetic_energy_j,
         .5*n3*cell_mass*(.02*.02+.5*.04*.04*factor*factor),3e-9,
         "final total bulk kinetic energy");
    near(result.bulk_energy_telescoping_residual_j,0,3e-9,
         "sequence kinetic telescoping closure");
    for(double residual:result.bulk_momentum_telescoping_residual_kg_m_s)
        near(residual,0,3e-9,"sequence momentum telescoping closure");
    check(s.staggered_velocity==initial.staggered_velocity&&
          s.water_fill==initial.water_fill,"input unchanged");
    std::cout<<"FOURIER "<<n<<" "<<expected_momentum<<" "
             <<result.bulk_initial.kinetic_energy_j<<" "
             <<result.bulk_final.kinetic_energy_j<<" "
             <<max_error<<" "<<result.bulk_energy_telescoping_residual_j<<"\n";
}
void mixed() {
    const int n=12;
    const auto s=setup(n,false);
    const auto result=upc::p2_viscous_mac_plic_sequence3d(
        s,{{.04,.005},{.03,.002},{.02,0}},n,n,n,.1,1000);
    near(result.initial_water_mass_kg,432,2e-8,"water-only mass");
    near(result.final_water_mass_kg,432,2e-8,"water-only mass conserved");
    check(result.bulk_initial.kinetic_energy_j>
          result.initial_water_mass_kg*.5*.02*.02,
          "bulk diagnostic includes modeled nonwater cells");
    near(result.bulk_energy_telescoping_residual_j,0,1e-8,
         "mixed fill bulk telescoping");
    for(double residual:result.bulk_momentum_telescoping_residual_kg_m_s)
        near(residual,0,1e-8,"mixed fill momentum telescoping");
    std::cout<<"MIXED "<<result.initial_water_mass_kg<<" "
             <<result.final_water_mass_kg<<" "
             <<result.bulk_initial.kinetic_energy_j<<" "
             <<result.bulk_final.kinetic_energy_j<<" "
             <<result.bulk_energy_telescoping_residual_j<<"\n";
}
void negatives() {
    int rejected=0;
    auto reject=[&](auto fn) {
        try {fn();}
        catch(const std::invalid_argument&) {++rejected;return;}
        throw std::runtime_error("invalid bulk diagnostic accepted");
    };
    auto s=setup(8,true);
    auto budget=[&](double dx=.1,double rho=1000) {
        return upc::p2_periodic_mac_bulk_budget3d(s,8,8,8,dx,rho);
    };
    reject([&]{budget(0);});
    reject([&]{budget(.1,0);});
    reject([&]{budget(std::numeric_limits<double>::infinity());});
    reject([&]{budget(.1,std::numeric_limits<double>::quiet_NaN());});
    s.staggered_velocity[0][8]+=.1;
    reject([&]{budget();});
    s=setup(8,true);
    s.staggered_velocity[1][0]=std::numeric_limits<double>::infinity();
    reject([&]{budget();});
    s=setup(8,true);
    s.staggered_velocity[2].pop_back();
    reject([&]{budget();});
    s=setup(8,true);
    reject([&]{upc::p2_periodic_mac_bulk_budget3d(s,2,8,8,.1,1000);});
    check(rejected==8,"eight invalid bulk diagnostic inputs");
    std::cout<<"NEGATIVE "<<rejected<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        eigen(8);
        eigen(12);
        mixed();
        negatives();
        std::cout<<"PASS: periodic MAC bulk budget assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<checks<<"\n";
        return 1;
    }
}
