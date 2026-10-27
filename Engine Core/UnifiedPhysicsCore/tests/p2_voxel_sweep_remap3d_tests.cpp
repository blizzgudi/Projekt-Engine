// P2 PARTIAL: analytical one-cell swept occupancy and VOF remap.
#include "upc/p2_voxel_sweep_remap3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void require(bool b,const char* message) {
    ++assertions;
    if(!b)throw std::runtime_error(message);
}
void close(double a,double b,double tol,const char* message) {
    require(std::isfinite(a)&&std::abs(a-b)<=tol,message);
}
using Mask=std::vector<std::uint8_t>;
using Water=std::vector<double>;
void scenario(int n,int axis,int sign) {
    const std::size_t side=static_cast<std::size_t>(n);
    const std::size_t cells=side*side*side;
    auto idx=[&](int x,int y,int z)->std::size_t {
        return static_cast<std::size_t>((z*n+y)*n+x);
    };
    Mask body(cells,0),fixed(cells,0);
    Water fill(cells,0);
    // Two disjoint solid runs on parallel axis lines; all three axes.
    std::array<int,3> p{{n/2,n/2,n/2}};
    for(int k=0;k<2;++k) {
        auto c=p;c[axis]+=k;body[idx(c[0],c[1],c[2])]=1;
    }
    auto c2=p;c2[(axis+1)%3]+=2;
    body[idx(c2[0],c2[1],c2[2])]=1;
    // Populate every nonsolid cell with different deterministic fractional VOF.
    double before=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=idx(x,y,z);
        if(!body[i])fill[i]=((x*7+y*11+z*13)%9)/10.0;
        before+=fill[i];
    }
    const auto original_body=body,original_fixed=fixed;
    const auto original_fill=fill;
    std::array<int,3> d{{0,0,0}};d[axis]=sign;
    const auto result=upc::VoxelSweepRemap3D::translate(
        side,side,side,0.125,body,fixed,fill,d);
    require(body==original_body&&fixed==original_fixed&&fill==original_fill,
            "Inputs must be immutable");
    require(result.entering_cells==2&&result.vacated_cells==2,
            "Two solid runs enter/vacate one cell each");
    double after=0;
    for(std::size_t i=0;i<cells;++i) {
        after+=result.water_fill[i];
        require(result.water_fill[i]>=0&&result.water_fill[i]<=1,
                "VOF fraction bounded");
        require(!result.body_mask[i]||result.water_fill[i]==0,
                "New body excludes water");
    }
    close(after,before,1e-11,"VOF mass conserved");
    close(result.mass_residual_m3,0,1e-13,"Physical volume residual");
    double displaced=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=idx(x,y,z);
        if(!body[i])continue;
        std::array<int,3> c{{x,y,z}};
        c[axis]+=sign;
        const auto dest=idx(c[0],c[1],c[2]);
        require(result.body_mask[dest]==1,"Exact translated occupancy");
        if(!body[dest])displaced+=fill[dest];
    }
    close(result.displaced_water_m3,displaced*0.125*0.125*0.125,1e-13,
          "Exact displaced volume");
    // Reversing the step conserves total mass, but does not reconstruct
    // original water distribution (irreversible remap, not fluid advection).
    const auto reverse=upc::VoxelSweepRemap3D::translate(
        side,side,side,0.125,result.body_mask,fixed,result.water_fill,
        {{-d[0],-d[1],-d[2]}});
    require(reverse.body_mask==body,"Reverse occupancy exactly restored");
    double reverse_total=0;
    for(double f:reverse.water_fill)reverse_total+=f;
    close(reverse_total,before,1e-11,"Reverse water volume conserved");
    std::cout<<n<<","<<axis<<","<<sign<<","
             <<result.entering_cells<<","<<result.displaced_water_m3<<","
             <<result.mass_residual_m3<<"\n";
}
void reject_inputs() {
    const int n=6;
    auto idx=[](int x,int y,int z){return static_cast<std::size_t>((z*6+y)*6+x);};
    Mask b(216,0),s(216,0);Water w(216,0);
    b[idx(2,2,2)]=1;
    w[idx(3,2,2)]=0.75;
    const auto oldb=b,olds=s;const auto oldw=w;
    auto rejects=[&](std::array<int,3> d) {
        bool rejected=false;
        try{(void)upc::VoxelSweepRemap3D::translate(6,6,6,0.1,b,s,w,d);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Expected invalid sweep rejection");
        require(b==oldb&&s==olds&&w==oldw,"Rejected sweep mutated input");
    };
    rejects({{0,0,0}});
    rejects({{1,1,0}});
    rejects({{2,0,0}});
    s[idx(3,2,2)]=1;w[idx(3,2,2)]=0;
    bool blocked=false;
    try{(void)upc::VoxelSweepRemap3D::translate(6,6,6,0.1,b,s,w,{{1,0,0}});}
    catch(const std::invalid_argument&){blocked=true;}
    require(blocked,"Static obstacle must block sweep");
    s[idx(3,2,2)]=0;w[idx(3,2,2)]=0.75;
    b[idx(5,2,2)]=1;
    blocked=false;
    try{(void)upc::VoxelSweepRemap3D::translate(6,6,6,0.1,b,s,w,{{1,0,0}});}
    catch(const std::invalid_argument&){blocked=true;}
    require(blocked,"Boundary must block sweep");
    b[idx(5,2,2)]=0;
    w[idx(2,2,2)]=0.1;
    blocked=false;
    try{(void)upc::VoxelSweepRemap3D::translate(6,6,6,0.1,b,s,w,{{1,0,0}});}
    catch(const std::invalid_argument&){blocked=true;}
    require(blocked,"Water/body overlap rejected");
    w[idx(2,2,2)]=0;
    w[idx(3,2,2)]=std::numeric_limits<double>::quiet_NaN();
    blocked=false;
    try{(void)upc::VoxelSweepRemap3D::translate(6,6,6,0.1,b,s,w,{{1,0,0}});}
    catch(const std::invalid_argument&){blocked=true;}
    require(blocked,"NaN VOF rejected");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17)
                 <<"grid,axis,sign,entering,displaced_m3,residual_m3\n";
        for(int n:{8,12,16})for(int a=0;a<3;++a)
            for(int sign:{-1,1})scenario(n,a,sign);
        reject_inputs();
        std::cout<<"PASS: P2 voxel sweep conservative remap assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
