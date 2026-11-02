// P2 partial: real staggered MAC full symmetric tensor viscosity.
// Independent Fourier-mode and global energy/normal-wall-impulse checks.
// Not an experimentally validated sharp-interface stress/FSI solver.
#include "upc/p2_two_phase_full_tensor_viscous_mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t checks=0;
void check(bool ok,const char* why) {
    ++checks;
    if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    if(!(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol))
        std::cerr<<std::setprecision(17)<<"MISMATCH "<<why
                 <<" got="<<a<<" want="<<b<<" tol="<<tol<<"\n";
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
template<class Fn>void rejected(Fn fn,const char* why) {
    bool fail=false;
    try{fn();}catch(const std::exception&){fail=true;}
    check(fail,why);
}
struct Case{
    upc::P2TwoPhaseMacPressureGrid3D mac;
    std::vector<double> water;
};
Case make(int nx,int ny,int nz,bool heterogeneous) {
    Case s;
    auto& m=s.mac;m.nx=nx;m.ny=ny;m.nz=nz;m.dx_m=.1;m.dt_s=.003;
    s.water.resize(nx*ny*nz);
    m.density_kg_m3.resize(nx*ny*nz);
    m.u_m_s.assign((nx+1)*ny*nz,0);
    m.v_m_s.assign(nx*(ny+1)*nz,0);
    m.w_m_s.assign(nx*ny*(nz+1),0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto i=(z*ny+y)*nx+x;
        const double alpha=heterogeneous?
            .03+.8*(x+y+z<(nx+ny+nz)/2?.9:.12):.5;
        s.water[i]=alpha;
        m.density_kg_m3[i]=heterogeneous?1.2+998.8*alpha:700.;
    }
    constexpr double pi=3.14159265358979323846;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=1;x<nx;++x) {
        m.u_m_s[(z*ny+y)*(nx+1)+x]=
            .06*std::sin(pi*x/nx)*std::cos(pi*(y+.5)/ny)*
            (heterogeneous?(1+.3*std::cos(pi*(z+.5)/nz)):1);
    }
    if(heterogeneous){
        for(int z=0;z<nz;++z)for(int y=1;y<ny;++y)
        for(int x=0;x<nx;++x)
            m.v_m_s[(z*(ny+1)+y)*nx+x]=
                .04*std::sin(pi*y/ny)*std::cos(pi*(z+.5)/nz);
        for(int z=1;z<nz;++z)for(int y=0;y<ny;++y)
        for(int x=0;x<nx;++x)
            m.w_m_s[(z*ny+y)*nx+x]=
                .03*std::sin(pi*z/nz)*std::cos(pi*(x+.5)/nx);
    }
    return s;
}
double independent_energy(const upc::P2TwoPhaseMacPressureGrid3D& m) {
    const auto nx=m.nx,ny=m.ny,nz=m.nz;
    const double vol=std::pow(m.dx_m,3);
    const auto ci=[&](std::size_t x,std::size_t y,std::size_t z){
        return (z*ny+y)*nx+x;};
    double sum=0;
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=1;x<nx;++x) {
        const double vel=m.u_m_s[(z*ny+y)*(nx+1)+x];
        sum+=.25*vol*(m.density_kg_m3[ci(x-1,y,z)]+
                       m.density_kg_m3[ci(x,y,z)])*vel*vel;
    }
    for(std::size_t z=0;z<nz;++z)for(std::size_t y=1;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double vel=m.v_m_s[(z*(ny+1)+y)*nx+x];
        sum+=.25*vol*(m.density_kg_m3[ci(x,y-1,z)]+
                       m.density_kg_m3[ci(x,y,z)])*vel*vel;
    }
    for(std::size_t z=1;z<nz;++z)for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double vel=m.w_m_s[(z*ny+y)*nx+x];
        sum+=.25*vol*(m.density_kg_m3[ci(x,y,z-1)]+
                       m.density_kg_m3[ci(x,y,z)])*vel*vel;
    }
    return sum;
}
void homogeneous_fourier(int nx,int ny,int nz) {
    auto c=make(nx,ny,nz,false);
    const auto original=c.mac;
    constexpr double rho=700., mu=.51,pi=3.14159265358979323846;
    const double lambda_x=4*std::pow(
        std::sin(pi/(2*nx)),2)/std::pow(c.mac.dx_m,2);
    const double lambda_y=4*std::pow(
        std::sin(pi/(2*ny)),2)/std::pow(c.mac.dx_m,2);
    const double factor=1-c.mac.dt_s*mu/rho*(2*lambda_x+lambda_y);
    const double original_energy=independent_energy(c.mac);
    const auto audit=upc::p2_two_phase_full_tensor_viscous_mac_step3d(
        c.mac,c.water,mu,mu);
    double max_reference_error=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=1;x<nx;++x) {
        const auto i=(z*ny+y)*(nx+1)+x;
        const double analytical=factor*original.u_m_s[i];
        max_reference_error=std::max(max_reference_error,
            std::abs(c.mac.u_m_s[i]-analytical));
        near(c.mac.u_m_s[i],analytical,4e-14,
             "3D MAC full-tensor manufactured Fourier u-mode");
    }
    near(audit.kinetic_before_j,original_energy,1e-12,
         "independent normal/shear kinetic energy before");
    near(audit.kinetic_after_j,independent_energy(c.mac),1e-12,
         "independent kinetic after");
    near(audit.energy_residual_j,0,1e-10,
         "full symmetric viscosity discrete energy identity");
    check(audit.stress_dissipation_rate_w>0,
          "nontrivial 3D symmetric stress should dissipate");
    check(audit.kinetic_after_j<audit.kinetic_before_j,
          "full viscous tensor cannot generate energy");
    check(audit.xy_shear_edges==(std::size_t)(nx-1)*(ny-1)*nz,
          "XY tensor stress edge count");
    check(audit.xz_shear_edges==(std::size_t)(nx-1)*ny*(nz-1),
          "XZ tensor stress edge count");
    check(audit.yz_shear_edges==(std::size_t)nx*(ny-1)*(nz-1),
          "YZ tensor stress edge count");
    check(!audit.physical_full_two_phase_stress_validated,
          "must not claim full stress-continuity validation");
    for(int i=0;i<3;++i)
        near(audit.momentum_residual_kg_m_s[i],0,1e-11,
             "wall traction closes actual MAC impulse");
    std::cout<<std::setprecision(17)
             <<"SYMMETRIC_MAC Fourier grid="<<nx<<"x"<<ny<<"x"<<nz
             <<" error_m_s="<<max_reference_error
             <<" dissipation_w="<<audit.stress_dissipation_rate_w
             <<" energy_residual_j="<<audit.energy_residual_j
             <<" phase_complete=false\n";
}
void heterogeneous(int nx,int ny,int nz) {
    auto c=make(nx,ny,nz,true);
    const auto old_water=c.water,old_rho=c.mac.density_kg_m3;
    const auto initial=independent_energy(c.mac);
    double max_stress_change=0,max_energy_error=0,max_wall_error=0;
    for(int i=0;i<9;++i) {
        const auto audit=upc::p2_two_phase_full_tensor_viscous_mac_step3d(
            c.mac,c.water,.0013,.000018);
        check(audit.stress_dissipation_rate_w>0,
              "3D heterogeneous two-phase stress dissipates");
        check(audit.kinetic_after_j<=audit.kinetic_before_j+1e-12,
              "non-increasing explicit viscosity kinetic energy");
        near(audit.kinetic_after_j,independent_energy(c.mac),1e-11,
             "heterogeneous 3D independent energy after");
        max_stress_change=std::max(max_stress_change,
                                    audit.max_velocity_change_m_s);
        max_energy_error=std::max(max_energy_error,
            std::abs(audit.energy_residual_j));
        for(int k=0;k<3;++k) {
            max_wall_error=std::max(max_wall_error,
                std::abs(audit.momentum_residual_kg_m_s[k]));
            near(audit.momentum_after_kg_m_s[k]-
                 audit.momentum_before_kg_m_s[k],
                 audit.wall_normal_traction_impulse_n_s[k],1e-10,
                 "all three wall reaction impulse components");
        }
    }
    check(max_stress_change>1e-9,"heterogeneous 3D stress velocity moved");
    check(c.mac.density_kg_m3==old_rho&&c.water==old_water,
          "3D viscosity does not fabricate mass or phase transfer");
    check(independent_energy(c.mac)<initial,
          "full symmetric two-phase stress dissipates energy over 9 steps");
    std::cout<<std::setprecision(17)
             <<"SYMMETRIC_MAC hetero_grid="<<nx<<"x"<<ny<<"x"<<nz
             <<" steps=9 max_velocity_delta="<<max_stress_change
             <<" max_energy_residual_j="<<max_energy_error
             <<" max_wall_impulse_residual="<<max_wall_error
             <<" phase_complete=false\n";
}
void negative() {
    auto valid=make(6,5,4,true);
    const auto original=valid;
    auto trial=valid;
    trial.mac.dt_s=1000;
    const auto before=trial.mac.u_m_s;
    rejected([&]{(void)upc::p2_two_phase_full_tensor_viscous_mac_step3d(
        trial.mac,trial.water,.002,.000018);},
        "explicit stability bound rejects unstable viscous dt");
    check(trial.mac.u_m_s==before,"unstable dt does not change MAC state");
    trial=valid;trial.water[4]=std::numeric_limits<double>::quiet_NaN();
    rejected([&]{(void)upc::p2_two_phase_full_tensor_viscous_mac_step3d(
        trial.mac,trial.water);},"NaN water fraction rejected");
    trial=valid;trial.mac.density_kg_m3[0]=-1;
    rejected([&]{(void)upc::p2_two_phase_full_tensor_viscous_mac_step3d(
        trial.mac,trial.water);},"negative density rejected");
    trial=valid;trial.mac.u_m_s[0]=.08;
    rejected([&]{(void)upc::p2_two_phase_full_tensor_viscous_mac_step3d(
        trial.mac,trial.water);},"nonzero impermeable wall rejected");
    trial=valid;trial.mac.v_m_s[14]=std::numeric_limits<double>::infinity();
    rejected([&]{(void)upc::p2_two_phase_full_tensor_viscous_mac_step3d(
        trial.mac,trial.water);},"nonfinite velocity rejected");
    trial=valid;
    rejected([&]{(void)upc::p2_two_phase_full_tensor_viscous_mac_step3d(
        trial.mac,trial.water,0,.000018);},"zero water viscosity rejected");
    trial=valid;
    rejected([&]{(void)upc::p2_two_phase_full_tensor_viscous_mac_step3d(
        trial.mac,trial.water,.001,-1);},"negative air viscosity rejected");
    check(valid.mac.u_m_s==original.mac.u_m_s&&
          valid.mac.v_m_s==original.mac.v_m_s&&
          valid.mac.w_m_s==original.mac.w_m_s&&
          valid.water==original.water,
          "all negative controls leave original live state unchanged");
}
}
int main() {
    try{
        homogeneous_fourier(4,4,4);
        homogeneous_fourier(6,5,4);
        homogeneous_fourier(8,6,5);
        heterogeneous(4,4,4);
        heterogeneous(6,5,4);
        heterogeneous(8,6,5);
        negative();
        std::cout<<"PASS P2_TWO_PHASE_FULL_TENSOR_VISCOUS_MAC assertions="
                 <<checks<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_TWO_PHASE_FULL_TENSOR_VISCOUS_MAC "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
