// P2 PARTIAL: quantify Test-02 binary obstacle aliasing against exact cut-cell
// box geometry. Diagnostic only: production MARIN MAC still uses binary solids.
#include "upc/p2_moving_box_open_volume3d.hpp"
#include "upc/p2_moving_box_mac_apertures3d.hpp"
#include "upc/p2_marin_pressure_sensors3d.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
unsigned assertions=0;
void check(bool ok,const char* message) {
    ++assertions;
    if(!ok)throw std::runtime_error(message);
}
void near(double got,double expected,double tol,const char* message) {
    check(std::isfinite(got)&&std::abs(got-expected)<=tol,message);
}
constexpr double xmin=.6635,xmax=.8245,ymin=.295,ymax=.698,zmin=0.0,zmax=.161;
const upc::P2Box3D box{{xmin,ymin,zmin},{xmax,ymax,zmax}};
constexpr double physical_volume=(xmax-xmin)*(ymax-ymin)*(zmax-zmin);
// Independently integrate blocked MAC face area at every grid plane.
// Exact static axis-aligned box cross-sections are analytical references.
double face_area_residual(std::size_t nx,std::size_t ny,std::size_t nz,
                          double dx) {
    const auto faces=upc::P2MovingBoxFaceApertures3D::sample(
        nx,ny,nz,dx,.01,box,box);
    const std::array<std::size_t,3> sizes{{nx,ny,nz}};
    const std::array<double,3> lo{{xmin,ymin,zmin}},hi{{xmax,ymax,zmax}};
    const std::array<const std::vector<double>*,3> arrays{{
        &faces.u_open,&faces.v_open,&faces.w_open}};
    double max_residual=0;
    for(int axis=0;axis<3;++axis) {
        const double expected_cross=(hi[(axis+1)%3]-lo[(axis+1)%3])*
                                    (hi[(axis+2)%3]-lo[(axis+2)%3]);
        for(std::size_t plane=0;plane<=sizes[axis];++plane) {
            double blocked_area=0;
            for(std::size_t a=0;a<sizes[(axis+1)%3];++a)
            for(std::size_t b=0;b<sizes[(axis+2)%3];++b) {
                std::array<std::size_t,3> ijk{};
                ijk[axis]=plane;
                ijk[(axis+1)%3]=a;
                ijk[(axis+2)%3]=b;
                const auto x=ijk[0],y=ijk[1],z=ijk[2];
                std::size_t id=0;
                if(axis==0)id=(z*ny+y)*(nx+1)+x;
                if(axis==1)id=(z*(ny+1)+y)*nx+x;
                if(axis==2)id=(z*ny+y)*nx+x;
                const double open=arrays[axis]->at(id);
                check(open>=-1e-12&&open<=1+1e-12,
                      "MAC face aperture bounded");
                blocked_area+=(1-open)*dx*dx;
            }
            const double coord=plane*dx;
            const double expected=coord>=lo[axis]&&coord<=hi[axis]?
                                  expected_cross:0.0;
            const double error=std::abs(blocked_area-expected);
            max_residual=std::max(max_residual,error);
            near(blocked_area,expected,2e-13,
                 "analytic MARIN box MAC face cross-section");
        }
    }
    return max_residual;
}
void grid(const char* name,std::size_t nx,std::size_t ny,std::size_t nz,double dx) {
    const auto g=upc::P2MovingBoxOpenVolumes3D::sample(
        nx,ny,nz,dx,.01,box,box);
    double cut_volume=0,binary_volume=0;
    unsigned cut_cells=0,unresolved_sensors=0;
    const double cell_volume=dx*dx*dx;
    for(std::size_t z=0;z<nz;++z)
    for(std::size_t y=0;y<ny;++y)
    for(std::size_t x=0;x<nx;++x) {
        const auto id=(z*ny+y)*nx+x;
        const double f=1.0-g.alpha_old.at(id);
        check(f>=-1e-12&&f<=1.0+1e-12,"cut fraction bounded");
        cut_volume+=f*cell_volume;
        if(f>1e-12&&f<1-1e-12)++cut_cells;
        const double cx=(x+.5)*dx,cy=(y+.5)*dx,cz=(z+.5)*dx;
        const bool binary=cx>=xmin&&cx<xmax&&cy>=ymin&&cy<ymax&&
                          cz>=zmin&&cz<zmax;
        if(binary)binary_volume+=cell_volume;
    }
    const auto probes=upc::P2MarinPressureSensors3D::transducers_m();
    for(std::size_t k=0;k<probes.size();++k) {
        const auto& p=probes[k];
        // The actual wall lies INSIDE the host voxel. A centre-only
        // occupancy test can miss that physical wall altogether.
        const auto x=std::size_t(std::floor(p[0]/dx));
        const auto y=std::size_t(std::floor(p[1]/dx));
        const auto z=std::size_t(std::floor(p[2]/dx));
        check(x<nx&&y<ny&&z<nz,"transducer host in grid");
        const double cut=1.0-g.alpha_old.at((z*ny+y)*nx+x);
        check(cut>0,"actual sensor host intersects box");
        const double cx=(x+.5)*dx,cy=(y+.5)*dx,cz=(z+.5)*dx;
        const bool binary=cx>=xmin&&cx<xmax&&cy>=ymin&&cy<ymax&&
                          cz>=zmin&&cz<zmax;
        if(!binary)++unresolved_sensors;
    }
    near(cut_volume,physical_volume,2e-13,"exact MARIN obstacle volume");
    near(g.solid_volume_m3,physical_volume,1e-15,"geometry box volume");
    near(g.solid_volume_residual_m3,0,2e-13,"static solid conservation");
    check(std::abs(binary_volume-physical_volume)>1e-5,
          "binary obstacle alias must be measured");
    check(cut_cells>0&&unresolved_sensors>0,
          "subvoxel obstacle and sensor hosts unresolved");
    const double face_residual=face_area_residual(nx,ny,nz,dx);
    std::cout<<"MARIN_BOX_GEOM grid="<<name<<" dx="<<dx
             <<" max_face_area_residual_m2="<<face_residual
             <<" analytic_m3="<<physical_volume
             <<" cut_m3="<<cut_volume
             <<" binary_m3="<<binary_volume
             <<" binary_error_m3="<<binary_volume-physical_volume
             <<" cut_cells="<<cut_cells
             <<" unresolved_sensors="<<unresolved_sensors<<"\n";
}
void aligned_and_negative() {
    const upc::P2Box3D aligned{{.2,.3,.1},{.4,.5,.3}};
    const auto g=upc::P2MovingBoxOpenVolumes3D::sample(
        10,10,10,.1,.01,aligned,aligned);
    near(g.solid_volume_m3,.008,1e-15,"aligned analytic box volume");
    for(double a:g.alpha_old)
        check(std::abs(a)<1e-12||std::abs(a-1)<1e-12,
              "aligned box must not produce partial cells");
    bool rejected=false;
    try {
        const upc::P2Box3D invalid{{.2,.3,.1},{1.1,.5,.3}};
        (void)upc::P2MovingBoxOpenVolumes3D::sample(
            10,10,10,.1,.01,invalid,invalid);
    }catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"out-of-domain geometry rejected");
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        aligned_and_negative();
        grid("coarse",32,10,10,.1);
        grid("medium",48,15,15,3.2/48.0);
        grid("fine",64,20,20,.05);
        std::cout<<"PASS MARIN_BOX_GEOMETRY assertions="<<assertions
                 <<" binary_obstacle_unresolved=true"
                 <<" production_cut_cell_integrated=false"
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL MARIN_BOX_GEOMETRY "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
