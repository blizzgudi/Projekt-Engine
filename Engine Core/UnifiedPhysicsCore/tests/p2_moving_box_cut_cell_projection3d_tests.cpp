// P2 PARTIAL: real weighted cut-cell pressure projection for a translating
// box, closed domain, no dynamic body reaction or free-surface stress.
#include "upc/p2_moving_box_cut_cell_projection3d.hpp"
#include "upc/p2_fractional_boundary_vof_flux3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Box=upc::P2Box3D;
using Solver=upc::P2MovingBoxCutCellProjection3DBuilder;
std::size_t assertions=0;
void check(bool ok,const char* message) {
    ++assertions;
    if(!ok)throw std::runtime_error(message);
}
void near(double actual,double expected,double tol,const char* message) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,message);
}
double overlap(double lo,double hi,double blo,double bhi) {
    return std::max(0.0,std::min(hi,bhi)-std::max(lo,blo));
}
double independent_open(int x,int y,int z,const Box& box) {
    double solid=1.0;
    for(int a=0;a<3;++a) {
        const int c=a==0?x:(a==1?y:z);
        solid*=overlap(c,c+1.0,box.min_m[a],box.max_m[a]);
    }
    return 1.0-solid;
}
Box translate(Box b,int axis,double delta) {
    b.min_m[axis]+=delta;
    b.max_m[axis]+=delta;
    return b;
}
void run(int n,int axis,int sign,double shift) {
    const Box initial{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    const Box moved=translate(initial,axis,sign*shift);
    const auto result=Solver::sample(n,n,n,1.0,0.25,1000.0,initial,moved);
    const auto& geom=result.wall.geometry.cell_geometry;
    check(result.pressure_pa.size()==static_cast<std::size_t>(n*n*n),
          "pressure shape");
    check(result.u_fluid_m3.size()==static_cast<std::size_t>((n+1)*n*n),
          "u shape");
    check(result.v_fluid_m3.size()==static_cast<std::size_t>(n*(n+1)*n),
          "v shape");
    check(result.w_fluid_m3.size()==static_cast<std::size_t>(n*n*(n+1)),
          "w shape");
    double l1=0.0, source_sum=0.0;
    const auto ci=[&](int x,int y,int z){return (z*n+y)*n+x;};
    const auto ui=[&](int x,int y,int z){return (z*n+y)*(n+1)+x;};
    const auto vi=[&](int x,int y,int z){return (z*(n+1)+y)*n+x;};
    const auto wi=[&](int x,int y,int z){return (z*n+y)*n+x;};
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const int i=ci(x,y,z);
        const double old_a=independent_open(x,y,z,initial);
        const double new_a=independent_open(x,y,z,moved);
        near(geom.alpha_old[i],old_a,2e-13,"old independent cut-cell alpha");
        near(geom.alpha_new[i],new_a,2e-13,"new independent cut-cell alpha");
        const double b=old_a-new_a;
        l1+=std::abs(b);
        source_sum+=b;
        const double div=result.u_fluid_m3[ui(x+1,y,z)]-
                         result.u_fluid_m3[ui(x,y,z)]+
                         result.v_fluid_m3[vi(x,y+1,z)]-
                         result.v_fluid_m3[vi(x,y,z)]+
                         result.w_fluid_m3[wi(x,y,z+1)]-
                         result.w_fluid_m3[wi(x,y,z)];
        near(div,b,1.1e-10,"local moving-boundary fluid continuity");
        near(div+(new_a-old_a),0.0,1.1e-10,"moving cut-cell GCL");
        check(std::isfinite(result.pressure_pa[i]),"finite pressure");
    }
    near(source_sum,0.0,1e-10,"closed domain source compatibility");
    near(result.max_continuity_residual_m3,0.0,1.1e-10,"PCG residual");
    near(result.max_closed_boundary_flux_m3,0.0,0.0,"no domain flux");
    check(result.cg_iterations>0&&result.cg_iterations<=4000,
          "PCG must converge");
    for(int z=0;z<n;++z)for(int y=0;y<n;++y) {
        near(result.u_fluid_m3[ui(0,y,z)],0.0,0.0,"u low boundary");
        near(result.u_fluid_m3[ui(n,y,z)],0.0,0.0,"u high boundary");
    }
    for(int z=0;z<n;++z)for(int x=0;x<n;++x) {
        near(result.v_fluid_m3[vi(x,0,z)],0.0,0.0,"v low boundary");
        near(result.v_fluid_m3[vi(x,n,z)],0.0,0.0,"v high boundary");
    }
    for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        near(result.w_fluid_m3[wi(x,y,0)],0.0,0.0,"w low boundary");
        near(result.w_fluid_m3[wi(x,y,n)],0.0,0.0,"w high boundary");
    }
    check(result.fluid_flux_l1_m3>0.0,"nontrivial pressure-driven fluid flow");
    // Full-water state should advect into the newly opened fluid volume.
    // This is an additional strict donor-CFL/VOF boundedness gate, not FSI.
    const auto to_velocity=[](const std::vector<double>& flux) {
        std::vector<double> velocity=flux;
        for(auto& v:velocity)v/=0.25;
        return velocity;
    };
    const auto water=upc::FractionalBoundaryVofFlux3D::step(
        n,n,n,1.0,0.25,geom.alpha_old,geom.alpha_new,geom.alpha_old,
        to_velocity(result.u_fluid_m3),to_velocity(result.v_fluid_m3),
        to_velocity(result.w_fluid_m3));
    near(water.mass_residual_m3,0.0,1e-9,"full water mass conservation");
    for(std::size_t i=0;i<water.water_cell_fraction.size();++i)
        near(water.water_cell_fraction[i],geom.alpha_new[i],2e-10,
             "saturated cut-cell water fill");
    std::cout<<std::setprecision(16)<<"CASE "<<n<<" "<<axis<<" "<<sign
             <<" "<<shift<<" "<<l1<<" "
             <<result.max_continuity_residual_m3<<" "
             <<result.cg_iterations<<" "<<result.fluid_flux_l1_m3<<"\n";
}
void stationary() {
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    const auto r=Solver::sample(8,8,8,1.0,0.25,1000.0,b,b);
    check(r.cg_iterations==0,"stationary PCG");
    near(r.fluid_flux_l1_m3,0.0,0.0,"stationary flux");
    near(r.max_continuity_residual_m3,0.0,0.0,"stationary continuity");
}
void failures() {
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    const auto invalid=[&](double density,std::size_t budget,Box after) {
        bool thrown=false;
        try { (void)Solver::sample(8,8,8,1.0,0.25,density,b,after,budget); }
        catch(const std::exception&) { thrown=true; }
        check(thrown,"invalid cut-cell input must throw");
    };
    invalid(0.0,4000,b);
    invalid(std::numeric_limits<double>::quiet_NaN(),4000,b);
    invalid(1000.0,0,b);
    invalid(1000.0,4000,translate(b,0,1.5));
    invalid(1000.0,4000,translate(b,0,5.0));
    invalid(1000.0,1,translate(b,0,0.5));
}
}
int main() {
    try {
        for(int n:{8,12,16})
            for(int axis=0;axis<3;++axis)
                for(int sign:{-1,1})
                    for(double shift:{0.25,0.5})
                        run(n,axis,sign,shift);
        stationary();
        failures();
        std::cout<<"PASS: "<<assertions
                 <<" native moving-box cut-cell pressure assertions\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
