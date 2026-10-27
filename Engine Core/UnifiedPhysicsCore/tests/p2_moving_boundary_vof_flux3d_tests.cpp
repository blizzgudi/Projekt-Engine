// P2 PARTIAL: independent exact discrete GCL + conservative moving-mask VOF.
#include "upc/p2_moving_boundary_vof_flux3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using A=std::array<int,3>;
std::size_t assertions=0;
void ck(bool b,const char* msg){++assertions;if(!b)throw std::runtime_error(msg);}
void eq(double a,double b,double tol,const char* msg){
    ck(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
A add(A p,int axis,int amount){p[axis]+=amount;return p;}
struct Grid {
    int n;
    std::vector<std::uint8_t> old_body,new_body,fixed;
    std::vector<double> fill,u,v,w;
    explicit Grid(int q):n(q),old_body(q*q*q,0),new_body(q*q*q,0),
        fixed(q*q*q,0),fill(q*q*q,0),
        u((q+1)*q*q,0),v(q*(q+1)*q,0),w(q*q*(q+1),0){}
    std::size_t ci(A p)const{return (p[2]*n+p[1])*n+p[0];}
    std::size_t ui(A p)const{return (p[2]*n+p[1])*(n+1)+p[0];}
    std::size_t vi(A p)const{return (p[2]*(n+1)+p[1])*n+p[0];}
    std::size_t wi(A p)const{return (p[2]*n+p[1])*n+p[0];}
    void face(A from,A to,double speed){
        int axis=-1,sign=0;
        for(int k=0;k<3;++k)if(from[k]!=to[k]){
            ck(axis<0,"Exactly one face axis");
            axis=k;sign=to[k]-from[k];
            ck(std::abs(sign)==1,"Adjacent face");
        }
        ck(axis>=0,"Face has direction");
        if(sign>0)++from[axis];
        if(axis==0)u[ui(from)]=sign*speed;
        if(axis==1)v[vi(from)]=sign*speed;
        if(axis==2)w[wi(from)]=sign*speed;
    }
    upc::MovingBoundaryVofFluxResult3D step(double dt=0.25)const{
        return upc::MovingBoundaryVofFlux3D::step(
            n,n,n,1.0,dt,old_body,new_body,fixed,fill,u,v,w);
    }
};
template<class F>void rejects(F fn,const char* msg){
    bool caught=false;
    try{fn();}catch(const std::invalid_argument&){caught=true;}
    ck(caught,msg);
}
void single(int n,int axis,int sign,int side){
    Grid g(n);
    const A p{{n/2,n/2,n/2}};
    const A q=add(p,axis,sign);
    const A r=add(q,side,1);
    const A s=add(p,side,1);
    g.old_body[g.ci(p)]=1;
    g.new_body[g.ci(q)]=1;
    g.fill[g.ci(q)]=0.6;
    g.fill[g.ci(r)]=0.2;
    g.fill[g.ci(s)]=0.8;
    const auto old=g.fill;
    g.face(q,r,4);g.face(r,s,4);g.face(s,p,4);
    const auto out=g.step();
    eq(out.water_fill[g.ci(q)],0,1e-13,"Entering solid dry");
    eq(out.water_fill[g.ci(r)],0.6,1e-13,"First exterior recipient");
    eq(out.water_fill[g.ci(s)],0.2,1e-13,"Second exterior recipient");
    eq(out.water_fill[g.ci(p)],0.8,1e-13,"Vacated cell fills");
    eq(out.initial_water_m3,1.6,1e-13,"Initial water");
    eq(out.final_water_m3,1.6,1e-13,"Final water");
    eq(out.mass_residual_m3,0,1e-13,"Water mass");
    eq(out.max_gcl_residual_cell_volumes,0,1e-13,"Exact discrete GCL");
    eq(out.max_outgoing_courant,1,1e-13,"Swept-volume CFL");
    ck(out.changed_solid_cells==2,"Changed solid count");
    ck(out.active_faces==3,"Three exterior swept faces");
    ck(g.fill==old,"Immutable VOF");
    for(std::size_t i=0;i<g.fill.size();++i){
        ck(out.water_fill[i]>=0&&out.water_fill[i]<=1,"Bounded VOF");
        if(g.new_body[i]||g.fixed[i])
            eq(out.water_fill[i],0,0,"New solid dry");
    }
    std::cout<<"single,"<<n<<","<<axis<<","<<sign<<","<<side
             <<","<<out.active_faces<<","<<out.changed_solid_cells
             <<","<<out.max_gcl_residual_cell_volumes
             <<","<<out.mass_residual_m3
             <<","<<out.water_fill[g.ci(r)]
             <<","<<out.water_fill[g.ci(s)]
             <<","<<out.water_fill[g.ci(p)]<<"\n";
}
void double_body(int n,int axis,int sign){
    Grid g(n);
    const A p{{n/2,n/2,n/2}};
    const A mid=add(p,axis,sign);
    const A q=add(mid,axis,sign);
    const int side=(axis+1)%3;
    const A a=add(q,side,1),b=add(mid,side,1),c=add(p,side,1);
    g.old_body[g.ci(p)]=g.old_body[g.ci(mid)]=1;
    g.new_body[g.ci(mid)]=g.new_body[g.ci(q)]=1;
    g.fill[g.ci(q)]=0.9;
    g.fill[g.ci(a)]=0.1;
    g.fill[g.ci(b)]=0.3;
    g.fill[g.ci(c)]=0.5;
    g.face(q,a,4);g.face(a,b,4);g.face(b,c,4);g.face(c,p,4);
    const auto out=g.step();
    eq(out.water_fill[g.ci(q)],0,1e-13,"Double body entering dry");
    eq(out.water_fill[g.ci(mid)],0,1e-13,"Double body overlap dry");
    eq(out.water_fill[g.ci(a)],0.9,1e-13,"Double route A");
    eq(out.water_fill[g.ci(b)],0.1,1e-13,"Double route B");
    eq(out.water_fill[g.ci(c)],0.3,1e-13,"Double route C");
    eq(out.water_fill[g.ci(p)],0.5,1e-13,"Double route vacated");
    eq(out.mass_residual_m3,0,1e-13,"Double mass");
    eq(out.max_gcl_residual_cell_volumes,0,1e-13,"Double GCL");
    ck(out.active_faces==4&&out.changed_solid_cells==2,"Double sweep topology");
    for(double f:out.water_fill)ck(f>=0&&f<=1,"Double boundedness");
    std::cout<<"double,"<<n<<","<<axis<<","<<sign
             <<","<<out.active_faces<<","<<out.mass_residual_m3
             <<","<<out.water_fill[g.ci(a)]
             <<","<<out.water_fill[g.ci(p)]<<"\n";
}
void static_ring(int n){
    Grid g(n);
    const A p{{2,2,2}},q{{3,2,2}},r{{3,3,2}},s{{2,3,2}};
    g.face(p,q,1);g.face(q,r,1);g.face(r,s,1);g.face(s,p,1);
    g.fill[g.ci(p)]=1;
    const auto out=g.step();
    eq(out.water_fill[g.ci(p)],0.75,1e-13,"Static source");
    eq(out.water_fill[g.ci(q)],0.25,1e-13,"Static downstream");
    eq(out.max_gcl_residual_cell_volumes,0,1e-13,"Static GCL");
    eq(out.mass_residual_m3,0,1e-13,"Static mass");
    ck(out.changed_solid_cells==0&&out.active_faces==4,"Static topology");
    std::cout<<"ring,"<<n<<","<<out.water_fill[g.ci(p)]
             <<","<<out.water_fill[g.ci(q)]
             <<","<<out.mass_residual_m3<<"\n";
}
void invalid(){
    Grid g(8);
    const A p{{3,3,3}},q{{4,3,3}},r{{4,4,3}},s{{3,4,3}};
    g.old_body[g.ci(p)]=1;
    g.new_body[g.ci(q)]=1;
    g.fill[g.ci(q)]=0.6;g.fill[g.ci(r)]=0.2;g.fill[g.ci(s)]=0.8;
    g.face(q,r,4);g.face(r,s,4);g.face(s,p,4);
    const auto original=g.fill;
    const auto base=g.step();
    eq(base.mass_residual_m3,0,1e-13,"Valid control");
    g.face(q,r,0);
    rejects([&]{(void)g.step();},"Missing swept volume rejected");
    g.face(q,r,8);
    rejects([&]{(void)g.step();},"Excess swept volume rejected");
    g.face(q,r,4);
    g.face(p,s,4);
    rejects([&]{(void)g.step();},"Old solid cannot donate");
    g.face(p,s,0);g.face(s,p,4);
    g.face(r,q,4);
    rejects([&]{(void)g.step();},"New solid cannot receive");
    g.face(r,q,0);g.face(q,r,4);
    g.fixed[g.ci(r)]=1;
    rejects([&]{(void)g.step();},"Fixed solid cannot pass flux");
    g.fixed[g.ci(r)]=0;
    g.u[g.ui({{0,0,0}})]=0.1;
    rejects([&]{(void)g.step();},"External boundary blocked");
    g.u[g.ui({{0,0,0}})]=0;
    g.v[g.vi({{0,0,0}})]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)g.step();},"NaN face rejected");
    g.v[g.vi({{0,0,0}})]=0;
    g.fill[g.ci(p)]=0.1;
    rejects([&]{(void)g.step();},"Old solid cannot hold water");
    g.fill=original;
    g.new_body[g.ci(p)]=1;
    rejects([&]{(void)g.step();},"Solid cell count mismatch");
    g.new_body[g.ci(p)]=0;
    g.w.pop_back();
    rejects([&]{(void)g.step();},"MAC shape mismatch");
    g.w.push_back(0);
    rejects([&]{(void)g.step(0);},"Zero timestep");
    ck(g.fill==original,"Rejected steps leave original input intact");
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        for(int n:{8,12,16}){
            for(int axis=0;axis<3;++axis)for(int sign:{-1,1}){
                for(int side=0;side<3;++side)
                    if(side!=axis)single(n,axis,sign,side);
                double_body(n,axis,sign);
            }
            static_ring(n);
        }
        invalid();
        std::cout<<"PASS: P2 moving-boundary GCL VOF assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
