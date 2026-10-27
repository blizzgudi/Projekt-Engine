// P2 PARTIAL: fixed-mask 3D MAC donor-cell VOF finite-volume flux.
#include "upc/p2_conservative_vof_flux3d.hpp"
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
std::size_t checks=0;
void ck(bool b,const char* msg){++checks;if(!b)throw std::runtime_error(msg);}
void eq(double a,double b,double tol,const char* msg){
    ck(std::isfinite(a)&&std::abs(a-b)<=tol,msg);
}
struct G {
    int n;
    std::vector<std::uint8_t> solid;
    std::vector<double> f,u,v,w;
    explicit G(int q):n(q),solid(q*q*q,0),f(q*q*q,0),
        u((q+1)*q*q,0),v(q*(q+1)*q,0),w(q*q*(q+1),0){}
    std::size_t c(A a)const{return (a[2]*n+a[1])*n+a[0];}
    std::size_t ui(A a)const{return (a[2]*n+a[1])*(n+1)+a[0];}
    std::size_t vi(A a)const{return (a[2]*(n+1)+a[1])*n+a[0];}
    std::size_t wi(A a)const{return (a[2]*n+a[1])*n+a[0];}
    void face(A a,A b,double speed){
        for(int k=0;k<3;++k)if(a[k]!=b[k]){
            int sign=b[k]-a[k];
            ck(sign==1||sign==-1,"Unit face");
            if(sign>0)++a[k];
            if(k==0)u[ui(a)]=sign*speed;
            if(k==1)v[vi(a)]=sign*speed;
            if(k==2)w[wi(a)]=sign*speed;
            return;
        }
        throw std::runtime_error("No face");
    }
    upc::ConservativeVofFluxResult3D step(double dt=0.25)const{
        return upc::ConservativeVofFlux3D::step(n,n,n,1,dt,solid,f,u,v,w);
    }
};
template<class F>void reject(F f,const char* msg){
    bool caught=false;
    try{f();}catch(const std::invalid_argument&){caught=true;}
    ck(caught,msg);
}
void ring(int n,int a,int b,int reverse){
    G g(n);
    A p{{2,2,2}},q=p,r=p,s=p;
    ++q[a];++r[a];++r[b];++s[b];
    const A cells[4]={p,q,r,s};
    for(int k=0;k<4;++k){
        int from=reverse?(4-k)%4:k;
        int to=reverse?(3-k+4)%4:(k+1)%4;
        g.face(cells[from],cells[to],1);
    }
    g.solid[g.c({{n-2,n-2,n-2}})]=1;
    g.f[g.c(p)]=1;
    auto initial=g.f;
    const auto out=g.step();
    const A next=reverse?s:q;
    eq(out.water_fill[g.c(p)],0.75,1e-14,"Donor retained 3/4");
    eq(out.water_fill[g.c(next)],0.25,1e-14,"Donor delivered 1/4");
    eq(out.max_outgoing_courant,0.25,1e-14,"CFL");
    eq(out.max_divergence_s_inv,0,1e-14,"Divergence");
    eq(out.mass_residual_m3,0,1e-14,"Mass residual");
    ck(out.active_faces==4,"Four face fluxes");
    ck(g.f==initial,"Immutable input");
    for(std::size_t i=0;i<initial.size();++i){
        ck(out.water_fill[i]>=0&&out.water_fill[i]<=1,"VOF bounds");
        if(g.solid[i])eq(out.water_fill[i],0,0,"Solid dry");
    }
    g.f=out.water_fill;
    auto two=g.step();
    eq(two.water_fill[g.c(p)],0.5625,1e-14,"Second step 9/16");
    eq(two.water_fill[g.c(next)],0.375,1e-14,"Second step 6/16");
    eq(two.mass_residual_m3,0,1e-14,"Second step mass");
    std::cout<<"ring,"<<n<<","<<a<<","<<b<<","<<reverse
             <<","<<out.active_faces<<","<<out.mass_residual_m3<<","<<out.water_fill[g.c(p)]<<","<<out.water_fill[g.c(next)]<<","<<two.water_fill[g.c(p)]<<","<<two.water_fill[g.c(next)]<<"\n";
}
void curl(int n){
    G g(n);
    const double pi=std::acos(-1.0);
    auto psi=[&](int x,int y){
        if(x==0||x==n||y==0||y==n)return 0.0;
        double sx=std::sin(pi*x/n),sy=std::sin(pi*y/n);
        return 0.2*sx*sx*sy*sy;
    };
    for(int z=1;z<n-1;++z){
        for(int y=0;y<n;++y)for(int x=0;x<=n;++x)
            g.u[g.ui({{x,y,z}})]=psi(x,y+1)-psi(x,y);
        for(int y=0;y<=n;++y)for(int x=0;x<n;++x)
            g.v[g.vi({{x,y,z}})]=-(psi(x+1,y)-psi(x,y));
    }
    g.solid[g.c({{n/2,n/2,0}})]=1;
    double before=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x){
        auto i=g.c({{x,y,z}});
        if(!g.solid[i])g.f[i]=0.1+0.8*((x+3*y+7*z)%17)/16.;
        before+=g.f[i];
    }
    const auto out=g.step(0.2);
    eq(out.max_divergence_s_inv,0,1e-12,"Curl divergence");
    ck(out.max_outgoing_courant<1,"Curl CFL");
    ck(out.active_faces>static_cast<std::size_t>(n*n),"3D active faces");
    double after=0,diff=0;
    for(std::size_t i=0;i<g.f.size();++i){
        const double f=out.water_fill[i];
        ck(f>=0&&f<=1,"3D bounds");
        if(g.solid[i])eq(f,0,0,"Obstacle dry");
        after+=f;diff+=std::abs(f-g.f[i]);
    }
    eq(after,before,1e-9,"3D water conserved");
    ck(diff>1e-4,"Nontrivial advection");
    std::cout<<"curl,"<<n<<","<<out.active_faces<<","
             <<out.max_outgoing_courant<<","<<out.mass_residual_m3<<"\n";
}
void invalid(){
    G g(6);
    A p{{2,2,2}},q{{3,2,2}},r{{3,3,2}},s{{2,3,2}};
    g.face(p,q,1);g.face(q,r,1);g.face(r,s,1);g.face(s,p,1);
    g.f[g.c(p)]=1;
    const auto original=g.f;
    reject([&]{(void)g.step(1.01);},"CFL reject");
    reject([&]{(void)g.step(0);},"Timestep reject");
    g.u[g.ui(q)]=2;
    reject([&]{(void)g.step();},"Divergence reject");
    g.u[g.ui(q)]=1;
    g.u[g.ui({{0,0,0}})]=0.1;
    reject([&]{(void)g.step();},"Domain flux reject");
    g.u[g.ui({{0,0,0}})]=0;
    g.solid[g.c(q)]=1;
    reject([&]{(void)g.step();},"Solid flux reject");
    g.solid[g.c(q)]=0;
    g.f[g.c(p)]=1.01;
    reject([&]{(void)g.step();},"VOF bound reject");
    g.f=original;
    g.v[g.vi({{0,0,0}})]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{(void)g.step();},"NaN reject");
    g.v[g.vi({{0,0,0}})]=0;
    g.w.pop_back();
    reject([&]{(void)g.step();},"Array length reject");
    ck(g.f==original,"Rejected step does not mutate input");
}
}
int main(){
    try{
        std::cout<<std::setprecision(17);
        for(int n:{8,12,16}){
            for(int a=0;a<3;++a)for(int b=a+1;b<3;++b)
                for(int r:{0,1})ring(n,a,b,r);
            curl(n);
        }
        invalid();
        std::cout<<"PASS: P2 conservative 3D MAC VOF flux assertions="
                 <<checks<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL: "<<e.what()<<" checks="<<checks<<"\n";
        return 1;
    }
}
