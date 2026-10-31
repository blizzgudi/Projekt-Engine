// P2 partial: residual-verified MAC pressure after VOF changes the wet mask.
#include "upc/coupled_water3d.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int checks=0;
void require(bool ok,const char* message){
    ++checks;
    if(!ok)throw std::runtime_error(message);
}
void near(double a,double b,double tol,const char* message){
    require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,message);
}
void compare(const upc::CoupledMacWater3D& a,const upc::CoupledMacWater3D& b,int n){
    near(a.mass_kg(),b.mass_kg(),0,"rollback mass");
    near(a.last_projection_divergence(),b.last_projection_divergence(),0,"rollback residual");
    require(a.last_substeps()==b.last_substeps(),"rollback steps");
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        near(a.water_fraction(x,y,z),b.water_fraction(x,y,z),0,"rollback VOF");
        near(a.pressure_pa(x,y,z),b.pressure_pa(x,y,z),0,"rollback pressure");
    }
}
void run(int n){
    const int c=n/2;
    upc::CoupledMacWater3D water(n,n,n,0.1,1000.0);
    for(int z=c;z<=c+1;++z)for(int y=c;y<=c+1;++y)
        for(int x=c;x<=c+1;++x)
            if(!(x==c+1&&y==c+1&&z==c+1))
                water.set_water(x,y,z,1.0);
    water.set_solid(c,c,c-1);
    const auto original=water;
    int callbacks=0;
    water.set_transport_listener([&](const upc::TransportFlux3D& event){
        require(water.last_substeps()>0,"callback after commit");
        require(event.volume_m3>0,"positive transported volume");
        ++callbacks;
    });
    bool failed=false;
    try{water.step_verified_reprojected(0.02,1,1e-12);}
    catch(const std::runtime_error&){failed=true;}
    require(failed,"underconvergence must reject");
    compare(water,original,n);
    require(callbacks==0,"no notifications on rollback");
    failed=false;
    try{water.step_verified_reprojected(0.02,4000,
                                        std::numeric_limits<double>::quiet_NaN());}
    catch(const std::invalid_argument&){failed=true;}
    require(failed,"NaN tolerance must reject");
    compare(water,original,n);
    failed=false;
    try{water.step_verified_reprojected(-0.01,4000,1e-8);}
    catch(const std::invalid_argument&){failed=true;}
    require(failed,"negative timestep must reject");
    compare(water,original,n);
    const double before=water.mass_kg();
    const double residual=water.step_verified_reprojected(0.02,4000,1e-8);
    const double actual=water.current_wet_divergence_s_inv();
    near(before,7.0,1e-11,"independent seven-cell mass");
    near(water.mass_kg(),before,1e-10,"conservative post-VOF mass");
    near(residual,actual,1e-12,"returned residual matches CURRENT wet mask");
    near(residual,water.last_projection_divergence(),0,"diagnostic residual");
    require(residual<=1e-8,"post-VOF residual tolerance");
    require(callbacks>0,"live transport occurred");
    int wet=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        const double f=water.water_fraction(x,y,z);
        require(std::isfinite(f)&&f>=0&&f<=1,"bounded final VOF");
        require(!(water.is_solid(x,y,z)&&f>0),"solid stays dry");
        if(f>1e-5)++wet;
    }
    require(wet>7,"VOF reached previously dry cells");
    // Original verified path can be compared on the SAME initial state:
    // it projects before advection and retains the old wet mask.
    auto original_verified=original;
    original_verified.step_verified(0.02,4000,1e-8);
    const double original_final=original_verified.current_wet_divergence_s_inv();
    require(std::isfinite(original_final),"baseline final divergence finite");
    require(original_final>1.0,"old-mask projection leaves measurable new-wet divergence");
    require(residual<original_final*1e-6,"new-mask projection reduces divergence by 1e6");
    std::cout<<"case,"<<n<<","<<before<<","<<residual<<","
             <<original_final<<","<<wet<<","<<callbacks<<"\n";
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,mass_kg,post_divergence,legacy_post_divergence,wet_cells,callbacks\n";
        run(8);
        run(12);
        std::cout<<"PASS: post-transport verified projection assertions="
                 <<checks<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" assertions\n";
        return 1;
    }
}
