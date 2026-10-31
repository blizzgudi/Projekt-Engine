#include "upc/p2_plic_periodic_sweep3d.hpp"
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
void check(bool ok,const char* why){
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why){
    ++assertions;
    if(!std::isfinite(a)||std::abs(a-b)>tol)
        throw std::runtime_error(std::string(why)+" actual="+
            std::to_string(a)+" expected="+std::to_string(b));
}
void analytic_slab(){
    using upc::p2_plic_outgoing_slab3d;
    near(p2_plic_outgoing_slab3d(.3,{1,0,0},0,1,.2),0,1e-13,"right dry");
    near(p2_plic_outgoing_slab3d(.3,{1,0,0},0,-1,.2),.2,1e-13,"left wet");
    near(p2_plic_outgoing_slab3d(.3,{-1,0,0},0,1,.2),.2,1e-13,"right wet");
    near(p2_plic_outgoing_slab3d(.3,{-1,0,0},0,-1,.2),0,1e-13,"left dry");
    // Liquid x+y<=1: right slab c^2/2, left slab c-c^2/2.
    near(p2_plic_outgoing_slab3d(.5,{1,1,0},0,1,.25),
         .03125,2e-13,"oblique right triangle");
    near(p2_plic_outgoing_slab3d(.5,{1,1,0},0,-1,.25),
         .21875,2e-13,"oblique left trapezoid");
    near(p2_plic_outgoing_slab3d(.5,{1,1,0},1,1,.25),
         .03125,2e-13,"oblique y symmetry");
    near(p2_plic_outgoing_slab3d(1,{0,0,0},2,1,.25),
         .25,0,"full slab");
    near(p2_plic_outgoing_slab3d(0,{0,0,0},2,1,.25),
         0,0,"empty slab");
    near(p2_plic_outgoing_slab3d(.5,{1,1,0},0,1,1),
         .5,0,"full width slab");
    std::cout<<"slab,oblique_right,0.03125,oblique_left,0.21875\n";
}
std::size_t idx(int x,int y,int z,int n){
    return static_cast<std::size_t>((z*n+y)*n+x);
}
void periodic(int axis,int direction){
    constexpr int n=8;
    const double dx=.1,dt=.05,velocity=direction*.5;
    std::vector<double> fill(n*n*n,0);
    std::vector<std::uint8_t> solid(n*n*n,0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x){
        const int p=axis==0?x:axis==1?y:z;
        fill[idx(x,y,z,n)]=p==2?.4:p==3||p==4?1.0:p==5?.6:0.0;
    }
    const auto before=fill;
    auto normals=upc::p2_reconstruct_vof_normals3d(fill,solid,n,n,n,dx);
    check(normals.partial_cells==2*n*n,"partial plug faces");
    auto result=upc::p2_plic_periodic_sweep3d(
        fill,normals.normals,n,n,n,dx,dt,axis,velocity);
    near(result.courant,.25,1e-14,"Courant");
    near(result.initial_volume_m3,.192,2e-13,"initial plug volume");
    near(result.final_volume_m3,.192,2e-13,"final plug volume");
    near(result.mass_residual_m3,0,2e-13,"conservative plug");
    check(result.transported_faces>0,"actual swept faces");
    double max_error=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x){
        const int p=axis==0?x:axis==1?y:z;
        const double expected=p==2?(direction>0?.15:.65):
                              p==3||p==4?1.0:
                              p==5?(direction>0?.85:.35):0.0;
        const double actual=result.water_fill[idx(x,y,z,n)];
        near(actual,expected,3e-12,"one step exact plug translation");
        max_error=std::max(max_error,std::abs(actual-expected));
    }
    check(fill==before,"input VOF unchanged");
    auto second_normals=upc::p2_reconstruct_vof_normals3d(
        result.water_fill,solid,n,n,n,dx);
    auto second=upc::p2_plic_periodic_sweep3d(
        result.water_fill,second_normals.normals,n,n,n,
        dx,dt,axis,velocity);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x){
        const int p=axis==0?x:axis==1?y:z;
        double expected=0;
        if(direction>0)
            expected=p==3?.9:p==4||p==5?1.0:p==6?.1:0.0;
        else
            expected=p==2?.9:p==3||p==4?1.0:p==5?.1:0.0;
        near(second.water_fill[idx(x,y,z,n)],expected,3e-12,
             "two steps exact plug translation");
    }
    near(second.final_volume_m3,.192,2e-13,"two step volume");
    std::cout<<"periodic,"<<axis<<","<<direction<<","
             <<result.initial_volume_m3<<","
             <<result.final_volume_m3<<","<<max_error<<","
             <<result.transported_faces<<","<<result.courant<<"\n";
}
void invalid(){
    using upc::p2_plic_outgoing_slab3d;
    auto reject_slab=[&](double f,V n,int axis,int dir,double c){
        bool caught=false;
        try{(void)p2_plic_outgoing_slab3d(f,n,axis,dir,c);}
        catch(const std::invalid_argument&){caught=true;}
        check(caught,"invalid slab must reject");
    };
    reject_slab(-.1,{1,0,0},0,1,.2);
    reject_slab(1.1,{1,0,0},0,1,.2);
    reject_slab(.5,{0,0,0},0,1,.2);
    reject_slab(.5,{1,0,0},3,1,.2);
    reject_slab(.5,{1,0,0},0,0,.2);
    reject_slab(.5,{1,0,0},0,1,1.01);
    reject_slab(.5,{1,0,0},0,1,-.01);
    reject_slab(.5,{std::numeric_limits<double>::quiet_NaN(),0,0},0,1,.2);
    std::vector<double> fill(8,.5);
    std::vector<V> normals(8,V{1,0,0});
    auto reject=[&](std::size_t nx,std::size_t ny,std::size_t nz,
                    double dx,double dt,int axis,double v){
        bool caught=false;
        try{(void)upc::p2_plic_periodic_sweep3d(
            fill,normals,nx,ny,nz,dx,dt,axis,v);}
        catch(const std::invalid_argument&){caught=true;}
        check(caught,"invalid periodic sweep must reject");
    };
    reject(1,2,4,.1,.05,0,.1);
    reject(2,2,2,0,.05,0,.1);
    reject(2,2,2,.1,.05,0,3.0);
    reject(2,2,2,.1,.05,3,.1);
    reject(2,2,2,.1,.05,0,
           std::numeric_limits<double>::infinity());
    normals[0]={0,0,0};reject(2,2,2,.1,.05,0,.1);
    normals[0]={1,0,0};fill[0]=1.1;
    reject(2,2,2,.1,.05,0,.1);
    fill[0]=.5;fill.pop_back();
    reject(2,2,2,.1,.05,0,.1);
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        analytic_slab();
        std::cout<<"case,axis,direction,initial_volume,final_volume,max_error,faces,courant\n";
        for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})periodic(axis,sign);
        invalid();
        std::cout<<"PASS: PLIC periodic sweep assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: PLIC periodic sweep assertions="<<assertions
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
