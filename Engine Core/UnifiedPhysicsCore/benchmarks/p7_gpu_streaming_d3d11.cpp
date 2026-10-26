// P7 hardware D3D11 resident brick streaming, not a CPU-only fake upload.
// 8^3 water/solid float2 bricks are transferred into real VRAM, one brick
// per frame. Readback proves byte-identical CPU/GPU data, bounded memory.
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include "upc/p7_brick_streamer3d.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using namespace upc;
using Microsoft::WRL::ComPtr;
void check(HRESULT hr,const char* s){
 if(FAILED(hr))throw std::runtime_error(std::string(s)+":"+std::to_string(hr));
}
int main(){
try{
 ComPtr<ID3D11Device> dev;ComPtr<ID3D11DeviceContext> ctx;
 D3D_FEATURE_LEVEL level;
 D3D_FEATURE_LEVEL desired[]={D3D_FEATURE_LEVEL_11_0};
 check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,
     desired,1,D3D11_SDK_VERSION,dev.GetAddressOf(),&level,
     ctx.GetAddressOf()),"Hardware D3D11 failed");
 ComPtr<IDXGIDevice> dxgi;check(dev.As(&dxgi),"DXGI interface");
 ComPtr<IDXGIAdapter> adapter;check(dxgi->GetAdapter(adapter.GetAddressOf()),"GPU adapter");
 DXGI_ADAPTER_DESC desc={};check(adapter->GetDesc(&desc),"GPU adapter info");
 if(!desc.VendorId||level<D3D_FEATURE_LEVEL_11_0)
   throw std::runtime_error("No real D3D11 feature-level 11 GPU");
 CoupledMacWater3D water(64,64,64,0.1);
 for(int z=0;z<7;++z)for(int y=0;y<7;++y)
 for(int x=0;x<7;++x)water.set_water(x,y,z,0.9);
 water.set_water(63,63,63,0.4);
 water.set_solid(58,58,58);
 SparseWaterLod3D lod;lod.rebuild(water,6);
 constexpr std::size_t total_bricks=16;
 const std::size_t brick_bytes=sizeof(WaterBrickStreamer3D::Brick);
 const std::size_t budget=brick_bytes;
 D3D11_BUFFER_DESC gd={};
 gd.ByteWidth=UINT(total_bricks*brick_bytes);gd.Usage=D3D11_USAGE_DEFAULT;
 gd.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 gd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
 gd.StructureByteStride=sizeof(WaterBrickStreamer3D::Pixel);
 ComPtr<ID3D11Buffer> gpu;
 check(dev->CreateBuffer(&gd,nullptr,gpu.GetAddressOf()),"GPU brick resident buffer");
 D3D11_BUFFER_DESC sd=gd;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;
 sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;sd.StructureByteStride=0;
 ComPtr<ID3D11Buffer> staging;
 check(dev->CreateBuffer(&sd,nullptr,staging.GetAddressOf()),"GPU readback staging");
 std::cout<<"scenario,generation,lod,active_bricks,frames,gpu_buffer_bytes,"
             "bytes_uploaded,max_bytes_per_frame,max_frame_upload_ms,"
             "gpu_readback_difference_bytes,adapter_vendor_id\n";
 for(const int new_lod:{0,1,2}){
   // A new generation never displays stale payload; upload clear first.
   std::vector<std::uint8_t> expected(total_bricks*brick_bytes,0);
   ctx->UpdateSubresource(gpu.Get(),0,nullptr,expected.data(),0,0);
   WaterBrickStreamer3D streamer(total_bricks*brick_bytes);
   streamer.request(lod,new_lod,{0,0,0});
   const auto requested=streamer.pending_count();
   if(requested<2)throw std::runtime_error("GPU streaming case too small");
   std::size_t offset=0,frames=0,uploaded=0,max_frame_bytes=0;
   double slowest=0;
   while(streamer.pending_count()){
     const auto before=std::chrono::steady_clock::now();
     const auto batch=streamer.update(budget);
     if(batch.bytes>budget||batch.committed.size()!=batch.bricks)
       throw std::runtime_error("Frame transfer budget violated");
     for(const auto& tile:batch.committed){
       if(offset+brick_bytes>expected.size())
         throw std::runtime_error("GPU resident capacity overflow");
       D3D11_BOX destination={UINT(offset),0,0,UINT(offset+brick_bytes),1,1};
       ctx->UpdateSubresource(gpu.Get(),0,&destination,tile.second.data.data(),0,0);
       memcpy(expected.data()+offset,tile.second.data.data(),brick_bytes);
       offset+=brick_bytes;
     }
     ctx->Flush();
     ++frames;uploaded+=batch.bytes;
     max_frame_bytes=std::max(max_frame_bytes,batch.bytes);
     slowest=std::max(slowest,std::chrono::duration<double,std::milli>(
          std::chrono::steady_clock::now()-before).count());
   }
   ctx->CopyResource(staging.Get(),gpu.Get());
   D3D11_MAPPED_SUBRESOURCE mapped={};
   check(ctx->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"D3D11 hardware staging map");
   const auto* actual=reinterpret_cast<const std::uint8_t*>(mapped.pData);
   std::size_t differences=0;
   for(std::size_t i=0;i<expected.size();++i)
     if(actual[i]!=expected[i])++differences;
   ctx->Unmap(staging.Get(),0);
   if(differences||frames!=requested||uploaded!=requested*brick_bytes||
      max_frame_bytes>budget||slowest>1000)
     throw std::runtime_error("Hardware streaming parity/latency/memory FAIL");
   std::cout<<"d3d11_live_brick_upload,"<<streamer.generation()<<","
      <<new_lod<<","<<requested<<","<<frames<<","
      <<expected.size()<<","<<uploaded<<","<<max_frame_bytes<<","
      <<slowest<<","<<differences<<","<<desc.VendorId<<"\n";
 }
 std::cerr<<"PASS: P7 real D3D11 GPU brick buffer, per-frame capacity bounded, readback byte parity\n";
 return 0;
}catch(const std::exception& e){
 std::cerr<<"FAIL: P7 D3D11 brick upload: "<<e.what()<<"\n";return 1;
}
}
