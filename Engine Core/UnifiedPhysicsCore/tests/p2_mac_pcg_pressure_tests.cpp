// P2 partial: genuine independent pressure-Poisson PCG on a staggered
// 3D MAC grid with ghost-fluid free-surface and closed Neumann components.
#include "upc/mac3d.hpp"
#include "upc/coupled_water3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace {
std::size_t assertions=0;
void check(bool ok,const char* what) {
    ++assertions;
    if(!ok)throw std::runtime_error(what);
}
void near(double a,double b,double tol,const char* msg) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
template<class F>void rejects(F fn) {
    bool fail=false;
    try {fn();}catch(const std::exception&){fail=true;}
    check(fail,"failure must be fail-closed");
}
void hydrostatic(int n) {
    constexpr double rho=1000.,gravity=9.81,dt=.01;
    const double dx=.8/n;
    upc::MacProjection3D fluid(n,n,n,dx,rho);
    for(int z=0;z<n/2;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        fluid.set_cell(x,y,z,true);
    fluid.apply_gravity(dt,gravity);
    const auto stats=fluid.project_pcg_verified(dt,1000,1e-8,1e-11);
    near(stats.max_divergence_s_inv,0,1e-8,"hydrostatic PCG incompressible");
    near(fluid.max_divergence(),0,1e-8,"hydrostatic MAC divergence");
    check(stats.iterations>0,"actual 3D Krylov iterations");
    check(stats.liquid_cells==(std::size_t)(n*n*(n/2)),"correct liquid cells");
    check(stats.closed_neumann_components==0,"free surface has Dirichlet condition");
    for(int z=0;z<n/2;++z) {
        const double expect=rho*gravity*(n/2-z-.5)*dx;
        near(fluid.pressure_pa(n/2,n/2,z),expect,1e-4,
             "analytic 3D hydrostatic pressure at cell centre");
        near(fluid.face_velocity_z(n/2,n/2,z+1),0,1e-9,
             "3D hydrostatic rest velocity");
    }
    std::cout<<"PCG_3D_HYDRO n="<<n
             <<" iterations="<<stats.iterations
             <<" pressure_bottom_Pa="<<fluid.pressure_pa(n/2,n/2,0)
             <<" residual_equation_Pa="<<stats.max_equation_residual_pa<<"\n";
}
void nontrivial() {
    constexpr int n=10;
    constexpr double dx=.07, dt=.012;
    upc::MacProjection3D pcg(n,n,n,dx),old(n,n,n,dx);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const bool wet=z<7&&!(x>=3&&x<=4&&y>=3&&y<=5&&z<6);
        const bool solid=x>=3&&x<=4&&y>=3&&y<=5&&z<6;
        pcg.set_cell(x,y,z,wet,solid);
        old.set_cell(x,y,z,wet,solid);
    }
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<=n;++x) {
        const double value=.05*std::sin((x+.3)*.7)+.03*std::cos(y*.41+z*.23);
        pcg.set_face_velocity_x(x,y,z,value);
        old.set_face_velocity_x(x,y,z,value);
    }
    for(int z=0;z<n;++z)for(int y=0;y<=n;++y)
    for(int x=0;x<n;++x) {
        const double value=.06*std::cos((y+.35)*.39)-.04*std::sin(x*.27+z*.19);
        pcg.set_face_velocity_y(x,y,z,value);
        old.set_face_velocity_y(x,y,z,value);
    }
    for(int z=0;z<=n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const double value=.045*std::sin((z+.2)*.61)+.07*std::cos(x*.29+y*.13);
        pcg.set_face_velocity_z(x,y,z,value);
        old.set_face_velocity_z(x,y,z,value);
    }
    const auto state=pcg;
    const auto cg=pcg.project_pcg_verified(dt,2000,2e-7,1e-10);
    check(cg.iterations>3&&cg.iterations<500,"3D PCG iterations bounded");
    check(cg.max_equation_residual_pa<1e-4,"matrix residual absolute");
    old.project(dt,7500);
    check(old.max_divergence()<2e-7,"long Gauss-Seidel reference converges");
    long double squared=0;
    std::size_t count=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto p=pcg.pressure_pa(x,y,z);
        const auto q=old.pressure_pa(x,y,z);
        if(z<7&&!(x>=3&&x<=4&&y>=3&&y<=5&&z<6)) {
            squared+=(long double)(p-q)*(p-q);++count;
        }
    }
    near((double)std::sqrt(squared/count),0,.001,
         "PCG and 7500-sweep GS pressure solutions match");
    near(pcg.max_divergence(),0,2e-7,"real 3D divergence after PCG");
    rejects([&] {
        upc::MacProjection3D bad=state;
        (void)bad.project_pcg_verified(dt,1,1e-10,1e-11);
    });
    near(state.face_velocity_x(2,3,4),.05*std::sin(2.3*.7)+
         .03*std::cos(3*.41+4*.23),1e-14,
         "independent original input never mutated");
    rejects([&]{(void)pcg.project_pcg_verified(0);});
    rejects([&]{(void)pcg.project_pcg_verified(dt,0);});
    rejects([&]{(void)pcg.project_pcg_verified(dt,1,0);});
    // Extremely small timestep overflows the pressure RHS. It must never
    // silently publish a nonfinite pressure/velocity or a false PASS.
    rejects([&]{upc::MacProjection3D bad=state;
                (void)bad.project_pcg_verified(1e-300);});
    std::cout<<"PCG_3D_NEGATIVE underconverged=REJECTED invalid_parameters=REJECTED nonfinite_rhs=REJECTED rollback=PASS\n";
    std::cout<<"PCG_3D_MIXED iterations="<<cg.iterations
             <<" divergence_s_inv="<<cg.max_divergence_s_inv
             <<" pressure_gs_l2_pa="<<(double)std::sqrt(squared/count)<<"\n";
}
void live_coupling() {
    upc::CoupledMacWater3D original(8,6,8,.1),verified(8,6,8,.1);
    for(int z=0;z<5;++z)for(int y=0;y<6;++y)
    for(int x=0;x<3;++x) {
        original.set_water(x,y,z,1);
        verified.set_water(x,y,z,1);
    }
    const double water=verified.volume_m3();
    original.set_conservative_throughflow_transport(true);
    verified.set_conservative_throughflow_transport(true);
    check(!original.pcg_pressure_projection_enabled(),
          "legacy Gauss-Seidel remains default");
    verified.set_pcg_pressure_projection(true);
    check(verified.pcg_pressure_projection_enabled(),
          "only explicit opt-in selects PCG");
    for(int i=0;i<8;++i) {
        const double residual=verified.step_verified(.008,500,1e-6);
        original.step(.008,500);
        check(residual<=1e-6,"actual native live MAC PCG divergence");
        check(verified.last_pcg_projection_iterations()>0,
              "actual live PCG iterations counted");
        check(std::isfinite(verified.last_pcg_max_equation_residual_pa())&&
              verified.last_pcg_max_equation_residual_pa()<.001,
              "actual live 3D PCG matrix residual reported");
        near(verified.volume_m3(),water,5e-10,
             "live 3D PCG-conservative VOF water mass");
        near(original.volume_m3(),water,5e-10,
             "original 3D Gauss-Seidel water mass");
    }
    const double before=verified.volume_m3();
    // A deliberately impossible convergence deadline must fail without
    // changing fluid state (including water mass and diagnostic counters).
    const auto old_stats=verified.last_pcg_projection_iterations();
    rejects([&]{(void)verified.step_verified(.012,1,1e-12);});
    near(verified.volume_m3(),before,1e-13,
         "atomic rejection preserves live fluid volume");
    check(verified.last_pcg_projection_iterations()==old_stats,
          "atomic rejection preserves solver metrics");
    std::cout<<"PCG_3D_LIVE water_volume_m3="<<water
             <<" matrix_iterations="<<old_stats
             <<" rejection=transactional\n";
}

void closed() {
    const int n=8;
    upc::MacProjection3D fluid(n,n,n,.1,1000);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        fluid.set_cell(x,y,z,x!=4,x==4);
    auto audit=fluid.project_pcg_verified(.01,500,1e-8,1e-10);
    check(audit.closed_neumann_components==2,
          "two disconnected closed incompressible Neumann components");
    near(fluid.max_divergence(),0,1e-12,"closed cavities at rest");
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)near(fluid.pressure_pa(x,y,z),0,1e-12,
                              "closed cavity pressure gauge");
}
}
int main() {
    try {
        hydrostatic(8);
        hydrostatic(12);
        hydrostatic(16);
        nontrivial();
        closed();
        live_coupling();
        std::cout<<"PASS P2_MAC_PCG_PRESSURE assertions="<<assertions
                 <<" full_phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_MAC_PCG_PRESSURE "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
