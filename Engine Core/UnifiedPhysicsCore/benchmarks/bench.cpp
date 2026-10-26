// Reproducible CPU timing on the same machine for distinct numerical modules.
// This benchmark does NOT measure GPU timing, UE FPS, or correctness.
#include "upc/core.hpp"
#include "upc/volume3d.hpp"
#include "upc/mac3d.hpp"
#include "upc/materials_ext.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace upc;
template<typename F> double measured_ms(F f){
    auto a=std::chrono::steady_clock::now();
    f();
    auto b=std::chrono::steady_clock::now();
    return std::chrono::duration<double,std::milli>(b-a).count();
}
int main(){
try{
    std::cout<<"experiment,cells,iterations,time_ms\n";
    for(int size : {32,64,96}){
        ShallowWater2D w(size,size,0.1,1);
        for(int y=0;y<size;++y)for(int x=0;x<size/3;++x)
            w.set_depth(x,y,0.6);
        const double ms=measured_ms([&]{for(int i=0;i<30;++i)w.step(0.02);});
        std::cout<<"shallow_water_2d,"<<size*size<<",30,"<<std::setprecision(8)<<ms<<"\n";
    }
    for(int size : {16,24,32}){
        VolumeTransport3D w(size,size,size,0.1);
        for(int z=2;z<size/2;++z)for(int y=0;y<size/2;++y)
            for(int x=0;x<size/2;++x)w.set_fill(x,y,z,1);
        w.set_uniform_velocity(0.2,0.1,-0.1);
        double before=w.volume_m3();
        const double ms=measured_ms([&]{for(int i=0;i<20;++i)w.advect(0.02);});
        if(std::abs(w.volume_m3()-before)>1e-8)throw std::runtime_error("Benchmark volume drift");
        std::cout<<"3d_volume_transport,"<<size*size*size<<",20,"<<ms<<"\n";
    }
    for(int size : {8,12,16}){
        MacProjection3D m(size,size,size,0.1);
        for(int z=0;z<size/2;++z)for(int y=0;y<size;++y)
            for(int x=0;x<size;++x)m.set_cell(x,y,z,true);
        const double ms=measured_ms([&]{
            for(int i=0;i<10;++i){m.apply_gravity(0.01);m.project(0.01,80);}
        });
        std::cout<<"3d_mac_projection,"<<size*size*size<<",10,"<<ms<<"\n";
    }
    for(int count : {1000,10000,100000}){
        SparseVoxelChunks s;
        const double ms=measured_ms([&]{
            for(int n=0;n<count;++n)
                s.set(n%1024,(n/1024)%1024,n/1048576,1);
        });
        if(s.active_voxels()!=(std::size_t)count)
            throw std::runtime_error("Chunk benchmark data mismatch");
        std::cout<<"sparse_chunk_insert,"<<count<<","<<count<<","<<ms<<"\n";
    }
    return 0;
}catch(const std::exception& e){
    std::cerr<<"BENCHMARK FAILED: "<<e.what()<<"\n";return 1;
}
}
