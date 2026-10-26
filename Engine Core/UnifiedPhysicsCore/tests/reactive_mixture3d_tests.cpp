#include "upc/reactive_mixture3d.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int assertions=0;
void require(bool b,const char* why){++assertions;if(!b)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why){
    require(std::isfinite(a)&&std::abs(a-b)<=tolerance,why);
}
void rejects(const std::function<void()>& fn,const char* why){
    bool caught=false;try{fn();}catch(const std::exception&){caught=true;}
    require(caught,why);
}
static void define(ReactiveMixtureVolume3D& v){
    v.register_element(100,0.010);  // synthetic element X
    v.register_element(200,0.020);  // synthetic element Y
    v.register_species({1,{{100,1}},0.010,1000,1e-4,0});
    v.register_species({2,{{200,1}},0.020,1000,1e-4,0});
    v.register_species({3,{{100,1},{200,1}},0.030,1000,0,-12000});
    v.register_reaction({{{1,-1},{2,-1},{3,+1}},2.0,0});
}
int main(){
try{
    MaterialLibrary lib;
    lib.add({1,Phase::Liquid,1000,0.001});
    lib.add({2,Phase::Liquid,1000,0.001});
    lib.add({3,Phase::Solid,1000,0});
    {
        ReactiveMixtureVolume3D mixture(lib,2,2,2,0.1);
        define(mixture);
        mixture.add_moles(0,0,0,1,1.0,300);
        mixture.add_moles(0,0,0,2,1.0,300);
        near(mixture.total_mass_kg(),0.03,1e-13,"Initial chemistry mass");
        near(mixture.total_element_mol(100),1,1e-13,"Element X amount");
        near(mixture.total_element_mol(200),1,1e-13,"Element Y amount");
        near(mixture.temperature_k(0,0,0),300,1e-12,"Initial mixture temperature");
        const double m0=mixture.total_mass_kg();
        const double e0=mixture.total_energy_j();
        const auto r=mixture.react(0.4);
        const double expected=1-std::exp(-2*0.4);
        near(r.extent_mol,expected,1e-12,"Pseudo-first-order analytical kinetics");
        near(mixture.mol(0,0,0,3),expected,1e-12,"Produced synthetic compound mol");
        near(mixture.mol(0,0,0,1),1-expected,1e-12,"Remaining reactant X");
        near(mixture.mol(0,0,0,2),1-expected,1e-12,"Remaining reactant Y");
        near(r.thermalized_j,12000*expected,1e-8,"Exothermic chemical heat");
        near(mixture.total_mass_kg(),m0,1e-12,"React chemistry conserved mass");
        near(mixture.total_energy_j(),e0,1e-8,"Chemical plus sensible energy conserved");
        near(mixture.total_element_mol(100),1,1e-12,"X atomic balance after reaction");
        near(mixture.total_element_mol(200),1,1e-12,"Y atomic balance after reaction");
        near(mixture.temperature_k(0,0,0),300+12000*expected/30,1e-7,
             "Reaction raised temperature by released heat");
        // Product mass is now dominant, resulting material proxy is C.
        VoxelGrid render_grid(2,2,2,0.1);
        mixture.write_material_proxy(render_grid,0,0,0,lib);
        require(render_grid.at(0,0,0).material_id==3,"Reaction product material ID not mapped");
        near(render_grid.at(0,0,0).fill,0.03/1000/0.001,1e-12,
             "Mixture physical fraction incorrect");
        near(render_grid.at(0,0,0).temperature_k,
             mixture.temperature_k(0,0,0),1e-9,"Temperature not written into core voxel");
        // Adding external heat is explicit; chemical reservoir unchanged.
        const double h0=mixture.total_energy_j();
        mixture.add_heat_j(0,0,0,25);
        near(mixture.total_energy_j(),h0+25,1e-9,"Explicit external heat not counted");
        rejects([&]{mixture.add_heat_j(0,0,0,-1e20);},"Sub-zero reaction cell accepted");
        rejects([&]{mixture.add_moles(0,0,0,3,100000,300);},"Overfilled voxel accepted");
        rejects([&]{mixture.add_moles(0,0,0,1,0,300);},"Zero constituent added");
    }
    {
        ReactiveMixtureVolume3D world(lib,12,6,4,0.1);
        define(world);
        for(int z=0;z<4;++z)for(int y=0;y<6;++y){
            world.add_moles(0,y,z,1,1.0,315);
            world.add_moles(11,y,z,2,1.0,285);
        }
        const double m0=world.total_mass_kg(),e0=world.total_energy_j();
        const double x0=world.total_element_mol(100),y0=world.total_element_mol(200);
        double moved=0;
        for(int i=0;i<200;++i){
            auto diff=world.diffuse(0.1);
            moved+=diff.amount_mol;
            near(diff.mass_residual_kg,0,1e-10,"3D Fick diffusion mass residual");
            near(diff.total_energy_residual_j,0,1e-8,"3D Fick energy residual");
            near(world.total_mass_kg(),m0,1e-10,"Material 3D diffusion mass drift");
            near(world.total_energy_j(),e0,1e-7,"Material 3D heat drift");
        }
        require(moved>0,"No material diffusion occurred");
        require(world.mol(1,0,0,1)>0,"Diffusion did not cross X face");
        require(world.mol(10,0,0,2)>0,"Diffusion did not cross reverse X face");
        near(world.total_element_mol(100),x0,1e-9,"Diffusion lost element X");
        near(world.total_element_mol(200),y0,1e-9,"Diffusion lost element Y");
        const auto heat=world.total_energy_j();
        const auto re=world.react(0.5);
        require(re.extent_mol>=0,"Invalid coupled diffusion-reaction extent");
        near(world.total_energy_j(),heat,1e-7,"Reaction energy lost after diffusion");
        near(world.total_mass_kg(),m0,1e-9,"Reaction mass lost after diffusion");
    }
    {
        ReactiveMixtureVolume3D world(lib,2,2,2,0.1);
        world.register_element(100,0.010);
        world.register_element(200,0.020);
        rejects([&]{world.register_element(100,0.010);},"Duplicate element accepted");
        rejects([&]{world.register_species({1,{{100,1}},0.015,1000,0,0});},
                "Inconsistent formula molar mass accepted");
        world.register_species({1,{{100,1}},0.010,1000,0,0});
        world.register_species({2,{{200,1}},0.020,1000,0,0});
        world.register_species({3,{{100,1},{200,1}},0.030,1000,0,-12000});
        rejects([&]{world.register_reaction({{{1,-1},{2,-1},{3,+2}},1,0});},
                "Unbalanced reaction accepted");
        rejects([&]{world.register_reaction({{{1,-1},{2,+1}},1,0});},
                "Atom-changing pseudo reaction accepted");
        rejects([&]{world.register_reaction({{{1,-1},{2,-1},{3,1}},-1,0});},
                "Negative kinetic rate accepted");
        world.register_reaction({{{1,-1},{2,-1},{3,1}},1,12000});
        world.add_moles(1,1,1,1,0.5,300);
        rejects([&]{world.add_moles(1,1,1,2,0.5,400);},
                "Unmodeled different cell temperatures combined");
        rejects([&]{world.diffuse(-1);},"Negative Fick dt accepted");
        rejects([&]{world.react(-1);},"Negative kinetics dt accepted");
        const auto rr=world.react(0.2);
        near(rr.extent_mol,0,1e-12,"Reaction without second reagent occurred");
    }
    std::cout<<"PASS: P6 synthetic 3D mixtures, balanced element/mass conservation,"
                " Fick diffusion, Arrhenius kinetics, chemical-to-sensible energy,"
                " material VoxelGrid writeback; checks="<<assertions<<"\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<assertions<<" assertions: "<<e.what()<<"\n";return 1;
}
}
