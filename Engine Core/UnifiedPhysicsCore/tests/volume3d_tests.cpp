#include "upc/volume3d.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
int checks=0;
void require(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
int main(){
try{
    VolumeTransport3D sim(16,12,16,0.1);
    // Occupied water region above the floor, isolated from solid wall.
    for(std::size_t z=6;z<10;++z)
      for(std::size_t y=3;y<9;++y)
        for(std::size_t x=3;x<9;++x)sim.set_fill(x,y,z,1);
    for(std::size_t z=0;z<16;++z)
      for(std::size_t y=0;y<12;++y)sim.set_solid(12,y,z);
    const double volume0=sim.volume_m3();
    require(std::abs(volume0-0.144)<1e-9,"Initial water volume");
    sim.set_uniform_velocity(0.4,0,-0.6);
    for(int i=0;i<30;++i)sim.advect(0.01);
    const double volume1=sim.volume_m3();
    require(std::abs(volume1-volume0)<1e-9,"3D transport mass drift");
    require(sim.fill(8,5,3)>0,"3D vertical/horizontal movement absent");
    for(std::size_t z=0;z<16;++z)
      for(std::size_t y=0;y<12;++y)
        for(std::size_t x=0;x<16;++x){
          const double f=sim.fill(x,y,z);
          require(std::isfinite(f)&&f>=0&&f<=1,"Invalid volume fraction");
          if(x==12)require(sim.is_solid(x,y,z)&&f==0,"Obstacle was flooded");
        }
    bool bad=false;try{sim.set_fill(0,0,0,1.2);}catch(const std::exception&){bad=true;}
    require(bad,"Invalid fraction accepted");
    std::cout<<"PASS: "<<checks<<" 3D conservative transport assertions, volume "<<volume0
             <<" -> "<<volume1<<" m3\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
