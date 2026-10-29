#include "upc/p2_static_cut_cell_vof3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
std::size_t assertions=0;
void check(bool yes,const char* why) {
    ++assertions;
    if(!yes)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
template<class F>void rejects(F fn,const char* why) {
    bool caught=false;
    try {fn();} catch(const std::invalid_argument&) {caught=true;}
    check(caught,why);
}
struct Setup {
    int n;
    upc::P2Box3D box{{3.0,3.0,3.0},{4.0,4.0,4.0}};
    std::vector<double> f,u,v,w;
    explicit Setup(int grid):n(grid),f(n*n*n),u((n+1)*n*n,0),
                             v(n*(n+1)*n,0),w(n*n*(n+1),0) {
        for(int z=0;z<n;++z)
        for(int y=0;y<n;++y)
        for(int x=0;x<n;++x)
            f[id(x,y,z)]=(x==3&&y==3&&z==3)?0.0:
                         0.15+0.01*(x+y+z);
        // Two intersecting discrete streamfunctions: an XY vortex at z=1
        // and a YZ vortex at x=1. Their exact discrete divergence is zero.
        u[ui(1,0,1)]=0.4; u[ui(1,1,1)]=-0.4;
        v[vi(0,1,1)]=-0.4; v[vi(1,1,1)]=0.4;
        v[vi(1,1,0)]+=0.25; v[vi(1,1,1)]-=0.25;
        w[wi(1,0,1)]=-0.25; w[wi(1,1,1)]=0.25;
    }
    std::size_t id(int x,int y,int z)const{return (z*n+y)*n+x;}
    std::size_t ui(int x,int y,int z)const{return (z*n+y)*(n+1)+x;}
    std::size_t vi(int x,int y,int z)const{return (z*(n+1)+y)*n+x;}
    std::size_t wi(int x,int y,int z)const{return (z*n+y)*n+x;}
    upc::P2StaticCutCellVofResult3D run(const std::vector<double>& fractions,
                                      double dt=0.1)const {
        return upc::P2StaticCutCellVof3D::step(
            n,n,n,1.0,dt,box,fractions,u,v,w);
    }
};
void run_case(int n) {
    Setup s(n);
    auto original=s.f;
    auto first=s.run(s.f);
    check(first.max_donor_cfl>0&&first.max_donor_cfl<1,"CFL bounded");
    check(first.max_cell_volume_divergence_m3<1e-13,"solenoidal 3D flux");
    double difference=0;
    for(std::size_t i=0;i<s.f.size();++i) {
        check(first.fraction_after[i]>=0&&first.fraction_after[i]<=1,
              "bounded 3D VOF");
        difference=std::max(difference,
                            std::abs(first.fraction_after[i]-s.f[i]));
    }
    check(difference>1e-4,"real 3D upwind transport");
    const double mass=first.water_before_m3;
    for(int t=0;t<40;++t) {
        auto next=s.run(s.f);
        near(next.water_before_m3,mass,1e-10,"before water volume");
        near(next.water_after_m3,mass,1e-10,"after water volume");
        for(double f:next.fraction_after)
            check(f>=0&&f<=1,"bounded after 40 time steps");
        s.f=std::move(next.fraction_after);
    }
    check(original[s.id(3,3,3)]==0&&s.f[s.id(3,3,3)]==0,
          "static solid stays dry");
    // Constant unit VOF is an exact invariant for divergence-free MAC flux.
    std::vector<double> full(original.size(),1.0);
    full[s.id(3,3,3)]=0;
    auto invariant=s.run(full);
    for(std::size_t i=0;i<full.size();++i)
        near(invariant.fraction_after[i],full[i],1e-14,
             "full-water solenoidal invariant");
    check(s.u[s.ui(1,0,1)]!=0&&s.v[s.vi(1,1,0)]!=0&&
          s.w[s.wi(1,1,1)]!=0,"all three 3D face axes active");
    std::cout<<"VOF_CASE "<<n<<" 40 "<<std::setprecision(17)<<mass
             <<" "<<invariant.water_after_m3<<" "<<difference
             <<" "<<first.max_donor_cfl<<"\n";
}
void analytic_upwind_reference() {
    Setup s(8);
    std::vector<double> isolated(s.f.size(),0.0);
    isolated[s.id(0,0,1)]=0.5;
    auto r=s.run(isolated);
    // u(1,0,1)=+0.4 m/s, dx=1, dt=0.1: 0.04 m^3 moved.
    // First-order donor VOF carries 0.04 * 0.5 = 0.02 m^3 water.
    near(r.fraction_after[s.id(0,0,1)],0.48,1e-14,
         "independent positive-u donor cell");
    near(r.fraction_after[s.id(1,0,1)],0.02,1e-14,
         "independent positive-u receiver cell");
    near(r.water_before_m3,0.5,1e-14,"isolated initial inventory");
    near(r.water_after_m3,0.5,1e-14,"isolated final inventory");
    std::cout<<"VOF_ANALYTIC 0.48 0.02 0.5\n";
}
void partial_cut_cells() {
    Setup s(8);
    s.box={{3.2,3.2,3.2},{4.8,4.8,4.8}};
    const auto geom=upc::P2MovingBoxFaceApertures3D::sample(
        8,8,8,1.0,0.1,s.box,s.box);
    int partial=0;
    for(double a:geom.cell_geometry.alpha_old)
        if(a>0&&a<1)++partial;
    check(partial>=8,"fractional 3D cut-cell volumes exercised");
    int open_faces=0;
    for(double a:geom.u_open)if(a>0&&a<1)++open_faces;
    for(double a:geom.v_open)if(a>0&&a<1)++open_faces;
    for(double a:geom.w_open)if(a>0&&a<1)++open_faces;
    check(open_faces>0,"fractional 3D face apertures exercised");
    auto first=s.run(s.f);
    near(first.water_after_m3,first.water_before_m3,1e-10,
         "fractional cut-cell water volume");
    std::vector<double> full(s.f.size(),1.0);
    auto invariant=s.run(full);
    for(std::size_t i=0;i<full.size();++i)
        near(invariant.fraction_after[i],1.0,1e-14,
             "fractional cut-cell unit VOF invariant");
    std::cout<<"VOF_PARTIAL "<<partial<<" "<<open_faces<<" "
             <<std::setprecision(17)<<first.water_before_m3<<" "
             <<first.water_after_m3<<"\n";
}
void temporal_subcycling(int n) {
    Setup s(n);
    for(double& x:s.u)x*=40;
    for(double& x:s.v)x*=40;
    for(double& x:s.w)x*=40;
    rejects([&]{s.run(s.f);},"single high-CFL step rejected");
    const auto actual=upc::P2StaticCutCellVof3D::step_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,4);
    auto manual=s.f;
    for(int i=0;i<4;++i)
        manual=s.run(manual,0.025).fraction_after;
    double error=0.0;
    for(std::size_t i=0;i<manual.size();++i) {
        error=std::max(error,std::abs(actual.fraction_after[i]-manual[i]));
        check(actual.fraction_after[i]>=0&&actual.fraction_after[i]<=1,
              "subcycled fraction bounded");
    }
    near(error,0.0,1e-14,"four substeps match manual calls");
    near(actual.water_before_m3,actual.water_after_m3,1e-10,
         "subcycled water volume conserved");
    check(actual.max_donor_cfl>0&&actual.max_donor_cfl<1,
          "subcycled CFL bounded");
    const auto one=upc::P2StaticCutCellVof3D::step_subcycled(
        n,n,n,1.0,0.0025,s.box,s.f,s.u,s.v,s.w,1);
    const auto direct=s.run(s.f,0.0025);
    check(one.fraction_after==direct.fraction_after,
          "single substep bitwise parity");
    rejects([&]{upc::P2StaticCutCellVof3D::step_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,0);},
        "zero substeps rejected");
    rejects([&]{upc::P2StaticCutCellVof3D::step_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,1025);},
        "too many substeps rejected");
    std::cout<<"VOF_SUBCYCLE "<<n<<" 4 "<<std::setprecision(17)
             <<actual.water_before_m3<<" "<<actual.water_after_m3
             <<" "<<error<<" "<<actual.max_donor_cfl<<"\n";
}
void negatives() {
    Setup s(8);
    auto bad=s;
    bad.u[bad.ui(0,0,0)]=0.1;
    rejects([&]{bad.run(bad.f);},"sealed outer boundary rejected");
    bad=s; bad.u[bad.ui(3,3,3)]=0.1;
    rejects([&]{bad.run(bad.f);},"sealed solid boundary rejected");
    bad=s; bad.u[bad.ui(1,0,1)]=0.5;
    rejects([&]{bad.run(bad.f);},"non-solenoidal flux rejected");
    bad=s;
    for(double& x:bad.u)x*=100;
    for(double& x:bad.v)x*=100;
    for(double& x:bad.w)x*=100;
    rejects([&]{bad.run(bad.f);},"donor CFL rejected");
    bad=s; bad.u[bad.ui(1,0,1)]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{bad.run(bad.f);},"nonfinite velocity rejected");
    bad=s; bad.f[bad.id(0,0,0)]=1.1;
    rejects([&]{bad.run(bad.f);},"invalid water fraction rejected");
    bad=s; bad.f.pop_back();
    rejects([&]{bad.run(bad.f);},"incorrect array size rejected");
    rejects([&]{s.run(s.f,0);},"zero dt rejected");
    std::cout<<"VOF_NEGATIVE 8\n";
}
}
int main() {
    try {
        run_case(8);
        run_case(12);
        analytic_upwind_reference();
        partial_cut_cells();
        temporal_subcycling(8);
        temporal_subcycling(12);
        negatives();
        std::cout<<"PASS: "<<assertions
                 <<" native static 3D cut-cell VOF assertions\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
