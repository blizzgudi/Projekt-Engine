// P2 partial: pressure-gradient face-impulse/energy ledger after MAC projection.
// Single-cell free-surface cases have a closed-form 3D Poisson reference.
#include "upc/p2_projection_gradient_budget3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool value,const char* why) {
    ++assertions;
    if(!value)throw std::runtime_error(why);
}
void near(double got,double expected,double tol,const char* why) {
    check(std::isfinite(got)&&std::isfinite(expected)&&
          std::abs(got-expected)<=tol,why);
}
template<class F>void invalid(F&& fn,const char* why) {
    bool caught=false;
    try{fn();}catch(const std::invalid_argument&){caught=true;}
    check(caught,why);
}
template<class F>void overflow(F&& fn,const char* why) {
    bool caught=false;
    try{fn();}catch(const std::overflow_error&){caught=true;}
    check(caught,why);
}
void run(int n,double dx,double rho,double dt,bool cube) {
    const int nx=n,ny=n,nz=n,c=n/2;
    const auto idx=[&](int x,int y,int z) {
        return static_cast<std::size_t>((z*ny+y)*nx+x);
    };
    const auto inside=[&](int x,int y,int z) {
        return x>=0&&y>=0&&z>=0&&x<nx&&y<ny&&z<nz;
    };
    const auto liquid=[&](int x,int y,int z) {
        return inside(x,y,z)&&
            (cube?(x==c||x==c-1)&&(y==c||y==c-1)&&(z==c||z==c-1):
                  x==c&&y==c&&z==c);
    };
    std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0);
    upc::MacProjection3D mac(nx,ny,nz,dx,rho);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x) {
        wet[idx(x,y,z)]=static_cast<std::uint8_t>(liquid(x,y,z));
        mac.set_cell(x,y,z,liquid(x,y,z));
    }
    const std::array<double,6> face{{-0.06,0.20,0.03,-0.11,0.05,0.08}};
    const auto seed=[&](int axis,int x,int y,int z) {
        if(!cube) {
            if(axis==0&&y==c&&z==c&&x==c)return face[0];
            if(axis==0&&y==c&&z==c&&x==c+1)return face[1];
            if(axis==1&&x==c&&z==c&&y==c)return face[2];
            if(axis==1&&x==c&&z==c&&y==c+1)return face[3];
            if(axis==2&&x==c&&y==c&&z==c)return face[4];
            if(axis==2&&x==c&&y==c&&z==c+1)return face[5];
            return 0.0;
        }
        const int ax=x-(axis==0),ay=y-(axis==1),az=z-(axis==2);
        if(!liquid(ax,ay,az)&&!liquid(x,y,z))return 0.0;
        return 0.011*(axis+1)+0.002*x-0.003*y+0.004*z+
               0.001*(axis==0?x*x:axis==1?y*y:z*z);
    };
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<=nx;++x)
        mac.set_face_velocity_x(x,y,z,seed(0,x,y,z));
    for(int z=0;z<nz;++z)for(int y=0;y<=ny;++y)for(int x=0;x<nx;++x)
        mac.set_face_velocity_y(x,y,z,seed(1,x,y,z));
    for(int z=0;z<=nz;++z)for(int y=0;y<ny;++y)for(int x=0;x<nx;++x)
        mac.set_face_velocity_z(x,y,z,seed(2,x,y,z));
    const auto before=mac;
    const std::array<double,3> centre{{(c+0.15)*dx,(c-0.17)*dx,(c+0.11)*dx}};
    const double prediv=mac.max_divergence();
    check(prediv>1e-5,"nontrivial initial divergence");
    mac.project(dt,4000);
    const auto result=upc::ProjectionGradientBudgetAudit3D::analyze(
        before,mac,nx,ny,nz,dx,rho,dt,wet,solid,centre);
    check(result.max_divergence_after_s_inv<1e-8,"projected 3D divergence");
    check(result.max_gradient_velocity_residual_m_s<1e-12,
          "pressure gradient exactly explains face delta");
    const double expected_energy=result.pressure_gradient_work_proxy_J+
                                 result.euler_quadratic_proxy_J;
    near(result.observed_face_energy_delta_proxy_J,expected_energy,1e-11,
         "discrete pressure work plus Euler quadratic identity");
    for(int a=0;a<3;++a) {
        near(result.observed_linear_Ns[a],result.gradient_linear_Ns[a],
             1e-11,"projection linear gradient impulse");
        near(result.observed_angular_Nms[a],result.gradient_angular_Nms[a],
             1e-11,"projection angular gradient impulse");
    }
    check(result.wet_wet_faces==(cube?12u:0u),"wet-wet face count");
    check(result.wet_air_faces==(cube?24u:6u),"wet-air face count");
    double analytic_pressure=0.0,analytic_energy=0.0;
    if(!cube) {
        // Exactly one wet cell with six air neighbours. The discrete Poisson
        // diagonal is 6*2=12, and the free-surface ghost gradient is 2.
        const double flux=face[1]-face[0]+face[3]-face[2]+face[5]-face[4];
        analytic_pressure=-rho*dx*flux/(12.0*dt);
        near(mac.pressure_pa(c,c,c),analytic_pressure,1e-10,
             "independent one-cell 3D Poisson solution");
        const double dv=flux/6.0;
        for(int axis=0;axis<3;++axis) {
            const int xm=c+(axis==0),ym=c+(axis==1),zm=c+(axis==2);
            const double vm=axis==0?mac.face_velocity_x(c,c,c):
                            axis==1?mac.face_velocity_y(c,c,c):
                                     mac.face_velocity_z(c,c,c);
            const double vp=axis==0?mac.face_velocity_x(xm,ym,zm):
                            axis==1?mac.face_velocity_y(xm,ym,zm):
                                     mac.face_velocity_z(xm,ym,zm);
            near(vm,face[2*axis]+dv,1e-12,"analytic negative face update");
            near(vp,face[2*axis+1]-dv,1e-12,"analytic positive face update");
        }
        const double mass=rho*dx*dx*dx;
        analytic_energy=-mass*flux*flux/12.0;
        near(result.observed_face_energy_delta_proxy_J,analytic_energy,1e-12,
             "independent one-cell pressure energy reduction");
        for(int a=0;a<3;++a) {
            near(result.gradient_linear_Ns[a],0,1e-12,
                 "isolated free surface pressure impulse cancels in each axis");
            near(result.gradient_angular_Nms[a],0,1e-12,
                 "isolated symmetric pressure angular impulse cancels");
        }
        check(analytic_energy<0,"one-cell pressure energy dissipates divergence");
    }
    if(n==6&&!cube) {
        invalid([&]{upc::ProjectionGradientBudgetAudit3D::analyze(
            before,mac,nx,ny,nz,dx,rho,0,wet,solid,centre);},"invalid dt");
        auto bad=wet;bad.pop_back();
        invalid([&]{upc::ProjectionGradientBudgetAudit3D::analyze(
            before,mac,nx,ny,nz,dx,rho,dt,bad,solid,centre);},"invalid length");
        bad=wet;auto solids=solid;solids[idx(c,c,c)]=1;
        invalid([&]{upc::ProjectionGradientBudgetAudit3D::analyze(
            before,mac,nx,ny,nz,dx,rho,dt,bad,solids,centre);},
            "invalid overlapping wet/solid");
        invalid([&]{upc::ProjectionGradientBudgetAudit3D::analyze(
            before,mac,nx,ny,nz,dx,rho,dt,wet,solid,
            {{std::numeric_limits<double>::quiet_NaN(),0,0}});},
            "invalid centre");
        overflow([&]{upc::ProjectionGradientBudgetAudit3D::analyze(
            before,mac,nx,ny,nz,1e200,rho,dt,wet,solid,centre);},
            "overflow face mass");
    }
    std::cout<<(cube?"cube":"single")<<","<<n<<","<<dx<<","<<rho<<","<<dt
             <<","<<analytic_pressure<<","<<analytic_energy
             <<","<<result.max_divergence_after_s_inv
             <<","<<result.max_gradient_velocity_residual_m_s
             <<","<<result.observed_face_energy_delta_proxy_J
             <<","<<result.pressure_gradient_work_proxy_J
             <<","<<result.euler_quadratic_proxy_J
             <<","<<result.wet_wet_faces<<","<<result.wet_air_faces<<"\n";
}

