#include "upc/core.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace upc;
static int checks=0;
void require(bool condition,const char* reason){
    ++checks;if(!condition)throw std::runtime_error(reason);
}
void near(double a,double b,double tol,const char* reason){
    require(std::abs(a-b)<=tol,reason);
}
void fails(const std::function<void()>& action,const char* reason){
    bool threw=false;try{action();}catch(const std::exception&){threw=true;}
    require(threw,reason);
}
int main(){
try{
    UnifiedPhysicsCore core(4,4,4,0.1);
    core.materials().add({1,Phase::Liquid,1000.0,0.001});
    core.materials().add({2,Phase::Solid,2500.0,0.0});
    require(core.materials().size()==2,"Materials count");
    fails([&]{core.materials().add({1,Phase::Liquid,1000,0});},"Duplicate ID accepted");
    fails([&]{core.materials().add({7,Phase::Solid,-1,0});},"Negative density accepted");
    fails([&]{core.materials().at(77);},"Unknown material accepted");
    core.voxels().set(0,0,0,{2,1.0,293.15,0},core.materials());
    near(core.voxels().mass_kg(core.materials()),2.5,1e-12,"Voxel mass wrong");
    fails([&]{core.voxels().set(1,0,0,{77,1.0,293,0},core.materials());},"Invalid voxel material accepted");
    fails([&]{core.voxels().at(500,0,0);},"Voxel out of range accepted");
    fails([&]{core.add_water(5,5,0.1,2);},"Solid used as liquid");
    ShallowWater2D& w=core.add_water(30,12,0.1,1);
    require(core.fluid_count()==1,"Fluid domain count");
    for(std::size_t y=0;y<w.ny();++y)
      for(std::size_t x=0;x<10;++x)w.set_depth(x,y,1.0);
    const double before=w.volume_m3();
    near(before,1.2,1e-12,"Initial water volume incorrect");
    near(w.mass_kg(core.materials()),1200,1e-8,"Water mass incorrect");
    near(w.pressure_pa(0,0,core.materials()),9810,1e-9,"Hydrostatic pressure incorrect");
    fails([&]{w.set_depth(0,0,-1);},"Negative water depth accepted");
    fails([&]{w.step(-0.01);},"Negative timestep accepted");
    for(int i=0;i<40;++i)core.step(0.025);
    near(w.volume_m3(),before,1e-7,"Water volume drift");
    require(w.at(16,5).h>0.001,"Dam-break failed to propagate");
    for(std::size_t y=0;y<w.ny();++y)
      for(std::size_t x=0;x<w.nx();++x)
        require(std::isfinite(w.at(x,y).h)&&w.at(x,y).h>=0,"Invalid fluid cell after simulation");
    ShallowWater2D pond(8,8,0.2,1);
    for(std::size_t y=0;y<8;++y)
      for(std::size_t x=0;x<8;++x)pond.set_depth(x,y,0.75);
    pond.step(0.4);
    near(pond.volume_m3(),1.92,1e-8,"Still pond volume");
    for(std::size_t y=0;y<8;++y)
      for(std::size_t x=0;x<8;++x)
        near(pond.at(x,y).h,0.75,1e-9,"Still water not still");
    ShallowWater2D wall(12,5,0.1,1);
    for(std::size_t y=0;y<5;++y){
        wall.set_solid(6,y);
        for(std::size_t x=0;x<4;++x)wall.set_depth(x,y,1.0);
    }
    const double wall_volume=wall.volume_m3();
    for(int i=0;i<35;++i)wall.step(0.02);
    near(wall.volume_m3(),wall_volume,1e-8,"Solid wall leaks mass");
    for(std::size_t y=0;y<5;++y){
        require(wall.at(6,y).solid&&wall.at(6,y).h==0,"Solid cell flooded");
        for(std::size_t x=7;x<12;++x)
            near(wall.at(x,y).h,0,1e-12,"Water passed solid barrier");
    }
    std::cout<<"PASS: "<<checks<<" physics assertions (materials, voxels, mass, hydrostatic pressure, dam-break, still water, obstacles)\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<checks<<" assertions: "<<e.what()<<"\n";
    return 1;
}
}
