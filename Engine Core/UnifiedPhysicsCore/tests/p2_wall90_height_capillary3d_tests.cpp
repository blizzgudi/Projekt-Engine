// P2 PARTIAL: opt-in 90-degree lateral-wall contact model for
// strictly single-valued 3D VOF height graphs. No dynamic contact line.
#include "upc/p2_two_phase_capillary_mac_vof3d.hpp"
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
int checks=0;
void require(bool ok,const char* what) {
    ++checks;
    if(!ok)throw std::runtime_error(what);
}
void near(double a,double b,double tol,const char* what) {
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,what);
}
template<class F> void reject(F f,const char* what){
    bool caught=false;
    try{f();}catch(const std::exception&){caught=true;}
    require(caught,what);
}
constexpr double pi=3.14159265358979323846;
std::vector<double> graph(int n,int axis){
    const int u=(axis+1)%3,v=(axis+2)%3;
    const double dx=1.2/n;
    std::vector<double> a(std::size_t(n)*n*n);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        const int c[3]{x,y,z};
        const double U=(c[u]+.5)*dx,V=(c[v]+.5)*dx;
        const double h=.6+.02*std::cos(2*pi*U/1.2)+
                              .015*std::cos(2*pi*V/1.2);
        a[(std::size_t(z)*n+y)*n+x]=
            std::clamp((h-c[axis]*dx)/dx,0.,1.);
    }
    return a;
}
P2TwoPhaseMacVofState3D state(int n,int axis){
    P2TwoPhaseMacVofState3D s;
    auto& m=s.mac;
    m.nx=m.ny=m.nz=n;m.dx_m=1.2/n;m.dt_s=.0005;
    s.water_fraction=graph(n,axis);
    m.density_kg_m3.resize(s.water_fraction.size());
    m.u_m_s.assign(std::size_t(n+1)*n*n,0);
    m.v_m_s.assign(std::size_t(n)*(n+1)*n,0);
    m.w_m_s.assign(std::size_t(n)*n*(n+1),0);
    for(std::size_t i=0;i<s.water_fraction.size();++i)
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*s.water_fraction[i];
    return s;
}
double face_max_diff(const P2TwoPhaseMacVofState3D& a,
                     const P2TwoPhaseMacVofState3D& b){
    double mx=0;
    for(int k=0;k<3;++k){
        const auto& x=k==0?a.mac.u_m_s:k==1?a.mac.v_m_s:a.mac.w_m_s;
        const auto& y=k==0?b.mac.u_m_s:k==1?b.mac.v_m_s:b.mac.w_m_s;
        require(x.size()==y.size(),"face size equality");
        for(std::size_t i=0;i<x.size();++i)
            mx=std::max(mx,std::abs(x[i]-y[i]));
    }
    return mx;
}
void geometry(){
    for(int axis=0;axis<3;++axis)for(int n:{12,20,32}){
        const auto alpha=graph(n,axis);
        const double dx=1.2/n;
        const auto old=P2VofHeightCurvatureBulk3D::reconstruct(
            n,n,n,dx,alpha,axis);
        const auto wall=P2VofHeightCurvatureBulk3D::reconstruct(
            n,n,n,dx,alpha,axis,true);
        require(old.boundary_extrapolated&&!old.boundary_neumann_90,
                "Legacy extrapolation remains default");
        require(!wall.boundary_extrapolated&&wall.boundary_neumann_90,
                "Explicit 90 degree wall ghost mode");
        require(wall.interface_columns==std::size_t(n)*n,
                "All graph columns resolved");
        const std::array<std::size_t,3> c{
            axis==0?std::size_t(n/2):std::size_t(0),
            axis==1?std::size_t(n/2):std::size_t(0),
            axis==2?std::size_t(n/2):std::size_t(0)
        };
        const auto id=(c[2]*n+c[1])*n+c[0];
        const double k=wall.cell_curvature_inv_m[id];
        const double U=.5*dx,V=.5*dx;
        const double hu=-.02*(2*pi/1.2)*std::sin(2*pi*U/1.2);
        const double hv=-.015*(2*pi/1.2)*std::sin(2*pi*V/1.2);
        const double huu=-.02*std::pow(2*pi/1.2,2)*std::cos(2*pi*U/1.2);
        const double hvv=-.015*std::pow(2*pi/1.2,2)*std::cos(2*pi*V/1.2);
        const double denom=1+hu*hu+hv*hv;
        const double analytic=-(huu*(1+hv*hv)+hvv*(1+hu*hu))/
                              (denom*std::sqrt(denom));
        const double err=std::abs(k-analytic);
        require(err<.03,"90 degree wall graph curvature bounded");
        const double old_err=std::abs(old.cell_curvature_inv_m[id]-analytic);
        require(err<old_err,"Neumann corner improves legacy extrapolation");
        std::cout<<std::setprecision(17)<<"WALL90_GEOMETRY axis="<<axis
                 <<" grid="<<n<<" curvature="<<k<<" analytic="<<analytic
                 <<" error="<<err<<" legacy_error="<<old_err
                 <<" phase_complete=false\n";
    }
}
void live(){
    auto s=state(12,2),legacy=s,initial=s;
    const auto new_result=p2_two_phase_capillary_project3d(s,.072,4000,2,true);
    const auto old_result=p2_two_phase_capillary_project3d(legacy,.072,4000,2);
    require(new_result.height_curvature_opt_in&&
            new_result.height_neumann_wall_opt_in,
            "Neumann curvature connected to live capillary PCG");
    require(!old_result.height_neumann_wall_opt_in,
            "Legacy capillary path unchanged");
    require(new_result.pressure.divergence_after_s_inv<1e-9,
            "Real variable-density MAC pressure projection");
    require(s.water_fraction==initial.water_fraction,
            "Static capillary step conserves water");
    require(face_max_diff(s,legacy)>1e-12,
            "Neumann wall mode affects projected MAC field");
    std::cout<<std::setprecision(17)
             <<"WALL90_PROJECT effect="<<face_max_diff(s,legacy)
             <<" divergence="<<new_result.pressure.divergence_after_s_inv
             <<" phase_complete=false\n";
    auto moving=state(8,2),saved=moving;
    const auto result=p2_two_phase_capillary_momentum_viscous_vof_step3d(
        moving,.072,4000,{0,0,-.1},.001,.000018,2,true);
    require(result.capillary_applied&&
            result.capillary.height_neumann_wall_opt_in,
            "90 degree wall source reached moving 3D MAC/VOF");
    require(result.live.last_divergence_s_inv<1e-9,
            "Moving pressure projection");
    near(result.live.water_mass_residual_kg,0,1e-8,
         "Moving water mass");
    near(result.live.air_mass_residual_kg,0,1e-8,
         "Moving air mass");
    require(moving.water_fraction!=saved.water_fraction,
            "Actual VOF transport");
    require(!result.full_P2_accepted&&!result.capillary.full_P2_accepted,
            "No full CFD acceptance");
    std::cout<<std::setprecision(17)
             <<"WALL90_MOVING divergence="<<result.live.last_divergence_s_inv
             <<" water_residual="<<result.live.water_mass_residual_kg
             <<" phase_complete=false\n";
}
void negatives(){
    auto s=state(8,2),saved=s;
    reject([&]{p2_two_phase_capillary_project3d(s,.072,4000,-1,true);},
           "No wall Neumann without height mode");
    require(s.water_fraction==saved.water_fraction&&
            s.mac.u_m_s==saved.mac.u_m_s,
            "Invalid opt-in transaction");
    auto bad=s;bad.water_fraction[0]=.25;
    reject([&]{p2_two_phase_capillary_project3d(bad,.072,4000,2,true);},
           "Invalid VOF density must fail closed");
    bad=s;bad.mac.dt_s=1;
    reject([&]{p2_two_phase_capillary_project3d(bad,.072,4000,2,true);},
           "Unstable capillary timestep must fail closed");
}
}
int main(){
    try{geometry();live();negatives();
        std::cout<<"PASS P2_WALL90_HEIGHT_CAPILLARY assertions="<<checks
                 <<" phase_complete=false\n";return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_WALL90_HEIGHT_CAPILLARY "<<e.what()
                 <<" assertions="<<checks<<"\n";return 1;
    }
}
