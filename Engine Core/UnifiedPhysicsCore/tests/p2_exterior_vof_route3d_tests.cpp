// P2 PARTIAL: exterior-only 3D voxel water reclassification tests.
#include "upc/p2_exterior_vof_route3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
std::size_t idx(int n,int x,int y,int z) {
    return static_cast<std::size_t>((z*n+y)*n+x);
}
using Mask=std::vector<std::uint8_t>;
using Fill=std::vector<double>;
void scenario(int n,int axis,int sign) {
    const auto cells=static_cast<std::size_t>(n*n*n);
    Mask body(cells,0),fixed(cells,0);
    Fill water(cells,0.0);
    std::array<int,3> centre{{n/2,n/2,n/2}};
    const auto old=idx(n,centre[0],centre[1],centre[2]);
    body[old]=1;
    double initial=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=idx(n,x,y,z);
        if(!body[i])water[i]=0.25+0.01*((x+2*y+3*z)%20);
        initial+=water[i];
    }
    auto leading=centre;
    leading[axis]+=sign;
    const auto source=idx(n,leading[0],leading[1],leading[2]);
    const auto body_before=body;
    const auto water_before=water;
    std::array<int,3> d{{0,0,0}};
    d[axis]=sign;
    const auto out=upc::ExteriorVoxelSweep3DRemap::translate(
        n,n,n,0.125,body,fixed,water,d);
    check(body==body_before&&water==water_before,
          "Caller arrays must remain unchanged");
    check(out.entering_cells==1&&out.vacated_cells==1,
          "Single body enters/vacates exactly one cell");
    check(out.routes.size()==1&&out.face_steps==3,
          "Shortest exterior path must be three face steps");
    check(out.body_mask[source]==1&&out.body_mask[old]==0,
          "Exact translated body occupancy");
    const auto& route=out.routes.at(0);
    check(route.cell_indices.size()==4&&
          route.cell_indices.front()==source&&
          route.cell_indices.back()==old,
          "Exterior route endpoints");
    for(std::size_t k=1;k+1<route.cell_indices.size();++k) {
        const auto i=route.cell_indices[k];
        check(!body[i]&&!out.body_mask[i]&&!fixed[i],
              "Route intermediate must be exterior in BOTH geometries");
    }
    Fill expected=water;
    for(std::size_t j=route.cell_indices.size()-1;j>0;--j)
        expected[route.cell_indices[j]]=expected[route.cell_indices[j-1]];
    expected[source]=0;
    double total=0;
    for(std::size_t i=0;i<cells;++i) {
        near(out.water_fill[i],expected[i],0,"Exact local permutation");
        check(out.water_fill[i]>=0&&out.water_fill[i]<=1,
              "All fractions remain bounded");
        check(!out.body_mask[i]||out.water_fill[i]==0,
              "No new body water overlap");
        total+=out.water_fill[i];
    }
    near(total,initial,1e-10,"Total water volume invariant");
    near(out.mass_residual_m3,0,1e-11,"Zero volume residual");
    near(out.leading_displaced_m3,water[source]*0.125*0.125*0.125,
         1e-14,"Correct leading displaced water");
    near(route.leading_fraction,water[source],0,
         "Route source fraction preserved");
    std::cout<<n<<","<<axis<<","<<sign<<","
             <<out.face_steps<<","<<out.leading_displaced_m3<<","
             <<out.mass_residual_m3<<"\n";
}
void multi_body() {
    const int n=8;
    const auto cells=static_cast<std::size_t>(n*n*n);
    Mask body(cells,0),fixed(cells,0);
    Fill water(cells,0.3);
    const auto a=idx(n,3,3,3),b=idx(n,3,5,3);
    body[a]=body[b]=1;
    water[a]=water[b]=0;
    const auto out=upc::ExteriorVoxelSweep3DRemap::translate(
        n,n,n,0.2,body,fixed,water,{{1,0,0}});
    check(out.entering_cells==2&&out.vacated_cells==2,
          "Two disjoint bodies");
    check(out.routes.size()==2&&out.face_steps>=6,
          "Two exterior paths required");
    double before=0,after=0;
    for(std::size_t i=0;i<cells;++i) {
        before+=water[i];after+=out.water_fill[i];
        check(!out.body_mask[i]||out.water_fill[i]==0,
              "Multi-body water/solid exclusion");
    }
    near(after,before,1e-10,"Multi-body mass");
}
void sealed_and_dry() {
    const int n=5;
    const auto cells=static_cast<std::size_t>(n*n*n);
    Mask body(cells,0),fixed(cells,1);
    Fill water(cells,0);
    const auto old=idx(n,2,2,2),enter=idx(n,3,2,2);
    body[old]=1;
    fixed[old]=fixed[enter]=0;
    water[enter]=0.75;
    bool rejected=false;
    try{
        (void)upc::ExteriorVoxelSweep3DRemap::translate(
            n,n,n,0.1,body,fixed,water,{{1,0,0}});
    }catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Sealed wet source must reject nonlocal transfer");
    check(body[old]==1&&water[enter]==0.75,
          "Rejected exterior sweep leaves caller unchanged");
    water[enter]=0;
    const auto dry=upc::ExteriorVoxelSweep3DRemap::translate(
        n,n,n,0.1,body,fixed,water,{{1,0,0}});
    check(dry.routes.empty()&&dry.body_mask[enter]==1&&
          dry.water_fill[old]==0,
          "Sealed dry sweep is allowed without fluid transport");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17)
                 <<"grid,axis,sign,face_steps,leading_m3,residual_m3\n";
        for(int n:{8,12,16})for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})scenario(n,axis,sign);
        multi_body();
        sealed_and_dry();
        std::cout<<"PASS: P2 exterior VOF routes assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
