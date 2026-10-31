#include "upc/p2_vof_normal_reconstruction3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using V=std::array<double,3>;
int assertions=0;
void check(bool ok,const char* msg) {
    ++assertions;
    if(!ok)throw std::runtime_error(msg);
}
void near(double actual,double expected,double tol,const char* msg) {
    ++assertions;
    if(!std::isfinite(actual)||std::abs(actual-expected)>tol)
        throw std::runtime_error(std::string(msg)+" got="+
            std::to_string(actual)+" expected="+std::to_string(expected));
}
std::size_t idx(int x,int y,int z,int n) {
    return static_cast<std::size_t>((z*n+y)*n+x);
}
V unit(V a) {
    const double norm=std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
    return {a[0]/norm,a[1]/norm,a[2]/norm};
}
double dot(V a,V b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
void affine(int n) {
    const double dx=.025,rho=1000.0;
    const std::size_t count=static_cast<std::size_t>(n*n*n);
    std::vector<double> fill(count);
    std::vector<std::uint8_t> solid(count,0);
    const V slope{.018,-.014,.011};
    const V expected=unit(slope);
    double expected_mass=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const double f=.5-(x-(n-1)*.5)*slope[0]
                           -(y-(n-1)*.5)*slope[1]
                           -(z-(n-1)*.5)*slope[2];
        check(f>0.0&&f<1.0,"affine bounded");
        fill[idx(x,y,z,n)]=f;
        expected_mass+=f*rho*dx*dx*dx;
    }
    const auto before=fill;
    const auto field=upc::p2_reconstruct_vof_normals3d(
        fill,solid,n,n,n,dx);
    check(field.partial_cells==count,"affine partial count");
    check(field.one_sided_axis_estimates==6*static_cast<std::size_t>(n*n),
          "affine boundary differences");
    for(V v:field.normals) {
        for(int a=0;a<3;++a)near(v[a],expected[a],2e-13,
                                  "affine exact gradient");
        near(dot(v,v),1,2e-13,"affine unit");
    }
    const V origin{-.17,.11,.09};
    const auto moments=upc::p2_vof_reconstructed_plic_geometry3d(
        fill,solid,n,n,n,dx,rho,origin);
    near(moments.geometry.mass_kg,expected_mass,3e-11,"affine mass");
    check(moments.reconstructed_partial_cells==count,
          "affine bridge count");
    check(moments.one_sided_axis_estimates==
          field.one_sided_axis_estimates,"affine bridge boundary count");
    const auto moved=upc::p2_vof_reconstructed_plic_geometry3d(
        fill,solid,n,n,n,dx,rho,{.02,-.08,.31});
    for(int a=0;a<3;++a) {
        near(moved.geometry.centre_of_mass_m[a],
             moments.geometry.centre_of_mass_m[a],3e-12,
             "affine origin invariant centroid");
        for(int c=0;c<3;++c)
            near(moved.geometry.inertia_about_com_kg_m2[a][c],
                 moments.geometry.inertia_about_com_kg_m2[a][c],
                 4e-10,"affine origin invariant inertia");
    }
    check(fill==before,"affine read only");
    std::cout<<"affine,"<<n<<","<<moments.geometry.mass_kg<<","
             <<field.normals[0][0]<<","<<field.normals[0][1]<<","
             <<field.normals[0][2]<<","<<field.partial_cells<<","
             <<field.one_sided_axis_estimates<<"\n";
}
void sharp_plane() {
    const int n=12;
    const double dx=.025,rho=1000.0;
    const V normal=unit({.8,.5,-.3});
    const double cut=dot(normal,V{n*.5,n*.5,n*.5});
    const auto count=static_cast<std::size_t>(n*n*n);
    std::vector<double> fill(count,0.0);
    std::vector<std::uint8_t> solid(count,0);
    double mass=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const double local=cut-dot(normal,V{
            static_cast<double>(x),static_cast<double>(y),
            static_cast<double>(z)});
        const double f=std::clamp(
            upc::p2_oblique_detail::clip_cube(normal,local).volume,
            0.0,1.0);
        // Treat roundoff at exactly dry/wet as pure phases.
        fill[idx(x,y,z,n)]=f<1e-12?0.0:f>1-1e-12?1.0:f;
        mass+=fill[idx(x,y,z,n)]*rho*dx*dx*dx;
    }
    auto field=upc::p2_reconstruct_vof_normals3d(
        fill,solid,n,n,n,dx);
    check(field.partial_cells>30,"sharp plane many partials");
    double sum_alignment=0,min_alignment=1;
    for(std::size_t i=0;i<count;++i) {
        if(fill[i]==0||fill[i]==1) {
            near(dot(field.normals[i],field.normals[i]),0,0,
                 "dry/full zero normal");
            continue;
        }
        const double alignment=dot(field.normals[i],normal);
        check(alignment>0.80,"sharp plane normal orientation");
        sum_alignment+=alignment;
        min_alignment=std::min(min_alignment,alignment);
    }
    check(sum_alignment/field.partial_cells>0.96,
          "sharp plane mean alignment");
    auto geom=upc::p2_vof_reconstructed_plic_geometry3d(
        fill,solid,n,n,n,dx,rho);
    near(geom.geometry.mass_kg,mass,3e-11,"sharp plane mass");
    check(geom.geometry.partial_cells==field.partial_cells,
          "sharp plane geometry count");
    std::cout<<"plane,"<<n<<","<<geom.geometry.mass_kg<<","
             <<normal[0]<<","<<normal[1]<<","<<normal[2]<<","
             <<field.partial_cells<<","<<field.one_sided_axis_estimates
             <<"\n";
    std::cout<<"plane_alignment,"<<min_alignment<<","
             <<sum_alignment/field.partial_cells<<"\n";
}
void masked_one_sided() {
    const int n=3;
    const V slope{.02,.03,-.01};
    std::vector<double> fill(27);
    std::vector<std::uint8_t> solid(27,0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        fill[idx(x,y,z,n)]=.5-(x-1)*slope[0]-
                                  (y-1)*slope[1]-
                                  (z-1)*slope[2];
    solid[idx(2,1,1,n)]=1;
    fill[idx(2,1,1,n)]=0;
    auto field=upc::p2_reconstruct_vof_normals3d(
        fill,solid,n,n,n,.2);
    const auto center=field.normals[idx(1,1,1,n)];
    const auto expected=unit(slope);
    for(int a=0;a<3;++a)
        near(center[a],expected[a],1e-13,"masked one-sided");
    check(field.one_sided_axis_estimates>0,"masked boundary count");
    auto moments=upc::p2_vof_reconstructed_plic_geometry3d(
        fill,solid,n,n,n,.2,1000);
    check(moments.geometry.mass_kg>0,"masked geometry mass");
    std::cout<<"masked,3,"<<moments.geometry.mass_kg<<","
             <<center[0]<<","<<center[1]<<","<<center[2]<<","
             <<field.partial_cells<<","<<field.one_sided_axis_estimates
             <<"\n";
}
void invalid() {
    std::vector<double> f(27,.5);
    std::vector<std::uint8_t> solid(27,0);
    auto reject=[&](const char* name) {
        bool caught=false;
        try{(void)upc::p2_reconstruct_vof_normals3d(
            f,solid,3,3,3,.2);}
        catch(const std::invalid_argument&){caught=true;}
        check(caught,name);
    };
    reject("flat partial no orientation");
    f[0]=1.01;reject("overfilled");
    f[0]=-.01;reject("negative");
    f[0]=std::numeric_limits<double>::quiet_NaN();reject("nan");
    f[0]=.5;solid[0]=1;reject("solid overlap");
    f[0]=0;solid[0]=2;reject("invalid mask");
    solid[0]=0;f.pop_back();reject("size mismatch");
    f.assign(27,0.0);
    auto empty=upc::p2_reconstruct_vof_normals3d(
        f,solid,3,3,3,.2);
    check(empty.partial_cells==0,"empty normal field");
    for(auto v:empty.normals)for(double c:v)
        near(c,0,0,"empty normal zero");
    f.assign(27,1.0);
    auto full=upc::p2_vof_reconstructed_plic_geometry3d(
        f,solid,3,3,3,.2,1000);
    near(full.geometry.mass_kg,216,1e-11,"full volume");
    bool rejected=false;
    try{(void)upc::p2_reconstruct_vof_normals3d(
        f,solid,3,3,3,0);}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"zero dx");
    rejected=false;
    try{(void)upc::p2_reconstruct_vof_normals3d(
        f,solid,std::numeric_limits<std::size_t>::max(),3,3,.2);}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"overflow dims");
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"case,grid,mass,normal_x,normal_y,normal_z,partial,one_sided\n";
        affine(8);affine(12);sharp_plane();masked_one_sided();invalid();
        std::cout<<"PASS: reconstructed VOF normals assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: reconstructed VOF normals assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
