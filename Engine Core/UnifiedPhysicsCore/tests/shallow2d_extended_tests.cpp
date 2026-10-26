#include "upc/core.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int assertions=0;
void require(bool yes,const char* message){
    ++assertions;if(!yes)throw std::runtime_error(message);
}
void close(double actual,double expected,double eps,const char* message){
    require(std::isfinite(actual)&&std::abs(actual-expected)<=eps,message);
}
void rejected(const std::function<void()>& op,const char* msg){
    bool yes=false;try{op();}catch(const std::exception&){yes=true;}
    require(yes,msg);
}
int main(){
try{
    MaterialLibrary lib;
    lib.add({1,Phase::Liquid,997.0,0.001});
    lib.add({2,Phase::Liquid,2000.0,0.01});
    // Independent equation: p = rho*g*h at the bottom, not an active 3D pressure field.
    ShallowWater2D column(9,4,0.125,1);
    for(int y=0;y<4;++y)for(int x=0;x<9;++x)column.set_depth(x,y,0.8);
    const double initial=column.volume_m3();
    close(column.pressure_pa(2,3,lib),997*9.81*0.8,1e-9,"Hydrostatic bottom p");
    for(int i=0;i<20;++i)column.step(0.05);
    close(column.volume_m3(),initial,1e-12,"Still pond conservation");
    for(int y=0;y<4;++y)for(int x=0;x<9;++x){
        auto c=column.at(x,y);
        close(c.h,0.8,1e-12,"Lake at rest h");
        close(c.hx,0,1e-12,"Lake at rest hx");
        close(c.hy,0,1e-12,"Lake at rest hy");
    }
    ShallowWater2D rotated(22,16,0.1,1),reference(16,22,0.1,1);
    // 90-degree axis transpose, symmetric two-dimensional equations.
    for(int y=0;y<16;++y)for(int x=0;x<22;++x){
        const double h=(x>=3&&x<9&&y>=5&&y<11)?1.1:0.03;
        rotated.set_depth(x,y,h);
        reference.set_depth(y,x,h);
    }
    const double shared=rotated.volume_m3();
    for(int i=0;i<18;++i){
        rotated.step(0.01);
        reference.step(0.01);
    }
    close(rotated.volume_m3(),shared,1e-10,"2D axis x mass conservation");
    close(reference.volume_m3(),shared,1e-10,"2D axis y mass conservation");
    for(int y=0;y<16;++y)for(int x=0;x<22;++x){
        const auto a=rotated.at(x,y),b=reference.at(y,x);
        close(a.h,b.h,1e-10,"2D depth transposition");
        close(a.hx,b.hy,1e-10,"2D horizontal momentum");
        close(a.hy,b.hx,1e-10,"2D vertical momentum");
        require(a.h>=0&&std::isfinite(a.h),"2D cell validity");
    }
    // Incompressible mass inside impermeable solid wall with a gate.
    ShallowWater2D channel(30,15,0.1,1);
    for(int y=0;y<15;++y)if(y!=7)channel.set_solid(14,y);
    for(int y=0;y<15;++y)for(int x=0;x<10;++x)channel.set_depth(x,y,0.9);
    const double channel_start=channel.volume_m3();
    for(int i=0;i<90;++i)channel.step(0.01);
    close(channel.volume_m3(),channel_start,1e-8,"Channel gate conservation");
    for(int y=0;y<15;++y){
        if(y!=7){
            require(channel.at(14,y).solid,"Gate wall disappeared");
            close(channel.at(14,y).h,0,1e-13,"Gate wall penetration");
        }
    }
    double through=0;
    for(int y=0;y<15;++y)for(int x=15;x<30;++x)
        through+=channel.at(x,y).h;
    require(through>0.01,"Water did not pass through the gate opening");
    rejected([&]{channel.set_solid(0,0,true);},"Overlapping obstacle accepted");
    rejected([&]{channel.set_depth(14,6,1);},"Water in solid accepted");
    rejected([&]{channel.step(0);},"Zero time accepted");
    rejected([&]{channel.step(11);},"Large time accepted");
    rejected([&]{channel.set_depth(1,1,-0.001);},"Negative depth accepted");
    rejected([&]{column.pressure_pa(999,0,lib);},"Invalid position accepted");

    // Conserved depth under repeated 2D smooth, non-zero initial velocities.
    ShallowWater2D smooth(32,19,0.08,1);
    for(int y=0;y<19;++y)for(int x=0;x<32;++x){
        const double h=0.35+0.05*std::sin(x*0.4)*std::cos(y*0.3);
        smooth.set_depth(x,y,h,0.03*std::sin(y*0.2),0.02*std::cos(x*0.4));
    }
    const double smooth_m=smooth.volume_m3();
    smooth.step(1.5);
    close(smooth.volume_m3(),smooth_m,1e-8,"High-step CFL volume conservation");
    for(int y=0;y<19;++y)for(int x=0;x<32;++x){
        auto c=smooth.at(x,y);
        require(c.h>=0&&std::isfinite(c.h)&&std::isfinite(c.hx)&&
                std::isfinite(c.hy),"Smooth 2D simulation finite");
    }
    std::cout<<"PASS: "<<assertions<<" assertions in 2D wet/dry gate, axis symmetry, pressure, CFL, positivity, conservation\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<assertions<<" assertions: "<<e.what()<<"\n";return 1;
}
}
