// P7 quantitative CPU LOD reference. This measures snapshot construction,
// NOT GPU throughput or physical 3D solver acceleration.
#include "upc/sparse_water_lod3d.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace upc;
int main(){
try{
    std::cout<<std::setprecision(14);
    std::cout<<"scenario,side,levels,source_volume_m3,finest_volume_m3,"
                "coarsest_volume_m3,active_bricks,brick_payload_bytes,"
                "dense_payload_bytes,build_ms\n";
    for(int n:{32,48,64}){
        CoupledMacWater3D source(n,n,n,0.1);
        for(int z=0;z<3;++z)for(int y=0;y<3;++y)
        for(int x=0;x<3;++x)source.set_water(x,y,z,0.25+0.01*x);
        source.set_water(n-1,n-1,n-1,0.5);
        source.set_solid(n-2,n-2,n-2);
        const double volume=source.volume_m3();
        std::size_t max_level=0;
        for(std::size_t side=n;side>1;side=(side+1)/2)++max_level;
        SparseWaterLod3D lod;
        const auto start=std::chrono::steady_clock::now();
        lod.rebuild(source,max_level);
        const auto end=std::chrono::steady_clock::now();
        const double millis=std::chrono::duration<double,std::milli>(end-start).count();
        for(std::size_t l=0;l<lod.level_count();++l)
            if(std::abs(lod.water_volume_m3(l)-volume)>1e-12)
                throw std::runtime_error("P7 benchmark LOD conservation failure");
        if(lod.resident_bytes_model()>=lod.dense_bytes_model()/2)
            throw std::runtime_error("P7 sparse payload not reduced by 50%");
        std::cout<<"sparse_3d_mac_snapshot,"<<n<<","<<lod.level_count()<<","
                 <<volume<<","<<lod.water_volume_m3(0)<<","
                 <<lod.water_volume_m3(lod.level_count()-1)<<","
                 <<lod.active_bricks(0)<<","<<lod.resident_bytes_model()
                 <<","<<lod.dense_bytes_model()<<","<<millis<<"\n";
    }
    return 0;
}catch(const std::exception& e){
    std::cerr<<"P7 reference FAILED: "<<e.what()<<"\n";return 1;
}
}
