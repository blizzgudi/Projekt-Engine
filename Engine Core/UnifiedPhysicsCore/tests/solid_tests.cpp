#include "upc/solid.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
int main(){
 try{
    SpringSolid3D solid;
    auto a=solid.add_node({{0,0,0},{0,0,0},1,true});
    auto b=solid.add_node({{1,0,0},{0,0,0},1,false});
    solid.add_bond(a,b,100.0,0.2);
    solid.set_gravity({0,0,0});
    if(solid.intact_bonds()!=1)throw std::runtime_error("Bond missing");
    for(int i=0;i<100;++i)solid.step(0.001);
    if(std::abs(solid.node(b).position.x-1)>1e-9)
        throw std::runtime_error("Undisturbed bond moved");
    solid.node(b).position.x=1.05;
    if(std::abs(solid.elastic_energy_j()-0.125)>1e-9)
        throw std::runtime_error("Spring strain energy incorrect");
    solid.step(0.001);
    if(solid.node(b).velocity.x>=0)
        throw std::runtime_error("Elastic restoring force absent");
    solid.node(b).position.x=1.50;
    solid.step(0.001);
    if(solid.intact_bonds()!=0)
        throw std::runtime_error("Fracture threshold not reached");
    if(solid.elastic_energy_j()!=0)
        throw std::runtime_error("Broken spring still stores elastic energy");
    std::cout<<"PASS: spring forces, elastic potential energy, fracture threshold\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
