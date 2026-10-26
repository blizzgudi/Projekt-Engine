#include "upc/coupled_water3d.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int checks=0;
void check(bool pass,const char* message){++checks;if(!pass)throw std::runtime_error(message);}
void near(double actual,double expected,double atol,const char* msg){
    check(std::isfinite(actual)&&std::abs(actual-expected)<atol,msg);
}
int main(){
try{
    // Case A: 3D dam-break in a closed tank, free x/y/z motion.
    CoupledMacWater3D dam(16,8,12,0.1);
    for(std::size_t z=0;z<8;++z)
        for(std::size_t y=0;y<8;++y)
            for(std::size_t x=0;x<5;++x)dam.set_water(x,y,z,1);
    const double initial=dam.volume_m3();
    near(initial,0.32,1e-10,"initial water volume");
    double initial_right=0,initial_bottom=0;
    for(std::size_t z=0;z<12;++z)
        for(std::size_t y=0;y<8;++y)for(std::size_t x=8;x<16;++x)
            initial_right+=dam.water_fraction(x,y,z);
    for(std::size_t z=0;z<3;++z)
        for(std::size_t y=0;y<8;++y)for(std::size_t x=5;x<16;++x)
            initial_bottom+=dam.water_fraction(x,y,z);
    check(initial_right==0&&initial_bottom==0,"unexpected initial water");
    for(int step=0;step<35;++step){
        dam.step(0.01,200);
        near(dam.volume_m3(),initial,1e-8,"3D dam-break mass drift");
    }
    double moved_right=0,moved_bottom=0;
    for(std::size_t z=0;z<12;++z)
      for(std::size_t y=0;y<8;++y)
        for(std::size_t x=0;x<16;++x){
            const double f=dam.water_fraction(x,y,z);
            check(std::isfinite(f)&&f>=0&&f<=1,"nonfinite or invalid 3D cell");
            if(x>=8)moved_right+=f;
            if(z<3&&x>=5)moved_bottom+=f;
        }
    check(moved_right>0.01,"3D dam-break did not spread across x");
    check(moved_bottom>0.01,"3D dam-break did not descend");
    check(dam.last_projection_divergence()<5,"MAC projection did not sufficiently lower divergence");
    // Case B: explicit 3D vertical solid slab, no water breakthrough.
    CoupledMacWater3D barrier(12,6,8,0.1);
    for(std::size_t z=0;z<8;++z)
      for(std::size_t y=0;y<6;++y)barrier.set_solid(6,y,z);
    for(std::size_t z=0;z<6;++z)
      for(std::size_t y=0;y<6;++y)
        for(std::size_t x=0;x<4;++x)barrier.set_water(x,y,z,1);
    const double closed=barrier.volume_m3();
    for(int n=0;n<30;++n)barrier.step(0.01,200);
    near(barrier.volume_m3(),closed,1e-8,"barrier mass loss");
    for(std::size_t z=0;z<8;++z)
      for(std::size_t y=0;y<6;++y){
        check(barrier.is_solid(6,y,z),"solid slab erased");
        near(barrier.water_fraction(6,y,z),0,1e-14,"wall flooded");
        for(std::size_t x=7;x<12;++x)
          near(barrier.water_fraction(x,y,z),0,1e-14,"water crossed full-height slab");
      }
    // Case C: flat pond should stay near rest; approximate hydrostatic pressure.
    CoupledMacWater3D pond(6,6,10,0.1);
    for(std::size_t z=0;z<6;++z)
      for(std::size_t y=0;y<6;++y)
        for(std::size_t x=0;x<6;++x)pond.set_water(x,y,z,1);
    const double original=pond.volume_m3();
    pond.step(0.01,600);
    near(pond.volume_m3(),original,1e-8,"pond mass");
    check(pond.pressure_pa(3,3,0)>pond.pressure_pa(3,3,4),
          "hydrostatic pressure slope missing");
    // Initial input hardening.
    bool denied=false;
    try{pond.set_water(2,2,2,1.2);}catch(const std::exception&){denied=true;}
    check(denied,"out of bounds water fraction accepted");
    denied=false;
    try{pond.set_solid(2,2,2,true);}catch(const std::exception&){denied=true;}
    check(denied,"overlapping solid and water accepted");
    check(pond.resident_bytes_model()>0,"missing resident memory estimate");
    std::cout<<"PASS: "<<checks<<" coupled-3D checks, initial="
             <<initial<<" final="<<dam.volume_m3()<<" right="
             <<moved_right<<" bottom="<<moved_bottom
             <<" max_div="<<dam.last_projection_divergence()
             <<" resident_bytes="<<dam.resident_bytes_model()<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}
}
