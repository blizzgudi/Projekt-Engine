// AI Game Studio One / Virtual Voxel Printer - isolated tree-trunk proof.
// True upc::MaterialLibrary + upc::VoxelGrid. No external studio or Unreal changes.
// This generates a voxelled solid; the renderer reads exactly this binary payload.
#include "upc/core.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>

namespace fs=std::filesystem;
static constexpr std::size_t NX=72,NY=72,NZ=112;
static constexpr double DX=0.012, PI=3.1415926535897932384626433832795;
static constexpr double HEIGHT=NZ*DX;
struct Species {unsigned char vox_id; unsigned int core_id; const char *name; double density;};
static constexpr std::array<Species,4> SPECIES {{
    {1,1101,"heartwood",720.0},
    {2,1102,"sapwood",550.0},
    {3,1103,"cambium",640.0},
    {4,1104,"bark",610.0}
}};
static int material_at(double x,double y,double z) {
    const double cx=(x-NX*DX*0.5),cy=(y-NY*DX*0.5);
    const double radius=std::hypot(cx,cy);
    const double a=std::atan2(cy,cx);
    const double h=NZ*DX;
    const double tapered=0.198+0.046*(1.0-z/h);
    // Five sculpted buttress roots emerge from the base; a trunk, not a cube.
    const double buttress=0.105*std::exp(-z/0.18)*std::pow(std::max(0.0,std::cos(5.0*a-0.9)),6.0);
    // Deep longitudinal bark furrows, subtle twist, and small irregularities.
    const double grooves=0.010*std::cos(17*a+0.6*z)+0.006*std::cos(31*a-0.55*z);
    const double side=tapered+buttress+grooves;
    if(radius>side)return 0;
    const double depth=side-radius;
    if(depth<0.028)return 4;
    if(depth<0.043)return 3;
    if(depth<0.106)return 2;
    return 1;
}
int main(int argc,char **argv) {
    try {
        fs::path out=argc>1?argv[1]:"output";
        fs::create_directories(out);
        upc::MaterialLibrary lib;
        for(const auto& s:SPECIES)lib.add({s.core_id,upc::Phase::Solid,s.density,0});
        upc::VoxelGrid grid(NX,NY,NZ,DX);
        std::vector<std::uint8_t> voxel(NX*NY*NZ,0);
        std::array<std::uint64_t,5> counts{};
        for(std::size_t z=0;z<NZ;++z)
          for(std::size_t y=0;y<NY;++y)
            for(std::size_t x=0;x<NX;++x){
                const auto id=material_at((x+0.5)*DX,(y+0.5)*DX,(z+0.5)*DX);
                const std::size_t idx=(z*NY+y)*NX+x;
                voxel[idx]=static_cast<std::uint8_t>(id);
                ++counts[static_cast<std::size_t>(id)];
                if(id){
                    upc::Voxel v;
                    v.material_id=SPECIES[id-1].core_id;
                    v.fill=1;
                    v.temperature_k=293.15;
                    v.damage=0;
                    grid.set(x,y,z,v,lib);
                }
            }
        if(!counts[1]||!counts[2]||!counts[3]||!counts[4])
            throw std::runtime_error("Required tree material layer empty.");
        const auto occupied=counts[1]+counts[2]+counts[3]+counts[4];
        double independentMass=0;
        for(const auto&s:SPECIES)independentMass+=counts[s.vox_id]*DX*DX*DX*s.density;
        const double coreMass=grid.mass_kg(lib);
        if(std::abs(coreMass-independentMass)>1e-6)
            throw std::runtime_error("UPC mass mismatch.");
        const auto data=out/"voxel_material_ids.bin";
        std::ofstream bin(data,std::ios::binary);
        if(!bin.write(reinterpret_cast<const char*>(voxel.data()),voxel.size()))
           throw std::runtime_error("Cannot export voxel payload.");
        bin.close();
        std::ofstream json(out/"report.json",std::ios::binary);
        json<<std::setprecision(15);
        json<<"{\n  \"status\":\"PASS\",\n  \"scope\":\"material_assigned_voxel_construction_only\",\n";
        json<<"  \"source\":\"real UPC MaterialLibrary and VoxelGrid\",\n";
        json<<"  \"dx_m\":"<<DX<<", \"nx\":"<<NX<<", \"ny\":"<<NY<<", \"nz\":"<<NZ<<",\n";
        json<<"  \"height_m\":"<<HEIGHT<<", \"seed\":1,\n";
        json<<"  \"occupied_voxels\":"<<occupied<<", \"empty_voxels\":"<<counts[0]<<",\n";
        json<<"  \"mass_kg_upc\":"<<coreMass<<", \"mass_kg_independent\":"<<independentMass<<",\n";
        json<<"  \"materials\":[\n";
        for(std::size_t i=0;i<SPECIES.size();++i){
            const auto&s=SPECIES[i];
            json<<"    {\"voxel_id\":"<<int(s.vox_id)<<",\"upc_material_id\":"<<s.core_id
                <<",\"name\":\""<<s.name<<"\",\"density_kg_m3\":"<<s.density
                <<",\"voxel_count\":"<<counts[s.vox_id]<<"}";
            if(i+1<SPECIES.size())json<<",";
            json<<"\n";
        }
        json<<"  ],\n  \"stages_percent\":[10,30,55,80,100],\n";
        json<<"  \"assumptions\":[\"Illustrative wood densities, not calibrated wood mechanics.\","
               "\"Bark grooves and root flares are geometric rules.\","
               "\"Cambium is exaggerated to one or more voxel cells for visibility.\","
               "\"No growth, water transport or realistic fracture solver involved.\"],\n";
        json<<"  \"evidence_files\":[\"voxel_material_ids.bin\",\"report.json\"]\n}\n";
        json.close();
        std::cout<<"PASS UPC_VOXEL_TREE_TRUNK nx="<<NX<<" ny="<<NY<<" nz="<<NZ
                 <<" occupied="<<occupied<<" mass_kg="<<std::setprecision(8)<<coreMass
                 <<" binary_bytes="<<voxel.size()<<"\n";
        for(const auto&s:SPECIES)std::cout<<s.name<<" count="<<counts[s.vox_id]
                                              <<" physical_material_id="<<s.core_id<<"\n";
        return 0;
    }catch(const std::exception&e){
        std::cerr<<"FAIL "<<e.what()<<"\n";
        return 2;
    }
}
