// P2 partial: independent spectral solution for transient wall-driven 3D MAC shear.
// Fixed solid geometry and a prescribed tangential ghost velocity; not FSI.
#include "upc/viscous_mac3d.hpp"
#include <algorithm>
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
void require(bool ok,const char* why) {
    ++assertions;
    if(!ok) throw std::runtime_error(why);
}
struct Row { int n,L; double alpha,center,spectral_center,max_error,energy; };
Row run(int n,int L) {
    constexpr int left=2;
    const int nx=left+L+3,ny=n,nz=n,iterations=40;
    constexpr double dx=0.1,nu=0.012,dt=0.03,wall_speed=0.4;
    const double alpha=nu*dt/(dx*dx);
    auto index=[&](int x,int y,int z) {
        return (static_cast<std::size_t>(z)*ny+y)*nx+x;
    };
    std::vector<std::uint8_t> wet(nx*ny*nz,0),solid(nx*ny*nz,0);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x) {
        if(x<left)solid[index(x,y,z)]=1;
        else if(x<left+L)wet[index(x,y,z)]=1;
    }
    MacProjection3D mac(nx,ny,nz,dx);
    ViscousMacDiffusion3D viscosity(nx,ny,nz,dx);
    MaskedMacMomentumLedger3D ledger;
    for(int t=0;t<iterations;++t) {
        const auto sub=viscosity.step_masked(
            mac,dt,nu,wet,solid,{0.0,wall_speed,0.0},&ledger);
        require(sub==1&&ledger.substeps==1,"wall-driven shear CFL");
        for(int axis=0;axis<3;++axis)
            require(std::abs(ledger.energy_closure_residual_proxy_m2_s2[axis])<2e-11,
                    "wall-driven shear energy proxy ledger");
    }
    // Independent exact DISCRETE solution via tensor-product sine modes.
    // x: Dirichlet solid ghost at i=-1, Neumann air at i=L.
    // y: Dirichlet zero at y=0,n; z: Dirichlet zero ghosts at -1,n.
    // For each mode, u_{k+1}=r*u_k+alpha*wall_speed*b_mode.
    std::vector<double> reference(static_cast<std::size_t>(L)*(ny-1)*nz,0.0);
    auto refidx=[&](int i,int y,int z) {
        return (static_cast<std::size_t>(z)*(ny-1)+(y-1))*L+i;
    };
    for(int mx=0;mx<L;++mx) {
        const double tx=(2*mx+1)*pi/(2*L+1);
        double nxnorm=0;
        for(int i=0;i<L;++i)nxnorm+=std::pow(std::sin((i+1)*tx),2);
        for(int my=1;my<ny;++my) {
            const double ty=my*pi/ny;
            double ynorm=0,ysum=0;
            for(int y=1;y<ny;++y) {
                const double s=std::sin(y*ty); ynorm+=s*s; ysum+=s;
            }
            for(int mz=1;mz<=nz;++mz) {
                const double tz=mz*pi/(nz+1);
                double znorm=0,zsum=0;
                for(int z=0;z<nz;++z) {
                    const double s=std::sin((z+1)*tz); znorm+=s*s; zsum+=s;
                }
                const double lambda=2*(std::cos(tx)+std::cos(ty)+std::cos(tz)-3);
                const double r=1+alpha*lambda;
                require(r>0&&r<1,"spectral mode unstable");
                const double forcing=std::sin(tx)*ysum*zsum/(nxnorm*ynorm*znorm);
                const double gain=alpha*wall_speed*forcing*
                    (1-std::pow(r,iterations))/(1-r);
                for(int z=0;z<nz;++z)for(int y=1;y<ny;++y)
                for(int i=0;i<L;++i)
                    reference[refidx(i,y,z)]+=gain*std::sin((i+1)*tx)
                        *std::sin(y*ty)*std::sin((z+1)*tz);
            }
        }
    }
    double max_error=0,expected_energy=0;
    for(int z=0;z<nz;++z)for(int y=0;y<=ny;++y)
    for(int x=0;x<nx;++x) {
        const double expected=x>=left&&x<left+L&&y>0&&y<ny
            ?reference[refidx(x-left,y,z)]:0.0;
        const double actual=mac.face_velocity_y(x,y,z);
        max_error=std::max(max_error,std::abs(actual-expected));
        expected_energy+=0.5*dx*dx*dx*expected*expected;
        require(std::isfinite(actual)&&std::abs(actual-expected)<3e-11,
                "3D driven shear disagrees with independent spectral solution");
        require(actual>=-1e-12&&actual<=wall_speed+1e-12,
                "driven shear violates maximum principle");
    }
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<=nx;++x)
        require(mac.face_velocity_x(x,y,z)==0,"spurious wall-normal flow");
    for(int z=0;z<=nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x)
        require(mac.face_velocity_z(x,y,z)==0,"spurious transverse flow");
    const double energy=viscosity.face_energy_proxy(mac);
    require(std::abs(energy-expected_energy)<2e-11,"spectral energy proxy mismatch");
    const double center=mac.face_velocity_y(left,ny/2,nz/2);
    require(center>0&&center<wall_speed,"wall shear center outside physical bounds");
    require(mac.face_velocity_y(left,ny/2,nz/2)>
            mac.face_velocity_y(left,1,0),"no three-dimensional transverse shear");
    return {n,L,alpha,center,reference[refidx(0,ny/2,nz/2)],max_error,energy};
}
}
int main() {
    try {
        const auto a=run(8,5),b=run(12,7);
        std::cout<<std::setprecision(17)
            <<"PASS P2 3D driven wall spectral: checks="<<assertions
            <<" n8_center="<<a.center<<" n8_reference="<<a.spectral_center
            <<" n8_error="<<a.max_error<<" n8_energy="<<a.energy
            <<" n12_center="<<b.center<<" n12_reference="<<b.spectral_center
            <<" n12_error="<<b.max_error<<" n12_energy="<<b.energy
            <<" alpha="<<a.alpha<<" steps=40\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL P2 driven wall spectral after "<<assertions
                 <<" checks: "<<e.what()<<"\n";
        return 1;
    }
}
