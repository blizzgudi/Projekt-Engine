#include "upc/material_profile.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <limits>

static int checks=0;
void check(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
void reject(const std::function<void()>& fn,const char* why){
    bool thrown=false;
    try{fn();}catch(const std::exception&){thrown=true;}
    check(thrown,why);
}
int main(){
    try {
        upc::MaterialLibrary physics;
        physics.add({1,upc::Phase::Liquid,1000.0,0.001});
        physics.add({2,upc::Phase::Solid,2500.0,0.0});
        physics.add({3,upc::Phase::Solid,7800.0,0.0});
        upc::MaterialProfileLibrary visuals(physics);
        visuals.set(upc::make_water_profile(1));
        visuals.set(upc::make_sand_profile(2));
        visuals.set(upc::make_metal_profile(3));
        check(visuals.size()==3,"Expected 3 central material profiles");
        check(visuals.at(1).appearance.base_color.blue>
              visuals.at(1).appearance.base_color.red,"Water must be blue");
        check(visuals.at(1).appearance.opacity<1,"Water should be translucent");
        check(visuals.at(3).appearance.metallic==1,"Metal visual property missing");

        upc::VoxelGrid volume(32,16,8,0.1);
        for(std::size_t z=0;z<volume.nz();++z)
          for(std::size_t y=0;y<volume.ny();++y)
            for(std::size_t x=0;x<volume.nx();++x)
                volume.set(x,y,z,{1,1.0,293.15,0},physics);
        const double mass_before=volume.mass_kg(physics);
        auto first=volume.at(0,0,0);
        check(first.material_id==1,"Water voxel did not store material ID");
        const auto color_before=visuals.at(first.material_id).appearance.base_color;
        check(color_before.blue>0.8f,"Expected blue default");
        auto updated=upc::make_water_profile(1);
        updated.appearance.base_color={0.04f,0.82f,0.73f};
        visuals.set(updated);
        const auto other=volume.at(31,15,7);
        const auto c1=visuals.at(first.material_id).appearance.base_color;
        const auto c2=visuals.at(other.material_id).appearance.base_color;
        check(c1.green==c2.green && c1.blue==c2.blue,
              "Shared profile update should apply to all voxels");
        check(c1.green!=color_before.green,"Profile update had no effect");
        check(volume.at(0,0,0).material_id==first.material_id,
              "Appearance edit modified physics voxel data");
        check(std::abs(volume.mass_kg(physics)-mass_before)<1e-10,
              "Changing visual metadata altered physics mass");
        reject([&]{visuals.at(99);},"Missing ID should not render silently");
        reject([&]{visuals.set(upc::make_water_profile(99));},
               "Unknown physics material ID should not be registered");
        auto broken=updated;
        broken.appearance.base_color.blue=std::numeric_limits<float>::quiet_NaN();
        reject([&]{visuals.set(broken);},"NaN colors accepted");
        broken=updated;broken.appearance.opacity=1.3f;
        reject([&]{visuals.set(broken);},"Opacity outside [0,1] accepted");
        broken=updated;broken.appearance.metallic=-0.1f;
        reject([&]{visuals.set(broken);},"Negative metallic accepted");
        broken=updated;broken.appearance.roughness=2.f;
        reject([&]{visuals.set(broken);},"Roughness >1 accepted");
        broken=updated;broken.name.clear();
        reject([&]{visuals.set(broken);},"Empty material name accepted");
        std::cout<<"PASS: central material appearance profile, blue water, shared 4096"
                    " voxels, validated visuals, independent physics; checks="<<checks<<"\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";
        return 1;
    }
}
