#include "upc/fluid3d_module.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
int main(){
try{
    UnifiedPhysicsRuntime engine(3,3,3,0.1);
    engine.core().materials().add({5,Phase::Liquid,998.2,0.001});
    engine.core().materials().add({8,Phase::Solid,2500.0,0});
    bool rejected=false;
    try{engine.add_water3d(4,4,4,0.1,8);}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("Solid registered as 3D liquid");
    auto& water=engine.add_water3d(8,4,6,0.2,5);
    for(int z=0;z<4;++z)for(int y=0;y<4;++y)for(int x=0;x<2;++x)
        water.set_water(x,y,z,1);
    const double initial=water.mass_kg();
    if(std::abs(initial-0.256*998.2)>1e-7)
        throw std::runtime_error("Shared material density not used");
    auto& second=engine.add_water3d(4,4,4,0.2,5);
    if(engine.fluid3d_count()!=2||&engine.fluid3d(0)!=&water ||
       &engine.fluid3d(1)!=&second)
        throw std::runtime_error("Unstable or incorrect 3D domain registration");
    engine.step(0.02,220);
    if(std::abs(water.mass_kg()-initial)>1e-7)
        throw std::runtime_error("Unified runtime mass drift");
    std::cout<<"PASS: 3D domains registered, shared phase/density respected, stable handles, unified step\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
