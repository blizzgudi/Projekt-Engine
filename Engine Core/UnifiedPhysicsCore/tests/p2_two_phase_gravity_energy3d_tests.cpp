#include "upc/p2_two_phase_mac_vof3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>
using namespace upc;
namespace {
int assertions=0;
void check(bool ok,const char* msg){++assertions;if(!ok)throw std::runtime_error(msg);}
void near(double a,double b,double tolerance,const char* msg){
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tolerance,msg);
}
std::size_t ci(int x,int y,int z,int nx,int ny){return (z*ny+y)*nx+x;}
P2TwoPhaseMacVofState3D make(int nx,int ny,int nz,bool oblique){
    P2TwoPhaseMacVofState3D s;auto& m=s.mac;
    m.nx=nx;m.ny=ny;m.nz=nz;m.dx_m=.1;m.dt_s=.005;
    m.u_m_s.assign((nx+1)*ny*nz,0);
    m.v_m_s.assign(nx*(ny+1)*nz,0);
    m.w_m_s.assign(nx*ny*(nz+1),0);
    s.water_fraction.resize(nx*ny*nz);
    m.density_kg_m3.resize(nx*ny*nz);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const double a=oblique ?
            (z<nz/2+(x<nx/2?1:0)+(y<ny/2?1:0)?.87:.06):
            (z<nz/2?.9:.05);
        const auto i=ci(x,y,z,nx,ny);
        s.water_fraction[i]=a;
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*a;
    }
    return s;
}
struct Independent {
    double potential_delta=0,flux_delta=0,kinetic_remap=0;
};
Independent reference(const P2TwoPhaseMacVofState3D& before,
                      const P2TwoPhaseMacVofState3D& after,
                      std::array<double,3> g) {
    const auto& m=before.mac;const auto& n=after.mac;
    const int nx=int(m.nx),ny=int(m.ny),nz=int(m.nz);
    const double v=m.dx_m*m.dx_m*m.dx_m,dt=m.dt_s;
    const double drho=before.water_density_kg_m3-before.air_density_kg_m3;
    Independent q;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x){
        const auto i=ci(x,y,z,nx,ny);
        const double gdotr=m.dx_m*(g[0]*(x+.5)+g[1]*(y+.5)+g[2]*(z+.5));
        q.potential_delta-=drho*v*(after.water_fraction[i]-
                                   before.water_fraction[i])*gdotr;
    }
    // Independent finite-volume donor flux with all three MAC directions.
    // No call to the implementation's ledger or pressure/VOF helpers.
    auto face=[&](std::size_t a,std::size_t b,double vel,int axis){
        const double donor=before.water_fraction[vel>=0?a:b];
        q.flux_delta-=drho*v*dt*g[axis]*vel*donor;
        q.kinetic_remap+=.25*(n.density_kg_m3[a]+n.density_kg_m3[b])*
                         v*vel*vel;
    };
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=1;x<nx;++x)
        face(ci(x-1,y,z,nx,ny),ci(x,y,z,nx,ny),
             n.u_m_s[(z*ny+y)*(nx+1)+x],0);
    for(int z=0;z<nz;++z)for(int y=1;y<ny;++y)
    for(int x=0;x<nx;++x)
        face(ci(x,y-1,z,nx,ny),ci(x,y,z,nx,ny),
             n.v_m_s[(z*(ny+1)+y)*nx+x],1);
    for(int z=1;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        face(ci(x,y,z-1,nx,ny),ci(x,y,z,nx,ny),
             n.w_m_s[(z*ny+y)*nx+x],2);
    return q;
}
void run(int nx,int ny,int nz,bool oblique,std::array<double,3> g){
    auto s=make(nx,ny,nz,oblique);
    double peak_flux=0,peak_unclosed=0,peak_flux_error=0,peak_remap=0;
    for(int t=0;t<6;++t){
        const auto before=s;
        const auto l=p2_two_phase_mac_vof_step3d(s,4000,g);
        const auto ref=reference(before,s,g);
        near(l.potential_delta_direct_j,ref.potential_delta,2e-9,
             "cell-centered gravitational potential change mismatch");
        near(l.potential_delta_face_flux_j,ref.flux_delta,2e-9,
             "independent 3D donor-face potential flux mismatch");
        near(l.potential_flux_residual_j,0,2e-9,
             "conservative potential flux identity failed");
        near(l.kinetic_after_density_remap_j,ref.kinetic_remap,2e-9,
             "remapped face mass/kinetic quadrature mismatch");
        near(l.kinetic_density_remap_delta_j,
             ref.kinetic_remap-l.pressure.kinetic_after_j,2e-9,
             "kinetic remap delta mismatch");
        near(l.potential_after_j-l.potential_before_j,
             ref.potential_delta,2e-9,"potential inventory delta mismatch");
        const double kinetic_initial=l.pressure.kinetic_before_j-
            l.gravity_source_work_j;
        const double mechanical=ref.kinetic_remap+l.potential_after_j-
            kinetic_initial-l.potential_before_j;
        near(l.unresolved_mechanical_energy_j,mechanical,2e-9,
             "missing-physics mechanical residual not faithfully reported");
        near(l.water_mass_after_kg,l.water_mass_before_kg,2e-9,
             "water mass lost in potential audit");
        near(l.air_mass_after_kg,l.air_mass_before_kg,2e-9,
             "air mass lost in potential audit");
        peak_flux=std::max(peak_flux,std::abs(ref.flux_delta));
        peak_flux_error=std::max(peak_flux_error,std::abs(l.potential_flux_residual_j));
        peak_unclosed=std::max(peak_unclosed,
            std::abs(l.unresolved_mechanical_energy_j));
        peak_remap=std::max(peak_remap,
            std::abs(l.kinetic_density_remap_delta_j));
    }
    if(oblique)check(peak_flux>1e-6&&peak_unclosed>1e-7&&peak_remap>1e-10,
                     "moving 3D gravity must expose unresolved energy");
    else check(peak_flux<1e-8&&peak_unclosed<1e-8,
               "static hydrostatic equilibrium should remain stationary");
    std::cout<<std::setprecision(17)<<"POTENTIAL grid="<<nx<<"x"<<ny<<"x"<<nz
        <<" oblique="<<int(oblique)
        <<" flux_j="<<peak_flux<<" unclosed_j="<<peak_unclosed
        <<" remap_j="<<peak_remap<<" flux_error_j="<<peak_flux_error
        <<" steps=6\n";
}
void negative(){
    auto s=make(6,5,4,true);
    const auto before=s;
    bool rejected=false;
    try{p2_two_phase_mac_vof_step3d(
        s,4000,{0,std::numeric_limits<double>::infinity(),-9.81});}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&s.water_fraction==before.water_fraction&&
          s.mac.density_kg_m3==before.mac.density_kg_m3,
          "invalid gravity must rollback all state");
}
}
int main(){
    try{
        run(4,4,4,false,{0,0,-9.81});
        run(6,5,4,false,{0,0,-9.81});
        run(6,5,4,true,{1.7,-3.2,-9.81});
        run(8,6,5,true,{1.7,-3.2,-9.81});
        negative();
        std::cout<<"PASS: TWO_PHASE_POTENTIAL_AUDIT assertions="<<assertions<<"\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
