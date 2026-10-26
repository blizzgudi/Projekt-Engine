#pragma once
// P7 bounded CPU-side resident streaming for read-only 3D MAC/VOF render LOD.
// Uses SparseWaterLod3D authoritative averages, not a replacement CFD solver.
// Residency and frame upload budgets measure actual fixed-size brick payload.
#include "upc/sparse_water_lod3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>
namespace upc {
class WaterBrickStreamer3D {
public:
 static constexpr std::size_t edge=8;
 struct Pixel {float water=0, solid=0;};
 struct Brick {std::array<Pixel,512> data{};};
 using Key=std::array<std::size_t,3>;
 struct Upload {
   std::size_t bytes=0, bricks=0, remaining=0;
   // The upload payload is the exact set of newly resident bricks,
   // allowing a real GPU driver to upload only this frame's budget.
   std::vector<std::pair<Key,Brick>> committed;
 };
private:
 std::map<Key,Brick> resident_;
 std::deque<std::pair<Key,Brick>> pending_;
 std::size_t level_=0, generation_=0, peak_bytes_=0;
 std::size_t capacity_bytes_;
 static Key key(std::size_t x,std::size_t y,std::size_t z){
   return {x/edge,y/edge,z/edge};
 }
 static std::size_t offset(std::size_t x,std::size_t y,std::size_t z){
   return ((z%edge)*edge+y%edge)*edge+x%edge;
 }
public:
 explicit WaterBrickStreamer3D(std::size_t bytes):capacity_bytes_(bytes){
   if(bytes<sizeof(Brick))throw std::invalid_argument("Brick capacity below one payload");
 }
 static std::size_t lod_for_distance(double meters){
   if(!std::isfinite(meters)||meters<0)throw std::invalid_argument("Invalid camera distance");
   return meters<10?0:(meters<25?1:2);
 }
 void request(const SparseWaterLod3D& lod,std::size_t level,
              std::array<double,3> camera_cell){
   if(level>=lod.level_count())throw std::out_of_range("Requested LOD missing");
   for(double v:camera_cell)if(!std::isfinite(v))
      throw std::invalid_argument("Nonfinite camera position");
   auto size=lod.dimensions(level);
   std::map<Key,Brick> all;
   for(std::size_t z=0;z<size[2];++z)
   for(std::size_t y=0;y<size[1];++y)
   for(std::size_t x=0;x<size[0];++x){
     auto s=lod.at(level,x,y,z);
     if(s.water_fraction<=0&&s.solid_fraction<=0)continue;
     auto& b=all[key(x,y,z)];
     b.data[offset(x,y,z)]={static_cast<float>(s.water_fraction),
                              static_cast<float>(s.solid_fraction)};
   }
   if(all.size()>capacity_bytes_/sizeof(Brick))
      throw std::length_error("Streaming requested bricks exceed capacity");
   std::vector<std::pair<Key,Brick>> ordered;
   ordered.reserve(all.size());
   for(auto& kv:all)ordered.emplace_back(kv);
   // Front load visual proximity, deterministic stable ties by key.
   const auto dist=[&](const Key& k){
      double sum=0;
      for(int j=0;j<3;++j){
       const double d=static_cast<double>(k[j]*edge+edge/2)-camera_cell[j];
       sum+=d*d;
      }
      return sum;
   };
   std::stable_sort(ordered.begin(),ordered.end(),[&](const auto& a,const auto& b){
      const auto d1=dist(a.first),d2=dist(b.first);
      return d1<d2;
   });
   // Atomic generation change: old visible bricks no longer represent
   // the new VOF field. Never mix stale and next-generation occupancy.
   resident_.clear();pending_.clear();level_=level;++generation_;
   for(auto& pair:ordered)pending_.push_back(std::move(pair));
 }
 Upload update(std::size_t upload_budget_bytes){
   if(upload_budget_bytes<sizeof(Brick))
       throw std::invalid_argument("Frame upload budget smaller than one brick");
   Upload r;
   while(!pending_.empty()&&r.bytes<=upload_budget_bytes-sizeof(Brick)){
     auto current=std::move(pending_.front());pending_.pop_front();
     r.committed.push_back(current);
     resident_.emplace(std::move(current.first),std::move(current.second));
     r.bytes+=sizeof(Brick);++r.bricks;
   }
   peak_bytes_=std::max(peak_bytes_,resident_.size()*sizeof(Brick));
   r.remaining=pending_.size();
   return r;
 }
 Pixel at(std::size_t x,std::size_t y,std::size_t z)const{
   const auto it=resident_.find(key(x,y,z));
   return it==resident_.end()?Pixel{}:it->second.data[offset(x,y,z)];
 }
 std::size_t resident_count()const{return resident_.size();}
 std::size_t pending_count()const{return pending_.size();}
 std::size_t resident_bytes()const{return resident_.size()*sizeof(Brick);}
 std::size_t peak_bytes()const{return peak_bytes_;}
 std::size_t capacity_bytes()const{return capacity_bytes_;}
 std::size_t level()const{return level_;}
 std::size_t generation()const{return generation_;}
};
} // namespace upc
