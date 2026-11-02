#include "upc/p2_two_phase_consistent_momentum3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace upc;
namespace {
int assertions=0;
void check(bool p,const char* msg){++assertions;if(!p)throw std::runtime_error(msg);}
void near(double a,double b,double eps,const char* msg) {
    if(!(std::isfinite(a)&&std::abs(a-b)<=eps)){
        std::cerr<<std::setprecision(17)<<"ERROR "<<msg<<" actual="<<a
                 <<" expected="<<b<<" tolerance="<<eps<<"\n";
    }
    check(std::isfinite(a)&&std::abs(a-b)<=eps,msg);
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
struct Case {
    P2TwoPhaseMacPressureGrid3D m;
    std::vector<double> alpha;
    std::vector<std::array<double,3>> q;
};
Case make(int nx,int ny,int nz,bool constant) {
    constexpr double pi=3.14159265358979323846;
    Case c;
    auto& m=c.m;
    m.nx=nx;m.ny=ny;m.nz=nz;
    m.dx_m=.11;m.dt_s=.003;
    m.density_kg_m3.resize(nx*ny*nz);
    m.u_m_s.assign((nx+1)*ny*nz,0);
    m.v_m_s.assign(nx*(ny+1)*nz,0);
    m.w_m_s.assign(nx*ny*(nz+1),0);
    c.alpha.resize(nx*ny*nz);
    c.q.resize(nx*ny*nz);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto i=ci(x,y,z,nx,ny);
        c.alpha[i]=.14+.56*(x<nx/2&&y<ny/2?1:0)+.016*z;
        m.density_kg_m3[i]=1.2+998.8*c.alpha[i];
        c.q[i]=constant?std::array<double,3>{.12,-.09,.035}:
            std::array<double,3>{
                .1+.05*std::sin(x*.41+y*.21+z*.15),
                -.06+.045*std::cos(y*.43-z*.27),
                .023+.03*std::sin(z*.55+x*.19)};
    }
    auto psi=[&](int x,int y,int z) {
        return .013*std::sin(pi*x/nx)*std::sin(pi*y/ny)*
            (1+.2*std::sin(pi*(z+.5)/nz));
    };
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=1;x<nx;++x)
        m.u_m_s[ui(x,y,z,nx,ny)]=
            (psi(x,y+1,z)-psi(x,y,z))/m.dx_m;
    for(int z=0;z<nz;++z)for(int y=1;y<ny;++y)
    for(int x=0;x<nx;++x)
        m.v_m_s[vi(x,y,z,nx,ny)]=
            -(psi(x+1,y,z)-psi(x,y,z))/m.dx_m;
    return c;
}
// Independent per-cell divergence-form FV baseline. Never invokes
// production conservative momentum or VOF face-sweep functions.
struct Independent {
    std::vector<double> alpha,rho;
    std::vector<std::array<double,3>> q;
};
Independent reference(const Case& c) {
    const int nx=int(c.m.nx),ny=int(c.m.ny),nz=int(c.m.nz);
    const auto& m=c.m;
    Independent out{c.alpha,m.density_kg_m3,c.q};
    const auto flux=[&](double face,int ax,int x,int y,int z,
                        double& mass,std::array<double,3>& momentum) {
        if(face==0)return;
        const int bx=x+(ax==0),by=y+(ax==1),bz=z+(ax==2);
        const int donor=face>0?ci(x,y,z,nx,ny):ci(bx,by,bz,nx,ny);
        const double rho=m.density_kg_m3[donor];
        const double mflow=rho*face;
        mass+=mflow;
        for(int k=0;k<3;++k)momentum[k]+=mflow*c.q[donor][k];
    };
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto cell=ci(x,y,z,nx,ny);
        double mass_div=0;
        std::array<double,3> momentum_div{};
        auto add=[&](double face,int ax,int xx,int yy,int zz,int sign) {
            double mf=0;std::array<double,3> mom{};
            flux(face,ax,xx,yy,zz,mf,mom);
            mass_div+=sign*mf;
            for(int k=0;k<3;++k)momentum_div[k]+=sign*mom[k];
        };
        if(x+1<nx)add(m.u_m_s[ui(x+1,y,z,nx,ny)],0,x,y,z,1);
        if(x>0)add(m.u_m_s[ui(x,y,z,nx,ny)],0,x-1,y,z,-1);
        if(y+1<ny)add(m.v_m_s[vi(x,y+1,z,nx,ny)],1,x,y,z,1);
        if(y>0)add(m.v_m_s[vi(x,y,z,nx,ny)],1,x,y-1,z,-1);
        if(z+1<nz)add(m.w_m_s[wi(x,y,z+1,nx,ny)],2,x,y,z,1);
        if(z>0)add(m.w_m_s[wi(x,y,z,nx,ny)],2,x,y,z-1,-1);
        const double factor=m.dt_s/m.dx_m;
        out.rho[cell]-=factor*mass_div;
        double alpha_div=0;
        auto aflux=[&](double face,int a,int x0,int y0,int z0,int sign){
            if(face==0)return;
            const int bx=x0+(a==0),by=y0+(a==1),bz=z0+(a==2);
            const auto donor=face>0?ci(x0,y0,z0,nx,ny):ci(bx,by,bz,nx,ny);
            alpha_div+=sign*face*c.alpha[donor];
        };
        if(x+1<nx)aflux(m.u_m_s[ui(x+1,y,z,nx,ny)],0,x,y,z,1);
        if(x>0)aflux(m.u_m_s[ui(x,y,z,nx,ny)],0,x-1,y,z,-1);
        if(y+1<ny)aflux(m.v_m_s[vi(x,y+1,z,nx,ny)],1,x,y,z,1);
        if(y>0)aflux(m.v_m_s[vi(x,y,z,nx,ny)],1,x,y-1,z,-1);
        if(z+1<nz)aflux(m.w_m_s[wi(x,y,z+1,nx,ny)],2,x,y,z,1);
        if(z>0)aflux(m.w_m_s[wi(x,y,z,nx,ny)],2,x,y,z-1,-1);
        out.alpha[cell]-=factor*alpha_div;
        for(int k=0;k<3;++k)
            out.q[cell][k]=(m.density_kg_m3[cell]*c.q[cell][k]-
                            factor*momentum_div[k])/out.rho[cell];
    }
    return out;
}
void run(int nx,int ny,int nz,bool constant) {
    auto c=make(nx,ny,nz,constant);
    const auto original=c;
    double max_error=0,max_impulse=0,max_loss=0;
    for(int step=0;step<7;++step) {
        const auto expected=reference(c);
        const auto actual=p2_two_phase_consistent_momentum3d(
            c.m,c.alpha,c.q,1000,1.2);
        near(actual.mass_after_kg,actual.mass_before_kg,2e-10,
             "mass-conserving 3D donor transport");
        near(actual.water_mass_after_kg,actual.water_mass_before_kg,2e-10,
             "water conserved");
        near(actual.air_mass_after_kg,actual.air_mass_before_kg,2e-10,
             "air conserved");
        check(actual.max_divergence_s_inv<1e-12,
              "3D projected face field must be divergence-free");
        check(actual.max_outgoing_courant<1,
              "strict six-face outgoing transport CFL");
        check(actual.active_mass_faces>0,"must transport in 3D");
        check(actual.upwind_kinetic_loss_j>=-1e-11,
              "mass-consistent momentum donor flux cannot create kinetic energy");
        if(!constant)
            check(actual.upwind_kinetic_loss_j>0,
                  "inhomogeneous velocity must dissipate kinetic energy");
        else near(actual.upwind_kinetic_loss_j,0,1e-9,
                  "constant cell velocity no artificial dissipation");
        max_loss=std::max(max_loss,std::abs(actual.upwind_kinetic_loss_j));
        for(int k=0;k<3;++k){
            max_impulse=std::max(max_impulse,
                std::abs(actual.internal_flux_impulse_residual_kg_m_s[k]));
            near(actual.internal_flux_impulse_residual_kg_m_s[k],0,1e-10,
                 "internal face cannot create net momentum");
        }
        for(std::size_t i=0;i<actual.water_fraction.size();++i) {
            max_error=std::max({max_error,
                std::abs(actual.water_fraction[i]-expected.alpha[i]),
                std::abs(actual.density_kg_m3[i]-expected.rho[i])});
            near(actual.water_fraction[i],expected.alpha[i],1e-12,
                 "independent donor 3D VOF reference");
            near(actual.density_kg_m3[i],expected.rho[i],1e-9,
                 "independent donor mixture density reference");
            for(int k=0;k<3;++k) {
                max_error=std::max(max_error,
                   std::abs(actual.velocity_m_s[i][k]-expected.q[i][k]));
                near(actual.velocity_m_s[i][k],expected.q[i][k],2e-12,
                     "independent 3D momentum divergence reference");
                if(constant)
                    near(actual.velocity_m_s[i][k],original.q[i][k],2e-12,
                         "uniform velocity must not be shifted by variable density");
            }
        }
        c.alpha=actual.water_fraction;
        c.m.density_kg_m3=actual.density_kg_m3;
        c.q=actual.velocity_m_s;
    }
    check(c.alpha!=original.alpha,"genuine moving 3D water/air mixture");
    std::cout<<std::setprecision(17)<<"MOMENTUM grid="
             <<nx<<"x"<<ny<<"x"<<nz<<" constant="<<int(constant)
             <<" steps=7 max_reference_error="<<max_error
             <<" impulse_error="<<max_impulse
             <<" max_donor_kinetic_loss_j="<<max_loss
             <<" f0="<<c.alpha[0]<<"\n";
}
void negatives() {
    auto c=make(6,5,4,false);
    const auto baseline=c;
    auto check_failure=[&](const char* name){
        bool rejected=false;
        try{(void)p2_two_phase_consistent_momentum3d(c.m,c.alpha,c.q);}
        catch(const std::exception&){rejected=true;}
        check(rejected,name);
        check(c.m.u_m_s==baseline.m.u_m_s ||
              c.m.density_kg_m3==baseline.m.density_kg_m3,
              "reject does not affect unrelated caller memory");
    };
    c.alpha[0]=1.01;check_failure("oversaturated VOF must fail");
    c=baseline;c.m.density_kg_m3[0]+=12;check_failure("density mismatch must fail");
    c=baseline;c.q[0][1]=std::numeric_limits<double>::quiet_NaN();
    check_failure("NaN momentum must fail");
    c=baseline;c.m.u_m_s[0]=.1;check_failure("nonzero box wall flux must fail");
    c=baseline;c.m.u_m_s[ui(2,2,2,6,5)]=.1;
    check_failure("3D non-solenoidal MAC must fail");
    c=baseline;c.m.dt_s=100;check_failure("3D FV CFL must fail");
    c=baseline;bool invalid=false;
    try{(void)p2_two_phase_consistent_momentum3d(c.m,c.alpha,c.q,1,1.2);}
    catch(const std::invalid_argument&){invalid=true;}
    check(invalid,"water density must exceed air");
    c=baseline;invalid=false;
    try{(void)p2_two_phase_consistent_momentum3d(
        c.m,c.alpha,c.q,1000,std::numeric_limits<double>::quiet_NaN());}
    catch(const std::invalid_argument&){invalid=true;}
    check(invalid,"nonfinite air density must fail");
    check(c.q==baseline.q&&c.alpha==baseline.alpha,
          "failed calls leave original finite state untouched");
}
}
int main() {
    try{
        run(4,4,4,true);
        run(6,5,4,true);
        run(8,6,5,false);
        run(12,8,6,false);
        negatives();
        std::cout<<"PASS P2_TWO_PHASE_CONSISTENT_MOMENTUM assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL P2_TWO_PHASE_CONSISTENT_MOMENTUM "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
