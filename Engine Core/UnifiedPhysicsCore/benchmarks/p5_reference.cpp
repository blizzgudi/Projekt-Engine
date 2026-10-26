// P5 reproducible CPU/numerical references. Elastic collision, analytical
// 1D equal-mass restitution, Hooke energy, fracture topology and granular
// stability under gravity. Measurements are single-run CPU observations.
#include "upc/granular3d.hpp"
#include "upc/solid_lattice3d.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace upc;
static void expect(bool yes,const char* msg){if(!yes)throw std::runtime_error(msg);}
static double length(Vec3 a){return norm(a);}
int main(){
try{
 MaterialLibrary lib;
 lib.add({7,Phase::Solid,2500,0});
 std::cout<<std::setprecision(12);
 std::cout<<"scenario,grid_n,particle_count,parameter,expected,observed,"
             "mass_initial_kg,mass_final_kg,max_overlap_m,elapsed_ms\n";
 for(const double e:{0.0,0.5,1.0}){
     GranularWorld3D world(lib,{0,0,0},{3,3,3});
     world.define_material(7,{e,0});world.set_gravity({0,0,0});
     world.add_grain(7,{1.0,1,1},0.1,{1,0,0});
     world.add_grain(7,{1.15,1,1},0.1,{-1,0,0});
     const double mass=world.total_mass_kg();
     const double energy=world.kinetic_energy_j();
     const Vec3 p=world.momentum();
     const auto t0=std::chrono::steady_clock::now();
     world.step(0.001);
     const auto t1=std::chrono::steady_clock::now();
     const double ratio=world.kinetic_energy_j()/energy;
     const double expected=e*e;
     expect(std::abs(ratio-expected)<1e-11,"Restitution energy reference");
     expect(length(world.momentum()-p)<1e-11,"Collision momentum conservation");
     const double ms=std::chrono::duration<double,std::milli>(t1-t0).count();
     std::cout<<"elastic_collision,0,2,"<<e<<","<<expected<<","<<ratio<<","
              <<mass<<","<<world.total_mass_kg()<<","
              <<world.max_pair_overlap_m()<<","<<ms<<"\n";
 }
 {
     SpringSolid3D solid;
     const auto a=solid.add_node({{0,0,0},{0,0,0},1,true});
     const auto b=solid.add_node({{1,0,0},{0,0,0},1,false});
     solid.add_bond(a,b,1000,0.2);solid.set_gravity({0,0,0});
     solid.node(b).position.x+=0.01;
     const double expected=0.5*1000*0.01*0.01;
     const double observed=solid.elastic_energy_j();
     expect(std::abs(observed-expected)<1e-11,"Hooke strain energy reference");
     std::cout<<"hooke_energy,0,2,1000,"<<expected<<","<<observed
              <<",2,2,0,0\n";
 }
 for(const int n:{2,3,4}){
     GranularWorld3D world(lib,{0,0,0},{4,4,4});
     world.define_material(7,{0,0.5});
     for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
         world.add_grain(7,{1+x*0.105,1+y*0.105,0.05+z*0.105},0.05);
     const double m0=world.total_mass_kg();
     const auto t0=std::chrono::steady_clock::now();
     for(int j=0;j<50;++j)world.step(0.002);
     const auto t1=std::chrono::steady_clock::now();
     const double ms=std::chrono::duration<double,std::milli>(t1-t0).count();
     const double overlap=world.max_pair_overlap_m();
     expect(std::abs(world.total_mass_kg()-m0)<1e-11,"Pile mass lost");
     expect(overlap<0.025,"Granular pile contact unstable");
     for(std::size_t i=0;i<world.count();++i)
       expect(world.grain(i).position.z>=world.grain(i).radius_m-1e-9,
              "Granular ground penetration");
     std::cout<<"granular_pile,"<<n<<","<<world.count()<<",0.5,0,"
              <<world.kinetic_energy_j()<<","<<m0<<","
              <<world.total_mass_kg()<<","<<overlap<<","<<ms<<"\n";
 }
 for(const int n:{4,6,8}){
     SpringLattice3D solid(lib,n,n,n,0.1,{7,300,0.3});
     solid.fix_layer(0);
     const double mass=solid.lattice_mass_kg();
     const auto bonds=solid.bond_count();
     const auto t0=std::chrono::steady_clock::now();
     for(int step=0;step<40;++step)solid.step(0.0002);
     const auto t1=std::chrono::steady_clock::now();
     const double ms=std::chrono::duration<double,std::milli>(t1-t0).count();
     const auto fragments=solid.component_sizes();
     expect(fragments.size()==1,"Unloaded lattice fractured");
     expect(solid.intact_bonds()==bonds,"Bond unexpectedly broken");
     expect(std::abs(solid.lattice_mass_kg()-mass)<1e-11,"Solid lattice mass changed");
     std::cout<<"lattice_3d,"<<n<<","<<solid.count()<<",300,"
              <<bonds<<","<<solid.intact_bonds()<<","<<mass<<","
              <<solid.lattice_mass_kg()<<",0,"<<ms<<"\n";
 }
 {
     SpringLattice3D solid(lib,4,4,4,0.1,{7,500,0.1});
     solid.solid().set_gravity({0,0,0});
     const double initial=solid.lattice_mass_kg();
     for(int z=2;z<4;++z)for(int y=0;y<4;++y)for(int x=0;x<4;++x)
         solid.node(x,y,z).position.z+=0.6;
     solid.step(0.0001);
     const auto fragments=solid.component_sizes();
     expect(fragments.size()==2&&fragments[0]==32&&fragments[1]==32,
            "3D fracture split reference");
     expect(solid.fracture_energy_j()>0,"Fracture energy loss absent");
     std::cout<<"fracture_topology,4,64,0.1,2,"<<fragments.size()<<","
              <<initial<<","<<solid.lattice_mass_kg()<<",0,0\n";
 }
 return 0;
}catch(const std::exception& e){
 std::cerr<<"P5 REFERENCE FAIL: "<<e.what()<<"\n";return 1;
}
}
