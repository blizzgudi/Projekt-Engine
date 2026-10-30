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
    near(r.u_water_flux_m3[s.ui(1,0,1)],0.02,1e-14,
         "analytic signed U-face water volume equals 0.02 cubic metres");
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
void automatic_subcycling(int n) {
    Setup s(n);
    for(double& x:s.u)x*=40;
    for(double& x:s.v)x*=40;
    for(double& x:s.w)x*=40;
    const auto pilot=upc::P2StaticCutCellVof3D::step(
        n,n,n,1.0,0.1/1024.0,s.box,s.f,s.u,s.v,s.w);
    const double total_cfl=pilot.max_donor_cfl*1024.0;
    const std::size_t expected=std::max<std::size_t>(
        1,static_cast<std::size_t>(std::ceil(total_cfl/0.8)));
    check(expected>1&&expected<=1024,"automatic CFL requires subcycling");
    const auto actual=upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w);
    check(actual.substeps_used==expected,"minimum geometric CFL substeps");
    check(actual.max_donor_cfl<=0.8+1e-12,"automatic donor CFL target");
    const auto manual=upc::P2StaticCutCellVof3D::step_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,expected);
    check(actual.fraction_after==manual.fraction_after,
          "automatic subcycling equals explicit identical time steps");
    near(actual.water_before_m3,actual.water_after_m3,1e-10,
         "automatic water volume conserved");
    const auto single=upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.001,s.box,s.f,s.u,s.v,s.w);
    check(single.substeps_used==1,"automatic safe time step remains single");
    check(single.fraction_after==s.run(s.f,0.001).fraction_after,
          "automatic single step exact legacy parity");
    auto bad=s;
    bad.u[bad.ui(1,0,1)]+=0.1;
    rejects([&]{upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.1,bad.box,bad.f,bad.u,bad.v,bad.w);},
        "automatic mode rejects non-solenoidal MAC velocity");
    rejects([&]{upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,0.5,2);},
        "automatic mode rejects insufficient substep budget");
    rejects([&]{upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,0.0);},
        "automatic mode rejects zero target CFL");
    rejects([&]{upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,1.01);},
        "automatic mode rejects target above one");
    rejects([&]{upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,
        std::numeric_limits<double>::quiet_NaN());},
        "automatic mode rejects nonfinite target");
    rejects([&]{upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,0.8,0);},
        "automatic mode rejects zero budget");
    rejects([&]{upc::P2StaticCutCellVof3D::step_auto_subcycled(
        n,n,n,1.0,0.1,s.box,s.f,s.u,s.v,s.w,0.8,1025);},
        "automatic mode rejects oversized budget");
    std::cout<<"VOF_AUTO "<<n<<" "<<expected<<" "
             <<std::setprecision(17)<<actual.water_before_m3<<" "
             <<actual.water_after_m3<<" "<<actual.max_donor_cfl<<"\n";
}
void velocity_sequence(int n) {
    Setup s(n);
    std::vector<std::vector<double>> us,vs,ws;
    for(int k=0;k<4;++k) {
        auto u=s.u,v=s.v,w=s.w;
        const double factor=(k%2==0?1.0:-1.0)*(k+1);
        for(double& x:u)x*=factor;
        for(double& x:v)x*=factor;
        for(double& x:w)x*=factor;
        us.push_back(std::move(u));
        vs.push_back(std::move(v));
        ws.push_back(std::move(w));
    }
    const auto actual=upc::P2StaticCutCellVof3D::step_velocity_sequence(
        n,n,n,1.0,0.2,s.box,s.f,us,vs,ws);
    auto manual=s.f;
    for(int k=0;k<4;++k)
        manual=upc::P2StaticCutCellVof3D::step(
            n,n,n,1.0,0.05,s.box,manual,us[k],vs[k],ws[k]).fraction_after;
    double error=0,difference=0;
    for(std::size_t i=0;i<manual.size();++i) {
        error=std::max(error,std::abs(manual[i]-actual.fraction_after[i]));
        difference=std::max(difference,std::abs(s.f[i]-actual.fraction_after[i]));
        check(actual.fraction_after[i]>=0&&actual.fraction_after[i]<=1,
              "time-varying VOF fractions bounded");
    }
    near(error,0,1e-14,"time-varying MAC sequence matches manual");
    check(difference>1e-5,"time-varying 3D MAC actually transports water");
    near(actual.water_before_m3,actual.water_after_m3,1e-10,
         "time-varying MAC conserves water");
    check(actual.substeps_used==4&&actual.max_donor_cfl>0&&
          actual.max_donor_cfl<=1,"time-varying MAC step diagnostics");
    const auto one=upc::P2StaticCutCellVof3D::step_velocity_sequence(
        n,n,n,1.0,0.05,s.box,s.f,{s.u},{s.v},{s.w});
    check(one.fraction_after==s.run(s.f,0.05).fraction_after,
          "one-field sequence matches legacy step");
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence(
        n,n,n,1.0,0.2,s.box,s.f,{}, {}, {});},
        "empty prescribed sequence rejected");
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence(
        n,n,n,1.0,0.2,s.box,s.f,us,{},ws);},
        "mismatched prescribed sequence rejected");
    auto bad=us;
    bad[2][s.ui(1,0,1)]+=0.1;
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence(
        n,n,n,1.0,0.2,s.box,s.f,bad,vs,ws);},
        "non-solenoidal later sequence step rejected");
    bad=us;
    bad[3][s.ui(1,0,1)]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence(
        n,n,n,1.0,0.2,s.box,s.f,bad,vs,ws);},
        "nonfinite later sequence step rejected");
    std::cout<<"VOF_SEQUENCE "<<n<<" 4 "<<std::setprecision(17)
             <<actual.water_before_m3<<" "<<actual.water_after_m3
             <<" "<<error<<" "<<difference<<" "<<actual.max_donor_cfl<<"\n";
}
void adaptive_velocity_sequence(int n) {
    Setup s(n);
    const std::vector<double> durations{0.1,0.025,0.08,0.05};
    const std::vector<double> factors{40.0,-30.0,50.0,-20.0};
    std::vector<std::vector<double>> us,vs,ws;
    for(double factor:factors) {
        auto u=s.u,v=s.v,w=s.w;
        for(double& x:u)x*=factor;
        for(double& x:v)x*=factor;
        for(double& x:w)x*=factor;
        us.push_back(std::move(u));
        vs.push_back(std::move(v));
        ws.push_back(std::move(w));
    }
    const auto actual=upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,durations,us,vs,ws);
    auto manual=s.f;
    std::size_t expected=0;
    for(std::size_t k=0;k<durations.size();++k) {
        const auto pilot=upc::P2StaticCutCellVof3D::step(
            n,n,n,1.0,durations[k]/1024.0,s.box,manual,
            us[k],vs[k],ws[k]);
        const auto steps=std::max<std::size_t>(1,static_cast<std::size_t>(
            std::ceil(pilot.max_donor_cfl*1024.0/0.8)));
        check(steps>=1&&steps<=1024,"independent per-slice CFL count");
        expected+=steps;
        manual=upc::P2StaticCutCellVof3D::step_subcycled(
            n,n,n,1.0,durations[k],s.box,manual,
            us[k],vs[k],ws[k],steps).fraction_after;
    }
    check(expected>durations.size()&&expected<=1024,
          "unequal time slices require adaptive substeps");
    check(actual.substeps_used==expected,"shared adaptive work count");
    check(actual.fraction_after==manual,
          "adaptive sequence exactly matches independent manual integration");
    near(actual.water_before_m3,actual.water_after_m3,1e-10,
         "adaptive unequal-slice volume conserved");
    check(actual.max_donor_cfl>0&&actual.max_donor_cfl<=0.8+1e-12,
          "adaptive unequal-slice donor CFL target");
    for(double x:actual.fraction_after)
        check(x>=0&&x<=1,"adaptive VOF bounded on all cells");
    const auto one=upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,{0.001},{s.u},{s.v},{s.w});
    check(one.substeps_used==1&&
          one.fraction_after==s.run(s.f,0.001).fraction_after,
          "one safe slice retains exact legacy parity");
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,{}, {}, {}, {});},
        "empty adaptive sequence rejected");
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,durations,us,vs,{});},
        "mismatched adaptive sequence rejected");
    auto bad_times=durations;
    bad_times[2]=0.0;
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,bad_times,us,vs,ws);},
        "zero later slice duration rejected");
    bad_times[2]=std::numeric_limits<double>::infinity();
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,bad_times,us,vs,ws);},
        "nonfinite later slice duration rejected");
    auto bad_u=us;
    bad_u[2][s.ui(1,0,1)]+=0.1;
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,durations,bad_u,vs,ws);},
        "non-solenoidal later slice rejected");
    bad_u=us;
    bad_u[2][s.ui(1,0,1)]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,durations,bad_u,vs,ws);},
        "nonfinite later velocity rejected");
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,durations,us,vs,ws,0.8,expected-1);},
        "insufficient shared substep budget rejected");
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,durations,us,vs,ws,0.8,0);},
        "zero shared substep budget rejected");
    rejects([&]{upc::P2StaticCutCellVof3D::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,durations,us,vs,ws,1.01);},
        "invalid adaptive target CFL rejected");
    std::cout<<"VOF_ADAPTIVE "<<n<<" "<<expected<<" "
             <<std::setprecision(17)<<actual.water_before_m3<<" "
             <<actual.water_after_m3<<" "<<actual.max_donor_cfl<<"\n";
}
// Independently reconstruct each cell's final water inventory from signed
// face-integrated VOF fluxes. This tests the ledger, not a momentum equation.
void face_water_flux_ledger(int n) {
    Setup s(n);
    using Vof=upc::P2StaticCutCellVof3D;
    std::vector<upc::P2StaticCutCellVofResult3D> cases;
    cases.push_back(s.run(s.f));
    cases.push_back(Vof::step_subcycled(
        n,n,n,1.0,0.2,s.box,s.f,s.u,s.v,s.w,4));
    cases.push_back(Vof::step_velocity_sequence(
        n,n,n,1.0,0.2,s.box,s.f,{s.u,s.u},{s.v,s.v},{s.w,s.w}));
    cases.push_back(Vof::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,{0.08,0.04},{s.u,s.u},
        {s.v,s.v},{s.w,s.w}));
    if(n==8) {
        Setup partial(n);
        partial.box={{3.2,3.2,3.2},{4.8,4.8,4.8}};
        cases.push_back(partial.run(partial.f));
    }
    double maximum_error=0,maximum_boundary=0;
    int positive=0,negative=0;
    for(std::size_t c=0;c<cases.size();++c) {
        const auto& r=cases[c];
        Setup geometry_source=s;
        if(c==4)geometry_source.box={{3.2,3.2,3.2},{4.8,4.8,4.8}};
        const auto geom=upc::P2MovingBoxFaceApertures3D::sample(
            n,n,n,1.0,0.1,geometry_source.box,geometry_source.box);
        check(r.u_water_flux_m3.size()==s.u.size()&&
              r.v_water_flux_m3.size()==s.v.size()&&
              r.w_water_flux_m3.size()==s.w.size(),
              "VOF signed face ledger has correct MAC shape");
        for(double x:r.u_water_flux_m3) {
            check(std::isfinite(x),"finite U water face flux");
            positive+=(x>1e-15); negative+=(x< -1e-15);
        }
        for(double x:r.v_water_flux_m3) {
            check(std::isfinite(x),"finite V water face flux");
            positive+=(x>1e-15); negative+=(x< -1e-15);
        }
        for(double x:r.w_water_flux_m3) {
            check(std::isfinite(x),"finite W water face flux");
            positive+=(x>1e-15); negative+=(x< -1e-15);
        }
        for(int z=0;z<n;++z)
        for(int y=0;y<n;++y)
        for(int x=0;x<n;++x) {
            const auto i=s.id(x,y,z);
            const double div=
                r.u_water_flux_m3[s.ui(x+1,y,z)]-
                r.u_water_flux_m3[s.ui(x,y,z)]+
                r.v_water_flux_m3[s.vi(x,y+1,z)]-
                r.v_water_flux_m3[s.vi(x,y,z)]+
                r.w_water_flux_m3[s.wi(x,y,z+1)]-
                r.w_water_flux_m3[s.wi(x,y,z)];
            const double capacity=geom.cell_geometry.alpha_old[i];
            const double expected=capacity*geometry_source.f[i]-div;
            const double actual=capacity*r.fraction_after[i];
            const double error=std::abs(expected-actual);
            maximum_error=std::max(maximum_error,error);
            near(actual,expected,1e-12,
                 "independent cellwise finite-volume face ledger");
        }
        for(int z=0;z<n;++z)
        for(int y=0;y<n;++y) {
            maximum_boundary=std::max(maximum_boundary,
                std::abs(r.u_water_flux_m3[s.ui(0,y,z)]));
            maximum_boundary=std::max(maximum_boundary,
                std::abs(r.u_water_flux_m3[s.ui(n,y,z)]));
        }
        for(int z=0;z<n;++z)
        for(int x=0;x<n;++x) {
            maximum_boundary=std::max(maximum_boundary,
                std::abs(r.v_water_flux_m3[s.vi(x,0,z)]));
            maximum_boundary=std::max(maximum_boundary,
                std::abs(r.v_water_flux_m3[s.vi(x,n,z)]));
        }
        for(int y=0;y<n;++y)
        for(int x=0;x<n;++x) {
            maximum_boundary=std::max(maximum_boundary,
                std::abs(r.w_water_flux_m3[s.wi(x,y,0)]));
            maximum_boundary=std::max(maximum_boundary,
                std::abs(r.w_water_flux_m3[s.wi(x,y,n)]));
        }
    }
    check(positive>0&&negative>0,"signed water transport in both directions");
    near(maximum_boundary,0.0,1e-14,"closed basin has no boundary water flux");
    std::cout<<"VOF_FACE_LEDGER "<<n<<" "<<cases.size()<<" "
             <<std::setprecision(17)<<maximum_error<<" "
             <<maximum_boundary<<" "<<positive<<" "<<negative<<"\n";
}
// Passive tracer is carried by the same signed WATER flux, not by the
// full geometric volume flux. Independently audit the solute inventory.
void passive_tracer(int n) {
    Setup s(n);
    std::vector<double> c(s.f.size(),0.0);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        if(s.f[s.id(x,y,z)]>0)
            c[s.id(x,y,z)]=0.2+0.01*(x+2*y+3*z);
    using Vof=upc::P2StaticCutCellVof3D;
    const auto initial=Vof::step_passive_tracer(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w);
    const auto plain=s.run(s.f);
    check(initial.u_tracer_flux_kg.size()==s.u.size()&&
          initial.v_tracer_flux_kg.size()==s.v.size()&&
          initial.w_tracer_flux_kg.size()==s.w.size(),
          "solute ledger matches 3D MAC face topology");
    check(initial.water.fraction_after==plain.fraction_after,
          "tracer opt-in does not change water step");
    double error=0.0,ledger_error=0.0;
    const auto transported=[&](double q,std::size_t a,std::size_t b) {
        return q*(q>=0?c[a]:c[b]);
    };
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        const auto& r=initial.water;
        const double east=x+1<n?transported(
            r.u_water_flux_m3[s.ui(x+1,y,z)],i,s.id(x+1,y,z)):0;
        const double west=x>0?transported(
            r.u_water_flux_m3[s.ui(x,y,z)],s.id(x-1,y,z),i):0;
        const double north=y+1<n?transported(
            r.v_water_flux_m3[s.vi(x,y+1,z)],i,s.id(x,y+1,z)):0;
        const double south=y>0?transported(
            r.v_water_flux_m3[s.vi(x,y,z)],s.id(x,y-1,z),i):0;
        const double top=z+1<n?transported(
            r.w_water_flux_m3[s.wi(x,y,z+1)],i,s.id(x,y,z+1)):0;
        const double bottom=z>0?transported(
            r.w_water_flux_m3[s.wi(x,y,z)],s.id(x,y,z-1),i):0;
        const auto& tr=initial;
        const double te=x+1<n?tr.u_tracer_flux_kg[s.ui(x+1,y,z)]:0;
        const double tw=x>0?tr.u_tracer_flux_kg[s.ui(x,y,z)]:0;
        const double tn=y+1<n?tr.v_tracer_flux_kg[s.vi(x,y+1,z)]:0;
        const double ts=y>0?tr.v_tracer_flux_kg[s.vi(x,y,z)]:0;
        const double tt=z+1<n?tr.w_tracer_flux_kg[s.wi(x,y,z+1)]:0;
        const double tb=z>0?tr.w_tracer_flux_kg[s.wi(x,y,z)]:0;
        ledger_error=std::max(ledger_error,std::abs(east-te));
        ledger_error=std::max(ledger_error,std::abs(west-tw));
        ledger_error=std::max(ledger_error,std::abs(north-tn));
        ledger_error=std::max(ledger_error,std::abs(south-ts));
        ledger_error=std::max(ledger_error,std::abs(top-tt));
        ledger_error=std::max(ledger_error,std::abs(bottom-tb));
        const double expected=s.f[i]*c[i]-
            (te-tw+tn-ts+tt-tb);
        const double actual=r.fraction_after[i]*
            initial.concentration_after_kg_m3[i];
        error=std::max(error,std::abs(actual-expected));
        near(actual,expected,1e-12,"independent solute face balance");
    }
    near(ledger_error,0.0,1e-14,"independent signed tracer ledger reference");
    const double mass=initial.tracer_before_kg;
    for(int t=0;t<40;++t) {
        auto next=Vof::step_passive_tracer(
            n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w);
        near(next.tracer_before_kg,mass,1e-10,
             "passive tracer mass before each step");
        near(next.tracer_after_kg,mass,1e-10,
             "passive tracer mass after each step");
        for(std::size_t i=0;i<c.size();++i) {
            check(next.concentration_after_kg_m3[i]>=0.0&&
                  next.concentration_after_kg_m3[i]<=
                      0.2+0.01*(n-1)*6+1e-12,
                  "passive tracer concentration bounded");
        }
        s.f=std::move(next.water.fraction_after);
        c=std::move(next.concentration_after_kg_m3);
    }
    check(c[s.id(3,3,3)]==0.0,
          "solid cell cannot accumulate dissolved tracer");
    if(n==8) {
        Setup analytic(n);
        std::vector<double> isolated(analytic.f.size(),0.0);
        std::vector<double> concentration(analytic.f.size(),0.0);
        isolated[analytic.id(0,0,1)]=0.5;
        concentration[analytic.id(0,0,1)]=3.0;
        const auto r=Vof::step_passive_tracer(
            n,n,n,1.0,0.1,analytic.box,isolated,concentration,
            analytic.u,analytic.v,analytic.w);
        near(r.tracer_before_kg,1.5,1e-14,"analytic tracer initial kg");
        near(r.tracer_after_kg,1.5,1e-14,"analytic tracer conserved kg");
        near(r.concentration_after_kg_m3[analytic.id(0,0,1)],3.0,1e-14,
             "analytic donor concentration");
        near(r.concentration_after_kg_m3[analytic.id(1,0,1)],3.0,1e-14,
             "analytic receiving concentration");
        near(r.u_tracer_flux_kg[analytic.ui(1,0,1)],
             0.06,1e-14,"analytic signed U-face solute mass");
        near(r.water.u_water_flux_m3[analytic.ui(1,0,1)]*3.0,
             0.06,1e-14,"analytic transported tracer mass");
        std::cout<<"VOF_TRACER_ANALYTIC 1.5 0.06 3\n";
        auto bad=concentration;
        bad[analytic.id(0,0,0)]=1.0;
        rejects([&]{Vof::step_passive_tracer(
            n,n,n,1.0,0.1,analytic.box,isolated,bad,
            analytic.u,analytic.v,analytic.w);},
            "dry-cell tracer rejected");
        bad=concentration;bad[analytic.id(0,0,1)]=-1.0;
        rejects([&]{Vof::step_passive_tracer(
            n,n,n,1.0,0.1,analytic.box,isolated,bad,
            analytic.u,analytic.v,analytic.w);},
            "negative tracer rejected");
        bad=concentration;
        bad[analytic.id(0,0,1)]=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{Vof::step_passive_tracer(
            n,n,n,1.0,0.1,analytic.box,isolated,bad,
            analytic.u,analytic.v,analytic.w);},
            "nonfinite tracer rejected");
        bad.pop_back();
        rejects([&]{Vof::step_passive_tracer(
            n,n,n,1.0,0.1,analytic.box,isolated,bad,
            analytic.u,analytic.v,analytic.w);},
            "wrong tracer array length rejected");
        Setup partial(n);
        partial.box={{3.2,3.2,3.2},{4.8,4.8,4.8}};
        std::vector<double> uniform(partial.f.size(),2.0);
        uniform[partial.id(3,3,3)]=0;
        const auto pr=Vof::step_passive_tracer(
            n,n,n,1.0,0.1,partial.box,partial.f,uniform,
            partial.u,partial.v,partial.w);
        near(pr.tracer_before_kg,pr.tracer_after_kg,1e-10,
             "fractional cut-cell solute mass conserved");
        for(std::size_t i=0;i<uniform.size();++i)
            if(pr.water.fraction_after[i]>0)
                near(pr.concentration_after_kg_m3[i],2.0,1e-12,
                     "uniform tracer invariant on fractional cut-cells");
        std::cout<<"VOF_TRACER_NEGATIVE 4\n";
    }
    std::cout<<"VOF_TRACER "<<n<<" 40 "<<std::setprecision(17)
             <<mass<<" "<<initial.tracer_after_kg<<" "<<error<<"\n";
}
// Multi-slice tracer advection must update the DONOR concentration after
// each actual CFL substep. Compare to independently scheduled one-step calls.
void adaptive_passive_tracer(int n) {
    Setup s(n);
    if(n==8)s.box={{3.2,3.2,3.2},{4.8,4.8,4.8}};
    using Vof=upc::P2StaticCutCellVof3D;
    std::vector<double> initial_c(s.f.size(),0.0);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        if(s.f[s.id(x,y,z)]>0)
            initial_c[s.id(x,y,z)]=0.3+0.01*(x+2*y+3*z);
    const std::vector<double> durations{0.1,0.025,0.08,0.05};
    const std::vector<double> factors{40.0,-30.0,50.0,-20.0};
    std::vector<std::vector<double>> us,vs,ws;
    for(double factor:factors) {
        auto u=s.u,v=s.v,w=s.w;
        for(double& x:u)x*=factor;
        for(double& x:v)x*=factor;
        for(double& x:w)x*=factor;
        us.push_back(std::move(u));
        vs.push_back(std::move(v));
        ws.push_back(std::move(w));
    }
    const auto actual=Vof::step_passive_tracer_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,initial_c,durations,us,vs,ws);
    const auto water_only=Vof::step_velocity_sequence_adaptive(
        n,n,n,1.0,s.box,s.f,durations,us,vs,ws);
    check(actual.water.fraction_after==water_only.fraction_after,
          "adaptive tracer does not modify water-only transport");
    auto f=s.f,c=initial_c;
    std::vector<double> lu(s.u.size(),0.0),lv(s.v.size(),0.0),
                        lw(s.w.size(),0.0);
    std::vector<double> tu(s.u.size(),0.0),tv(s.v.size(),0.0),
                        tw(s.w.size(),0.0);
    std::size_t steps=0;
    double initial_mass=0.0,initial_water=0.0;
    for(std::size_t k=0;k<durations.size();++k) {
        const auto remaining=1024-steps;
        const auto pilot=Vof::step(n,n,n,1.0,
            durations[k]/static_cast<double>(remaining),
            s.box,f,us[k],vs[k],ws[k]);
        const auto count=std::max<std::size_t>(1,static_cast<std::size_t>(
            std::ceil(pilot.max_donor_cfl*static_cast<double>(remaining)/0.8)));
        for(std::size_t j=0;j<count;++j) {
            auto r=Vof::step_passive_tracer(
                n,n,n,1.0,durations[k]/static_cast<double>(count),
                s.box,f,c,us[k],vs[k],ws[k]);
            if(k==0&&j==0) {
                initial_mass=r.tracer_before_kg;
                initial_water=r.water.water_before_m3;
            }
            near(r.tracer_after_kg,initial_mass,1e-10,
                 "independent tracer substep mass conserved");
            for(std::size_t i=0;i<lu.size();++i)
                lu[i]+=r.water.u_water_flux_m3[i];
            for(std::size_t i=0;i<lv.size();++i)
                lv[i]+=r.water.v_water_flux_m3[i];
            for(std::size_t i=0;i<lw.size();++i)
                lw[i]+=r.water.w_water_flux_m3[i];
            for(std::size_t i=0;i<tu.size();++i)
                tu[i]+=r.u_tracer_flux_kg[i];
            for(std::size_t i=0;i<tv.size();++i)
                tv[i]+=r.v_tracer_flux_kg[i];
            for(std::size_t i=0;i<tw.size();++i)
                tw[i]+=r.w_tracer_flux_kg[i];
            f=std::move(r.water.fraction_after);
            c=std::move(r.concentration_after_kg_m3);
        }
        steps+=count;
    }
    check(steps>durations.size()&&steps<=1024,
          "adaptive tracer requires real CFL substeps");
    check(actual.water.substeps_used==steps,
          "adaptive tracer work count matches independent schedule");
    double error=0.0,difference=0.0;
    for(std::size_t i=0;i<f.size();++i) {
        error=std::max(error,std::abs(f[i]-actual.water.fraction_after[i]));
        error=std::max(error,
                       std::abs(c[i]-actual.concentration_after_kg_m3[i]));
        difference=std::max(difference,
                            std::abs(c[i]-initial_c[i]));
        check(actual.water.fraction_after[i]>=0&&
              actual.water.fraction_after[i]<=1,
              "adaptive tracer water bounded");
        check(actual.concentration_after_kg_m3[i]>=0&&
              actual.concentration_after_kg_m3[i]<=
                  0.3+0.01*(n-1)*6+1e-12,
              "adaptive tracer concentration bounded");
    }
    for(std::size_t i=0;i<lu.size();++i)
        error=std::max(error,std::abs(lu[i]-actual.water.u_water_flux_m3[i]));
    for(std::size_t i=0;i<lv.size();++i)
        error=std::max(error,std::abs(lv[i]-actual.water.v_water_flux_m3[i]));
    for(std::size_t i=0;i<lw.size();++i)
        error=std::max(error,std::abs(lw[i]-actual.water.w_water_flux_m3[i]));
    for(std::size_t i=0;i<tu.size();++i)
        error=std::max(error,std::abs(tu[i]-actual.u_tracer_flux_kg[i]));
    for(std::size_t i=0;i<tv.size();++i)
        error=std::max(error,std::abs(tv[i]-actual.v_tracer_flux_kg[i]));
    for(std::size_t i=0;i<tw.size();++i)
        error=std::max(error,std::abs(tw[i]-actual.w_tracer_flux_kg[i]));
    // A telescoping signed-face ledger must reconstruct the FINAL
    // solute inventory from the INITIAL state, even across changing donors.
    const auto geom=upc::P2MovingBoxFaceApertures3D::sample(
        n,n,n,1.0,0.1,s.box,s.box);
    double cell_error=0.0,outer=0.0;
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        const double east=actual.u_tracer_flux_kg[s.ui(x+1,y,z)];
        const double west=actual.u_tracer_flux_kg[s.ui(x,y,z)];
        const double north=actual.v_tracer_flux_kg[s.vi(x,y+1,z)];
        const double south=actual.v_tracer_flux_kg[s.vi(x,y,z)];
        const double top=actual.w_tracer_flux_kg[s.wi(x,y,z+1)];
        const double bottom=actual.w_tracer_flux_kg[s.wi(x,y,z)];
        const double vol=geom.cell_geometry.alpha_old[i];
        const double expected=vol*s.f[i]*initial_c[i]-
            (east-west+north-south+top-bottom);
        const double observed=vol*actual.water.fraction_after[i]*
            actual.concentration_after_kg_m3[i];
        cell_error=std::max(cell_error,std::abs(expected-observed));
        near(observed,expected,1e-11,"adaptive cumulative local tracer balance");
        if(x==0)outer=std::max(outer,std::abs(west));
        if(x==n-1)outer=std::max(outer,std::abs(east));
        if(y==0)outer=std::max(outer,std::abs(south));
        if(y==n-1)outer=std::max(outer,std::abs(north));
        if(z==0)outer=std::max(outer,std::abs(bottom));
        if(z==n-1)outer=std::max(outer,std::abs(top));
    }
    near(outer,0.0,0.0,"sealed outer tracer faces");
    check(difference>1e-5,"adaptive tracer physically moves in 3D");
    near(error,0.0,1e-13,"adaptive tracer matches independent one-step replay");
    near(actual.tracer_before_kg,initial_mass,1e-10,
         "adaptive tracer initial mass");
    near(actual.tracer_after_kg,initial_mass,1e-10,
         "adaptive tracer final mass");
    near(actual.water.water_before_m3,initial_water,1e-10,
         "adaptive tracer initial water");
    near(actual.water.water_after_m3,initial_water,1e-10,
         "adaptive tracer final water");
    check(actual.water.max_donor_cfl>0&&
          actual.water.max_donor_cfl<=0.8+1e-12,
          "adaptive tracer geometric CFL");
    if(n==8) {
        const auto geom=upc::P2MovingBoxFaceApertures3D::sample(
            n,n,n,1.0,0.1,s.box,s.box);
        check(std::any_of(geom.cell_geometry.alpha_old.begin(),
              geom.cell_geometry.alpha_old.end(),
              [](double a){return a>0&&a<1;}),
              "adaptive tracer uses fractional cut-cell geometry");
        auto one=Vof::step_passive_tracer_sequence_adaptive(
            n,n,n,1.0,s.box,s.f,initial_c,{0.001},{s.u},{s.v},{s.w});
        auto reference=Vof::step_passive_tracer(
            n,n,n,1.0,0.001,s.box,s.f,initial_c,s.u,s.v,s.w);
        check(one.water.substeps_used==1&&
              one.water.fraction_after==reference.water.fraction_after&&
              one.concentration_after_kg_m3==
                  reference.concentration_after_kg_m3,
              "one safe tracer slice equals legacy one-step");
        rejects([&]{Vof::step_passive_tracer_sequence_adaptive(
            n,n,n,1.0,s.box,s.f,initial_c,{}, {}, {}, {});},
            "empty adaptive tracer schedule rejected");
        auto bad_times=durations;
        bad_times[2]=0;
        rejects([&]{Vof::step_passive_tracer_sequence_adaptive(
            n,n,n,1.0,s.box,s.f,initial_c,bad_times,us,vs,ws);},
            "zero later tracer slice rejected");
        auto bad_c=initial_c;
        bad_c[s.id(0,0,0)]=std::numeric_limits<double>::quiet_NaN();
        rejects([&]{Vof::step_passive_tracer_sequence_adaptive(
            n,n,n,1.0,s.box,s.f,bad_c,durations,us,vs,ws);},
            "nonfinite tracer input rejected");
        bad_c=initial_c;
        bad_c[s.id(3,3,3)]=1.0;
        rejects([&]{Vof::step_passive_tracer_sequence_adaptive(
            n,n,n,1.0,s.box,s.f,bad_c,durations,us,vs,ws);},
            "dry-cell tracer input rejected");
        rejects([&]{Vof::step_passive_tracer_sequence_adaptive(
            n,n,n,1.0,s.box,s.f,initial_c,durations,us,vs,ws,0.8,steps-1);},
            "insufficient shared tracer CFL budget rejected");
        auto bad_u=us;
        bad_u[2][s.ui(1,0,1)]+=0.1;
        rejects([&]{Vof::step_passive_tracer_sequence_adaptive(
            n,n,n,1.0,s.box,s.f,initial_c,durations,bad_u,vs,ws);},
            "non-solenoidal later tracer field rejected");
        std::cout<<"VOF_TRACER_ADAPTIVE_NEGATIVE 6\n";
    }
    std::cout<<"VOF_TRACER_FACE_LEDGER "<<n<<" "<<steps<<" "
             <<std::setprecision(17)<<cell_error<<" "<<outer<<"\n";
    std::cout<<"VOF_TRACER_ADAPTIVE "<<n<<" "<<steps<<" "
             <<std::setprecision(17)<<actual.tracer_before_kg<<" "
             <<actual.tracer_after_kg<<" "<<actual.water.water_before_m3
             <<" "<<actual.water.water_after_m3<<" "<<error<<"\n";
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
        automatic_subcycling(8);
        automatic_subcycling(12);
        velocity_sequence(8);
        velocity_sequence(12);
        adaptive_velocity_sequence(8);
        adaptive_velocity_sequence(12);
        face_water_flux_ledger(8);
        face_water_flux_ledger(12);
        passive_tracer(8);
        passive_tracer(12);
        adaptive_passive_tracer(8);
        adaptive_passive_tracer(12);
        negatives();
        std::cout<<"PASS: "<<assertions
                 <<" native static 3D cut-cell VOF assertions\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
