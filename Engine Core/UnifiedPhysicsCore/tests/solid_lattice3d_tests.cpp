#include "upc/solid_lattice3d.hpp"
#include "upc/solid_contact3d.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <numeric>
#include <stdexcept>
using namespace upc;
static int checks=0;
void need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void near(double actual,double wanted,double tolerance,const char* why){
    need(std::isfinite(actual)&&std::abs(actual-wanted)<=tolerance,why);
}
void reject(std::function<void()> f,const char* why){
    bool b=false;try{f();}catch(const std::exception&){b=true;}
    need(b,why);
}
int main(){
try{
    MaterialLibrary lib;
    lib.add({1,Phase::Solid,2200,0});
    lib.add({2,Phase::Liquid,998,0.001});
    {
        SpringLattice3D s(lib,3,3,3,0.1,{1,200,0.20});
        need(s.count()==27,"Not a volumetric lattice");
        need(s.bond_count()>27,"Not volumetrically connected");
        near(s.lattice_mass_kg(),27*2.2,1e-10,"Lumped volumetric mass formula");
        need(s.component_sizes()==std::vector<std::size_t>{27},
             "Initial lattice disconnected");
        s.solid().set_gravity({0,0,0});
        const auto elastic0=s.solid().elastic_energy_j();
        near(elastic0,0,1e-10,"Initial strain energy");
        s.node(1,1,1).position.x+=0.003;
        need(s.solid().elastic_energy_j()>0,"Lattice strain lacks elastic energy");
        auto previous=s.node(1,1,1).position.x;
        auto substeps=s.step(0.003);
        need(substeps>=1,"No lattice substeps");
        need(s.node(1,1,1).position.x<previous,"Lattice restoring impulse absent");
        need(s.intact_bonds()==s.bond_count(),"Small elastic strain fractured");
        need(s.fracture_energy_j()==0,"Unexpected crack dissipation");
    }
    {
        SpringLattice3D s(lib,4,4,4,0.1,{1,500,0.15});
        need(s.count()==64,"Wrong lattice volume");
        s.solid().set_gravity({0,0,0});
        const double m0=s.lattice_mass_kg();
        s.fix_layer(0);
        for(int z=2;z<4;++z)for(int y=0;y<4;++y)
            for(int x=0;x<4;++x)s.node(x,y,z).position.z+=0.7;
        s.step(0.00005);
        const auto fragments=s.component_sizes();
        need(fragments.size()==2,"Fracture did not separate two 3D pieces");
        need(fragments[0]==32&&fragments[1]==32,"Volumetric fracture fragments incorrect");
        need(s.fracture_energy_j()>0,"Fracture released no potential energy");
        need(s.intact_bonds()<s.bond_count(),"Crack did not remove bonds");
        near(s.lattice_mass_kg(),m0,1e-8,"Breaking manufactured or removed mass");
        VoxelGrid material_voxels(4,4,4,0.1);
        s.export_damage_to_voxels(material_voxels,lib);
        near(material_voxels.mass_kg(lib),m0,1e-8,
             "Fragment damage/material export manufactured mass");
        double damage_sum=0;
        for(int z=0;z<4;++z)for(int y=0;y<4;++y)
          for(int x=0;x<4;++x){
            const auto voxel=material_voxels.at(x,y,z);
            need(voxel.material_id==1,"Lattice material ID not preserved");
            damage_sum+=voxel.damage;
          }
        need(damage_sum>0,"Crack damage did not reach shared VoxelGrid");
        material_voxels.set(0,0,0,{2,1,293.15,0},lib);
        reject([&]{s.export_damage_to_voxels(material_voxels,lib);},
               "Damage export overwrote another material");
        for(int k=0;k<250;++k)s.step(0.0001);
        need(s.component_sizes().size()==2,"Broken bonds spontaneously rejoined");
        near(s.lattice_mass_kg(),m0,1e-8,"Fragment drift changes mass");
    }
    {
        SpringLattice3D s(lib,3,3,3,0.10,{1,200,0.25},{1,1,0.2});
        s.fix_layer(0);
        SolidPlaneContact3D floor({});
        for(int n=0;n<600;++n){
            const auto sub=s.step(0.0002);
            need(sub>=1,"No lattice step");
            floor.step(s.solid(),0.0002);
            for(std::size_t i=0;i<s.count();++i){
                const auto& a=s.solid().node(i);
                if(a.fixed)continue;
                need(a.position.z>=-1e-10,"Solid penetrated static ground");
                need(std::isfinite(a.position.x)&&std::isfinite(a.position.y)&&
                     std::isfinite(a.position.z),"3D lattice became nonfinite");
            }
        }
        need(s.component_sizes().size()==1,"Unloaded solid fractured on floor");
    }
    {
        reject([&]{SpringLattice3D bad(lib,2,2,2,-1,{1,100,0.1});},
               "Bad lattice spacing accepted");
        reject([&]{SpringLattice3D bad(lib,2,2,2,0.1,{2,100,0.1});},
               "Liquid material accepted as solid");
        reject([&]{SpringLattice3D bad(lib,2,2,2,0.1,{1,0,0.1});},
               "Zero stiffness accepted");
        reject([&]{SpringLattice3D bad(lib,2,2,2,0.1,{1,10,0});},
               "Zero fracture strain accepted");
        SpringLattice3D s(lib,2,2,2,0.1,{1,100,0.2});
        reject([&]{s.step(-1);},"Invalid lattice dt accepted");
        reject([&]{s.node(4,0,0);},"Out of range node accepted");
    }
    std::cout<<"PASS: P5 volumetric spring lattice, restoring stress proxy,"
                " controlled fracture, 2 fragments, ground contact; checks="
             <<checks<<"\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;
}
}
