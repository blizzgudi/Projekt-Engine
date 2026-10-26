#include "upc/p7_brick_streamer3d.hpp"
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace upc;
static std::size_t checks=0;
static void req(bool v,const char* msg){++checks;if(!v)throw std::runtime_error(msg);}
int main(){
try{
 CoupledMacWater3D source(48,48,48,0.1);
 for(int z=0;z<5;++z)for(int y=0;y<6;++y)
 for(int x=0;x<7;++x)source.set_water(x,y,z,0.4);
 source.set_water(47,47,47,0.6);
 source.set_solid(42,42,42);
 SparseWaterLod3D snap;snap.rebuild(source,6);
 const auto baseline=source.volume_m3();
 WaterBrickStreamer3D streamer(sizeof(WaterBrickStreamer3D::Brick)*30);
 req(WaterBrickStreamer3D::lod_for_distance(0)==0,"near LOD");
 req(WaterBrickStreamer3D::lod_for_distance(12)==1,"medium LOD");
 req(WaterBrickStreamer3D::lod_for_distance(40)==2,"far LOD");
 for(std::size_t level=0;level<3;++level){
   const auto gen=streamer.generation();
   streamer.request(snap,level,{0,0,0});
   req(streamer.generation()==gen+1,"generation reset");
   req(streamer.level()==level,"LOD selection not applied");
   const auto wanted=streamer.pending_count();
   req(wanted>0,"sparse world has no nonempty bricks");
   std::size_t frames=0,transferred=0;
   const auto budget=sizeof(WaterBrickStreamer3D::Brick); // exactly one tile/frame
   while(streamer.pending_count()>0){
      const auto current=streamer.update(budget);
      req(current.bytes<=budget,"upload exceeds frame budget");
      req(current.bricks<=1,"unbounded residency upload");
      transferred+=current.bytes;
      ++frames;
      req(frames<=wanted,"streaming exceeded theoretical frame bound");
   }
   req(frames==wanted,"1 brick/frame deterministic convergence");
   req(transferred==wanted*budget,"upload byte count wrong");
   req(streamer.resident_bytes()==transferred,"resident payload wrong");
   req(streamer.resident_bytes()<=streamer.capacity_bytes(),"VRAM staging budget exceeded");
   const auto dim=snap.dimensions(level);
   double streamed_volume=0;
   const auto scale=std::size_t(1)<<level;
   for(std::size_t z=0;z<dim[2];++z)
    for(std::size_t y=0;y<dim[1];++y)
     for(std::size_t x=0;x<dim[0];++x){
      const auto v=streamer.at(x,y,z);
      const auto ref=snap.at(level,x,y,z);
      req(std::abs(v.water-ref.water_fraction)<1e-6,"missing/altered streamed water");
      req(std::abs(v.solid-ref.solid_fraction)<1e-6,"missing/altered streamed solids");
      auto covered=std::min(scale,std::size_t(source.nx())-x*scale)*
                   std::min(scale,std::size_t(source.ny())-y*scale)*
                   std::min(scale,std::size_t(source.nz())-z*scale);
      streamed_volume+=double(v.water)*covered*0.001;
     }
   req(std::abs(streamed_volume-baseline)<1e-6,"streamed LOD changed mass");
 }
 source.set_water(47,47,47,0);
 source.set_water(46,46,46,0.6);
 snap.rebuild(source,6);
 const auto before=streamer.generation();
 streamer.request(snap,1,{47,47,47});
 req(streamer.generation()==before+1,"stale generation accepted");
 req(streamer.resident_count()==0,"old scene not invalidated");
 while(streamer.pending_count())streamer.update(sizeof(WaterBrickStreamer3D::Brick)*2);
 req(streamer.at(23,23,23).water>0,"new snapshot not rendered");
 bool failed=false;
 try{streamer.update(1);}catch(const std::invalid_argument&){failed=true;}
 req(failed,"invalid low upload budget accepted");
 failed=false;try{streamer.request(snap,4,{0,0,0});}
 catch(const std::exception&){failed=false;} // valid request
 failed=false;try{streamer.request(snap,99,{0,0,0});}
 catch(const std::exception&){failed=true;}
 req(failed,"out of range LOD accepted");
 WaterBrickStreamer3D tight(sizeof(WaterBrickStreamer3D::Brick));
 failed=false;try{tight.request(snap,0,{0,0,0});}
 catch(const std::length_error&){failed=true;}
 req(failed,"too many bricks accepted despite capacity limit");
 std::cout<<"PASS: P7 streaming conservative sparse 3D MAC, near/far LOD,"
             " bounded brick/frame, deterministic convergence and snapshots; checks="
             <<checks<<"\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: P7 streaming after "<<checks<<": "<<e.what()<<"\n";return 1;}
}
