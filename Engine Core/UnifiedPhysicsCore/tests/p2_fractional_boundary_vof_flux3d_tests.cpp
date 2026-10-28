// P2 PARTIAL: fractional moving-open-volume MAC GCL with exact references.
#include "upc/p2_fractional_boundary_vof_flux3d.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using P=std::array<int,3>;
std::size_t assertions=0;
void ck(bool b,const char* m){++assertions;if(!b)throw std::runtime_error(m);}
void eq(double a,double b,double tol,const char* m){
    ck(std::isfinite(a)&&std::abs(a-b)<=tol,m);
}
P move(P p,int axis,int sign){p[axis]+=sign;return p;}
struct Grid {
    int n;double dx=2.0,dt=0.4;
    std::vector<double> old_a,new_a,water,u,v,w;
    explicit Grid(int k):n(k),old_a(k*k*k,1),new_a(k*k*k,1),
        water(k*k*k,0),u((k+1)*k*k,0),v(k*(k+1)*k,0),
        w(k*k*(k+1),0){}
    std::size_t ci(P p)const{return (p[2]*n+p[1])*n+p[0];}
    std::size_t ui(P p)const{return (p[2]*n+p[1])*(n+1)+p[0];}
    std::size_t vi(P p)const{return (p[2]*(n+1)+p[1])*n+p[0];}
    std::size_t wi(P p)const{return (p[2]*n+p[1])*n+p[0];}
    void face(P from,P to,double swept) {
        int axis=-1,sign=0;
        for(int a=0;a<3;++a)if(from[a]!=to[a]) {
            ck(axis<0,"One face axis");axis=a;sign=to[a]-from[a];
            ck(std::abs(sign)==1,"Adjacent face");
        }
        ck(axis>=0,"Nonzero face");
        if(sign>0)++from[axis];
        const double speed=sign*swept*dx/dt;
        if(axis==0)u[ui(from)]=speed;
        if(axis==1)v[vi(from)]=speed;
        if(axis==2)w[wi(from)]=speed;
    }
    upc::FractionalBoundaryVofResult3D step()const {
        return upc::FractionalBoundaryVofFlux3D::step(
            n,n,n,dx,dt,old_a,new_a,water,u,v,w);
    }
};
template<class F>void reject(F f,const char* msg) {
    bool caught=false;
    try{f();}catch(const std::invalid_argument&){caught=true;}
    ck(caught,msg);
}
void partial(int n,int axis,int sign,double c) {
    Grid g(n);
    P p{{n/2,n/2,n/2}},q=move(p,axis,sign);
    const auto a=g.ci(p),b=g.ci(q);
    g.old_a[b]=0.5;g.new_a[a]=1.0-c;g.new_a[b]=0.5+c;
    g.water[a]=0.8;g.water[b]=0.2;
    g.face(p,q,c);
    const auto old=g.water;
    const auto out=g.step();
    eq(out.water_cell_fraction[a],0.8*(1.0-c),1e-13,"Donor saturation");
    eq(out.water_cell_fraction[b],0.2+0.8*c,1e-13,"Receiver volume");
    eq(out.initial_water_m3,8.0,1e-12,"Initial physical water");
    eq(out.final_water_m3,8.0,1e-12,"Final physical water");
    eq(out.mass_residual_m3,0.0,1e-12,"Mass closure");
    eq(out.open_volume_residual_m3,0.0,1e-12,"Open volume closure");
    eq(out.max_gcl_residual_cell_volumes,0.0,1e-13,"Local GCL");
    eq(out.max_outgoing_cell_volumes,c,1e-13,"Swept volume");
    ck(out.active_faces==1,"One physical face");
    ck(g.water==old,"Input remains unchanged");
    for(std::size_t i=0;i<old.size();++i) {
        ck(out.water_cell_fraction[i]>=0.0,"Nonnegative water");
        ck(out.water_cell_fraction[i]<=g.new_a[i]+1e-13,"Open volume bound");
        if(i!=a&&i!=b)eq(out.water_cell_fraction[i],old[i],0,"Unaffected cell");
    }
    std::cout<<"partial,"<<n<<","<<axis<<","<<sign<<","<<c<<","
             <<out.initial_water_m3<<","<<out.final_water_m3<<","
             <<out.mass_residual_m3<<","<<out.open_volume_residual_m3<<","
             <<out.water_cell_fraction[a]<<","<<out.water_cell_fraction[b]<<","
             <<out.active_faces<<","<<out.max_gcl_residual_cell_volumes<<","
             <<out.max_outgoing_cell_volumes<<"\n";
}
void newly_opened(int n,int axis,int sign,double c) {
    Grid g(n);
    P p{{n/2,n/2,n/2}},q=move(p,axis,sign);
    const auto a=g.ci(p),b=g.ci(q);
    g.old_a[b]=0;g.new_a[a]=1.0-c;g.new_a[b]=c;
    g.water[a]=0.8;g.face(p,q,c);
    const auto out=g.step();
    eq(out.water_cell_fraction[a],0.8*(1.0-c),1e-13,"Closing donor");
    eq(out.water_cell_fraction[b],0.8*c,1e-13,"Newly opened receiver");
    eq(out.mass_residual_m3,0,1e-12,"Newly opened mass");
    eq(out.max_gcl_residual_cell_volumes,0,1e-13,"Newly opened GCL");
    std::cout<<"opened,"<<n<<","<<axis<<","<<sign<<","<<c<<","
             <<out.water_cell_fraction[a]<<","
             <<out.water_cell_fraction[b]<<","<<out.mass_residual_m3<<"\n";
}
void split(int n) {
    Grid g(n);
    P p{{n/2,n/2,n/2}},q=move(p,0,1),r=move(p,1,1);
    const auto a=g.ci(p),b=g.ci(q),d=g.ci(r);
    g.old_a[b]=0.25;g.old_a[d]=0.25;
    g.new_a[a]=0.5;g.new_a[b]=0.5;g.new_a[d]=0.5;
    g.water[a]=0.6;g.water[b]=0.1;g.water[d]=0.05;
    g.face(p,q,0.25);g.face(p,r,0.25);
    const auto out=g.step();
    eq(out.water_cell_fraction[a],0.3,1e-13,"Split donor");
    eq(out.water_cell_fraction[b],0.25,1e-13,"Split receiver X");
    eq(out.water_cell_fraction[d],0.2,1e-13,"Split receiver Y");
    eq(out.initial_water_m3,6,1e-12,"Split water before");
    eq(out.final_water_m3,6,1e-12,"Split water after");
    eq(out.max_outgoing_cell_volumes,0.5,1e-13,"Split outgoing");
    eq(out.max_gcl_residual_cell_volumes,0,1e-13,"Split GCL");
    ck(out.active_faces==2,"Two split faces");
    std::cout<<"split,"<<n<<","<<out.water_cell_fraction[a]<<","
             <<out.water_cell_fraction[b]<<","
             <<out.water_cell_fraction[d]<<","<<out.mass_residual_m3<<","
             <<out.max_gcl_residual_cell_volumes<<","<<out.active_faces<<"\n";
}
void ring(int n) {
    Grid g(n);
    const P p{{2,2,2}},q{{3,2,2}},r{{3,3,2}},s{{2,3,2}};
    g.water[g.ci(p)]=1;
    for(const auto pair:std::array<std::array<P,2>,4>{{{{p,q}},{{q,r}},{{r,s}},{{s,p}}}})
        g.face(pair[0],pair[1],0.25);
    const auto out=g.step();
    eq(out.water_cell_fraction[g.ci(p)],0.75,1e-13,"Ring source");
    eq(out.water_cell_fraction[g.ci(q)],0.25,1e-13,"Ring receiver");
    eq(out.mass_residual_m3,0,1e-12,"Ring mass");
    eq(out.max_gcl_residual_cell_volumes,0,1e-13,"Ring GCL");
    ck(out.active_faces==4,"Ring four faces");
    std::cout<<"ring,"<<n<<","<<out.water_cell_fraction[g.ci(p)]
             <<","<<out.water_cell_fraction[g.ci(q)]<<","
             <<out.mass_residual_m3<<","<<out.active_faces<<"\n";
}
void invalid() {
    Grid g(8);
    const P p{{3,3,3}},q{{4,3,3}};
    const auto a=g.ci(p),b=g.ci(q);
    g.old_a[b]=0.5;g.new_a[a]=0.75;g.new_a[b]=0.75;
    g.water[a]=0.8;g.water[b]=0.2;g.face(p,q,0.25);
    const auto original=g.water;
    eq(g.step().mass_residual_m3,0,1e-12,"Valid control");
    g.new_a[b]=0.7;
    reject([&]{(void)g.step();},"Wrong GCL rejected");
    g.new_a[b]=0.75;
    g.face(p,q,1.25);
    reject([&]{(void)g.step();},"Excess swept volume rejected");
    g.face(p,q,0.25);
    g.new_a[b]=0;
    reject([&]{(void)g.step();},"Closed receiver rejected");
    g.new_a[b]=0.75;
    g.old_a[a]=0;
    reject([&]{(void)g.step();},"Closed donor rejected");
    g.old_a[a]=1;
    g.water[b]=0.6;
    reject([&]{(void)g.step();},"Overfilled old cell rejected");
    g.water[b]=0.2;
    g.u[g.ui({{0,0,0}})]=0.1;
    reject([&]{(void)g.step();},"Domain boundary rejected");
    g.u[g.ui({{0,0,0}})]=0;
    g.v[g.vi({{0,0,0}})]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{(void)g.step();},"Nonfinite face rejected");
    g.v[g.vi({{0,0,0}})]=0;
    g.new_a[b]=std::numeric_limits<double>::infinity();
    reject([&]{(void)g.step();},"Nonfinite geometry rejected");
    g.new_a[b]=0.75;
    g.w.pop_back();
    reject([&]{(void)g.step();},"MAC shape rejected");
    g.w.push_back(0);
    reject([&]{(void)upc::FractionalBoundaryVofFlux3D::step(
        g.n,g.n,g.n,g.dx,0,g.old_a,g.new_a,g.water,g.u,g.v,g.w);},
        "Zero timestep rejected");
    ck(g.water==original,"No input water mutation on failure");
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        for(int n:{8,12,16}) {
            for(int axis=0;axis<3;++axis)for(int sign:{-1,1})
                for(double c:{0.25,0.5}) {
                    partial(n,axis,sign,c);
                    newly_opened(n,axis,sign,c);
                }
            split(n);ring(n);
        }
        invalid();
        std::cout<<"PASS: P2 fractional moving-boundary VOF assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" assertions="<<assertions<<"\n";
        return 1;
    }
}
