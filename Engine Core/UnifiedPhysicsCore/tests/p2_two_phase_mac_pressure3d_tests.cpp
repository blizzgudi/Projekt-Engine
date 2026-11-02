#include "upc/p2_two_phase_mac_pressure3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace upc;
namespace {
int assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;if(!ok)throw std::runtime_error(msg);
}
void near(double actual,double expected,double tol,const char* msg){
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,msg);
}
void rejects(const std::function<void()>& f){
    bool caught=false;try{f();}catch(const std::exception&){caught=true;}
    check(caught,"Expected rejection");
}
using Grid=P2TwoPhaseMacPressureGrid3D;
Grid make_grid(int nx,int ny,int nz){
    Grid g;g.nx=nx;g.ny=ny;g.nz=nz;g.dx_m=.08;g.dt_s=.015;
    g.density_kg_m3.resize(nx*ny*nz);
    g.u_m_s.resize((nx+1)*ny*nz,0);
    g.v_m_s.resize(nx*(ny+1)*nz,0);
    g.w_m_s.resize(nx*ny*(nz+1),0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)
        g.density_kg_m3[(z*ny+y)*nx+x]=x<nx/2?
            985+2*z+3*y+.5*x:1.15+.02*y+.03*z;
    return g;
}
double analytic_pressure(int x,int y,int z){
    return .4*x*x-.6*y+.2*y*y+.35*z*z+.13*x*y-.09*y*z;
}
double psi(int x,int y,int z,int nx,int ny){
    if(x==0||x==nx||y==0||y==ny)return 0;
    return .004*(1+.15*z)*std::sin(3.141592653589793*x/nx)*
        std::sin(3.141592653589793*y/ny);
}
void manufactured(int nx,int ny,int nz){
    auto g=make_grid(nx,ny,nz);
    const auto ci=[&](int x,int y,int z){return (z*ny+y)*nx+x;};
    const auto ui=[&](int x,int y,int z){return (z*ny+y)*(nx+1)+x;};
    const auto vi=[&](int x,int y,int z){return (z*(ny+1)+y)*nx+x;};
    const auto wi=[&](int x,int y,int z){return (z*ny+y)*nx+x;};
    double max_initial_div=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=1;x<nx;++x){
        const double beta=2/(g.density_kg_m3[ci(x-1,y,z)]+
                             g.density_kg_m3[ci(x,y,z)]);
        g.u_m_s[ui(x,y,z)]=g.dt_s*beta*
            (analytic_pressure(x,y,z)-analytic_pressure(x-1,y,z))/g.dx_m+
            (psi(x,y+1,z,nx,ny)-psi(x,y,z,nx,ny))/g.dx_m;
    }
    for(int z=0;z<nz;++z)for(int y=1;y<ny;++y)for(int x=0;x<nx;++x){
        const double beta=2/(g.density_kg_m3[ci(x,y-1,z)]+
                             g.density_kg_m3[ci(x,y,z)]);
        g.v_m_s[vi(x,y,z)]=g.dt_s*beta*
            (analytic_pressure(x,y,z)-analytic_pressure(x,y-1,z))/g.dx_m-
            (psi(x+1,y,z,nx,ny)-psi(x,y,z,nx,ny))/g.dx_m;
    }
    for(int z=1;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
        const double beta=2/(g.density_kg_m3[ci(x,y,z-1)]+
                             g.density_kg_m3[ci(x,y,z)]);
        g.w_m_s[wi(x,y,z)]=g.dt_s*beta*
            (analytic_pressure(x,y,z)-analytic_pressure(x,y,z-1))/g.dx_m;
    }
    const auto old=g;
    const auto l=p2_two_phase_mac_pressure_step3d(g);
    double expected_mass=0;
    for(double rho:old.density_kg_m3)
        expected_mass+=rho*std::pow(old.dx_m,3);
    near(l.total_mass_kg,expected_mass,1e-11*expected_mass,
         "Two-phase physical mass inventory");
    check(g.density_kg_m3==old.density_kg_m3,
          "Pressure projection modified phase densities");
    check(l.pcg_iterations>0&&l.pcg_iterations<4000,"PCG did not solve");
    check(l.divergence_before_s_inv>1e-3,"Manufactured input not divergent");
    check(l.divergence_after_s_inv<1e-9,"Manufactured pressure divergence");
    check(l.final_pressure_residual_s_inv<1e-8,"Independent PCG residual");
    near(l.energy_residual_j,0,1e-8,"Projection energy orthogonality");
    check(l.projection_dissipation_j>0,"Projection energy loss absent");
    for(double v:l.momentum_residual_n_s)
        near(v,0,1e-9,"Pressure wall impulse does not close");
    double pmean=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)
        pmean+=analytic_pressure(x,y,z);
    pmean/=nx*ny*nz;
    double pmax=0,vmax=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){
        const auto i=ci(x,y,z);
        pmax=std::max(pmax,std::abs(l.pressure_pa[i]-
            (analytic_pressure(x,y,z)-pmean)));
        near(l.pressure_pa[i],analytic_pressure(x,y,z)-pmean,3e-6,
             "Independent manufactured 3D pressure");
    }
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=1;x<nx;++x){
        const double expected=(psi(x,y+1,z,nx,ny)-psi(x,y,z,nx,ny))/g.dx_m;
        vmax=std::max(vmax,std::abs(g.u_m_s[ui(x,y,z)]-expected));
        near(g.u_m_s[ui(x,y,z)],expected,3e-7,
             "Manufactured divergence-free x vortex");
    }
    for(int z=0;z<nz;++z)for(int y=1;y<ny;++y)for(int x=0;x<nx;++x){
        const double expected=-(psi(x+1,y,z,nx,ny)-psi(x,y,z,nx,ny))/g.dx_m;
        vmax=std::max(vmax,std::abs(g.v_m_s[vi(x,y,z)]-expected));
        near(g.v_m_s[vi(x,y,z)],expected,3e-7,
             "Manufactured divergence-free y vortex");
    }
    for(int z=1;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)
        near(g.w_m_s[wi(x,y,z)],0,3e-7,
             "Manufactured zero vertical vortex");
    // Idempotent pressure projection on a verified divergence-free field.
    const auto projected=g;
    const auto again=p2_two_phase_mac_pressure_step3d(g);
    check(again.divergence_after_s_inv<1e-9,"Second projection divergence");
    for(std::size_t i=0;i<g.u_m_s.size();++i)
        near(g.u_m_s[i],projected.u_m_s[i],3e-8,"Idempotent U");
    for(std::size_t i=0;i<g.v_m_s.size();++i)
        near(g.v_m_s[i],projected.v_m_s[i],3e-8,"Idempotent V");
    for(std::size_t i=0;i<g.w_m_s.size();++i)
        near(g.w_m_s[i],projected.w_m_s[i],3e-8,"Idempotent W");
    std::cout<<std::setprecision(17)<<"MANUFACTURED_3D grid="
        <<nx<<"x"<<ny<<"x"<<nz<<" pmax="<<pmax<<" vmax="<<vmax
        <<" div_before="<<l.divergence_before_s_inv
        <<" div_after="<<l.divergence_after_s_inv
        <<" energy_residual="<<l.energy_residual_j
        <<" momentum_residual="<<std::max({std::abs(l.momentum_residual_n_s[0]),
          std::abs(l.momentum_residual_n_s[1]),
          std::abs(l.momentum_residual_n_s[2])})
        <<" p0="<<l.pressure_pa[0]
        <<" pmid="<<l.pressure_pa[ci(nx/2,ny/2,nz/2)]
        <<" plast="<<l.pressure_pa.back()
        <<" u111="<<g.u_m_s[ui(1,1,1)]
        <<" v111="<<g.v_m_s[vi(1,1,1)]
        <<" w111="<<g.w_m_s[wi(1,1,1)]
        <<" pcg_iterations="<<l.pcg_iterations<<"\n";
    (void)old;(void)max_initial_div;
}
void reject_invalid(){
    auto base=make_grid(4,3,3);
    // A deliberately non-gradient, divergence-producing interior face.
    base.u_m_s[(1*3+1)*5+2]=.3;
    const auto unchanged=base;
    auto bad=base;bad.density_kg_m3[0]=0;
    rejects([&]{p2_two_phase_mac_pressure_step3d(bad);});
    check(bad.u_m_s==unchanged.u_m_s,"Bad density changed state");
    bad=base;bad.density_kg_m3[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{p2_two_phase_mac_pressure_step3d(bad);});
    bad=base;bad.u_m_s[0]=.01;
    rejects([&]{p2_two_phase_mac_pressure_step3d(bad);});
    bad=base;bad.w_m_s.pop_back();
    rejects([&]{p2_two_phase_mac_pressure_step3d(bad);});
    bad=base;bad.dt_s=0;
    rejects([&]{p2_two_phase_mac_pressure_step3d(bad);});
    bad=base;bad.v_m_s[0]=std::numeric_limits<double>::infinity();
    rejects([&]{p2_two_phase_mac_pressure_step3d(bad);});
    bad=base;
    rejects([&]{p2_two_phase_mac_pressure_step3d(bad,1);});
    check(bad.u_m_s==unchanged.u_m_s&&bad.v_m_s==unchanged.v_m_s,
          "PCG nonconvergence changed state");
    // Closed stationary equilibrium: no fictitious pressure or energy.
    bad=make_grid(3,3,3);
    const auto zero=p2_two_phase_mac_pressure_step3d(bad);
    check(zero.pcg_iterations==0&&zero.divergence_after_s_inv==0,
          "Zero flow changed under pressure projection");
    near(zero.kinetic_after_j,0,0,"Zero kinetic energy");
}
}
int main(){
    try{
        manufactured(4,3,3);manufactured(6,4,4);manufactured(8,6,5);
        reject_invalid();
        std::cout<<"PASS: TWO_PHASE_MAC_PRESSURE assertions="<<assertions<<"\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
