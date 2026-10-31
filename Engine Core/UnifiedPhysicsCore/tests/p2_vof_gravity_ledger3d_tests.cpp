// P2 bounded VOF gravity-potential transport audit, not full energy conservation.
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void near(double a,double b,double tol,const char* why){
    need(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
void run(int n){
    constexpr double dx=0.1,rho=1000,g=9.81;
    upc::CoupledMacWater3D static_water(n,n,n,dx,rho);
    static_water.set_water(1,1,1,0.25);
    static_water.set_water(2,2,3,0.75);
    near(static_water.mass_kg(),1,1e-13,"static mass");
    near(static_water.water_gravitational_potential_j(),
         g*(0.25*0.15+0.75*0.35),1e-12,"static analytic potential");
    near(static_water.last_vof_transport_potential_delta_j(),0,0,"initial ledger");
    const int c=n/2;
    upc::CoupledMacWater3D water(n,n,n,dx,rho);
    double analytic_before=0;
    for(int z=c;z<=c+1;++z)for(int y=c;y<=c+1;++y)
    for(int x=c;x<=c+1;++x){
        if(x==c+1&&y==c+1&&z==c+1)continue;
        water.set_water(x,y,z,1);
        analytic_before+=rho*dx*dx*dx*g*(z+0.5)*dx;
    }
    water.set_solid(c,c,c-1);
    const auto original=water;
    const double before=water.water_gravitational_potential_j();
    near(before,analytic_before,1e-12,"independent initial potential");
    double event_delta=0;
    int events=0;
    water.set_transport_listener([&](const upc::TransportFlux3D& e){
        const auto z1=e.from/(static_cast<std::size_t>(n)*n);
        const auto z2=e.to/(static_cast<std::size_t>(n)*n);
        event_delta+=rho*g*e.volume_m3*(static_cast<double>(z2)-static_cast<double>(z1))*dx;
        need(e.volume_m3>0&&std::isfinite(e.volume_m3),"committed event volume");
        ++events;
    });
    bool rejected=false;
    try{water.step_verified_reprojected(0.02,1,1e-12);}
    catch(const std::runtime_error&){rejected=true;}
    need(rejected,"underconvergence rejected");
    near(water.water_gravitational_potential_j(),before,0,"rollback potential");
    near(water.last_vof_transport_potential_delta_j(),0,0,"rollback ledger");
    need(events==0,"no failed notifications");
    const double residual=water.step_verified_reprojected(0.02,4000,1e-8);
    const double after=water.water_gravitational_potential_j();
    const double delta=water.last_vof_transport_potential_delta_j();
    near(water.mass_kg(),original.mass_kg(),1e-10,"mass conservation");
    near(after-before,delta,2e-12,"state potential ledger closure");
    near(event_delta,delta,2e-12,"independent event ledger closure");
    need(events>0,"actual transport");
    need(delta<0,"gravity lowers potential");
    need(residual<=1e-8,"post VOF divergence");
    const auto accepted=water;
    rejected=false;
    try{water.step_verified_reprojected(0.02,1,1e-14);}
    catch(const std::runtime_error&){rejected=true;}
    need(rejected,"second underconvergence rejected");
    near(water.last_vof_transport_potential_delta_j(),
         accepted.last_vof_transport_potential_delta_j(),0,"ledger rollback after commit");
    near(water.water_gravitational_potential_j(),
         accepted.water_gravitational_potential_j(),0,"potential rollback after commit");
    std::cout<<"case,"<<n<<","<<original.mass_kg()<<","<<water.mass_kg()
             <<","<<before<<","<<after<<","<<delta<<","<<event_delta
             <<","<<residual<<","<<events<<"\n";
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,mass_before,mass_after,potential_before,potential_after,ledger_delta,event_delta,residual,events\n";
        run(8);run(12);
        std::cout<<"PASS: VOF gravitational transfer ledger assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" assertions\n";
        return 1;
    }
}
