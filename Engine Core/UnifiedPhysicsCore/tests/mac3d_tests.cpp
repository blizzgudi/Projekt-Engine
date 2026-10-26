#include "upc/mac3d.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
void require(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
int main(){
try{
    MacProjection3D water(6,6,6,0.1);
    for(std::size_t z=0;z<4;++z)
      for(std::size_t y=0;y<6;++y)
        for(std::size_t x=0;x<6;++x)water.set_cell(x,y,z,true);
    water.apply_gravity(0.01);
    const double divergence_before=water.max_divergence();
    require(divergence_before>0.1,"Gravity did not create divergence");
    water.project(0.01,1500);
    const double divergence_after=water.max_divergence();
    require(divergence_after<divergence_before*0.01,"Projection insufficient");
    require(std::isfinite(water.pressure_pa(3,3,1)),"Pressure not finite");
    // The hydrostatic pressure gradient should make the lowest cell higher pressure.
    require(water.pressure_pa(3,3,0)>water.pressure_pa(3,3,3),
            "Hydrostatic pressure gradient incorrect");
    MacProjection3D full(4,4,4,0.1);
    for(std::size_t z=0;z<4;++z)
      for(std::size_t y=0;y<4;++y)
        for(std::size_t x=0;x<4;++x)full.set_cell(x,y,z,true);
    full.set_face_velocity_x(2,2,2,0.5);
    const double d0=full.max_divergence();
    full.project(0.01,3000);
    const double d1=full.max_divergence();
    require(d1<d0*0.01,"Closed volume divergence reduction");
    require(std::abs(full.face_velocity_x(0,2,2))<1e-14,"Boundary not closed");
    bool rejected=false;
    try{full.set_cell(1,1,1,true,true);}catch(const std::exception&){rejected=true;}
    require(rejected,"Solid-fluid overlap accepted");
    std::cout<<"PASS: 3D MAC pressure projection, divergence before "
             <<divergence_before<<", after "<<divergence_after
             <<"; closed before "<<d0<<", after "<<d1<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
