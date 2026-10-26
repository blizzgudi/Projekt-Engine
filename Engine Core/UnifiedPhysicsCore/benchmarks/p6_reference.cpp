// P6 numerical reference, controlled synthetic X+Y->XY transformation.
// Species/energies are illustrative calibrated INPUTS, not predicted chemistry.
#include "upc/reactive_mixture3d.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace upc;
static void verify(bool condition,const char* message){
 if(!condition)throw std::runtime_error(message);
}
static void species(ReactiveMixtureVolume3D& mix,double k,double activation){
 mix.register_element(17,0.010);mix.register_element(19,0.020);
 mix.register_species({1,{{17,1}},0.010,1100,8e-5,0});
 mix.register_species({2,{{19,1}},0.020,900,8e-5,0});
 mix.register_species({3,{{17,1},{19,1}},0.030,1000,0,-12000});
 mix.register_reaction({{{1,-1},{2,-1},{3,1}},k,activation});
}
int main(){
try{
 MaterialLibrary lib;
 lib.add({1,Phase::Liquid,1000,0.001});
 lib.add({2,Phase::Liquid,1000,0.001});
 lib.add({3,Phase::Solid,1000,0});
 std::cout<<std::setprecision(14);
 std::cout<<"scenario,nx,temp_k,activation_j_mol,expected,observed,"
              "mass_before_kg,mass_after_kg,energy_residual_j,elapsed_ms\n";
 for(double T:{285.0,320.0,380.0}){
   ReactiveMixtureVolume3D mix(lib,2,2,2,0.1);
   species(mix,2.0,14000.0);
   mix.add_moles(0,0,0,1,1, T);mix.add_moles(0,0,0,2,1,T);
   const auto m= mix.total_mass_kg(),e=mix.total_energy_j();
   const auto t0=std::chrono::steady_clock::now();
   const auto result=mix.react(0.3);
   const auto end=std::chrono::steady_clock::now();
   const double expected=-std::expm1(-2.0*std::exp(-14000.0/
       (8.31446261815324*T))*0.3);
   const double actual=mix.total_mol(3);
   verify(std::abs(expected-actual)<1e-13,"Arrhenius analytical reaction extent mismatch");
   verify(std::abs(result.total_energy_residual_j)<1e-9,"Reaction heat lost");
   const auto ms=std::chrono::duration<double,std::milli>(end-t0).count();
   std::cout<<"arrhenius_kinetics,2,"<<T<<",14000,"<<expected<<","
            <<actual<<","<<m<<","<<mix.total_mass_kg()<<","
            <<(mix.total_energy_j()-e)<<","<<ms<<"\n";
 }
 for(const int n:{8,12,16}){
   ReactiveMixtureVolume3D mix(lib,n,n,n,0.1);
   species(mix,0,0); // diffusion only
   for(int z=0;z<n;++z)for(int y=0;y<n;++y){
       mix.add_moles(0,y,z,1,1,300);
       mix.add_moles(n-1,y,z,2,1,340);
   }
   const double m0=mix.total_mass_kg(),e0=mix.total_energy_j();
   const auto t0=std::chrono::steady_clock::now();
   double transferred=0;
   for(int step=0;step<40;++step){
       const auto r=mix.diffuse(0.05);
       transferred+=r.amount_mol;
   }
   const auto end=std::chrono::steady_clock::now();
   verify(transferred>1e-7,"No diffusion across 3D volume");
   verify(std::abs(mix.total_mass_kg()-m0)<1e-9,"3D material mass lost");
   verify(std::abs(mix.total_energy_j()-e0)<1e-7,"3D enthalpy diffusion loss");
   verify(std::abs(mix.total_element_mol(17)-n*n)<1e-7,"3D elemental X loss");
   verify(std::abs(mix.total_element_mol(19)-n*n)<1e-7,"3D elemental Y loss");
   const auto ms=std::chrono::duration<double,std::milli>(end-t0).count();
   std::cout<<"fick_3d,"<<n<<",300,0,"<<n*n<<","
            <<mix.total_element_mol(17)<<","<<m0<<","
            <<mix.total_mass_kg()<<","<<(mix.total_energy_j()-e0)<<","
            <<ms<<"\n";
 }
 return 0;
}catch(const std::exception& e){
 std::cerr<<"P6 REFERENCE FAILED: "<<e.what()<<"\n";return 1;
}
}
