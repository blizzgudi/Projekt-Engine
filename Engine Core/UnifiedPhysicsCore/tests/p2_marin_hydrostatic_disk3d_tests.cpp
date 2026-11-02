// P2 PARTIAL: independent hydrostatic disk geometry vs actual native
// MAC/VOF footprint pressure sampler. Never equate this with lab validation.
#include "upc/p2_marin_hydrostatic_disk3d.hpp"
#include "upc/p2_marin_footprint_pressure3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Disk=upc::P2MarinHydrostaticDisk3D;
int checks=0;
void require(bool ok,const char* why) {
    ++checks;if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    require(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
template<class F>void reject(F f,const char* why) {
    bool threw=false;
    try {f();}catch(const std::exception&){threw=true;}
    require(threw,why);
}
std::array<double,2> numerical_disk(double H,double zc,double R) {
    // Independent Simpson integral of vertical circular strips, not the
    // closed-form antiderivative used by the production reference.
    constexpr int n=200000;
    constexpr double pi=3.141592653589793238462643383279502884;
    double wet=0,pressure=0;
    const double dz=2*R/double(n);
    for(int k=0;k<=n;++k) {
        const double z=zc-R+k*dz;
        const double chord=2*std::sqrt(std::max(0.,R*R-(z-zc)*(z-zc)));
        const double weight=(k==0||k==n)?1.:(k%2?4.:2.);
        if(z<H)wet+=weight*chord;
        pressure+=weight*chord*std::max(0.,H-z);
    }
    return {wet*dz/(3*pi*R*R),
            1000.*9.81*pressure*dz/(3*pi*R*R)};
}
void analytic() {
    constexpr double R=.01;
    const auto sensors=upc::P2MarinPressureSensors3D::transducers_m();
    for(double H:{0.,.011,.021,.026,.061,.101,.141,.161,.19,.30}) {
        const auto result=Disk::test02(H,R);
        for(int i=0;i<8;++i) {
            require(result[i].wet_area_fraction>=0&&
                    result[i].wet_area_fraction<=1,"bounded disk wet area");
            if(i<4) {
                const auto numerical=numerical_disk(H,sensors[i][2],R);
                near(result[i].wet_area_fraction,numerical[0],5e-6,
                     "independent circular segment wet area");
                near(result[i].disk_mean_gauge_pressure_pa,numerical[1],.02,
                     "independent circular segment pressure integral");
            } else {
                const double expected=1000.*9.81*
                    std::max(0.,H-.161);
                near(result[i].disk_mean_gauge_pressure_pa,expected,1e-10,
                     "roof pressure from actual physical roof elevation");
            }
            constexpr double pi=3.141592653589793238462643383279502884;
            near(result[i].integrated_gauge_load_n,
                 result[i].disk_mean_gauge_pressure_pa*pi*R*R,1e-12,
                 "pressure load integral");
        }
    }
    const auto middle=Disk::front(.021,.021,R);
    near(middle.wet_area_fraction,.5,1e-14,"half disk at waterline");
    constexpr double pi=3.141592653589793238462643383279502884;
    near(middle.disk_mean_gauge_pressure_pa,
         1000.*9.81*(2.*R/(3.*pi)),1e-12,"half disk exact pressure");
    for(double invalid:{-1.,std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity()})
        reject([&]{Disk::test02(invalid,R);},"invalid waterline rejected");
    reject([&]{Disk::test02(.2,0);},"zero radius rejected");
    reject([&]{Disk::test02(.2,.021);},"disk crosses physical box edge");
    reject([&]{Disk::front(.005,.2,.01);},"disk crosses tank floor");
    reject([&]{Disk::front(.021,.2,.01,-1);},"negative density rejected");
    reject([&]{Disk::roof(.161,.2,.01,1000.,0);},"zero gravity rejected");
    std::cout<<"DISK_ANALYTIC assertions="<<checks
             <<" half_wet="<<middle.wet_area_fraction
             <<" half_pressure_pa="<<middle.disk_mean_gauge_pressure_pa<<"\n";
}
void native_footprint_bias(std::size_t nx) {
    const double dx=3.2/double(nx);
    const std::size_t ny=nx*10/32,nz=ny;
    constexpr double H=.19,R=.01;
    constexpr double xmin=.6635,xmax=.8245,ymin=.295,ymax=.698,zmax=.161;
    const auto index=[&](std::size_t x,std::size_t y,std::size_t z) {
        return (z*ny+y)*nx+x;
    };
    std::vector<double> p(nx*ny*nz,0.),f(nx*ny*nz,0.);
    std::vector<unsigned char> s(nx*ny*nz,0);
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const double xc=(x+.5)*dx,yc=(y+.5)*dx,zc=(z+.5)*dx;
        const auto i=index(x,y,z);
        s[i]=(xc>=xmin&&xc<xmax&&yc>=ymin&&yc<ymax&&zc<zmax)?1:0;
        if(!s[i]) {
            f[i]=std::clamp((H-z*dx)/dx,0.,1.);
            p[i]=1000.*9.81*std::max(0.,H-zc);
        }
    }
    const auto sample=upc::P2MarinFootprintPressure3D::sample_field(
        dx,nx,ny,nz,R,8,32,
        [&](std::size_t x,std::size_t y,std::size_t z){return p[index(x,y,z)];},
        [&](std::size_t x,std::size_t y,std::size_t z){return f[index(x,y,z)];},
        [&](std::size_t x,std::size_t y,std::size_t z){return s[index(x,y,z)]!=0;});
    const auto exact=Disk::test02(H,R);
    const double roof_expected=exact[4].disk_mean_gauge_pressure_pa;
    const double roof_observed=sample.probes[4].area_mean_gauge_pressure_pa;
    require(std::isfinite(roof_observed),"finite native roof pressure");
    require(roof_expected>0,"actual MARIN roof is submerged");
    // This is a KNOWN-FAIL physics discrepancy, not a green accuracy gate:
    // grid-cell VOF wetness and pressure at z=.25/.1667/.175 differ from
    // the actual roof at z=.161 even for manufactured hydrostatics.
    require(std::abs(roof_observed-roof_expected)>10.,
            "demonstrate unresolved coarse-grid transducer physics");
    std::cout<<"DISK_NATIVE nx="<<nx<<" dx="<<dx
             <<" roof_exact_pa="<<roof_expected
             <<" roof_current_pa="<<roof_observed
             <<" roof_error_pa="<<roof_observed-roof_expected
             <<" roof_current_wet="<<sample.probes[4].wet_area_fraction<<"\n";
}
}
int main() {
    try {
        analytic();
        native_footprint_bias(32);
        native_footprint_bias(48);
        native_footprint_bias(64);
        std::cout<<"PASS P2_MARIN_HYDROSTATIC_DISK assertions="<<checks
                 <<" measured_validation=false phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL P2_MARIN_HYDROSTATIC_DISK "<<e.what()
                 <<" assertions="<<checks<<"\n";
        return 1;
    }
}
