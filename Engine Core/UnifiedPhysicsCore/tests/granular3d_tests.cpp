#include "upc/granular3d.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int assertions=0;
void check(bool ok,const char* msg){++assertions;if(!ok)throw std::runtime_error(msg);}
void near(double a,double b,double tol,const char* msg){
    check(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
void reject(std::function<void()> f,const char* msg){
    bool thrown=false;try{f();}catch(const std::exception&){thrown=true;}
    check(thrown,msg);
}
int main(){
try{
    MaterialLibrary lib;
    lib.add({10,Phase::Solid,2400,0});
    lib.add({20,Phase::Solid,1700,0});
    lib.add({30,Phase::Liquid,1000,0.001});
    const auto box=Vec3{0,0,0},far=Vec3{4,4,4};
    {
        GranularWorld3D w(lib,box,far);
        w.define_material(10,{1,0});
        w.set_gravity({0,0,0});
        auto ia=w.add_grain(10,{1,1,1},0.1,{1,0,0});
        auto ib=w.add_grain(10,{1.15,1,1},0.1,{-1,0,0});
        const double m0=w.total_mass_kg();
        const Vec3 p0=w.momentum();
        const auto k0=w.kinetic_energy_j();
        auto report=w.step(0.001);
        check(report.particle_contacts>0,"Two-body overlap missed");
        near(w.grain(ia).velocity.x,-1,1e-10,"Elastic equal-mass collision a");
        near(w.grain(ib).velocity.x,1,1e-10,"Elastic equal-mass collision b");
        near(w.momentum().x,p0.x,1e-12,"Pair linear momentum");
        near(w.kinetic_energy_j(),k0,1e-12,"Perfect elasticity energy");
        near(w.total_mass_kg(),m0,1e-12,"Elastic mass drift");
        check(w.max_pair_overlap_m()<1e-10,"Elastic overlap not removed");
    }
    {
        GranularWorld3D w(lib,box,far);
        w.define_material(10,{0,0.5});
        w.set_gravity({0,0,0});
        auto a=w.add_grain(10,{1,1,1},0.1,{1,0.2,0});
        auto b=w.add_grain(10,{1.15,1,1},0.1,{-1,-0.2,0});
        const auto e0=w.kinetic_energy_j();
        const auto p0=w.momentum();
        w.step(0.001);
        check(w.kinetic_energy_j()<e0,"Inelastic pair generated energy");
        near(w.momentum().x,p0.x,1e-12,"Inelastic pair momentum x");
        near(w.momentum().y,p0.y,1e-12,"Friction impulse momentum y");
        check(w.max_pair_overlap_m()<1e-10,"Inelastic interpenetration");
        (void)a;(void)b;
    }
    {
        GranularWorld3D w(lib,box,far);
        w.define_material(10,{0,0.6});
        const auto grain=w.add_grain(10,{2,2,0.15},0.1,{0.2,0,0});
        const auto m0=w.total_mass_kg();
        for(int n=0;n<1500;++n){
            const auto report=w.step(0.002);
            (void)report;
            const auto c=w.grain(grain);
            check(c.position.z>=c.radius_m-1e-10,"Floor penetration while resting");
            check(c.position.z<=far.z-c.radius_m+1e-10,"Box wall penetration");
            check(std::isfinite(c.velocity.z),"Infinite contact velocity");
        }
        near(w.grain(grain).position.z,0.1,1e-9,"Resting grain drift");
        near(w.grain(grain).velocity.z,0,1e-9,"Resting grain vertical jitter");
        near(w.total_mass_kg(),m0,1e-12,"Resting mass conservation");
        check(w.grain(grain).velocity.x<0.2,"Friction did not act");
    }
    {
        GranularWorld3D w(lib,box,far);
        w.define_material(10,{0,0.6});
        w.define_material(20,{0.1,0.3});
        for(int z=0;z<3;++z)for(int y=0;y<3;++y)
          for(int x=0;x<3;++x)
            w.add_grain(((x+y+z)%2)?10:20,
                        {1.5+x*0.105,1.5+y*0.105,0.1+z*0.105},0.05);
        check(w.count()==27,"Missing granular particles");
        const auto total=w.total_mass_kg();
        for(int n=0;n<400;++n){
            w.step(0.001);
            for(std::size_t i=0;i<w.count();++i){
                const auto& g=w.grain(i);
                check(g.position.z>=g.radius_m-1e-9,"Pile falls through floor");
                check(g.position.x>=g.radius_m-1e-9,"Pile crosses side wall");
                check(std::isfinite(g.velocity.x)&&std::isfinite(g.velocity.y)&&
                      std::isfinite(g.velocity.z),"DEM pile diverged");
            }
            check(w.max_pair_overlap_m()<0.025,"DEM granular pile overlap excessive");
        }
        near(w.total_mass_kg(),total,1e-11,"Granular mass changed");
    }
    {
        GranularWorld3D w(lib,box,far);
        reject([&]{w.define_material(30,{0,0.1});},"Liquid registered as granular");
        reject([&]{w.define_material(10,{2,0.1});},"Over-unity restitution accepted");
        w.define_material(10,{0,0});
        reject([&]{w.define_material(10,{0,0});},"Duplicate granular properties");
        reject([&]{w.add_grain(20,{1,1,1},0.1);},"Unknown grain material accepted");
        reject([&]{w.add_grain(10,{1,1,1},-1);},"Negative grain radius accepted");
        reject([&]{w.add_grain(10,{0,0,0},0.2);},"Out-of-domain grain accepted");
        reject([&]{w.step(0);},"Zero DEM time accepted");
    }
    std::cout<<"PASS: P5 granular 3D pair collision, restitution, friction,"
                " gravity, 27-grain pile, mass/momentum/energy; checks="
             <<assertions<<"\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<assertions<<" checks: "<<e.what()<<"\n";return 1;
}
}
