#include "upc/p2_two_phase_mac_momentum_remap3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
int assertions=0;
void check(bool condition,const char* msg) {
    ++assertions;
    if(!condition)throw std::runtime_error(msg);
}
void near(double actual,double expect,double tol,const char* msg) {
    check(std::isfinite(actual)&&std::abs(actual-expect)<=tol,msg);
}
std::size_t ci(int x,int y,int z,int nx,int ny) {
    return (z*ny+y)*nx+x;
}
template<class Fn>void rejected(Fn fn,const char* msg) {
    bool failed=false;try{fn();}catch(const std::exception&){failed=true;}
    check(failed,msg);
}
void verify(int nx,int ny,int nz,bool uniform) {
    upc::P2TwoPhaseMacPressureGrid3D m;
    m.nx=nx;m.ny=ny;m.nz=nz;m.dx_m=.09;m.dt_s=.004;
    m.density_kg_m3.resize(nx*ny*nz);
    m.u_m_s.assign((nx+1)*ny*nz,0);
    m.v_m_s.assign(nx*(ny+1)*nz,0);
    m.w_m_s.assign(nx*ny*(nz+1),0);
    std::vector<std::array<double,3>> q(nx*ny*nz);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto i=ci(x,y,z,nx,ny);
        m.density_kg_m3[i]=1.2+998.8*(.02+.85*double(x+z)/(nx+nz));
        q[i]=uniform?std::array<double,3>{.12,-.09,.035}:
            std::array<double,3>{
                .07+.03*std::sin(x*.21+y*.1),
                -.03+.06*std::cos(y*.31+z*.23),
                .01+.04*std::sin(z*.53+x*.26)};
    }
    const auto original_density=m.density_kg_m3;
    const auto original_velocity=q;
    const auto a=upc::p2_two_phase_mac_momentum_remap3d(m,q);
    check(a.interior_face_count==
          std::size_t((nx-1)*ny*nz+nx*(ny-1)*nz+nx*ny*(nz-1)),
          "all 3D interior dual MAC faces visited exactly once");
    const double cellvol=std::pow(m.dx_m,3);
    double max_speed_error=0,max_unweighted_difference=0;
    std::array<double,3> independent_interior_impulse{};
    std::array<double,3> independent_boundary_impulse{};
    std::array<double,3> independent_cell_impulse{};
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        const auto id=ci(x,y,z,nx,ny);
        const double mass=m.density_kg_m3[id]*cellvol;
        for(int k=0;k<3;++k) {
            independent_cell_impulse[k]+=mass*q[id][k];
            if((k==0&&(x==0||x==nx-1))||
               (k==1&&(y==0||y==ny-1))||
               (k==2&&(z==0||z==nz-1)))
                independent_boundary_impulse[k]+=.5*mass*q[id][k];
        }
    }
    const auto compare=[&](std::size_t left,std::size_t right,int axis,
                           double actual) {
        const double rho1=m.density_kg_m3[left];
        const double rho2=m.density_kg_m3[right];
        const double reference=(rho1*q[left][axis]+rho2*q[right][axis])/
                               (rho1+rho2);
        near(actual,reference,1e-15,
             "Independent two-mass staggered-face momentum reference");
        independent_interior_impulse[axis]+=
            .5*(rho1+rho2)*cellvol*actual;
        max_speed_error=std::max(max_speed_error,std::abs(actual-reference));
        max_unweighted_difference=std::max(max_unweighted_difference,
            std::abs(actual-.5*(q[left][axis]+q[right][axis])));
    };
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y) {
        near(a.u_m_s[(z*ny+y)*(nx+1)],0,0,"x-wall no penetration");
        near(a.u_m_s[(z*ny+y)*(nx+1)+nx],0,0,"x-wall no penetration right");
        for(int x=1;x<nx;++x)
            compare(ci(x-1,y,z,nx,ny),ci(x,y,z,nx,ny),0,
                    a.u_m_s[(z*ny+y)*(nx+1)+x]);
    }
    for(int z=0;z<nz;++z)for(int x=0;x<nx;++x) {
        near(a.v_m_s[(z*(ny+1))*nx+x],0,0,"front y-wall no penetration");
        near(a.v_m_s[(z*(ny+1)+ny)*nx+x],0,0,
             "back y-wall no penetration");
        for(int y=1;y<ny;++y)
            compare(ci(x,y-1,z,nx,ny),ci(x,y,z,nx,ny),1,
                    a.v_m_s[(z*(ny+1)+y)*nx+x]);
    }
    for(int y=0;y<ny;++y)for(int x=0;x<nx;++x) {
        near(a.w_m_s[y*nx+x],0,0,"bottom z-wall no penetration");
        near(a.w_m_s[(nz*ny+y)*nx+x],0,0,"top z-wall no penetration");
        for(int z=1;z<nz;++z)
            compare(ci(x,y,z-1,nx,ny),ci(x,y,z,nx,ny),2,
                    a.w_m_s[(z*ny+y)*nx+x]);
    }
    for(int axis=0;axis<3;++axis) {
        near(a.cell_impulse_kg_m_s[axis],independent_cell_impulse[axis],
             1e-12,"independent volumetric original momentum");
        near(a.interior_mac_impulse_kg_m_s[axis],
             independent_interior_impulse[axis],1e-12,
             "independent interior dual face momentum");
        near(a.omitted_boundary_half_face_impulse_kg_m_s[axis],
             independent_boundary_impulse[axis],1e-12,
             "independent virtual wall half-face momentum");
        near(independent_interior_impulse[axis]+
             independent_boundary_impulse[axis],
             independent_cell_impulse[axis],1e-12,
             "interior plus virtual boundary partitions all cell impulse");
        near(a.exact_partition_residual_kg_m_s[axis],0,1e-12,
             "strict combined wall/interior momentum identity");
    }
    check(a.interior_interpolation_loss_j>=-1e-11,
          "mass weighted MAC interpolation cannot create kinetic energy");
    if(uniform) {
        near(a.interior_interpolation_loss_j,0,1e-12,
             "uniform velocity must not be dissipated by density interpolation");
        near(a.max_density_weight_disagreement_m_s,0,1e-13,
             "uniform velocity invariant at heterogeneous density");
    }else{
        check(a.interior_interpolation_loss_j>0,
              "inhomogeneous velocity has positive Jensen interpolation loss");
        check(a.max_density_weight_disagreement_m_s>1e-8,
              "real density weighted face differs from nonconservative mean");
    }
    check(m.density_kg_m3==original_density&&q==original_velocity,
          "input grid/velocity must not mutate");
    std::cout<<std::setprecision(17)
             <<"MAC_REMAP grid="<<nx<<"x"<<ny<<"x"<<nz
             <<" uniform="<<int(uniform)
             <<" faces="<<a.interior_face_count
             <<" max_face_ref_error="<<max_speed_error
             <<" max_impulse_residual="<<
                 std::max({std::abs(a.exact_partition_residual_kg_m_s[0]),
                           std::abs(a.exact_partition_residual_kg_m_s[1]),
                           std::abs(a.exact_partition_residual_kg_m_s[2])})
             <<" kinetic_loss_j="<<a.interior_interpolation_loss_j
             <<" unweighted_difference_m_s="<<max_unweighted_difference
             <<" phase_complete=false\n";
}
void negative() {
    upc::P2TwoPhaseMacPressureGrid3D m;
    m.nx=4;m.ny=4;m.nz=4;m.dx_m=.1;m.dt_s=.01;
    m.density_kg_m3.assign(64,1000);
    m.u_m_s.assign(80,0);m.v_m_s.assign(80,0);m.w_m_s.assign(80,0);
    std::vector<std::array<double,3>> q(64,{.3,.2,-.1});
    const auto before=q;
    rejected([&]{auto p=m;p.density_kg_m3[0]=0;
        (void)upc::p2_two_phase_mac_momentum_remap3d(p,q);},
        "zero material density must reject");
    rejected([&]{auto p=m;p.density_kg_m3[0]=
        std::numeric_limits<double>::quiet_NaN();
        (void)upc::p2_two_phase_mac_momentum_remap3d(p,q);},
        "nonfinite material density must reject");
    rejected([&]{auto bad=q;bad[12][2]=
        std::numeric_limits<double>::infinity();
        (void)upc::p2_two_phase_mac_momentum_remap3d(m,bad);},
        "nonfinite cell velocity must reject");
    rejected([&]{auto p=m;p.nx=0;
        (void)upc::p2_two_phase_mac_momentum_remap3d(p,q);},
        "zero dimension must reject");
    rejected([&]{auto p=m;p.dx_m=0;
        (void)upc::p2_two_phase_mac_momentum_remap3d(p,q);},
        "zero dx must reject");
    rejected([&]{auto bad=q;bad.pop_back();
        (void)upc::p2_two_phase_mac_momentum_remap3d(m,bad);},
        "truncated velocity array must reject");
    check(q==before,"rejected independent remap leaves original cell state");
}
}
int main() {
    try {
        verify(4,4,4,true);
        verify(6,5,4,true);
        verify(8,6,5,false);
        verify(12,8,6,false);
        negative();
        std::cout<<"PASS P2_TWO_PHASE_MAC_MOMENTUM_REMAP assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_TWO_PHASE_MAC_MOMENTUM_REMAP "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
