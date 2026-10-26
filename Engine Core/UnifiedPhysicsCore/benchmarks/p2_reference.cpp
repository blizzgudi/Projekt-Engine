// P2 quantitative 3D validation and CPU budget. No gameplay/Unreal/GPU claims.
#include "upc/coupled_water3d.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace upc;

struct Result {
    double ms=0, volume0=0, volume1=0, center_x=0, pressure=0, reference=0;
    double max_div=0;
    std::size_t bytes=0;
};
template<class F> double time_ms(F f){
    auto s=std::chrono::steady_clock::now(); f();
    return std::chrono::duration<double,std::milli>(
         std::chrono::steady_clock::now()-s).count();
}
void require(bool ok,const char* message){
    if(!ok)throw std::runtime_error(message);
}
Result dam_break(int resolution){
    // All resolutions model a 1.6 x 0.8 x 1.2m tank; initial 0.4m-wide
    // dam of 0.8m-high water across the entire y dimension.
    const int nx=resolution,ny=resolution/2,nz=3*resolution/4;
    const double dx=1.6/resolution;
    CoupledMacWater3D w(nx,ny,nz,dx);
    for(int z=0;z<2*nz/3;++z)
      for(int y=0;y<ny;++y)
        for(int x=0;x<nx/4;++x)w.set_water(x,y,z,1);
    const double v0=w.volume_m3();
    const double ms=time_ms([&]{
        for(int frame=0;frame<20;++frame)w.step(0.01,250);
    });
    const double v1=w.volume_m3();
    require(std::isfinite(ms)&&ms>=0,"Dam break time invalid");
    require(std::abs(v1-v0)<1e-8,"Dam break volume conservation");
    double weighted=0,total=0;
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
      for(int x=0;x<nx;++x){
        const double f=w.water_fraction(x,y,z);
        require(f>=0&&f<=1&&std::isfinite(f),"Dam break fraction bounds");
        total+=f;weighted+=f*(x+0.5)*dx;
    }
    require(total>0,"Dam break evacuated unexpectedly");
    const double com=weighted/total;
    require(com>0.2,"Dam break failed to propagate");
    require(w.last_projection_divergence()<0.5,"Divergence above budget");
    return {ms,v0,v1,com,0,0,w.last_projection_divergence(),w.resident_bytes_model()};
}
Result still_pond(int resolution){
    // 1.2m cube water column: H = 0.6m in a closed bottom/side tank;
    // water surface open to air with gauge pressure = 0 Pa.
    const int n=resolution;
    const double dx=1.2/n;
    const int levels=n/2;
    CoupledMacWater3D w(n,n,n,dx);
    for(int z=0;z<levels;++z)for(int y=0;y<n;++y)
      for(int x=0;x<n;++x)w.set_water(x,y,z,1);
    const double initial=w.volume_m3();
    const double ms=time_ms([&]{w.step(0.01,1500);});
    const double simulated=w.pressure_pa(n/2,n/2,0);
    // Hydrostatic p= rho*g*depth. Compare bottommost cell CENTER, not wall.
    const double theoretical=1000.0*9.81*(levels-0.5)*dx;
    std::cerr<<"hydrostatic probe grid="<<n<<" value="<<simulated<<" theory="<<theoretical<<" divergence="<<w.last_projection_divergence()<<"\\n";
    require(std::isfinite(simulated)&&simulated>0,"Hydrostatic pressure missing");
    require(std::abs(w.volume_m3()-initial)<1e-8,"Still pond mass drift");
    require(std::abs(simulated-theoretical)/theoretical<0.13,
            "Hydrostatic reference error exceeds 13%");
    return {ms,initial,w.volume_m3(),0,simulated,theoretical,
            w.last_projection_divergence(),w.resident_bytes_model()};
}
void print(const char* kind,int nx,int ny,int nz,double dx,Result v){
    std::cout<<kind<<","<<nx<<","<<ny<<","<<nz<<","
             <<dx<<","<<v.ms<<","<<v.volume0<<","<<v.volume1<<","
             <<v.center_x<<","<<v.pressure<<","<<v.reference<<","
             <<v.max_div<<","<<v.bytes<<"\n";
}
int main(){
try{
    std::cout<<std::fixed<<std::setprecision(12);
    std::cout<<"scenario,nx,ny,nz,cell_m,elapsed_ms,initial_m3,final_m3,"
               "centroid_x_m,pressure_pa,reference_pa,max_div_1_s,resident_bytes\n";
    for(int n:{8,16,32}){
        auto result=dam_break(n);
        print("dam_break",n,n/2,n*3/4,1.6/n,result);
    }
    for(int n:{8,16,24}){
        auto result=still_pond(n);
        print("hydrostatic",n,n,n,1.2/n,result);
    }
    return 0;
}catch(const std::exception& e){
    std::cerr<<"P2 REFERENCE FAIL: "<<e.what()<<"\n";
    return 1;
}
}
