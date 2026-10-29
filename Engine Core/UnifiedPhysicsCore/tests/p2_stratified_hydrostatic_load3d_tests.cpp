#include "upc/p2_stratified_hydrostatic_load3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Model=upc::P2StratifiedHydrostaticLoad3D;
std::size_t checks=0;
void require(bool ok,const char* msg) {
    ++checks;
    if(!ok)throw std::runtime_error(msg);
}
void near(double a,double b,double tolerance,const char* msg) {
    require(std::isfinite(a)&&std::isfinite(b)&&
            std::abs(a-b)<=tolerance,msg);
}
template<class F>void rejects(F fn,const char* msg) {
    bool caught=false;
    try{fn();}catch(const std::exception&){caught=true;}
    require(caught,msg);
}
std::vector<double> column(int n,double height) {
    std::vector<double> fill(n*n*n);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        fill[(z*n+y)*n+x]=std::clamp(height-z,0.0,1.0);
    return fill;
}
void scenario(int n,double height,double g) {
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=10000.0;body.centre_m={{4,4,4}};
    const auto fill=column(n,height),snapshot=fill;
    const auto out=Model::step(n,n,n,1.0,0.01,1000.0,g,box,body,fill);
    // Independent cell-centre hydrostatic pressure, NOT face traction.
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=(z*n+y)*n+x;
        const double expected=fill[i]>0.0?
            1000.0*g*(fill[i]<1.0 ? 0.5*fill[i] :
                         std::max(0.0,height-(z+0.5))):0.0;
        near(out.water_pressure_pa[i],expected,1e-8,
             "independent hydrostatic cell pressure");
    }
    // Independent hydrostatic column integral: integral_0^h rho*g*(h-z) dz
    // = rho*g*h^2/2. The partial top-cell contribution MUST be
    // weighted by its wet volume, not the entire cell volume.
    for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        double pressure_integral=0.0;
        for(int z=0;z<n;++z) {
            const auto i=(z*n+y)*n+x;
            pressure_integral+=out.water_pressure_pa[i]*fill[i];
        }
        near(pressure_integral,0.5*1000.0*g*height*height,1e-7,
             "exact wet-volume hydrostatic pressure integral");
    }
    // Independently integrate vertical pressure on ACTUAL solid faces:
    // rho*g*A*(wet box depth). This is exact for horizontal water level.
    const double wet=std::clamp(height-3.2,0.0,1.6);
    const double displaced=1.6*1.6*wet;
    const double force=1000.0*g*displaced;
    near(out.water_level_m,height,1e-12,"water level");
    near(out.displaced_water_volume_m3,displaced,1e-12,
         "independent displaced water volume");
    near(out.hydrostatic_force_n,force,1e-7,
         "analytic Archimedes pressure force");
    const double velocity=force*0.01/10000.0;
    near(out.rigid.after.velocity_m_s[2],velocity,1e-11,
         "body velocity from pressure impulse");
    near(out.rigid.after.centre_m[2],4.0+0.5*velocity*0.01,1e-11,
         "body displacement");
    near(out.box_after.min_m[2],3.2+0.5*velocity*0.01,1e-11,
         "box geometry moved");
    near(out.rigid.body_linear_impulse_Ns[2],force*0.01,1e-8,
         "body pressure impulse");
    near(out.rigid.fluid_reaction_impulse_Ns[2],-force*0.01,1e-8,
         "diagnostic UNAPPLIED fluid reaction");
    near(out.rigid.kinetic_energy_change_J,
         0.5*10000.0*velocity*velocity,1e-9,
         "independent body kinetic energy");
    near(out.rigid.energy_residual_J,0.0,1e-9,
         "rigid work/kinetic identity only");
    for(int a=0;a<2;++a) {
        near(out.rigid.after.velocity_m_s[a],0.0,0.0,
             "zero lateral velocity");
        near(out.rigid.body_angular_impulse_Nms[a],0.0,0.0,
             "zero angular impulse");
    }
    near(out.rigid.body_angular_impulse_Nms[2],0.0,0.0,
         "zero vertical angular impulse");
    near(out.rigid.after.mass_kg,10000.0,0.0,"rigid mass constant");
    require(fill==snapshot,"water fill unchanged");
    near(body.centre_m[2],4.0,0.0,"input rigid unchanged");
    std::cout<<"HYDRO "<<n<<" "<<std::setprecision(17)<<height<<" "<<g
             <<" "<<force<<" "<<out.hydrostatic_force_n
             <<" "<<velocity<<"\n";
}
void negative() {
    auto f=column(8,4.0);
    auto bad=f;bad[0]=-0.01;
    rejects([&]{Model::pressure(8,8,8,1,1000,9.81,bad);},
            "negative fraction rejected");
    bad=f;bad[0]=1.01;
    rejects([&]{Model::pressure(8,8,8,1,1000,9.81,bad);},
            "overfilled fraction rejected");
    bad=f;bad[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Model::pressure(8,8,8,1,1000,9.81,bad);},
            "NaN rejected");
    bad=f;bad[0]=0.5;bad[8*8]=0.5;
    rejects([&]{Model::pressure(8,8,8,1,1000,9.81,bad);},
            "disconnected wet column rejected");
    bad=f;bad.pop_back();
    rejects([&]{Model::pressure(8,8,8,1,1000,9.81,bad);},
            "wrong shape rejected");
    rejects([&]{Model::pressure(8,8,8,0,1000,9.81,f);},
            "zero spacing rejected");
    rejects([&]{Model::pressure(8,8,8,1,0,9.81,f);},
            "zero density rejected");
    rejects([&]{Model::pressure(8,8,8,1,1000,-9.81,f);},
            "wrong gravity rejected");
    rejects([&]{Model::pressure(8,8,8,1,1000,
            std::numeric_limits<double>::infinity(),f);},
            "infinite gravity rejected");
    rejects([&]{Model::pressure(8,8,8,1,1000,9.81,
            std::vector<double>(8*8*8,2.0));},
            "all invalid fractions rejected");
    const upc::P2Box3D box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    upc::RigidPressureState3D body;
    body.mass_kg=10000;body.centre_m={{4,4,4}};
    bad=f;bad[0]=0.5;
    rejects([&]{Model::step(8,8,8,1,0.01,1000,9.81,box,body,bad);},
            "nonhorizontal free surface rejected");
    body.centre_m[0]=4.1;
    rejects([&]{Model::step(8,8,8,1,0.01,1000,9.81,box,body,f);},
            "off-centre body rejected");
    require(f==column(8,4.0),"no mutation on failure");
}
}
int main() {
    try {
        for(int n:{8,12})for(double height:{0.0,4.0,double(n),
                                       3.125,4.35,5.75})
            for(double g:{0.0,9.81})scenario(n,height,g);
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native stratified hydrostatic body-load assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
