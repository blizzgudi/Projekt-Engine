#include "upc/sparse_water_lod3d.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int checks=0;
static void require(bool value,const char* message){
    ++checks;if(!value)throw std::runtime_error(message);
}
static void near(double value,double expected,double tol,const char* message){
    require(std::isfinite(value)&&std::abs(value-expected)<=tol,message);
}
static void rejects(const std::function<void()>& fn,const char* message){
    bool caught=false;
    try{fn();}catch(const std::exception&){caught=true;}
    require(caught,message);
}
int main(){
try{
    {
        CoupledMacWater3D fluid(17,13,9,0.25);
        fluid.set_water(0,0,0,1.0);
        fluid.set_water(16,12,8,0.25);
        fluid.set_water(8,6,4,0.75);
        fluid.set_solid(1,1,1);
        fluid.set_solid(16,0,8);
        SparseWaterLod3D lod;
        lod.rebuild(fluid,5);
        require(lod.level_count()==6,"Expected six 3D LOD levels");
        const auto baseline=fluid.volume_m3();
        near(baseline,0.03125,1e-14,"Source partial-cell volume");
        for(std::size_t l=0;l<lod.level_count();++l){
            near(lod.water_volume_m3(l),baseline,1e-13,
                 "Partial-edge 3D LOD loses fluid volume");
            const auto dims=lod.dimensions(l);
            const std::size_t scale=std::size_t(1)<<l;
            require(dims[0]==(17+scale-1)/scale,"X edge LOD dimension");
            require(dims[1]==(13+scale-1)/scale,"Y edge LOD dimension");
            require(dims[2]==(9+scale-1)/scale,"Z edge LOD dimension");
            require(lod.active_bricks(l)>0,"Nonempty LOD missing brick");
        }
        near(lod.at(0,16,12,8).water_fraction,0.25,1e-14,
             "Fine edge water fraction");
        near(lod.at(1,8,6,4).water_fraction,0.25,1e-14,
             "Coarse clipped edge water fraction");
        near(lod.at(1,0,0,0).water_fraction,0.125,1e-14,
             "2x2x2 water volume weighted");
        near(lod.at(1,0,0,0).solid_fraction,0.125,1e-14,
             "2x2x2 solid volume weighted");
        near(lod.at(0,1,1,1).solid_fraction,1.0,1e-14,
             "Solid occupancy source");
        near(lod.at(0,7,7,7).water_fraction,0,1e-14,
             "Sparse unallocated brick must read as empty");
        rejects([&]{lod.at(0,17,0,0);},"Out-of-domain sample accepted");
        rejects([&]{lod.at(7,0,0,0);},"Invalid LOD index accepted");
        rejects([&]{lod.rebuild(fluid,7);},"Excessive LOD level accepted");
        fluid.set_water(0,0,0,0);
        fluid.set_water(2,2,2,1);
        lod.rebuild(fluid,5);
        near(lod.at(0,0,0,0).water_fraction,0,1e-14,
             "Stale old water brick after rebuild");
        near(lod.at(0,2,2,2).water_fraction,1,1e-14,
             "Moved water not reflected in LOD");
        near(lod.water_volume_m3(5),baseline,1e-13,
             "Snapshot rebuild not conservative");
    }
    {
        CoupledMacWater3D fluid(64,64,64,0.1);
        fluid.set_water(0,0,0,1);
        fluid.set_water(63,63,63,0.5);
        SparseWaterLod3D lod;
        lod.rebuild(fluid,6);
        require(lod.level_count()==7,"64 cube level count");
        for(std::size_t i=0;i<lod.level_count();++i)
            near(lod.water_volume_m3(i),fluid.volume_m3(),1e-12,
                 "Sparse 64 cube volume drift");
        require(lod.resident_bytes_model()<lod.dense_bytes_model()/20,
                "Sparse brick payload does not beat dense representation");
        require(lod.active_bricks(0)==2,"Sparse leaf allocated empty bricks");
        require(lod.active_bricks(6)==1,"Top-level should contain one brick");
        const auto memory=lod.resident_bytes_model();
        lod.rebuild(fluid,6);
        require(lod.resident_bytes_model()==memory,
                "Identical source must produce deterministic brick footprint");
    }
    {
        CoupledMacWater3D fluid(12,8,6,0.1);
        for(int z=0;z<4;++z)for(int y=0;y<8;++y)
        for(int x=0;x<4;++x)fluid.set_water(x,y,z,1);
        const double mass0=fluid.mass_kg();
        SparseWaterLod3D lod;
        bool reached=false;
        for(int step=0;step<30;++step){
            fluid.step(0.012,100);
            lod.rebuild(fluid,4);
            for(std::size_t level=0;level<lod.level_count();++level)
                near(lod.water_volume_m3(level)*fluid.density_kg_m3(),
                     fluid.mass_kg(),1e-9,
                     "Live MAC 3D solver to render LOD changed water mass");
            near(fluid.mass_kg(),mass0,1e-9,"Live 3D MAC source mass drift");
            if(fluid.water_fraction(4,3,2)>0.001)reached=true;
        }
        require(reached,"Native MAC+VOF did not move into neighbor column");
        require(lod.active_bricks(0)>0,"Live source generated no sparse bricks");
    }
    std::cout<<"PASS: P7 conservative 3D MAC sparse render LOD; checks="
             <<checks<<"\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";
    return 1;
}
}
