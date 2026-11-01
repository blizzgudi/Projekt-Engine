// P2 PARTIAL: analytic oblique planar interface as a pressure boundary.
// This does not establish stress-consistent two-phase CFD or full P2.
#include "upc/p2_planar_vof_pcg_geometry3d.hpp"
#include "upc/coupled_water3d.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int count=0;
void check(bool condition,const char* reason){
    ++count;
    if(!condition)throw std::runtime_error(reason);
}
void near(double actual,double expected,double tol,const char* reason){
    if(!std::isfinite(actual)||std::abs(actual-expected)>tol)
        std::cerr<<"MISMATCH "<<reason<<" actual="<<actual
                 <<" expected="<<expected<<"\n";
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tol,reason);
}
template<class F>void reject(F fn,const char* reason) {
    bool caught=false;
    try{fn();}catch(const std::exception&){caught=true;}
    check(caught,reason);
}
using Vec=std::array<double,3>;
double dot(Vec a,Vec b){
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
struct Slab {
    std::vector<double> fill;
    std::vector<std::uint8_t> solid;
    std::vector<Vec> normals;
};
Slab make_slab(int n,Vec v,double location) {
    Slab s;
    s.fill.resize(n*n*n);
    s.solid.resize(s.fill.size(),0);
    s.normals.resize(s.fill.size());
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=(z*n+y)*n+x;
        s.normals[i]=v;
        const double local_cut=location-dot(v,Vec{double(x),double(y),double(z)});
        s.fill[i]=std::clamp(upc::p2_oblique_detail::clip_cube(v,local_cut).volume,0.0,1.0);
        if(s.fill[i]<1e-12)s.fill[i]=0;
        if(s.fill[i]>1-1e-12)s.fill[i]=1;
    }
    return s;
}
void manufactured(int n,Vec normal,double cut) {
    Slab s=make_slab(n,normal,cut);
    auto geometry=upc::p2_planar_vof_pcg_geometry3d(
        s.fill,s.solid,s.normals,n,n,n,1.);
    check(!geometry.resolved.empty(),"real oblique faces resolved");
    check(geometry.oblique_faces>0,"oblique geometrical cuts recognized");
    for(const auto& f:geometry.resolved){
        const std::array<std::size_t,3> pos{f.x,f.y,f.z};
        auto a=pos;
        --a[f.axis];
        const auto i=(a[2]*n+a[1])*n+a[0],j=(pos[2]*n+pos[1])*n+pos[0];
        const bool leftwet=s.fill[i]>.5;
        const int direction=leftwet?1:-1;
        const Vec wet=leftwet?Vec{double(a[0])+.5,double(a[1])+.5,double(a[2])+.5}:
            Vec{double(pos[0])+.5,double(pos[1])+.5,double(pos[2])+.5};
        const double expected=(cut-dot(normal,wet))/(normal[f.axis]*direction);
        near(f.theta,expected,2e-8,"fractional 3D oblique pressure distance");
        check(f.theta>0&&f.theta<=1,"physical ghost-fluid interpolation");
    }
    upc::MacProjection3D mac(n,n,n,1.,1000.);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        mac.set_cell(x,y,z,s.fill[(z*n+y)*n+x]>.5);
    upc::p2_apply_planar_vof_pcg_geometry3d(mac,geometry);
    for(const auto& f:geometry.resolved)
        if(f.axis==0)mac.set_face_velocity_x(f.x,f.y,f.z,.05);
        else if(f.axis==1)mac.set_face_velocity_y(f.x,f.y,f.z,.05);
        else mac.set_face_velocity_z(f.x,f.y,f.z,.05);
    const auto audit=mac.project_pcg_verified(.1,3000,1e-7,1e-10);
    check(audit.max_divergence_s_inv<1e-7,
          "oblique 3D pressure interface projection converges");
    check(audit.liquid_cells>0,"nonzero fluid pressure cells");
    std::cout<<"OBLIQUE_3D n="<<n<<" cut="<<cut
             <<" faces="<<geometry.resolved.size()
             <<" two_partials="<<geometry.two_partial_faces
             <<" ghost_div="<<audit.max_divergence_s_inv<<"\n";
}
void axis_equivalence(){
    const int n=8;
    std::vector<double> fill(n*n*n,0);
    std::vector<std::uint8_t> solid(fill.size(),0);
    std::vector<Vec> normals(fill.size(),Vec{1,0,0});
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=(z*n+y)*n+x;
        fill[i]=x<3?1:(x==3?.75:0);
    }
    const auto p=upc::p2_planar_vof_pcg_geometry3d(
        fill,solid,normals,n,n,n,1);
    const auto a=upc::p2_axis_vof_pcg_geometry3d(
        fill,solid,n,n,n,1);
    check(p.resolved.size()==std::size_t(n*n),"axis equivalence face count");
    check(p.resolved.size()==a.resolved.size(),"axis fallback face count");
    check(p.oblique_faces==0,"axis geometry must not claim obliqueness");
    for(std::size_t i=0;i<p.resolved.size();++i) {
        check(p.resolved[i].axis==a.resolved[i].axis,"axis index");
        check(p.resolved[i].x==a.resolved[i].x,"x face");
        check(p.resolved[i].y==a.resolved[i].y,"y face");
        check(p.resolved[i].z==a.resolved[i].z,"z face");
        near(p.resolved[i].theta,a.resolved[i].theta,1e-12,
             "axis plane is exact original theta");
    }
}
void contradictory_planes(){
    const int n=8;
    const Vec unit{1./std::sqrt(2.),1./std::sqrt(2.),0.0};
    auto s=make_slab(n,unit,6.17/std::sqrt(2.));
    const auto original=upc::p2_planar_vof_pcg_geometry3d(
        s.fill,s.solid,s.normals,n,n,n,1);
    check(original.two_partial_faces>0,"need two-partial analytical faces");
    // Change just one partial-cell normal: two sides of the same
    // neighbour link cannot be one planar interface anymore.
    for(std::size_t i=0;i<s.fill.size();++i)if(s.fill[i]>0&&s.fill[i]<1) {
        s.normals[i]=Vec{1,0,0};
        break;
    }
    reject([&]{(void)upc::p2_planar_vof_pcg_geometry3d(
        s.fill,s.solid,s.normals,n,n,n,1);},
        "inconsistent two-cell normals fail closed");
}

