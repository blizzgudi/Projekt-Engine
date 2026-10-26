// P5 affine-strain 3D lattice stress response reference.
// It is an elastic spring-lattice calibration, not continuum FEM validation.
#include "upc/solid_lattice3d.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace upc;
struct Measurement{double axial_n=0,shear_n=0,elastic_j=0;};
static Measurement load_lattice(const MaterialLibrary& lib,int n,
                                double axial_strain,double shear_strain){
    SpringLattice3D sample(lib,n,n,n,0.1,{7,1200,0.15});
    // Homogeneous imposed affine displacements; no dynamic relaxation.
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        auto& node=sample.node(x,y,z);
        const auto ref=node.position;
        node.position={ref.x+shear_strain*ref.z,ref.y,
                       ref.z*(1+axial_strain)};
    }
    double axial=0,shear=0;
    for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        const std::size_t i=(static_cast<std::size_t>(n-1)*n+y)*n+x;
        const auto force=sample.solid().elastic_force_on_node(i);
        axial+=force.z;shear+=force.x;
    }
    return {-axial,-shear,sample.solid().elastic_energy_j()};
}
int main(){
try{
    MaterialLibrary lib;lib.add({7,Phase::Solid,2500,0});
    std::cout<<std::setprecision(13);
    std::cout<<"scenario,nx,epsilon,applied_shear,axial_force_n,"
                "shear_force_n,elastic_energy_j\n";
    for(const int n:{4,6,8}){
        for(const double eps:{0.001,0.002,0.004}){
            const auto s=load_lattice(lib,n,eps,0);
            std::cout<<"uniaxial_lattice,"<<n<<","<<eps<<",0,"
                     <<s.axial_n<<","<<s.shear_n<<","<<s.elastic_j<<"\n";
        }
        const auto s=load_lattice(lib,n,0,0.002);
        std::cout<<"shear_lattice,"<<n<<",0,0.002,"
                 <<s.axial_n<<","<<s.shear_n<<","<<s.elastic_j<<"\n";
    }
    return 0;
}catch(const std::exception& e){
    std::cerr<<"P5 CONSTITUTIVE FAIL: "<<e.what()<<"\n";return 1;
}
}
