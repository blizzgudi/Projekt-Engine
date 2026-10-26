// P2 independent masked 3D MAC diffusion reference: mixed solid Dirichlet
// and liquid/air Neumann boundaries, 3 grids, adaptive explicit substeps.
// This is a DISCRETE eigenmode check, not continuum FSI or measured CFD.
#include "upc/viscous_mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace upc;
namespace {
constexpr double pi=3.141592653589793238462643383279502884;
std::size_t assertions=0;
void require(bool condition,const char* reason){
    ++assertions;
    if(!condition)throw std::runtime_error(reason);
}
void near(double actual,double expected,double tol,const char* reason){
    require(std::isfinite(actual)&&std::isfinite(expected)
            &&std::abs(actual-expected)<=tol,reason);
}
struct Row {
    int nx,ny,nz,L;
    std::size_t substeps;
    double alpha,lambda,factor,max_error,energy_ratio,ledger_residual;
};
Row run(int L,int ny,int nz){
    constexpr int left=2;
    const int nx=left+L+3;
    constexpr double dx=0.1,nu=0.05,dt=0.08;
    const auto cell=[&](int x,int y,int z){
        return (static_cast<std::size_t>(z)*ny+y)*nx+x;
    };
    std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0);
    for(int z=0;z<nz;++z)
    for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x){
        if(x<left)solid[cell(x,y,z)]=1;
        else if(x<left+L)wet[cell(x,y,z)]=1;
    }
    const double theta_x=pi/(2*L+1);
    const double theta_y=pi/ny;
    const double theta_z=pi/(nz+1);
    const double lambda=2*(std::cos(theta_x)-1)
        +2*(std::cos(theta_y)-1)+2*(std::cos(theta_z)-1);
    ViscousMacDiffusion3D diffusion(nx,ny,nz,dx);
    MacProjection3D mac(nx,ny,nz,dx);
    const auto shape=[&](int x,int y,int z){
        return std::sin((x-left+1)*theta_x)
            *std::sin(y*theta_y)*std::sin((z+1)*theta_z);
    };
    for(int z=0;z<nz;++z)
    for(int y=1;y<ny;++y)
    for(int x=left;x<left+L;++x)
        mac.set_face_velocity_y(x,y,z,shape(x,y,z));
    const double initial=diffusion.face_energy_proxy(mac);
    MaskedMacMomentumLedger3D ledger;
    const auto steps=diffusion.step_masked(mac,dt,nu,wet,solid,{0,0,0},&ledger);
    const double alpha=nu*dt/(dx*dx*static_cast<double>(steps));
    const double factor=std::pow(1+alpha*lambda,static_cast<double>(steps));
    require(steps==3&&ledger.substeps==steps,"wrong adaptive substeps");
    require(factor>0&&factor<1,"unstable eigenmode factor");
    double max_error=0;
    for(int z=0;z<nz;++z)
    for(int y=0;y<=ny;++y)
    for(int x=0;x<nx;++x){
        const double expected=(x>=left&&x<left+L&&y>0&&y<ny)
            ?shape(x,y,z)*factor:0;
        const double actual=mac.face_velocity_y(x,y,z);
        max_error=std::max(max_error,std::abs(actual-expected));
        near(actual,expected,2e-12,"mixed-BC 3D eigenmode mismatch");
    }
    for(int z=0;z<nz;++z)
    for(int y=0;y<ny;++y)
    for(int x=0;x<=nx;++x)
        near(mac.face_velocity_x(x,y,z),0,0,"spurious normal velocity");
    for(int z=0;z<=nz;++z)
    for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        near(mac.face_velocity_z(x,y,z),0,0,"spurious z velocity");
    const double energy_ratio=diffusion.face_energy_proxy(mac)/initial;
    near(energy_ratio,factor*factor,3e-12,"mixed-BC energy eigenratio mismatch");
    double residual=0;
    for(int axis=0;axis<3;++axis){
        const auto k=static_cast<std::size_t>(axis);
        const double expected_delta=
            -ledger.internal_gradient_loss_proxy_m2_s2[k]
            -ledger.solid_gradient_loss_proxy_m2_s2[k]
            -ledger.exterior_gradient_loss_proxy_m2_s2[k]
            +ledger.prescribed_wall_work_proxy_m2_s2[k]
            +ledger.explicit_euler_correction_proxy_m2_s2[k];
        near(ledger.face_energy_change_proxy_m2_s2[k],expected_delta,
             2e-11,"energy budget closure");
        near(ledger.closure_residual_sum_m_s[k],0,2e-11,
             "momentum-proxy closure");
        near(ledger.prescribed_wall_work_proxy_m2_s2[k],0,0,
             "stationary wall performed work");
        residual=std::max(residual,std::abs(ledger.energy_closure_residual_proxy_m2_s2[k]));
        near(ledger.energy_closure_residual_proxy_m2_s2[k],0,2e-11,
             "energy-proxy residual");
    }
    // One-step inhomogeneous Dirichlet shear: a prescribed wall
    // adds alpha * u_wall ONLY to the first wet x-layer.
    MacProjection3D forced(nx,ny,nz,dx);
    for(int z=0;z<nz;++z)
    for(int y=1;y<ny;++y)
    for(int x=left;x<left+L;++x)
        forced.set_face_velocity_y(x,y,z,shape(x,y,z));
    constexpr double short_dt=0.01,wall_speed=0.3;
    const double one_alpha=nu*short_dt/(dx*dx);
    require(diffusion.step_masked(forced,short_dt,nu,wet,solid,
                                  {0,wall_speed,0})==1,
            "wall-driven one-step substeps mismatch");
    for(int z=0;z<nz;++z)
    for(int y=1;y<ny;++y)
    for(int x=left;x<left+L;++x){
        const double expected=shape(x,y,z)*(1+one_alpha*lambda)
            +(x==left?one_alpha*wall_speed:0);
        near(forced.face_velocity_y(x,y,z),expected,2e-12,
             "mixed-BC forced wall eigenmode mismatch");
    }
    // No-op nu=0 must preserve existing face values.
    const double saved=forced.face_velocity_y(left,1,1);
    require(diffusion.step_masked(forced,short_dt,0,wet,solid)==0,
            "nu=0 invoked diffusion");
    near(forced.face_velocity_y(left,1,1),saved,0,"nu=0 changed face");
    return {nx,ny,nz,L,steps,alpha,lambda,factor,max_error,energy_ratio,residual};
}

