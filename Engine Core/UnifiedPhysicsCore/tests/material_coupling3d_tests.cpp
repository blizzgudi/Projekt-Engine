#include "upc/material_coupling3d.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace upc;
static int checks=0;
void require(bool a,const char* why){++checks;if(!a)throw std::runtime_error(why);}
void near(double v,double target,double eps,const char* why){
    require(std::isfinite(v)&&std::abs(v-target)<=eps,why);
}
void rejects(std::function<void()> fn,const char* why){
    bool bad=false;try{fn();}catch(const std::exception&){bad=true;}
    require(bad,why);
}
void materials(UnifiedPhysicsRuntime& rt){
    rt.core().materials().add({1,Phase::Liquid,1000,0.001});
    rt.core().materials().add({2,Phase::Solid,1700,0});
    rt.core().materials().add({3,Phase::Solid,2600,0});
}
void infiltration_test(){
    UnifiedPhysicsRuntime rt(8,6,8,0.1);materials(rt);
    auto& water=rt.add_water3d(8,6,8,0.1,1);
    MaterialVolumeCoupler3D physics(rt,water);
    physics.register_porous_material(2,0.35,0.02);
    rejects([&]{physics.register_porous_material(2,0.4,0.01);},
            "Duplicate porous material definition accepted");
    rejects([&]{physics.register_porous_material(1,0.3,0.02);},
            "Liquid material accepted as porous solid");
    for(int y=0;y<6;++y)for(int x=0;x<8;++x){
        physics.set_soil_from_material(x,y,0,2);
        physics.set_soil_from_material(x,y,1,2);
        water.set_water(x,y,2,1.0);
    }
    const double initial=physics.total_water_volume_m3();
    near(initial,0.048,1e-12,"Initial porous volume");
    const auto p0=physics.gravity_potential_j();
    const auto first=physics.exchange(0.025);
    require(first.free_to_soil_m3>0,"No free-water infiltration");
    require(first.soil_to_soil_m3>0,"No downward percolation");
    require(first.potential_released_j>0,"No gravity energy release");
    near(first.mass_residual_kg,0,1e-8,"Mass imbalance during exchange");
    near(first.gravitational_energy_residual_j,0,1e-8,"Transfer energy ledger mismatch");
    near(physics.gravity_potential_j()+physics.dissipated_energy_j(),p0,1e-8,
         "Exchange mechanical energy is not accounted");
    for(int i=0;i<45;++i){
        const auto sample=physics.step(0.015,180);
        near(sample.mass_residual_kg,0,1e-8,"Coupled simulation soil mass drift");
        near(sample.gravitational_energy_residual_j,0,1e-8,
             "Coupled simulation exchange energy imbalance");
        near(physics.total_water_volume_m3(),initial,1e-8,
             "Coupled 3D free water and pores lost volume");
    }
    require(physics.total_infiltrated_volume_m3()>1e-4,
            "Insignificant infiltration after 3D fluid steps");
    require(physics.soil(0,0,0).pore_water_m3>0,"Water did not reach lower soil layer");
    require(physics.soil(0,0,1).pore_water_m3>0,"Upper soil layer did not wet");
    for(int y=0;y<6;++y)for(int x=0;x<8;++x)for(int z=0;z<2;++z){
        auto cell=physics.soil(x,y,z);
        require(cell.pore_water_m3>=-1e-12&&
                cell.pore_water_m3<=0.35*0.001+1e-12,"Pore capacity broken");
        near(water.water_fraction(x,y,z),0,1e-12,"Free water penetrated soil matrix");
        require(rt.core().voxels().at(x,y,z).material_id==2,"Material id desynchronised");
    }
    rejects([&]{physics.set_soil(0,0,0,3,0.35,0.01);},"Overlapping soil accepted");
    rejects([&]{physics.set_soil(2,2,3,2,1.0,0.01);},"Invalid porosity accepted");
    rejects([&]{physics.step(-1);},"Invalid dt accepted");
}
void saturated_and_rock_test(){
    UnifiedPhysicsRuntime rt(5,5,5,0.2);materials(rt);
    auto& water=rt.add_water3d(5,5,5,0.2,1);
    MaterialVolumeCoupler3D world(rt,water);
    world.set_soil(0,0,1,2,0.25,0.1,1);
    world.set_fixed_rock(1,1,1,3);
    water.set_water(0,0,2,0.75);
    water.set_water(1,1,2,0.75);
    const double initial=world.total_water_volume_m3();
    const auto result=world.exchange(0.2);
    near(result.free_to_soil_m3,0,1e-15,"Saturated pore accepted water");
    near(result.soil_to_soil_m3,0,1e-15,"Saturated pore should not drain");
    near(world.total_water_volume_m3(),initial,1e-15,"Rock trial volume");
    near(water.water_fraction(1,1,2),0.75,1e-15,"Rock absorbed water");
    rejects([&]{world.set_soil(2,2,1,1,0.3,0.1);},"Liquid as solid soil accepted");
    rejects([&]{world.wall_force(0,0,1);},"Soil is not rigid body");
    rejects([&]{world.set_fixed_rock(2,2,2,1);},"Liquid registered as rock");
}
void pressure_and_rigid_test(){
    UnifiedPhysicsRuntime rt(8,8,10,0.1);materials(rt);
    auto& water=rt.add_water3d(8,8,10,0.1,1);
    MaterialVolumeCoupler3D world(rt,water);
    world.set_rigid_body(3,3,3,3,0.25); // lighter than displaced water
    for(int z=0;z<8;++z)for(int y=0;y<8;++y)for(int x=0;x<8;++x)
      if(!(x==3&&y==3&&z==3))water.set_water(x,y,z,1.0);
    const double initial=world.total_water_volume_m3();
    rt.step(0.01,500);
    const auto force=world.wall_force(3,3,3);
    require(std::isfinite(force.z)&&force.z>0,
            "Pressure buoyancy not produced for immersed solid");
    // Quantitative approximate buoyancy: displaced cube 0.001 m3.
    const double displaced_buoyancy=1000*9.81*0.001;
    std::cerr<<"PRESSURE_FORCE_N="<<force.z<<" BUOYANCY_EXPECTED_N="<<displaced_buoyancy<<" BOTTOM_P="<<water.pressure_pa(3,3,2)<<" TOP_P="<<water.pressure_pa(3,3,4)<<"\\n";
    require(std::abs(force.z-displaced_buoyancy)<
            displaced_buoyancy*0.05,"Unexpected immersed 3D pressure force");
    const auto original_grid_z=world.rigid_body().z;
    for(int n=0;n<24;++n){
        world.step_body(0.003);
        near(world.rigid_body().last_mechanical_residual_j,0,1e-8,
             "Rigid pressure/kinetic/potential work imbalance");
        near(world.total_water_volume_m3(),initial,1e-8,
             "Moving rigid voxel lost or created water");
    }
    require(world.rigid_body().center_z_m>0.35,"Body does not rise under pressure");
    require(world.rigid_body().z>original_grid_z,"Body failed to change voxel cell");
    require(rt.core().voxels().at(3,3,world.rigid_body().z).material_id==3,
            "Moving rigid material id not updated");
    require(water.is_solid(3,3,world.rigid_body().z),"Moving body not in collision mask");
    require(!water.is_solid(3,3,original_grid_z),
            "Old body position not released");
    near(water.water_fraction(3,3,original_grid_z),1,1e-10,
         "Displaced water was not conservatively returned to vacated cell");
    // Verify the actual 3D MAC solver resumes after moving solid geometry;
    // a mere voxel swap without a subsequent fluid step is insufficient.
    world.step(0.01,300);
    near(world.total_water_volume_m3(),initial,1e-8,
         "3D fluid did not conserve volume after rigid voxel translation");
    require(water.is_solid(3,3,world.rigid_body().z) &&
            water.water_fraction(3,3,world.rigid_body().z)==0,
            "MAC water leaked into relocated rigid solid");
    rejects([&]{world.set_rigid_body(1,1,1,3,0.3);},"Multiple body accepted");
    rejects([&]{world.step_body(-1);},"Negative solid dt accepted");
}
int main(){
try{
    infiltration_test();
    saturated_and_rock_test();
    pressure_and_rigid_test();
    std::cout<<"PASS: "<<checks<<" 3D porous-material/voxel/pressure/moving-body tests\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;
}
}
