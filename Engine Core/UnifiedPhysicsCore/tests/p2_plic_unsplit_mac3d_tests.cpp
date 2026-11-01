#include "upc/p2_plic_unsplit_mac3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using D=std::array<std::size_t,3>;
using Vel=std::array<std::vector<double>,3>;
std::size_t checks=0;
void require(bool yes,const char* what){
    ++checks;
    if(!yes)throw std::runtime_error(what);
}
void near(double a,double b,double tol,const char* what){
    ++checks;
    if(!std::isfinite(a)||std::abs(a-b)>tol)
        throw std::runtime_error(std::string(what)+": "+std::to_string(a)+
                                 " expected "+std::to_string(b));
}
std::size_t id(std::size_t x,std::size_t y,std::size_t z,D d){
    return (z*d[1]+y)*d[0]+x;
}
Vel constant(D d,std::array<double,3> speed){
    Vel v;
    for(int a=0;a<3;++a)v[a].assign(d[0]*d[1]*d[2],speed[a]);
    return v;
}
std::vector<double> box(D d,D lo,D hi){
    std::vector<double> f(d[0]*d[1]*d[2],0);
    for(std::size_t z=lo[2];z<hi[2];++z)
    for(std::size_t y=lo[1];y<hi[1];++y)
    for(std::size_t x=lo[0];x<hi[0];++x)f[id(x,y,z,d)]=1;
    return f;
}
double overlap(double a,double b,double c,double d){
    return std::max(0.0,std::min(b,d)-std::max(a,c));
}
// Independently construct a periodic MAC field as the discrete curl of
// three edge potentials. Every component varies along its own axis,
// unlike the previously supported transverse-lane shears.
Vel curl_field(D d,double dx){
    const double pi=std::acos(-1.0);
    const auto wave=[&](std::size_t a,std::size_t n){
        return std::sin(2*pi*double(a)/double(n));
    };
    const auto coswave=[&](std::size_t a,std::size_t n){
        return std::cos(2*pi*double(a)/double(n));
    };
    const auto az=[&](std::size_t x,std::size_t y,std::size_t z){
        return .003*wave(x,d[0])*wave(y,d[1])*
               (1+.3*coswave(z,d[2]));
    };
    const auto ax=[&](std::size_t x,std::size_t y,std::size_t z){
        return .002*wave(y,d[1])*wave(z,d[2])*
               (1+.2*coswave(x,d[0]));
    };
    const auto ay=[&](std::size_t x,std::size_t y,std::size_t z){
        return .0025*wave(x,d[0])*wave(z,d[2])*
               (1+.2*coswave(y,d[1]));
    };
    Vel v=constant(d,{0,0,0});
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const auto i=id(x,y,z,d);
        // x-face curl(A)_x = d_y A_z - d_z A_y
        v[0][i]=(az(x+1,y+1,z)-az(x+1,y,z)
                 -ay(x+1,y,z+1)+ay(x+1,y,z))/dx;
        // y-face curl(A)_y = d_z A_x - d_x A_z
        v[1][i]=(ax(x,y+1,z+1)-ax(x,y+1,z)
                 -az(x+1,y+1,z)+az(x,y+1,z))/dx;
        // z-face curl(A)_z = d_x A_y - d_y A_x
        v[2][i]=(ay(x+1,y,z+1)-ay(x,y,z+1)
                 -ax(x,y+1,z+1)+ax(x,y,z+1))/dx;
    }
    return v;
}
void constant_translation(D d,D lo,D hi){
    const double dx=.1,dt=.05;
    const std::array<double,3> speeds{.12,-.08,.1};
    const auto input=box(d,lo,hi);
    const auto r=upc::p2_plic_unsplit_mac3d(
        input,constant(d,speeds),d[0],d[1],d[2],dx,dt);
    require(r.transported_faces>0,"translation geometric faces");
    require(r.positive_faces>0&&r.negative_faces>0,
            "translation both velocity signs");
    near(r.mass_residual_m3,0,1e-12,"translation mass");
    near(r.max_divergence_s_inv,0,1e-12,"translation divergence");
    double l1=0,volume=0;
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const D p{x,y,z};
        double exact=1;
        for(int a=0;a<3;++a)
            exact*=overlap(p[a]*dx,(p[a]+1)*dx,
                           lo[a]*dx+speeds[a]*dt,
                           hi[a]*dx+speeds[a]*dt)/dx;
        const double f=r.water_fill[id(x,y,z,d)];
        require(f>=0&&f<=1,"translation bounded");
        l1+=std::abs(f-exact);
        volume+=input[id(x,y,z,d)];
    }
    near(r.initial_volume_m3,volume*dx*dx*dx,1e-12,"box initial");
    near(r.final_volume_m3,volume*dx*dx*dx,1e-12,"box final");
    const double relative=l1/volume;
    // Simultaneous first-order face slabs miss diagonal corner transfers.
    // The independent exact translated cuboid must expose this error.
    require(relative>0&&relative<.015,"unsplit corner reference tolerance");
    std::cout<<"translation,"<<d[0]<<","<<d[1]<<","<<d[2]<<","
             <<r.initial_volume_m3<<","<<r.final_volume_m3<<","
             <<r.max_divergence_s_inv<<","<<relative<<","
             <<r.max_cell_face_courant_sum<<","<<r.transported_faces<<"\n";
}
void variable_vortex(D d){
    const double dx=.1,dt=.05;
    const auto v=curl_field(d,dx);
    // Independent discrete-divergence reference and along-axis variation.
    double max_div=0;
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const auto i=id(x,y,z,d);
        const double div=(
            v[0][i]-v[0][id((x+d[0]-1)%d[0],y,z,d)]+
            v[1][i]-v[1][id(x,(y+d[1]-1)%d[1],z,d)]+
            v[2][i]-v[2][id(x,y,(z+d[2]-1)%d[2],d)])/dx;
        max_div=std::max(max_div,std::abs(div));
    }
    require(max_div<1e-13,"curl independent divergence");
    std::array<double,3> own_axis_variation{};
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const D p{x,y,z};
        for(int a=0;a<3;++a){
            auto next=p;
            next[a]=(next[a]+1)%d[a];
            own_axis_variation[a]=std::max(
                own_axis_variation[a],
                std::abs(v[a][id(x,y,z,d)]-
                         v[a][id(next[0],next[1],next[2],d)]));
        }
    }
    for(double variation:own_axis_variation)
        require(variation>1e-6,"curl component varies along own axis");
    const std::size_t n=d[0]*d[1]*d[2];
    // Uniform full-water is an exact discrete steady solution when
    // the MAC divergence is zero; every face flux is analytic c=|u|dt/dx.
    std::vector<double> full(n,1);
    const auto full_result=upc::p2_plic_unsplit_mac3d(
        full,v,d[0],d[1],d[2],dx,dt);
    require(full_result.transported_faces==0 &&
            full_result.reconstructed_partial_cells==0,
            "pure-water fast path does not fabricate PLIC transport");
    for(double f:full_result.water_fill)
        near(f,1,2e-14,"curl full-water analytic steady solution");
    near(full_result.mass_residual_m3,0,1e-12,"curl full-water mass");
    const D lo{2,2,2},hi{d[0]-2,d[1]-2,d[2]-2};
    const auto fill=box(d,lo,hi);
    const auto r=upc::p2_plic_unsplit_mac3d(
        fill,v,d[0],d[1],d[2],dx,dt);
    require(r.transported_faces>0,"curl interface transport");
    require(r.positive_faces>0&&r.negative_faces>0,"curl both signs");
    near(r.mass_residual_m3,0,1e-12,"curl interface mass");
    require(r.water_fill!=fill,"curl actually advects interface");
    for(double f:r.water_fill)require(f>=0&&f<=1,"curl VOF bounded");
    // Reconstruct normals on the actual fractional output, not just on
    // the original binary box; test conservative multi-step evolution.
    const auto second=upc::p2_plic_unsplit_mac3d(
        r.water_fill,v,d[0],d[1],d[2],dx,dt);
    near(second.mass_residual_m3,0,1e-12,"second curl step mass");
    near(second.final_volume_m3,r.initial_volume_m3,1e-12,
         "two-step curl total volume");
    require(second.reconstructed_partial_cells>0,
            "second curl step reconstructs interface");
    for(double f:second.water_fill)
        require(f>=0&&f<=1,"two-step curl bounded");
    std::cout<<"vortex,"<<d[0]<<","<<d[1]<<","<<d[2]<<","
             <<r.initial_volume_m3<<","<<r.final_volume_m3<<","
             <<r.max_divergence_s_inv<<","<<0<<","
             <<r.max_cell_face_courant_sum<<","<<r.transported_faces<<"\n";
}
void invalid(){
    const D d{6,6,6};
    auto f=box(d,{2,2,2},{4,4,4});
    auto v=constant(d,{.1,.1,.1});
    auto reject=[&](const std::vector<double>& fill,const Vel& speed,
                    double dx,double dt){
        bool thrown=false;
        try{(void)upc::p2_plic_unsplit_mac3d(
            fill,speed,d[0],d[1],d[2],dx,dt);}
        catch(const std::invalid_argument&){thrown=true;}
        require(thrown,"invalid unsplit MAC input rejected");
    };
    reject(f,v,0,.05);
    reject(f,v,.1,0);
    v[0][0]=.2;reject(f,v,.1,.05); // nonzero discrete divergence
    v[0][0]=.1;v[1][0]=std::numeric_limits<double>::quiet_NaN();
    reject(f,v,.1,.05);
    v[1][0]=.1;v[2].pop_back();reject(f,v,.1,.05);
    v=constant(d,{1,1,1});reject(f,v,.1,.1); // six-face CFL
    v=constant(d,{.1,.1,.1});f[0]=-0.1;reject(f,v,.1,.05);
    f.assign(216,.5);reject(f,v,.1,.05); // unresolved PLIC interface
    // Three positive outgoing slabs may geometrically overlap the same
    // small corner of a fractional donor. Never silently clip a negative
    // fraction or report conservation after such a nonphysical update.
    auto corner=std::vector<double>(216,0);
    corner[id(3,3,3,d)]=.04;
    corner[id(4,3,3,d)]=1;
    corner[id(3,4,3,d)]=1;
    corner[id(3,3,4,d)]=1;
    bool overlap_rejected=false;
    try{
        (void)upc::p2_plic_unsplit_mac3d(
            corner,constant(d,{.2,.2,.2}),d[0],d[1],d[2],.1,.05);
    }catch(const std::runtime_error&){
        overlap_rejected=true;
    }
    require(overlap_rejected,"overlapping multi-axis slabs fail closed");
    const auto stopped=upc::p2_plic_unsplit_mac3d(
        f,constant(d,{0,0,0}),d[0],d[1],d[2],.1,.05);
    require(stopped.water_fill==f,"no-op does not reconstruct interface");
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"case,nx,ny,nz,initial_volume,final_volume,"
                    "max_divergence,analytic_relative_l1,"
                    "max_six_face_courant,transported_faces\n";
        constant_translation({12,10,8},{3,3,2},{7,6,5});
        constant_translation({20,16,12},{5,4,3},{11,10,8});
        variable_vortex({8,8,8});
        variable_vortex({12,10,8});
        invalid();
        std::cout<<"PASS: PLIC unsplit MAC 3D assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: PLIC unsplit MAC 3D assertions="<<checks
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