struct ContinuumRow {
    int nx,ny,nz,L;
    int timesteps;
    double dx,continuum_factor,numerical_factor,max_error,max_ledger_residual;
};
// Independent continuum manufactured 3D shear mode. The exact PDE solution
// is exp(-nu*(kx^2+ky^2+kz^2)*T) times the initial separable sine profile.
// X: solid Dirichlet at x=0 and free-surface Neumann at x=1m.
// Y,Z: zero Dirichlet exterior. The finite-difference grid has ghost-node
// boundaries; the analytic physical positions reflect that discretization.
// This is a continuum diffusion benchmark, NOT measured Navier-Stokes/FSI.
ContinuumRow continuum_run(int L) {
    constexpr int left=2;
    const int nx=left+L+3,ny=L+1,nz=L;
    const double dx=1.0/(L+0.5);
    constexpr double nu=0.01,T=0.04;
    constexpr int timesteps=32;
    const double dt=T/timesteps;
    const double tx=pi/(2*L+1),ty=pi/ny,tz=pi/(nz+1);
    const double continuum_factor=std::exp(
        -nu*T*(tx*tx+ty*ty+tz*tz)/(dx*dx));
    const double discrete_lambda=2*(std::cos(tx)-1)
        +2*(std::cos(ty)-1)+2*(std::cos(tz)-1);
    const double alpha=nu*dt/(dx*dx);
    const double numerical_factor=std::pow(1+alpha*discrete_lambda,timesteps);
    require(continuum_factor>0&&continuum_factor<1,
            "invalid continuum decay factor");
    require(alpha>0&&alpha<0.15,"continuum benchmark CFL mismatch");
    const auto cell=[&](int x,int y,int z){
        return (static_cast<std::size_t>(z)*ny+y)*nx+x;
    };
    std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0);
    for(int z=0;z<nz;++z)
    for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x){
        if(x<left)solid[cell(x,y,z)]=1;
        else if(x<left+L)wet[cell(x,y,z)]=1;
    }
    const auto shape=[&](int x,int y,int z){
        return std::sin((x-left+1)*tx)
            *std::sin(y*ty)*std::sin((z+1)*tz);
    };
    MacProjection3D mac(nx,ny,nz,dx);
    ViscousMacDiffusion3D diffusion(nx,ny,nz,dx);
    for(int z=0;z<nz;++z)
    for(int y=1;y<ny;++y)
    for(int x=left;x<left+L;++x)
        mac.set_face_velocity_y(x,y,z,shape(x,y,z));
    double max_residual=0;
    for(int t=0;t<timesteps;++t){
        MaskedMacMomentumLedger3D ledger;
        const auto substeps=diffusion.step_masked(mac,dt,nu,wet,solid,
                                                  {0,0,0},&ledger);
        require(substeps==1&&ledger.substeps==1,
                "continuum benchmark time subdivision mismatch");
        for(int axis=0;axis<3;++axis){
            const auto k=static_cast<std::size_t>(axis);
            const double e=std::abs(ledger.energy_closure_residual_proxy_m2_s2[k]);
            const double p=std::abs(ledger.closure_residual_sum_m_s[k]);
            max_residual=std::max({max_residual,e,p});
            // Summing tens of thousands of faces accumulates roundoff;
            // bound absolute ledger closure to 1e-8 in face-sum units.
            require(e<1e-8&&p<1e-8,"continuum mode ledger did not close");
        }
    }
    double max_error=0;
    for(int z=0;z<nz;++z)
    for(int y=0;y<=ny;++y)
    for(int x=0;x<nx;++x){
        const double initial=(x>=left&&x<left+L&&y>0&&y<ny)
            ?shape(x,y,z):0;
        const double actual=mac.face_velocity_y(x,y,z);
        near(actual,initial*numerical_factor,3e-11,
             "continuum benchmark numerical mode mismatch");
        max_error=std::max(max_error,
                           std::abs(actual-initial*continuum_factor));
    }
    require(max_error>0&&max_error<1e-3,
            "continuum manufactured mode accuracy bound");
    return {nx,ny,nz,L,timesteps,dx,continuum_factor,numerical_factor,
            max_error,max_residual};
}
}
int main(){
try{
    std::cout<<std::setprecision(17);
    std::cout<<"nx,ny,nz,wet_columns,substeps,alpha,lambda,expected_factor,max_error,energy_ratio,ledger_residual\n";
    for(const auto dims:std::array<std::array<int,3>,3>{{{3,7,6},{5,9,8},{7,11,10}}}){
        const auto row=run(dims[0],dims[1],dims[2]);
        std::cout<<row.nx<<","<<row.ny<<","<<row.nz<<","<<row.L<<","
                 <<row.substeps<<","<<row.alpha<<","<<row.lambda<<","
                 <<row.factor<<","<<row.max_error<<","<<row.energy_ratio
                 <<","<<row.ledger_residual<<"\n";
    }
    std::cout<<"continuum_nx,ny,nz,wet_columns,timesteps,dx,continuum_factor,numerical_factor,max_error,ledger_residual\n";
    double previous_error=0;
    for(int L:std::array<int,4>{{5,9,17,33}}){
        const auto row=continuum_run(L);
        if(previous_error>0)
            require(row.max_error<previous_error*0.65,
                    "3D continuum manufactured mode did not converge");
        previous_error=row.max_error;
        std::cout<<row.nx<<","<<row.ny<<","<<row.nz<<","<<row.L<<","
                 <<row.timesteps<<","<<row.dx<<","<<row.continuum_factor
                 <<","<<row.numerical_factor<<","<<row.max_error
                 <<","<<row.max_ledger_residual<<"\n";
    }
    std::cout<<"PASS: P2 masked mixed-BC eigenmode assertions="<<assertions
             <<" phase_complete=false\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL: P2 mixed-BC eigenmode after assertions="<<assertions
             <<" reason="<<e.what()<<"\n";
    return 1;
}
}
