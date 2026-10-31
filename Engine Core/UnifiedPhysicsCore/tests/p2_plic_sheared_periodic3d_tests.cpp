#include "upc/p2_plic_sheared_periodic3d.hpp"
#include "upc/p2_vof_normal_reconstruction3d.hpp"
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
void check(bool ok,const char* reason){
    ++assertions;
    if(!ok)throw std::runtime_error(reason);
}
void near(double actual,double expected,double tol,const char* reason){
    ++assertions;
    if(!std::isfinite(actual)||std::abs(actual-expected)>tol)
        throw std::runtime_error(std::string(reason)+" actual="+
            std::to_string(actual)+" expected="+std::to_string(expected));
}
std::size_t index(std::size_t x,std::size_t y,std::size_t z,
                  const std::array<std::size_t,3>& d){
    return (z*d[1]+y)*d[0]+x;
}
void sheared(int axis,const std::array<std::size_t,3>& d){
    const std::size_t n=d[0]*d[1]*d[2];
    std::vector<double> fill(n,0),faces(n,0);
    std::vector<std::uint8_t> solid(n,0);
    const double dx=.1,dt=.05;
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(x,y,z,d);
        const auto a=p[axis];
        fill[i]=a==2?.4:(a==3||a==4)?1:a==5?.6:0;
        const int transverse=int(p[(axis+1)%3]+2*p[(axis+2)%3])%5;
        faces[i]=transverse==0?0:transverse==1?.1:
                 transverse==2?-.2:transverse==3?.3:-.1;
    }
    const auto original=fill;
    auto normals=upc::p2_reconstruct_vof_normals3d(
        fill,solid,d[0],d[1],d[2],dx);
    auto first=upc::p2_plic_sheared_periodic3d(
        fill,normals.normals,faces,d[0],d[1],d[2],dx,dt,axis);
    near(first.max_courant,.15,1e-14,"max shear CFL");
    near(first.mass_residual_m3,0,1e-12,"first mass");
    check(first.positive_faces>0&&first.negative_faces>0,"both shear signs");
    check(first.transported_faces>0,"geometric flux nonzero");
    check(fill==original,"immutable input");
    double max_error=0;
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(x,y,z,d);
        const double c=std::abs(faces[i])*dt/dx;
        const double sign=faces[i]>0?1:faces[i]<0?-1:0;
        const auto a=p[axis];
        const double expected=a==2?.4-sign*c:
                              a==5?.6+sign*c:
                              a==3||a==4?1:0;
        near(first.water_fill[i],expected,4e-12,"shear step analytic");
        max_error=std::max(max_error,std::abs(first.water_fill[i]-expected));
    }
    auto next_normals=upc::p2_reconstruct_vof_normals3d(
        first.water_fill,solid,d[0],d[1],d[2],dx);
    // Frozen axis normals give the exact lane-wise translation reference.
    // Reconstructed normals respond to transverse shear and are only a
    // gradient approximation: do not claim second-step analytic exactness.
    auto second=upc::p2_plic_sheared_periodic3d(
        first.water_fill,normals.normals,faces,
        d[0],d[1],d[2],dx,dt,axis);
    auto reconstructed=upc::p2_plic_sheared_periodic3d(
        first.water_fill,next_normals.normals,faces,
        d[0],d[1],d[2],dx,dt,axis);
    near(reconstructed.mass_residual_m3,0,1e-12,
         "reconstructed shear mass");
    double reconstruction_error=0;
    near(second.mass_residual_m3,0,1e-12,"second mass");
    near(second.final_volume_m3,first.initial_volume_m3,1e-12,
         "two-step volume");
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const std::array<std::size_t,3> p{x,y,z};
        const auto i=index(x,y,z,d);
        const double c=std::abs(faces[i])*dt/dx;
        const double sign=faces[i]>0?1:faces[i]<0?-1:0;
        const auto a=p[axis];
        const double expected=a==2?.4-2*sign*c:
                              a==5?.6+2*sign*c:
                              a==3||a==4?1:0;
        near(second.water_fill[i],expected,4e-12,"two-step frozen normal");
        check(reconstructed.water_fill[i]>=0&&
              reconstructed.water_fill[i]<=1,
              "reconstructed shear bounded");
        reconstruction_error=std::max(
            reconstruction_error,
            std::abs(reconstructed.water_fill[i]-expected));
    }
    check(reconstruction_error<.08,
          "reconstructed shear approximation error unexpectedly large");
    std::cout<<"shear,"<<axis<<","<<d[0]<<","<<d[1]<<","<<d[2]
             <<","<<first.initial_volume_m3<<","
             <<first.final_volume_m3<<","<<max_error<<","
             <<first.max_courant<<","<<first.positive_faces<<","
             <<first.negative_faces<<","<<reconstruction_error<<"\n";
    // Uniform-face specialization must match the previously verified
    // periodic PLIC operator on every axis and both signs.
    for(double speed:{-.2,.2}){
        std::vector<double> constant(n,speed);
        const auto a=upc::p2_plic_sheared_periodic3d(
            fill,normals.normals,constant,d[0],d[1],d[2],dx,dt,axis);
        const auto b=upc::p2_plic_periodic_sweep3d(
            fill,normals.normals,d[0],d[1],d[2],dx,dt,axis,speed);
        for(std::size_t i=0;i<n;++i)
            near(a.water_fill[i],b.water_fill[i],2e-13,
                 "constant speed parity");
        near(a.final_volume_m3,b.final_volume_m3,2e-13,"mass parity");
    }
}
void invalid(){
    std::array<std::size_t,3> d{4,3,2};
    std::vector<double> fill(24,.5),speed(24,.2);
    std::vector<V> normals(24,V{1,0,0});
    auto reject=[&](int axis,double dx,double dt){
        bool caught=false;
        try{(void)upc::p2_plic_sheared_periodic3d(
            fill,normals,speed,d[0],d[1],d[2],dx,dt,axis);}
        catch(const std::invalid_argument&){caught=true;}
        check(caught,"invalid input must reject");
    };
    reject(3,.1,.05);reject(0,0,.05);reject(0,.1,0);
    speed[0]=.3;reject(0,.1,.05); // non-solenoidal lane
    speed[0]=.2;speed[1]=std::numeric_limits<double>::quiet_NaN();
    reject(0,.1,.05);
    speed.assign(24,3.);reject(0,.1,.05); // CFL > 1
    speed.assign(24,.2);
    fill[0]=1.1;reject(0,.1,.05);
    fill[0]=.5;normals[0]={0,0,0};reject(0,.1,.05);
    normals[0]={1,0,0};speed.pop_back();reject(0,.1,.05);
    speed.push_back(.2);fill.pop_back();reject(0,.1,.05);
    fill.push_back(.5);normals.pop_back();reject(0,.1,.05);
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"case,axis,nx,ny,nz,initial_volume,final_volume,"
                    "max_error,max_courant,positive_faces,negative_faces,"
                    "reconstruction_error\n";
        for(int axis=0;axis<3;++axis){
            sheared(axis,{8,8,8});
            sheared(axis,{12,8,6});
        }
        invalid();
        std::cout<<"PASS: PLIC sheared periodic assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: PLIC sheared periodic assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
