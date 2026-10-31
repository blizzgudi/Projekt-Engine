// P2 partial: opt-in verified MAC/VOF whole-step commit and rollback.
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool ok,const char* why){
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why){
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
void same(const upc::CoupledMacWater3D& a,const upc::CoupledMacWater3D& b,int n){
    near(a.mass_kg(),b.mass_kg(),0,"mass rollback");
    near(a.last_projection_divergence(),b.last_projection_divergence(),0,"divergence rollback");
    near(a.last_viscous_energy_before(),b.last_viscous_energy_before(),0,"energy before rollback");
    near(a.last_viscous_energy_after(),b.last_viscous_energy_after(),0,"energy after rollback");
    check(a.last_substeps()==b.last_substeps(),"substep rollback");
    check(a.last_viscosity_substeps()==b.last_viscosity_substeps(),"viscous count rollback");
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        near(a.water_fraction(x,y,z),b.water_fraction(x,y,z),0,"VOF rollback");
        near(a.pressure_pa(x,y,z),b.pressure_pa(x,y,z),0,"pressure rollback");
    }
}
void run(int n){
    const int c=n/2;
    upc::CoupledMacWater3D water(n,n,n,0.1,1000);
    // Asymmetric 3D liquid component: pressure coupling cannot converge
    // exactly in a single Gauss-Seidel iteration.
    for(int z=c;z<=c+1;++z)for(int y=c;y<=c+1;++y)
        for(int x=c;x<=c+1;++x)
            if(!(x==c+1&&y==c+1&&z==c+1))
                water.set_water(x,y,z,1);
    water.set_solid(c,c,c-1);
    const double mass=water.mass_kg();
    std::vector<upc::TransportFlux3D> delivered;
    water.set_transport_listener([&](const upc::TransportFlux3D& e){
        check(water.last_substeps()>0,"listener must see committed live state");
        delivered.push_back(e);
    });
    const auto original=water;
    bool rejected=false;
    try{water.step_verified(0.02,1,1e-12);}
    catch(const std::runtime_error&){rejected=true;}
    check(rejected,"underconverged live projection rejected");
    same(water,original,n);
    check(delivered.empty(),"failed live step emitted no transport");
    rejected=false;
    try{water.step_verified(0.02,4000,0);}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"zero live tolerance rejected");
    same(water,original,n);
    rejected=false;
    try{water.step_verified(0.02,4000,std::numeric_limits<double>::quiet_NaN());}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"NaN live tolerance rejected");
    same(water,original,n);
    // A successful verified step must match the original solver numerically,
    // with notifications emitted only after the successful commit.
    upc::CoupledMacWater3D legacy=original;
    std::vector<upc::TransportFlux3D> legacy_events;
    legacy.set_transport_listener([&](const upc::TransportFlux3D& e){
        legacy_events.push_back(e);
    });
    legacy.step(0.02,4000);
    const double residual=water.step_verified(0.02,4000,1e-8);
    near(residual,water.last_projection_divergence(),0,"returned live residual");
    check(residual<=1e-8,"live verified residual bound");
    near(water.mass_kg(),mass,1e-10,"verified water mass conservation");
    same(water,legacy,n);
    check(!delivered.empty(),"actual live transport notifications emitted");
    check(delivered.size()==legacy_events.size(),"transport notification count");
    for(std::size_t i=0;i<delivered.size();++i){
        check(delivered[i].from==legacy_events[i].from &&
              delivered[i].to==legacy_events[i].to,"transport notification indices");
        near(delivered[i].volume_m3,legacy_events[i].volume_m3,0,
             "transport notification volume");
    }
    std::cout<<"case,"<<n<<","<<mass<<","<<residual<<","
             <<water.last_substeps()<<","<<delivered.size()<<"\n";
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,mass_kg,divergence_s_inv,substeps,notifications\n";
        run(8);
        run(12);
        std::cout<<"PASS: P2 verified coupled MAC VOF assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" after "<<assertions<<" assertions\n";
        return 1;
    }
}
