#include "upc/core.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace upc;
int main(int argc,char** argv){
try {
    UnifiedPhysicsCore core(1,1,1,0.1);
    core.materials().add({1,Phase::Liquid,1000.0,0.001});
    auto& w=core.add_water(40,15,0.1,1);
    for(std::size_t y=0;y<w.ny();++y)
      for(std::size_t x=0;x<13;++x)w.set_depth(x,y,0.8);
    std::ofstream csv;
    if(argc>1){csv.open(argv[1]);if(!csv)throw std::runtime_error("Cannot open CSV");}
    std::ostream& out=argc>1?csv:std::cout;
    out<<"time_seconds,volume_m3,mass_kg,left_depth_m,right_depth_m\n";
    for(int step=0;step<=40;++step){
        out<<std::fixed<<std::setprecision(9)<<step*0.025<<","
           <<w.volume_m3()<<","<<w.mass_kg(core.materials())<<","
           <<w.at(4,7).h<<","<<w.at(26,7).h<<"\n";
        if(step<40)core.step(0.025);
    }
    if(argc>1)std::cout<<"Dam-break CSV written: "<<argv[1]<<"\n";
    return 0;
} catch(const std::exception& e){
    std::cerr<<"DAM BREAK FAILED: "<<e.what()<<"\n";return 1;
}
}