void live_coupled(){
    const int n=8;
    const Vec normal{1./std::sqrt(2.),1./std::sqrt(2.),0.0};
    const auto s=make_slab(n,normal,6.17/std::sqrt(2.));
    upc::CoupledMacWater3D water(n,n,n,.1,1000);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=(z*n+y)*n+x;
        if(s.fill[i]>0)water.set_water(x,y,z,s.fill[i]);
    }
    const double mass=water.mass_kg();
    check(!water.planar_vof_pcg_geometry_enabled(),
          "new planar geometry must default to OFF");
    reject([&]{water.set_planar_vof_pcg_geometry(true);},
           "planar mode must require PCG enabled");
    water.set_pcg_pressure_projection(true);
    reject([&]{water.set_planar_vof_pcg_geometry(true);},
           "planar mode must require explicitly supplied normals");
    water.set_planar_vof_pcg_normals(s.normals);
    water.set_planar_vof_pcg_geometry(true);
    check(water.planar_vof_pcg_geometry_enabled(),"planar mode active");
    reject([&]{water.set_axis_vof_pcg_geometry(true);},
           "mutually exclusive geometry modes");
    reject([&]{water.set_pcg_pressure_projection(false);},
           "PCG cannot be turned off underneath planar mode");
    // The nominal 3D VOF field is a cut plane; its analytic known
    // normals are supplied independently, NOT inferred by a heuristic.
    const double divergence=water.step_verified(.0001,2000,1e-7);
    check(divergence<=1e-7,"actual live MAC oblique pressure solve");
    check(water.last_planar_vof_resolved_faces()>0,
          "real live planar pressure distances applied");
    check(water.last_planar_vof_oblique_faces()>0,
          "real live oblique pressure distances applied");
    check(water.last_planar_vof_two_partial_faces()>0,
          "real two-partial-cell planar interface used");
    near(water.mass_kg(),mass,1e-6,"live oblique water mass conserved");
    check(water.last_pcg_projection_iterations()>0,
          "real live oblique Jacobi-PCG iterations");
    // The same geometry must be valid in the REAL optional
    // post-advection projection too, on a microscopic timestep
    // that leaves the manufactured plane effectively unchanged.
    upc::CoupledMacWater3D reprojected(n,n,n,.1,1000);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=(z*n+y)*n+x;
        if(s.fill[i]>0)reprojected.set_water(x,y,z,s.fill[i]);
    }
    reprojected.set_pcg_pressure_projection(true);
    reprojected.set_planar_vof_pcg_normals(s.normals);
    reprojected.set_planar_vof_pcg_geometry(true);
    const auto div_post=reprojected.step_verified_reprojected(
        .00000001,2000,1e-7);
    near(reprojected.mass_kg(),mass,1e-6,
         "live post-transport planar VOF conservative mass");
    check(div_post<=1e-7,
          "live post-transport oblique interface projected divergence");
    check(reprojected.last_pcg_projection_iterations()>0,
          "live post-transport oblique pressure projection ran");

    // Deliberately inconsistent caller normals must be rejected
    // transactionally, without publishing a partial VOF/pressure state.
    auto broken=s.normals;
    for(std::size_t i=0;i<s.fill.size();++i)if(s.fill[i]>0&&s.fill[i]<1){
        broken[i]=Vec{1,0,0};
        break;
    }
    water.set_planar_vof_pcg_normals(broken);
    const double old_mass=water.mass_kg();
    const auto old_pressure=water.pressure_pa(2,2,2);
    const auto old_iterations=water.last_pcg_projection_iterations();
    reject([&]{(void)water.step_verified(.0001,2000,1e-7);},
           "stale/inconsistent live prescribed normal must reject");
    near(water.mass_kg(),old_mass,1e-13,
         "failed live oblique step preserves exact water mass");
    near(water.pressure_pa(2,2,2),old_pressure,1e-12,
         "failed live oblique step preserves actual pressure");
    check(water.last_pcg_projection_iterations()==old_iterations,
          "failed live oblique step preserves diagnostics");
    std::cout<<"OBLIQUE_LIVE water_mass_kg="<<mass
             <<" resolved="<<water.last_planar_vof_resolved_faces()
             <<" oblique="<<water.last_planar_vof_oblique_faces()
             <<" divergence="<<divergence<<" rollback=PASS\n";
}
void negative(){
    const int n=8;
    auto slab=make_slab(n,Vec{1,1,0},6.17);
    const auto original=slab.fill;
    reject([&]{upc::p2_planar_vof_pcg_geometry3d(
        slab.fill,slab.solid,slab.normals,n,n,n,1,.1);},
        "non-half threshold must reject");
    slab.fill[2]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{upc::p2_planar_vof_pcg_geometry3d(
        slab.fill,slab.solid,slab.normals,n,n,n,1);},
        "NaN fraction rejected");
    slab.fill=original;
    const auto before=slab.fill;
    slab.normals[0][1]=std::numeric_limits<double>::infinity();
    reject([&]{upc::p2_planar_vof_pcg_geometry3d(
        slab.fill,slab.solid,slab.normals,n,n,n,1);},
        "infinite normal rejected");
    slab.normals[0][1]=1;
    slab.solid[0]=1;
    reject([&]{upc::p2_planar_vof_pcg_geometry3d(
        slab.fill,slab.solid,slab.normals,n,n,n,1);},
        "solid contains water rejected");
    slab.solid[0]=0;
    check(slab.fill==before,"invalid test does not touch real VOF input");
    upc::MacProjection3D mac(n,n,n,1.);
    auto geometry=upc::p2_planar_vof_pcg_geometry3d(
        slab.fill,slab.solid,slab.normals,n,n,n,1);
    upc::p2_apply_planar_vof_pcg_geometry3d(mac,geometry);
    const auto old=mac;
    geometry.resolved[0].x=n+1;
    reject([&]{upc::p2_apply_planar_vof_pcg_geometry3d(mac,geometry);},
           "out-of-grid face rejected without changing MAC state");
    // Compare deep 3D MAC pressure solve, ensuring cloned state is identical.
    const auto pa=mac.project_pcg_verified(.1,300,1e-7);
    auto untouched=old;
    const auto pb=untouched.project_pcg_verified(.1,300,1e-7);
    near(pa.max_divergence_s_inv,pb.max_divergence_s_inv,1e-13,
         "transactional planar geometry apply preserved pressure state");
}
}
int main() {
    try {
        for(int n:{8,12}) {
            manufactured(n,{1./std::sqrt(2.),1./std::sqrt(2.),0.0},
                         6.17/std::sqrt(2.));
            manufactured(n,{1./std::sqrt(3.),1./std::sqrt(3.),1./std::sqrt(3.)},
                         7.21/std::sqrt(3.));
        }
        axis_equivalence();
        contradictory_planes();
        negative();
        live_coupled();
        std::cout<<"PASS P2_PLANAR_VOF_PCG assertions="<<count
                 <<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& ex) {
        std::cerr<<"FAIL P2_PLANAR_VOF_PCG "<<ex.what()<<" count="<<count<<"\n";
        return 1;
    }
}
