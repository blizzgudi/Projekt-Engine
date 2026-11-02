#include "upc/p2_two_phase_mac_vof3d.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
using namespace upc;
namespace {
int assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double tol,const char* msg) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
using State=P2TwoPhaseMacVofState3D;
constexpr double pi=3.14159265358979323846;
double pressure(int x,int y,int z) {
    return .4*x*x-.6*y+.2*y*y+.35*z*z+.13*x*y-.09*y*z;
}
double xy(int x,int y,int z,int nx,int ny,int nz) {
    if(!x||x==nx||!y||y==ny)return 0;
    return .012*std::sin(pi*x/nx)*std::sin(pi*y/ny)*
        std::sin(pi*(z+.5)/nz);
}
double xz(int x,int y,int z,int nx,int ny,int nz) {
    if(!x||x==nx||!z||z==nz)return 0;
    return .009*std::sin(pi*x/nx)*std::sin(pi*z/nz)*
        std::sin(pi*(y+.5)/ny);
}
std::size_t ci(int x,int y,int z,int nx,int ny){
    return (z*ny+y)*nx+x;
}
std::size_t ui(int x,int y,int z,int nx,int ny){
    return (z*ny+y)*(nx+1)+x;
}
std::size_t vi(int x,int y,int z,int nx,int ny){
    return (z*(ny+1)+y)*nx+x;
}
std::size_t wi(int x,int y,int z,int nx,int ny){
    return (z*ny+y)*nx+x;
}
State make(int nx,int ny,int nz,bool add_pressure=true) {
    State s;s.mac.nx=nx;s.mac.ny=ny;s.mac.nz=nz;
    s.mac.dx_m=.1;s.mac.dt_s=.015;
    s.water_fraction.resize(nx*ny*nz);
    s.mac.density_kg_m3.resize(nx*ny*nz);
    s.mac.u_m_s.assign((nx+1)*ny*nz,0);
    s.mac.v_m_s.assign(nx*(ny+1)*nz,0);
    s.mac.w_m_s.assign(nx*ny*(nz+1),0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto k=ci(x,y,z,nx,ny);
        const double a=.08+((x<nx/2&&z<nz/2)?.72:0)+
            .04*y/(ny-1.0);
        s.water_fraction[k]=a;
        s.mac.density_kg_m3[k]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*a;
    }
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=1;x<nx;++x) {
        const auto a=ci(x-1,y,z,nx,ny),b=ci(x,y,z,nx,ny);
        const double beta=2/(s.mac.density_kg_m3[a]+s.mac.density_kg_m3[b]);
        s.mac.u_m_s[ui(x,y,z,nx,ny)]=
            (xy(x,y+1,z,nx,ny,nz)-xy(x,y,z,nx,ny,nz)-
             xz(x,y,z+1,nx,ny,nz)+xz(x,y,z,nx,ny,nz))/.1+
            (add_pressure?s.mac.dt_s*beta*(pressure(x,y,z)-pressure(x-1,y,z))/.1:0);
    }
    for(int z=0;z<nz;++z)for(int y=1;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto a=ci(x,y-1,z,nx,ny),b=ci(x,y,z,nx,ny);
        const double beta=2/(s.mac.density_kg_m3[a]+s.mac.density_kg_m3[b]);
        s.mac.v_m_s[vi(x,y,z,nx,ny)]=
            -(xy(x+1,y,z,nx,ny,nz)-xy(x,y,z,nx,ny,nz))/.1+
            (add_pressure?s.mac.dt_s*beta*(pressure(x,y,z)-pressure(x,y-1,z))/.1:0);
    }
    for(int z=1;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto a=ci(x,y,z-1,nx,ny),b=ci(x,y,z,nx,ny);
        const double beta=2/(s.mac.density_kg_m3[a]+s.mac.density_kg_m3[b]);
        s.mac.w_m_s[wi(x,y,z,nx,ny)]=
            (xz(x+1,y,z,nx,ny,nz)-xz(x,y,z,nx,ny,nz))/.1+
            (add_pressure?s.mac.dt_s*beta*(pressure(x,y,z)-pressure(x,y,z-1))/.1:0);
    }
    return s;
}
// Independent cell-centric donor flux, not the production face-sweep routine.
std::vector<double> donor(const State& velocity,const std::vector<double>& a) {
    const int nx=int(velocity.mac.nx),ny=int(velocity.mac.ny),
              nz=int(velocity.mac.nz);
    const auto& u=velocity.mac.u_m_s;
    const auto& v=velocity.mac.v_m_s;
    const auto& w=velocity.mac.w_m_s;
    auto out=a;
    const auto value=[&](int x,int y,int z)->double{
        return a[ci(x,y,z,nx,ny)];
    };
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const double c=value(x,y,z);
        const double ux0=u[ui(x,y,z,nx,ny)],ux1=u[ui(x+1,y,z,nx,ny)];
        const double vy0=v[vi(x,y,z,nx,ny)],vy1=v[vi(x,y+1,z,nx,ny)];
        const double wz0=w[wi(x,y,z,nx,ny)],wz1=w[wi(x,y,z+1,nx,ny)];
        const double flux=ux1*(ux1>=0?c:value(std::min(nx-1,x+1),y,z))-
            ux0*(ux0>=0?value(std::max(0,x-1),y,z):c)+
            vy1*(vy1>=0?c:value(x,std::min(ny-1,y+1),z))-
            vy0*(vy0>=0?value(x,std::max(0,y-1),z):c)+
            wz1*(wz1>=0?c:value(x,y,std::min(nz-1,z+1)))-
            wz0*(wz0>=0?value(x,y,std::max(0,z-1)):c);
        out[ci(x,y,z,nx,ny)]=c-velocity.mac.dt_s/velocity.mac.dx_m*flux;
    }
    return out;
}
void run(int nx,int ny,int nz) {
    auto s=make(nx,ny,nz);
    auto curl=make(nx,ny,nz,false);
    const auto initial=s.water_fraction;
    auto reference=initial;
    const double volume=std::pow(s.mac.dx_m,3);
    const double water_volume=volume*std::accumulate(
        initial.begin(),initial.end(),0.0);
    const double expected_water=water_volume*s.water_density_kg_m3;
    const double expected_air=(nx*ny*nz*volume-water_volume)*s.air_density_kg_m3;
    double pmean=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)pmean+=pressure(x,y,z);
    pmean/=nx*ny*nz;
    double max_pressure_error=0,max_velocity_error=0;
    double max_vof_error=0,max_mass_residual=0,max_div=0,max_cfl=0;
    int first_iterations=0;
    for(int step=0;step<8;++step) {
        reference=donor(curl,reference);
        const auto l=p2_two_phase_mac_vof_step3d(s);
        if(step==0) {
            first_iterations=l.pressure.pcg_iterations;
            check(first_iterations>0,"Initial variable-density pressure not solved");
            for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
            for(int x=0;x<nx;++x) {
                const auto k=ci(x,y,z,nx,ny);
                max_pressure_error=std::max(max_pressure_error,
                    std::abs(l.pressure.pressure_pa[k]-
                    (pressure(x,y,z)-pmean)));
            }
        }
        near(l.water_mass_before_kg,expected_water,1e-10,"Water before mass");
        near(l.water_mass_after_kg,expected_water,1e-10,"Water after mass");
        near(l.air_mass_before_kg,expected_air,1e-11,"Air before mass");
        near(l.air_mass_after_kg,expected_air,1e-11,"Air after mass");
        near(l.pressure.total_mass_kg,expected_water+expected_air,1e-10,
             "Pressure phase inventory");
        max_mass_residual=std::max(max_mass_residual,
            std::abs(l.total_mass_residual_kg));
        max_div=std::max(max_div,l.pressure.divergence_after_s_inv);
        max_cfl=std::max(max_cfl,l.vof.max_outgoing_courant);
        for(std::size_t i=0;i<reference.size();++i) {
            max_vof_error=std::max(max_vof_error,
                std::abs(reference[i]-s.water_fraction[i]));
            near(s.water_fraction[i],reference[i],1e-9,
                 "Independent 3D moving VOF donor reference");
            near(s.mac.density_kg_m3[i],s.air_density_kg_m3+
                (s.water_density_kg_m3-s.air_density_kg_m3)*reference[i],
                1e-6,"Updated two-phase mixture density");
        }
        for(std::size_t i=0;i<curl.mac.u_m_s.size();++i) {
            max_velocity_error=std::max(max_velocity_error,
                std::abs(s.mac.u_m_s[i]-curl.mac.u_m_s[i]));
            near(s.mac.u_m_s[i],curl.mac.u_m_s[i],1e-7,"Curl U");
        }
        for(std::size_t i=0;i<curl.mac.v_m_s.size();++i) {
            max_velocity_error=std::max(max_velocity_error,
                std::abs(s.mac.v_m_s[i]-curl.mac.v_m_s[i]));
            near(s.mac.v_m_s[i],curl.mac.v_m_s[i],1e-7,"Curl V");
        }
        for(std::size_t i=0;i<curl.mac.w_m_s.size();++i) {
            max_velocity_error=std::max(max_velocity_error,
                std::abs(s.mac.w_m_s[i]-curl.mac.w_m_s[i]));
            near(s.mac.w_m_s[i],curl.mac.w_m_s[i],1e-7,"Curl W");
        }
    }
    const auto at=[&](int x,int y,int z) {
        return ci(x,y,z,nx,ny);
    };
    double moved=0;
    for(std::size_t i=0;i<initial.size();++i)
        moved=std::max(moved,std::abs(initial[i]-s.water_fraction[i]));
    check(moved>1e-4,"VOF interface did not move");
    check(max_pressure_error<1e-6,"Manufactured pressure recovery");
    check(max_velocity_error<1e-7,"Projected 3D curl recovery");
    check(max_vof_error<1e-9,"Independent moving VOF mismatch");
    check(max_div<1e-9,"Two-phase divergence after projection");
    check(max_mass_residual<1e-10,"Two-phase total mass residual");
    check(std::abs(s.mac.w_m_s[wi(1,1,1,nx,ny)])>1e-5,
          "Vertical 3D velocity absent");
    std::cout<<std::setprecision(17)<<"MOVING_TWO_PHASE_VOF grid="
        <<nx<<"x"<<ny<<"x"<<nz<<" steps=8 water_mass="<<expected_water
        <<" air_mass="<<expected_air<<" mass_residual="<<max_mass_residual
        <<" pmax="<<max_pressure_error<<" u_err="<<max_velocity_error
        <<" vof_err="<<max_vof_error<<" vof_delta="<<moved
        <<" w111="<<s.mac.w_m_s[wi(1,1,1,nx,ny)]
        <<" f000="<<s.water_fraction[at(0,0,0)]
        <<" fmid="<<s.water_fraction[at(nx/2,ny/2,nz/2)]
        <<" flast="<<s.water_fraction.back()
        <<" div_after="<<max_div<<" max_courant="<<max_cfl
        <<" pcg_iterations="<<first_iterations<<"\n";
}
void reject_invalid() {
    const auto base=make(6,5,4);
    auto expect=[&](State bad,const char* what) {
        const auto original=bad;
        bool threw=false;
        try{p2_two_phase_mac_vof_step3d(bad);}
        catch(const std::exception&){threw=true;}
        check(threw,what);
        const auto same=[](const std::vector<double>& a,const std::vector<double>& b){
            if(a.size()!=b.size())return false;
            for(std::size_t i=0;i<a.size();++i)
                if(a[i]!=b[i]&&!(std::isnan(a[i])&&std::isnan(b[i])))return false;
            return true;
        };
        check(same(bad.mac.u_m_s,original.mac.u_m_s)&&
              same(bad.mac.v_m_s,original.mac.v_m_s)&&
              same(bad.mac.w_m_s,original.mac.w_m_s)&&
              same(bad.mac.density_kg_m3,original.mac.density_kg_m3)&&
              same(bad.water_fraction,original.water_fraction),
              "Failure changed two-phase state");
    };
    auto bad=base;bad.water_fraction[0]=1.1;
    expect(bad,"Invalid alpha accepted");
    bad=base;bad.mac.density_kg_m3[0]+=1;
    expect(bad,"Inconsistent density accepted");
    bad=base;bad.water_density_kg_m3=0;
    expect(bad,"Invalid water density accepted");
    bad=base;bad.mac.u_m_s[0]=.1;
    expect(bad,"Wall leak accepted");
    bad=base;bad.mac.v_m_s[0]=std::numeric_limits<double>::quiet_NaN();
    expect(bad,"Nonfinite velocity accepted");
    bad=make(6,5,4,false);bad.mac.dt_s=10;
    expect(bad,"Unstable outgoing VOF CFL accepted");
    bad=base;
    const auto original=bad;
    bool threw=false;
    try{p2_two_phase_mac_vof_step3d(bad,1);}
    catch(const std::exception&){threw=true;}
    check(threw,"Under-iterated two-phase PCG accepted");
    check(bad.water_fraction==original.water_fraction&&
          bad.mac.u_m_s==original.mac.u_m_s,
          "PCG failure changed two-phase state");
}
}
int main() {
    try {
        run(4,4,4);run(6,5,4);run(8,6,5);
        reject_invalid();
        std::cout<<"PASS: MOVING_TWO_PHASE_VOF assertions="<<assertions<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
