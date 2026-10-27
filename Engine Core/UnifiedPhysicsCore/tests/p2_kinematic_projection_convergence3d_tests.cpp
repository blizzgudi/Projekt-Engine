// P2 PARTIAL: reject unconverged moving-wall MAC projections atomically.
// A bounded iteration count is not itself a convergence certificate.
#include "upc/kinematic_wall_projection3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::size_t checks=0;
void require(bool condition,const char* why) {
    ++checks;
    if(!condition)throw std::runtime_error(why);
}
using Vec=std::array<double,3>;
struct Scene {
    int n;
    double dx=0.08;
    upc::MacProjection3D mac;
    std::vector<std::uint8_t> wet,solid,body;
    Vec centre;
    Scene(int dim,bool open):n(dim),mac(dim,dim,dim,dx),
        wet(static_cast<std::size_t>(dim*dim*dim),1),
        solid(wet.size(),0),body(wet.size(),0),
        centre{{0.5*dim*dx,0.5*dim*dx,0.5*dim*dx}} {
        const int lo=dim/2-1;
        for(int z=lo;z<lo+2;++z)
        for(int y=lo;y<lo+2;++y)
        for(int x=lo;x<lo+2;++x) {
            const auto i=idx(x,y,z);
            wet[i]=0;solid[i]=body[i]=1;
        }
        if(open) {
            // An air boundary makes this a mixed Dirichlet/Neumann solve.
            for(int z=1;z<dim-1;++z)
            for(int y=1;y<dim-1;++y)wet[idx(1,y,z)]=0;
        }
        mac.set_face_velocity_x(2,2,2,0.037);
        mac.set_face_velocity_y(2,2,2,-0.021);
        mac.set_face_velocity_z(2,2,2,0.014);
    }
    std::size_t idx(int x,int y,int z)const {
        return static_cast<std::size_t>((z*n+y)*n+x);
    }
    std::vector<double> snapshot()const {
        std::vector<double> result;
        for(int z=0;z<n;++z)for(int y=0;y<n;++y)
        for(int x=0;x<=n;++x)result.push_back(mac.face_velocity_x(x,y,z));
        for(int z=0;z<n;++z)for(int y=0;y<=n;++y)
        for(int x=0;x<n;++x)result.push_back(mac.face_velocity_y(x,y,z));
        for(int z=0;z<=n;++z)for(int y=0;y<n;++y)
        for(int x=0;x<n;++x)result.push_back(mac.face_velocity_z(x,y,z));
        return result;
    }
    upc::KinematicWallProjectionResult3D run(int iterations,bool moving=true) {
        return upc::KinematicWallMacProjection3D::project(
            mac,n,n,n,dx,1000.0,0.025,wet,solid,body,centre,
            moving?Vec{{0.12,-0.08,0.05}}:Vec{{0,0,0}},
            moving?Vec{{0.4,-0.5,0.7}}:Vec{{0,0,0}},iterations);
    }
};
void scenario(int n,bool open) {
    Scene s(n,open);
    const auto original=s.snapshot();
    for(int budget:{1,2}) {
        bool rejected=false;
        try {(void)s.run(budget);}
        catch(const std::runtime_error& e) {
            rejected=std::string(e.what()).find("divergence tolerance")!=
                std::string::npos;
        }
        require(rejected,"under-converged MAC projection must fail");
        const auto now=s.snapshot();
        require(now.size()==original.size(),"snapshot sizes");
        for(std::size_t i=0;i<now.size();++i)
            require(now[i]==original[i],"failed projection mutated MAC face");
    }
    const auto accepted=s.run(5000);
    require(accepted.max_divergence_after_s_inv<=1e-8,
            "accepted projection exceeds divergence bound");
    require(accepted.pressure_iterations>2,
            "nontrivial pressure field converged spuriously");
    require(s.snapshot()!=original,"successful solve did not commit");
    std::cout<<n<<","<<(open?"open":"closed")<<",2,"
             <<accepted.pressure_iterations<<","
             <<accepted.max_divergence_after_s_inv<<","
             <<original.size()<<"\n";
}
void zero_flow() {
    Scene s(8,false);
    // Clear all pre-existing face disturbances before the stationary case.
    for(int z=0;z<s.n;++z)for(int y=0;y<s.n;++y)
    for(int x=0;x<=s.n;++x)s.mac.set_face_velocity_x(x,y,z,0);
    for(int z=0;z<s.n;++z)for(int y=0;y<=s.n;++y)
    for(int x=0;x<s.n;++x)s.mac.set_face_velocity_y(x,y,z,0);
    for(int z=0;z<=s.n;++z)for(int y=0;y<s.n;++y)
    for(int x=0;x<s.n;++x)s.mac.set_face_velocity_z(x,y,z,0);
    const auto accepted=s.run(1,false);
    require(accepted.max_divergence_after_s_inv==0,
            "zero flow should pass even with one iteration");
    require(accepted.pressure_iterations==1,
            "stationary zero flow should not require iterations");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17)
                 <<"grid,boundary,rejected_budgets,accepted_iterations,"
                   "max_divergence_s_inv,mac_faces\n";
        for(int n:{8,12,16})
            for(bool open:{false,true})scenario(n,open);
        zero_flow();
        std::cout<<"PASS: P2 atomic convergence checks="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" checks="<<checks<<"\n";
        return 1;
    }
}
