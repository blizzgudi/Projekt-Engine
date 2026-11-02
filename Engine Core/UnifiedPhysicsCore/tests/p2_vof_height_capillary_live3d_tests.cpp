// P2 PARTIAL: strict height-function field -> actual two-phase MAC/PCG
// capillary source and one moving MAC/VOF step. NOT sharp PLIC or CFD E2E.
#include "upc/p2_two_phase_capillary_mac_vof3d.hpp"
#include "upc/p2_vof_height_curvature3d.hpp"
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
void check(bool ok,const char* why){
    ++checks;
    if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why){
    if(!(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol)){
        std::cerr<<std::setprecision(17)<<"DIFF "<<why<<" "<<a<<" "<<b<<"\n";
        throw std::runtime_error(why);
    }
    ++checks;
}
template<class F> void rejects(F f,const char* why){
    bool threw=false;
    try{f();}catch(const std::exception&){threw=true;}
    check(threw,why);
}
std::vector<double> fractions(int n,int axis,bool spherical){
    const double dx=1.2/n;
    const int u=(axis+1)%3,v=(axis+2)%3;
    std::vector<double> a(std::size_t(n)*n*n);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x){
        const int c[3]{x,y,z};
        const double U=(c[u]+.5)*dx-.6,V=(c[v]+.5)*dx-.6;
        const double h=spherical?.05+std::sqrt(1-U*U-V*V):
                                 .63+.1*U-.12*V;
        a[(std::size_t(z)*n+y)*n+x]=
            std::clamp((h-c[axis]*dx)/dx,0.,1.);
    }
    return a;
}
P2TwoPhaseMacVofState3D state(int n,int axis,bool spherical){
    P2TwoPhaseMacVofState3D s;
    auto& m=s.mac;
    m.nx=m.ny=m.nz=n;m.dx_m=1.2/n;m.dt_s=.0005;
    s.water_fraction=fractions(n,axis,spherical);
    m.density_kg_m3.resize(s.water_fraction.size());
    m.u_m_s.assign((n+1)*n*n,0);
    m.v_m_s.assign(n*(n+1)*n,0);
    m.w_m_s.assign(n*n*(n+1),0);
    for(std::size_t i=0;i<s.water_fraction.size();++i)
        m.density_kg_m3[i]=s.air_density_kg_m3+
            (s.water_density_kg_m3-s.air_density_kg_m3)*
            s.water_fraction[i];
    return s;
}
double face_difference(const P2TwoPhaseMacVofState3D& a,
                       const P2TwoPhaseMacVofState3D& b){
    double mx=0;
    for(int k=0;k<3;++k){
        const auto& x=k==0?a.mac.u_m_s:k==1?a.mac.v_m_s:a.mac.w_m_s;
        const auto& y=k==0?b.mac.u_m_s:k==1?b.mac.v_m_s:b.mac.w_m_s;
        check(x.size()==y.size(),"face sizes");
        for(std::size_t i=0;i<x.size();++i)
            mx=std::max(mx,std::abs(x[i]-y[i]));
    }
    return mx;
}
void bulk_geometry(){
    std::array<double,3> error{};
    for(int g=0;g<3;++g){
        const int n=g==0?12:g==1?20:40;
        const double dx=1.2/n;
        for(int axis=0;axis<3;++axis){
            const auto alpha=fractions(n,axis,true);
            const auto field=P2VofHeightCurvatureBulk3D::reconstruct(
                n,n,n,dx,alpha,axis);
            check(field.interface_columns==std::size_t(n)*n &&
                  field.interface_cut_cells==field.interface_columns,
                  "Every column has one actual 3D cut");
            check(!field.full_p2_accepted&&field.boundary_extrapolated,
                  "No full P2/boundary claims");
            const auto sample=P2VofHeightCurvature3D::sample(
                n,n,n,dx,alpha,axis,n/2-1,n/2-1);
            std::array<std::size_t,3> c{};
            c[axis]=n/2;c[(axis+1)%3]=n/2-1;
            c[(axis+2)%3]=n/2-1;
            const auto id=(c[2]*n+c[1])*n+c[0];
            const double k=field.cell_curvature_inv_m[id];
            near(k,sample.curvature_inv_m,2e-12,
                 "Bulk stencil matches independent single-column sampler");
            const double e=std::abs(k-2.);
            if(axis==2)error[g]=e;
            check(e<.007,"3D manufactured sphere cap curvature");
            std::cout<<std::setprecision(17)
                     <<"HEIGHT_CAPILLARY_FIELD axis="<<axis
                     <<" grid="<<n<<" curvature="<<k
                     <<" error="<<e<<" phase_complete=false\n";
        }
    }
    check(error[1]<error[0]&&error[2]<error[1]/2,
          "3-grid manufactured sphere cap convergence");
    for(int axis=0;axis<3;++axis){
        const int n=12;
        const auto f=P2VofHeightCurvatureBulk3D::reconstruct(
            n,n,n,1.2/n,fractions(n,axis,false),axis);
        check(f.max_abs_curvature_inv_m<1e-10,
              "Oblique plane must have zero curvature");
    }
}
void projected_and_live(){
    auto plane=state(12,2,false);
    const auto p=p2_two_phase_capillary_project3d(plane,.072,4000,2);
    check(p.height_curvature_opt_in&&p.height_interface_columns==144,
          "Oblique planar height capillary actually enabled");
    check(p.max_post_projection_velocity_m_s<1e-12,
          "Oblique plane no parasitic capillary flow");
    std::array<double,3> residual{};
    for(int q=0;q<3;++q){
        const int n=q==0?8:q==1?12:16;
        auto grid=state(n,2,true);
        const auto l=p2_two_phase_capillary_project3d(grid,.072,4000,2);
        residual[q]=l.max_post_projection_velocity_m_s;
        check(std::isfinite(residual[q])&&residual[q]<1e-6,
              "Bounded static height-graph parasitic velocity");
        std::cout<<std::setprecision(17)
                 <<"HEIGHT_CAPILLARY_STATIC grid="<<n
                 <<" spurious_velocity="<<residual[q]
                 <<" phase_complete=false\n";
    }
    // This is a manufactured smooth graph, NOT a full 3D drop with
    // two-valued vertical columns or an experimental benchmark.
    auto s=state(12,2,true);
    auto diffuse=s;
    const auto original=s;
    const auto a=p2_two_phase_capillary_project3d(s,.072,4000,2);
    const auto b=p2_two_phase_capillary_project3d(diffuse,.072,4000);
    check(a.height_curvature_opt_in&&a.height_normal_axis==2&&
          a.height_interface_columns==144,
          "Height field used in actual MAC/PCG source");
    check(!b.height_curvature_opt_in&&b.height_normal_axis==-1,
          "Legacy diffuse default unchanged");
    check(a.active_capillary_faces>0&&
          a.max_source_velocity_change_m_s>0,
          "Nonzero 3D height curvature surface tension");
    check(a.pressure.divergence_after_s_inv<1e-9,
          "Actual variable density MAC PCG");
    check(s.water_fraction==original.water_fraction &&
          s.mac.density_kg_m3==original.mac.density_kg_m3,
          "Capillary-only step does not change mass");
    check(!a.sharp_plic_traction_validated&&
          !a.global_surface_energy_closed&&!a.full_P2_accepted,
          "No full physical surface-energy claim");
    const double effect=face_difference(s,diffuse);
    check(effect>1e-11,"Actual height vs diffuse source differs");
    std::cout<<std::setprecision(17)
             <<"HEIGHT_CAPILLARY_PROJECT grid=12 curvature="
             <<a.interface_curvature_sum_m_inv/
                 double(a.interface_curvature_cells)
             <<" velocity="<<a.max_post_projection_velocity_m_s
             <<" diffuse_velocity="<<b.max_post_projection_velocity_m_s
             <<" effect="<<effect<<" phase_complete=false\n";

    auto live=state(8,2,true),legacy=live;
    const auto initial=live;
    const auto la=p2_two_phase_capillary_momentum_viscous_vof_step3d(
        live,.072,4000,{0,0,-.1},.001,.000018,2);
    const auto lb=p2_two_phase_capillary_momentum_viscous_vof_step3d(
        legacy,.072,4000,{0,0,-.1},.001,.000018);
    check(la.capillary_applied&&lb.capillary_applied&&
          la.capillary.height_curvature_opt_in,
          "Height capillary reached moving 3D MAC/VOF");
    check(la.live.last_divergence_s_inv<1e-9,
          "Moving height capillary final PCG divergence");
    near(la.live.water_mass_residual_kg,0,1e-8,
         "Moving water mass conserved");
    near(la.live.air_mass_residual_kg,0,1e-8,
         "Moving air mass conserved");
    check(live.water_fraction!=initial.water_fraction,
          "Real moving 3D VOF field");
    const double moving_effect=face_difference(live,legacy);
    check(moving_effect>1e-12,"Height option changes real moving MAC state");
    check(!la.full_P2_accepted&&!la.live.full_P2_accepted,
          "Moving solver not full P2");
    std::cout<<std::setprecision(17)
             <<"HEIGHT_CAPILLARY_LIVE grid=8 effect="<<moving_effect
             <<" divergence="<<la.live.last_divergence_s_inv
             <<" phase_complete=false\n";
}
void negatives(){
    auto s=state(8,2,true);
    auto before=s;
    rejects([&]{p2_two_phase_capillary_project3d(s,.072,4000,3);},
            "Unsupported axis rejected");
    check(s.water_fraction==before.water_fraction&&
          s.mac.u_m_s==before.mac.u_m_s,"Invalid axis rollback");
    auto drop=s;
    const int n=8;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x){
        const double X=(x+.5)*.15-.6,Y=(y+.5)*.15-.6,
                     Z=(z+.5)*.15-.6;
        const double a=std::clamp(.5+(.31-std::sqrt(X*X+Y*Y+Z*Z))/.3,0.,1.);
        const auto i=(z*n+y)*n+x;
        drop.water_fraction[i]=a;
        drop.mac.density_kg_m3[i]=drop.air_density_kg_m3+
            (drop.water_density_kg_m3-drop.air_density_kg_m3)*a;
    }
    const auto saved=drop;
    rejects([&]{p2_two_phase_capillary_project3d(drop,.072,4000,2);},
            "Overhanging sphere rejected, no diffuse fallback");
    check(drop.water_fraction==saved.water_fraction&&
          drop.mac.density_kg_m3==saved.mac.density_kg_m3,
          "Sphere rejection transactional");
    auto bad=s;bad.water_fraction[0]=.2;
    rejects([&]{p2_two_phase_capillary_project3d(bad,.072,4000,2);},
            "Inconsistent VOF density rejected");
    bad=s;bad.mac.dt_s=1;
    rejects([&]{p2_two_phase_capillary_project3d(bad,.072,4000,2);},
            "Capillary CFL rejected");
    check(s.mac.u_m_s==before.mac.u_m_s,
          "Negative cases never mutate original");
}
}
int main(){
    try{
        bulk_geometry();
        projected_and_live();
        negatives();
        std::cout<<"PASS P2_VOF_HEIGHT_CAPILLARY_LIVE assertions="
                 <<checks<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_VOF_HEIGHT_CAPILLARY_LIVE "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
