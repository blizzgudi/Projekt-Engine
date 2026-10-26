#include "upc/core.hpp"
#include <cmath>
#include <iostream>
using namespace upc;
int main(){
for(double dx:{0.1,0.05,0.025,0.0125}){
  const auto nx=(std::size_t)std::llround(12./dx);
  ShallowWater2D sim(nx,1,dx,1);
  for(std::size_t i=0;i<nx/2;++i)sim.set_depth(i,0,1.0);
  sim.step(0.25);
  const double actual=6+2*std::sqrt(9.81)*0.25;
  std::cout<<"dx="<<dx<<" theoretical_front="<<actual;
  for(double thr:{1e-8,1e-6,1e-5,1e-4,5e-4,0.001,0.002,0.004}){
    double position=0;
    for(std::size_t i=0;i<nx;++i)if(sim.at(i,0).h>thr)position=(i+0.5)*dx;
    std::cout<<" thr="<<thr<<" dx_error="<<position-actual;
  }
  std::cout<<"\n";
}
}
