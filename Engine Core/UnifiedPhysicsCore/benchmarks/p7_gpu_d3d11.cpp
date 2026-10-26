// P7 true hardware Direct3D11 GPU compute, crosschecked cell-for-cell
// with independent CPU SparseWaterLod3D over actual 3D MAC/VOF states.
// DOES NOT claim running the Navier-Stokes physics solver on the GPU.
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#include "upc/sparse_water_lod3d.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace upc;
using Microsoft::WRL::ComPtr;
struct Pair{float w,s;};
static_assert(sizeof(Pair)==8,"Unexpected shader structured-buffer stride");
struct Constants{UINT nx,ny,nz,scale,cx,cy,cz,padding;};
void check(HRESULT hr,const char* label){
  if(FAILED(hr))throw std::runtime_error(std::string(label)+", HRESULT="+std::to_string(hr));
}
ComPtr<ID3D11Buffer> buffer(ID3D11Device* dev,UINT size,UINT stride,UINT bind,
                           D3D11_USAGE usage,UINT cpu_flags,const void* data=nullptr){
 D3D11_BUFFER_DESC desc={};
 desc.ByteWidth=size;desc.Usage=usage;desc.BindFlags=bind;
 desc.CPUAccessFlags=cpu_flags;desc.MiscFlags=(stride?D3D11_RESOURCE_MISC_BUFFER_STRUCTURED:0);
 desc.StructureByteStride=stride;
 D3D11_SUBRESOURCE_DATA init={};init.pSysMem=data;
 ComPtr<ID3D11Buffer> out;
 check(dev->CreateBuffer(&desc,data?&init:nullptr,out.GetAddressOf()),"CreateBuffer");
 return out;
}
struct GPU {
 ComPtr<ID3D11Device> device;
 ComPtr<ID3D11DeviceContext> context;
 ComPtr<ID3D11ComputeShader> compute;
 DXGI_ADAPTER_DESC adapter{};
 D3D_FEATURE_LEVEL feature{};
 GPU(){
   const D3D_FEATURE_LEVEL requested[]={D3D_FEATURE_LEVEL_11_0};
   check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,
         requested,1,D3D11_SDK_VERSION,device.GetAddressOf(),
         &feature,context.GetAddressOf()),"HARDWARE D3D11CreateDevice (WARP NOT ALLOWED)");
   if(feature<D3D_FEATURE_LEVEL_11_0)
      throw std::runtime_error("Hardware compute shader 5.0 unsupported");
   ComPtr<IDXGIDevice> dxgi;
   check(device.As(&dxgi),"DXGI device");
   ComPtr<IDXGIAdapter> gpu;
   check(dxgi->GetAdapter(gpu.GetAddressOf()),"DXGI adapter");
   check(gpu->GetDesc(&adapter),"DXGI adapter description");
   ComPtr<ID3DBlob> blob,errors;
   // Both source + CTest cwd copy require shaders/ subdirectory.
   const HRESULT h=D3DCompileFromFile(L"shaders/p7_vof_reduce.hlsl",nullptr,
      D3D_COMPILE_STANDARD_FILE_INCLUDE,"CSMain","cs_5_0",
      D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,
      blob.GetAddressOf(),errors.GetAddressOf());
   if(FAILED(h)){
     if(errors)std::cerr<<(const char*)errors->GetBufferPointer()<<"\n";
     check(h,"HLSL D3DCompileFromFile");
   }
   check(device->CreateComputeShader(blob->GetBufferPointer(),blob->GetBufferSize(),
        nullptr,compute.GetAddressOf()),"CreateComputeShader");
 }
 struct Out{
   std::vector<Pair> result;
   double gpu_ms=0,wall_ms=0;
   std::size_t allocated_gpu_bytes=0;
 };
 Out reduce(const std::vector<Pair>& input,
     UINT nx,UINT ny,UINT nz,UINT scale){
   if(!scale||scale>16||input.size()!=std::size_t(nx)*ny*nz)
     throw std::invalid_argument("GPU input shape/scale invalid");
   const UINT cx=nx/scale+(nx%scale!=0),cy=ny/scale+(ny%scale!=0),
              cz=nz/scale+(nz%scale!=0);
   const UINT output_count=cx*cy*cz;
   const UINT in_bytes=static_cast<UINT>(input.size()*sizeof(Pair)),
              out_bytes=output_count*sizeof(Pair);
   const auto host0=std::chrono::steady_clock::now();
   auto inbuf=buffer(device.Get(),in_bytes,sizeof(Pair),D3D11_BIND_SHADER_RESOURCE,
                     D3D11_USAGE_DEFAULT,0,input.data());
   auto outbuf=buffer(device.Get(),out_bytes,sizeof(Pair),D3D11_BIND_UNORDERED_ACCESS,
                      D3D11_USAGE_DEFAULT,0);
   auto staging=buffer(device.Get(),out_bytes,0,0,D3D11_USAGE_STAGING,
                       D3D11_CPU_ACCESS_READ);
   Constants params{nx,ny,nz,scale,cx,cy,cz,0};
   auto constant=buffer(device.Get(),sizeof(params),0,D3D11_BIND_CONSTANT_BUFFER,
                        D3D11_USAGE_DEFAULT,0,&params);
   D3D11_SHADER_RESOURCE_VIEW_DESC sd={};
   sd.Format=DXGI_FORMAT_UNKNOWN;sd.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;
   sd.Buffer.NumElements=static_cast<UINT>(input.size());
   ComPtr<ID3D11ShaderResourceView> srv;
   check(device->CreateShaderResourceView(inbuf.Get(),&sd,srv.GetAddressOf()),"SRV");
   D3D11_UNORDERED_ACCESS_VIEW_DESC ud={};
   ud.Format=DXGI_FORMAT_UNKNOWN;ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;
   ud.Buffer.NumElements=output_count;
   ComPtr<ID3D11UnorderedAccessView> uav;
   check(device->CreateUnorderedAccessView(outbuf.Get(),&ud,uav.GetAddressOf()),"UAV");
   ComPtr<ID3D11Query> disjoint,start,end;
   D3D11_QUERY_DESC q={D3D11_QUERY_TIMESTAMP_DISJOINT,0};
   check(device->CreateQuery(&q,disjoint.GetAddressOf()),"GPU query disjoint");
   q.Query=D3D11_QUERY_TIMESTAMP;
   check(device->CreateQuery(&q,start.GetAddressOf()),"GPU query start");
   check(device->CreateQuery(&q,end.GetAddressOf()),"GPU query end");
   ID3D11ShaderResourceView* src[]={srv.Get()};
   ID3D11UnorderedAccessView* dst[]={uav.Get()};
   ID3D11Buffer* cbuf[]={constant.Get()};
   context->CSSetShader(compute.Get(),nullptr,0);
   context->CSSetShaderResources(0,1,src);
   context->CSSetUnorderedAccessViews(0,1,dst,nullptr);
   context->CSSetConstantBuffers(0,1,cbuf);
   context->Begin(disjoint.Get());
   context->End(start.Get());
   context->Dispatch((cx+7)/8,(cy+7)/8,(cz+3)/4);
   context->End(end.Get());
   context->End(disjoint.Get());
   ID3D11ShaderResourceView* clear_src[]={nullptr};
   ID3D11UnorderedAccessView* clear_dst[]={nullptr};
   context->CSSetShaderResources(0,1,clear_src);
   context->CSSetUnorderedAccessViews(0,1,clear_dst,nullptr);
   context->CSSetShader(nullptr,nullptr,0);
   context->CopyResource(staging.Get(),outbuf.Get());
   context->Flush();
   D3D11_QUERY_DATA_TIMESTAMP_DISJOINT freq={};
   UINT64 ta=0,tb=0;
   const auto startwait=std::chrono::steady_clock::now();
   auto wait_query=[&](ID3D11Query* query,void* ptr,UINT bytes){
      while(context->GetData(query,ptr,bytes,0)==S_FALSE){
        if(std::chrono::steady_clock::now()-startwait>std::chrono::seconds(6))
          throw std::runtime_error("GPU D3D11 query completion timeout");
        Sleep(1);
      }
   };
   wait_query(disjoint.Get(),&freq,sizeof(freq));
   wait_query(start.Get(),&ta,sizeof(ta));
   wait_query(end.Get(),&tb,sizeof(tb));
   if(freq.Disjoint||!freq.Frequency||tb<ta)
     throw std::runtime_error("GPU device timestamp query disjoint");
   D3D11_MAPPED_SUBRESOURCE mapped={};
   check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"staging Map");
   std::vector<Pair> result(output_count);
   memcpy(result.data(),mapped.pData,out_bytes);
   context->Unmap(staging.Get(),0);
   const double ms=std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-host0).count();
   return {std::move(result),(tb-ta)*1000.0/freq.Frequency,ms,
           std::size_t(in_bytes)+out_bytes*2+sizeof(params)};
 }
};
int main(){
 try{
  GPU gpu;
  std::cout<<std::setprecision(12);
  std::cout<<"gpu_adapter_vendor_id,gpu_adapter_device_id,feature_level,gpu_dedicated_vram_bytes\n";
  std::cout<<gpu.adapter.VendorId<<","<<gpu.adapter.DeviceId<<","<<gpu.feature<<","
           <<gpu.adapter.DedicatedVideoMemory<<"\n";
  std::cerr<<"P7_GPU_HARDWARE_VENDOR="<<gpu.adapter.VendorId
           <<" DEVICE="<<gpu.adapter.DeviceId<<"\n";
  std::cout<<"scenario,nx,ny,nz,scale,coarse_cells,source_volume_m3,"
             "gpu_volume_m3,cpu_reference_volume_m3,max_cell_error,"
             "gpu_kernel_ms,wall_upload_compute_readback_ms,"
             "gpu_buffer_budget_bytes,cpu_reference_ms\n";
  for(int scenario=0;scenario<5;++scenario){
    const int nx=scenario==0?17:scenario==1?12:scenario==2?32:scenario==3?48:64;
    const int ny=scenario==0?13:scenario==1?8:nx;
    const int nz=scenario==0?9:scenario==1?6:nx;
    CoupledMacWater3D source(nx,ny,nz,0.1);
    if(scenario==0){
       source.set_water(0,0,0,1);
       source.set_water(nx-1,ny-1,nz-1,0.25);
       source.set_water(8,6,4,0.75);
       source.set_solid(1,1,1);
    }else if(scenario==1){
       for(int z=0;z<4;++z)for(int y=0;y<8;++y)
       for(int x=0;x<4;++x)source.set_water(x,y,z,1);
       for(int i=0;i<30;++i)source.step(0.012,100);
    }else {
       for(int z=0;z<5;++z)for(int y=0;y<6;++y)
       for(int x=0;x<7;++x)source.set_water(x,y,z,0.3+0.01*x);
       source.set_water(nx-1,ny-1,nz-1,0.5);
       source.set_solid(nx-2,ny-2,nz-2);
    }
    std::vector<Pair> input(std::size_t(nx)*ny*nz);
    for(int z=0;z<nz;++z)for(int y=0;y<ny;++y)
    for(int x=0;x<nx;++x){
      const auto i=(z*ny+y)*nx+x;
      input[i]={float(source.water_fraction(x,y,z)),
                source.is_solid(x,y,z)?1.0f:0.0f};
    }
    const auto snap0=std::chrono::steady_clock::now();
    SparseWaterLod3D reference;
    std::size_t max_level=0;
    for(int n=std::max({nx,ny,nz});n>1;n=(n+1)/2)++max_level;
    reference.rebuild(source,max_level);
    const double cpu_ms=std::chrono::duration<double,std::milli>(
                  std::chrono::steady_clock::now()-snap0).count();
    for(const int scale:{2,4,8,16}){
      auto l=std::size_t(0);
      for(int s=scale;s>1;s/=2)++l;
      if(l>=reference.level_count())continue;
      const auto sizes=reference.dimensions(l);
      const auto out=gpu.reduce(input,nx,ny,nz,scale);
      if(out.result.size()!=sizes[0]*sizes[1]*sizes[2])
        throw std::runtime_error("GPU 3D output shape mismatch");
      double vol_gpu=0,error=0;
      for(std::size_t z=0;z<sizes[2];++z)
      for(std::size_t y=0;y<sizes[1];++y)
      for(std::size_t x=0;x<sizes[0];++x){
        const auto item=out.result[(z*sizes[1]+y)*sizes[0]+x];
        const auto cpu=reference.at(l,x,y,z);
        error=std::max(error,std::abs(double(item.w)-cpu.water_fraction));
        error=std::max(error,std::abs(double(item.s)-cpu.solid_fraction));
        if(!std::isfinite(item.w)||!std::isfinite(item.s)||
           item.w<-1e-6||item.s<-1e-6||item.w+item.s>1.00001)
           throw std::runtime_error("GPU output invalid occupancy");
        const double covered=std::min(std::size_t(scale),std::size_t(nx)-x*scale)*
            std::min(std::size_t(scale),std::size_t(ny)-y*scale)*
            std::min(std::size_t(scale),std::size_t(nz)-z*scale);
        vol_gpu+=double(item.w)*covered*0.001;
      }
      const double baseline=source.volume_m3();
      const double cpu=reference.water_volume_m3(l);
      if(error>2e-6||std::abs(vol_gpu-baseline)>2e-5||
         std::abs(cpu-baseline)>1e-9)
          throw std::runtime_error("P7 CPU/GPU 3D LOD parity/mass FAIL");
      std::cout<<"real_mac_vof_lod,"<<nx<<","<<ny<<","<<nz<<","<<scale<<","
          <<out.result.size()<<","<<baseline<<","<<vol_gpu<<","<<cpu<<","
          <<error<<","<<out.gpu_ms<<","<<out.wall_ms<<","
          <<out.allocated_gpu_bytes<<","<<cpu_ms<<"\n";
    }
  }
  std::cerr<<"PASS: P7 native D3D11 HARDWARE GPU compute with 3D MAC/VOF CPU reference parity\n";
  return 0;
 }catch(const std::exception& err){
  std::cerr<<"FAIL: P7 GPU native: "<<err.what()<<"\n";
  return 1;
 }
}
