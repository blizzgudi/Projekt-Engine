#include "upc/p2_plic_strang_periodic3d.hpp"
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
void require(bool ok,const char* message){
    ++checks;
    if(!ok)throw std::runtime_error(message);
}
void near(double got,double want,double tolerance,const char* message){
    ++checks;
    if(!std::isfinite(got)||std::abs(got-want)>tolerance)
        throw std::runtime_error(std::string(message)+" got="+
                                 std::to_string(got)+" want="+std::to_string(want));
}
std::size_t id(std::size_t x,std::size_t y,std::size_t z,D d){
    return (z*d[1]+y)*d[0]+x;
}
Vel constant(D d,const std::array<double,3>& v){
    Vel f;
    for(int a=0;a<3;++a)f[a].assign(d[0]*d[1]*d[2],v[a]);
    return f;
}
double overlap(double left,double right,double lo,double hi){
    return std::max(0.0,std::min(right,hi)-std::max(left,lo));
}
void box_translation(D d,D lo,D hi){
    const double dx=.1,dt=.05;
    const std::array<double,3> velocity{.2,-.1,.15};
    auto speeds=constant(d,velocity);
    std::vector<double> fill(d[0]*d[1]*d[2],0);
    for(std::size_t z=lo[2];z<hi[2];++z)
    for(std::size_t y=lo[1];y<hi[1];++y)
    for(std::size_t x=lo[0];x<hi[0];++x)fill[id(x,y,z,d)]=1;
    const auto original=fill;
    const auto result=upc::p2_plic_strang_periodic3d(
        fill,speeds,d[0],d[1],d[2],dx,dt);
    require(result.active_stages==5,"all Strang stages active");
    require(result.reconstructed_partial_cells>0,"reconstruction executed");
    require(result.transported_faces>0,"nonzero geometric transfers");
    near(result.max_stage_courant,.075,1e-14,"stage CFL");
    near(result.mass_residual_m3,0,1e-12,"translated volume conserved");
    require(fill==original,"input fractions unchanged");
    double absolute_error=0,initial=0;
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const D p{x,y,z};
        double exact=1;
        for(int a=0;a<3;++a)
            exact*=overlap(p[a]*dx,(p[a]+1)*dx,
                           lo[a]*dx+velocity[a]*dt,
                           hi[a]*dx+velocity[a]*dt)/dx;
        const double f=result.water_fill[id(x,y,z,d)];
        require(f>=0&&f<=1,"bounded translated volume fraction");
        absolute_error+=std::abs(f-exact);
        initial+=fill[id(x,y,z,d)];
    }
    near(result.initial_volume_m3,initial*dx*dx*dx,1e-12,
         "initial analytic box volume");
    near(result.final_volume_m3,initial*dx*dx*dx,1e-12,
         "final analytic box volume");
    // A reconstructed PLIC normal is approximate at 3D corners. Record
    // the error against exact translated cuboid overlap, do not claim zero.
    require(absolute_error<6.0,"box translation error exceeds bounded reference");
    std::cout<<"box,"<<d[0]<<","<<d[1]<<","<<d[2]<<","
             <<result.initial_volume_m3<<","<<result.final_volume_m3<<","
             <<absolute_error<<","<<result.max_stage_courant<<","
             <<result.active_stages<<","<<result.transported_faces<<"\n";
}
void transverse_shear(D d){
    const auto n=d[0]*d[1]*d[2];
    std::vector<double> fill(n,0);
    Vel velocities=constant(d,{0,0,0});
    for(std::size_t z=0;z<d[2];++z)
    for(std::size_t y=0;y<d[1];++y)
    for(std::size_t x=0;x<d[0];++x){
        const auto i=id(x,y,z,d);
        if(x>=2&&x<d[0]-2&&y>=2&&y<d[1]-2&&z>=2&&z<d[2]-2)
            fill[i]=1;
        const std::array<int,3> v{
            int((y+2*z)%4),int((x+3*z)%4),int((2*x+y)%4)};
        const std::array<double,4> table{.2,-.15,.1,0};
        for(int a=0;a<3;++a)velocities[a][i]=table[v[a]];
    }
    const auto input=fill;
    auto result=upc::p2_plic_strang_periodic3d(
        fill,velocities,d[0],d[1],d[2],.1,.05);
    require(result.active_stages==5,"shear has five stages");
    near(result.mass_residual_m3,0,1e-12,"sheared volume conserved");
    require(result.transported_faces>0,"sheared geometric flux");
    require(fill==input,"sheared input unchanged");
    for(double f:result.water_fill)require(f>=0&&f<=1,"shear bounded");
    std::cout<<"shear,"<<d[0]<<","<<d[1]<<","<<d[2]<<","
             <<result.initial_volume_m3<<","<<result.final_volume_m3<<","
             <<0<<","<<result.max_stage_courant<<","
             <<result.active_stages<<","<<result.transported_faces<<"\n";
}
void periodic_seam(){
    const D d{12,10,8};
    for(int axis=0;axis<3;++axis){
        std::vector<double> fill(d[0]*d[1]*d[2],1);
        for(std::size_t z=0;z<d[2];++z)
        for(std::size_t y=0;y<d[1];++y)
        for(std::size_t x=0;x<d[0];++x){
            const D p{x,y,z};
            const auto a=p[axis];
            fill[id(x,y,z,d)]=a==0||a==1?.5:
                              a>=d[axis]-3?0:1;
        }
        const auto normals=upc::p2_periodic_plic_normals3d(
            fill,d[0],d[1],d[2]);
        for(std::size_t z=0;z<d[2];++z)
        for(std::size_t y=0;y<d[1];++y)
        for(std::size_t x=0;x<d[0];++x){
            const D p{x,y,z};
            if(p[axis]>1)continue;
            near(normals[id(x,y,z,d)][axis],-1,1e-14,
                 "periodic seam partial normal");
        }
        std::array<double,3> speed{0,0,0};
        speed[axis]=-.2;
        const auto result=upc::p2_plic_strang_periodic3d(
            fill,constant(d,speed),d[0],d[1],d[2],.1,.05);
        near(result.mass_residual_m3,0,1e-12,"seam mass conservation");
        require(result.active_stages==(axis==2?1u:2u),
                "single active axis stage count");
    }
}
void invalid(){
    const D d{4,4,4};
    std::vector<double> fill(64,0);
    fill[id(1,1,1,d)]=1;
    auto speeds=constant(d,{.2,-.1,.1});
    auto reject=[&](const std::vector<double>& f,const Vel& v,
                    double dx,double dt){
        bool thrown=false;
        try{(void)upc::p2_plic_strang_periodic3d(
            f,v,d[0],d[1],d[2],dx,dt);}
        catch(const std::invalid_argument&){thrown=true;}
        require(thrown,"invalid Strang input rejected");
    };
    reject(fill,speeds,0,.05);
    reject(fill,speeds,.1,0);
    speeds[0][0]=.3;reject(fill,speeds,.1,.05); // non-solenoidal x lane
    speeds[0][0]=.2;speeds[2][0]=std::numeric_limits<double>::quiet_NaN();
    reject(fill,speeds,.1,.05);
    speeds[2][0]=.1;speeds[2].assign(64,3);
    reject(fill,speeds,.1,.05); // full-stage CFL > 1
    speeds[2].assign(64,.1);
    fill[0]=1.1;reject(fill,speeds,.1,.05);
    fill.assign(64,.5);reject(fill,speeds,.1,.05); // unresolved interface
    fill.assign(64,0);speeds[1].pop_back();
    reject(fill,speeds,.1,.05);
    fill.assign(64,.5);
    auto stopped=constant(d,{0,0,0});
    const auto result=upc::p2_plic_strang_periodic3d(
        fill,stopped,d[0],d[1],d[2],.1,.05);
    require(result.active_stages==0,"zero speed is no-op");
    require(result.water_fill==fill,"zero speed preserves uniform fill");
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        std::cout<<"case,nx,ny,nz,initial_volume,final_volume,"
                    "analytic_l1_fraction_error,max_stage_courant,"
                    "active_stages,transported_faces\n";
        box_translation({12,10,8},{3,3,2},{7,6,5});
        box_translation({20,16,12},{5,4,3},{11,10,8});
        transverse_shear({12,10,8});
        transverse_shear({16,12,10});
        periodic_seam();
        invalid();
        std::cout<<"PASS: PLIC Strang 3D assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: PLIC Strang 3D assertions="<<checks
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
