// Native D3D11 GPU P7: volume-weighted VOF and solids downsampling.
// Authoritative data from CoupledMacWater3D; this is RENDER LOD, not CFD.
StructuredBuffer<float2> SourceCells : register(t0);
RWStructuredBuffer<float2> OutputCells : register(u0);
cbuffer Params : register(b0) {
 uint NX, NY, NZ, Scale;
 uint CX, CY, CZ, Reserved;
};
[numthreads(8,8,4)]
void CSMain(uint3 id:SV_DispatchThreadID) {
 if(id.x>=CX || id.y>=CY || id.z>=CZ) return;
 uint x0=id.x*Scale,y0=id.y*Scale,z0=id.z*Scale;
 uint xe=min(x0+Scale,NX),ye=min(y0+Scale,NY),ze=min(z0+Scale,NZ);
 float water=0,solid=0;
 for(uint z=z0;z<ze;++z)
 for(uint y=y0;y<ye;++y)
 for(uint x=x0;x<xe;++x) {
   float2 c=SourceCells[(z*NY+y)*NX+x];
   water+=c.x; solid+=c.y;
 }
 float count=(float)((xe-x0)*(ye-y0)*(ze-z0));
 OutputCells[(id.z*CY+id.y)*CX+id.x]=float2(water/count,solid/count);
}