// A nonzero wet/solid, dry/solid and exterior face must be zeroed by
// project() before pressure solve. Independent reference: dp=-rho*dx^3*v0,
// dE=-rho*dx^3*v0^2/2. No pressure correction is needed in this scene.
void run_clamp(int n,double dx,double rho,double dt) {
    const int c=n/2;
    upc::MacProjection3D mac(n,n,n,dx,rho);
    const auto idx=[&](int x,int y,int z) {
        return static_cast<std::size_t>((z*n+y)*n+x);
    };
    std::vector<std::uint8_t> wet(static_cast<std::size_t>(n)*n*n,0),solid(wet);
    wet[idx(c,c,c)]=1;
    solid[idx(c+1,c,c)]=1;
    mac.set_cell(c,c,c,true);
    mac.set_cell(c+1,c,c,false,true);
    mac.set_face_velocity_x(c+1,c,c,0.25);     // wet/solid
    mac.set_face_velocity_y(c+1,c+1,c,-0.17);  // solid/air
    mac.set_face_velocity_z(c,c,0,0.31);       // exterior
    const auto before=mac;
    mac.project(dt,4000);
    const std::array<double,3> centre{{0,0,0}};
    const auto b=upc::ProjectionGradientBudgetAudit3D::analyze(
        before,mac,n,n,n,dx,rho,dt,wet,solid,centre);
    const double m=rho*dx*dx*dx;
    const std::array<double,3> j{{-0.25*m,0.17*m,-0.31*m}};
    const std::array<std::array<double,3>,3> r{{
        {{(c+1)*dx,(c+0.5)*dx,(c+0.5)*dx}},
        {{(c+1.5)*dx,(c+1)*dx,(c+0.5)*dx}},
        {{(c+0.5)*dx,(c+0.5)*dx,0}}
    }};
    std::array<double,3> torque{};
    for(int axis=0;axis<3;++axis) {
        torque[(axis+1)%3]+=r[axis][(axis+2)%3]*j[axis];
        torque[(axis+2)%3]-=r[axis][(axis+1)%3]*j[axis];
    }
    const double energy=-0.5*m*(0.25*0.25+0.17*0.17+0.31*0.31);
    for(int axis=0;axis<3;++axis) {
        near(b.clamp_linear_Ns[axis],j[axis],1e-12,"analytic clamp impulse");
        near(b.clamp_angular_Nms[axis],torque[axis],1e-12,
             "analytic clamp angular impulse");
        near(b.gradient_linear_Ns[axis],0,1e-12,"zero pressure gradient");
    }
    near(b.clamp_face_energy_delta_proxy_J,energy,1e-12,
         "analytic wall/exterior clamp energy");
    near(b.max_clamp_after_velocity_m_s,0,0,"zeroed wall/exterior faces");
    check(b.clamp_solid_faces==6,"six solid-adjacent interior faces");
    check(b.clamp_exterior_faces==static_cast<std::size_t>(6*n*n),
          "all external MAC faces accounted for");
    near(mac.max_divergence(),0,0,"clamped fluid has no divergence");
    std::cout<<"clamp,"<<n<<","<<dx<<","<<rho<<","<<dt;
    for(double v:j)std::cout<<","<<v;
    for(double v:torque)std::cout<<","<<v;
    std::cout<<","<<energy<<","<<b.clamp_solid_faces<<","
             <<b.clamp_exterior_faces<<","<<b.max_clamp_after_velocity_m_s<<"\n";
}

}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,dx,rho,dt,analytic_pressure_pa,analytic_energy_J,"
                    "div_after,gradient_residual,energy_observed_J,work_J,"
                    "quadratic_J,wetwet,wetair\n";
        run(6,0.1,1000.0,0.02,false);
        run(8,0.125,900.0,0.03,false);
        run(12,0.08,1100.0,0.015,false);
        run(8,0.1,1000.0,0.02,true);
        run(12,0.08,1100.0,0.015,true);
        run_clamp(8,0.1,1000.0,0.02);
        run_clamp(12,0.08,1100.0,0.015);
        std::cout<<"PASS: P2 projection-gradient budget assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" after "<<assertions<<" checks\n";
        return 1;
    }
}
