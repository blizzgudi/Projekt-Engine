#include "upc/p2_cut_cell_tracer_diffusion3d.hpp"
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
void diffusive_tracer(int n) {
    Setup s(n);
    using Diff=upc::P2CutCellTracerDiffusion3D;
    using Vof=upc::P2StaticCutCellVof3D;
    std::vector<double> c(s.f.size(),0.0);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        if(s.f[s.id(x,y,z)]>0)
            c[s.id(x,y,z)]=0.3+0.01*(x+2*y+3*z);
    const auto adv=Vof::step_passive_tracer(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w);
    const auto zero=Diff::step(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,0.0);
    check(zero.concentration_after_kg_m3==adv.concentration_after_kg_m3&&
          zero.tracer_after_kg==adv.tracer_after_kg,
          "D=0 must exactly preserve legacy advection");
    const auto first=Diff::step(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,0.04);
    check(first.advected.water.fraction_after==adv.water.fraction_after,
          "diffusion opt-in cannot alter water transport");
    check(first.max_diffusion_cfl>0&&first.max_diffusion_cfl<1,
          "3D diffusion explicit CFL safe");
    double residual=0.0;
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        const auto& a=first.advected;
        const auto U=[&](int xx) {
            auto j=s.ui(xx,y,z);
            return a.u_tracer_flux_kg[j]+first.u_diffusion_flux_kg[j];
        };
        const auto V=[&](int yy) {
            auto j=s.vi(x,yy,z);
            return a.v_tracer_flux_kg[j]+first.v_diffusion_flux_kg[j];
        };
        const auto W=[&](int zz) {
            auto j=s.wi(x,y,zz);
            return a.w_tracer_flux_kg[j]+first.w_diffusion_flux_kg[j];
        };
        const double expected=s.f[i]*c[i]-
            (U(x+1)-U(x)+V(y+1)-V(y)+W(z+1)-W(z));
        const double actual=a.water.fraction_after[i]*
            first.concentration_after_kg_m3[i];
        residual=std::max(residual,std::abs(actual-expected));
        near(actual,expected,1e-12,"independent 3D advective+diffusive face balance");
        check(first.concentration_after_kg_m3[i]>=0.0,
              "diffusive concentration remains nonnegative");
    }
    const double mass=first.advected.tracer_before_kg;
    for(int t=0;t<40;++t) {
        auto next=Diff::step(
            n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,0.04);
        near(next.tracer_after_kg,mass,1e-10,
             "40-step advective+diffusive tracer mass conserved");
        near(next.advected.water.water_after_m3,
             next.advected.water.water_before_m3,1e-10,
             "40-step diffusion leaves water mass invariant");
        check(next.max_diffusion_cfl<=1.0,"diffusion CFL bounded over 40 steps");
        s.f=std::move(next.advected.water.fraction_after);
        c=std::move(next.concentration_after_kg_m3);
    }
    std::cout<<"VOF_TRACER_DIFFUSION "<<n<<" 40 "<<std::setprecision(17)
             <<mass<<" "<<first.tracer_after_kg<<" "<<residual<<" "
             <<first.max_diffusion_cfl<<"\n";
}
void anisotropic_diffusion_reference(int n) {
    using Diff=upc::P2CutCellTracerDiffusion3D;
    Setup s(n);
    std::fill(s.u.begin(),s.u.end(),0.0);
    std::fill(s.v.begin(),s.v.end(),0.0);
    std::fill(s.w.begin(),s.w.end(),0.0);
    std::fill(s.f.begin(),s.f.end(),1.0);
    s.f[s.id(3,3,3)]=0.0;
    std::vector<double> c(s.f.size(),0.0);
    c[s.id(1,1,1)]=1.0;
    const auto a=Diff::step_anisotropic(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,{0.01,0.02,0.03});
    near(a.concentration_after_kg_m3[s.id(1,1,1)],0.988,1e-14,
         "independent anisotropic 3D center 1-2*dt*(Dx+Dy+Dz)");
    near(a.max_diffusion_cfl,0.012,1e-14,
         "anisotropic 3D explicit six-face CFL");
    for(int axis=0;axis<3;++axis) {
        const double expected=0.001*(axis+1);
        for(int side=-1;side<=1;side+=2) {
            int x=1,y=1,z=1;
            if(axis==0)x+=side;
            if(axis==1)y+=side;
            if(axis==2)z+=side;
            near(a.concentration_after_kg_m3[s.id(x,y,z)],expected,1e-14,
                 "independent anisotropic six-neighbor concentration");
        }
    }
    near(a.u_diffusion_flux_kg[s.ui(1,1,1)],-0.001,1e-14,
         "negative signed anisotropic U face");
    near(a.u_diffusion_flux_kg[s.ui(2,1,1)],0.001,1e-14,
         "positive signed anisotropic U face");
    near(a.v_diffusion_flux_kg[s.vi(1,2,1)],0.002,1e-14,
         "positive anisotropic V face");
    near(a.w_diffusion_flux_kg[s.wi(1,1,2)],0.003,1e-14,
         "positive anisotropic W face");
    near(a.tracer_after_kg,1.0,1e-13,"anisotropic total tracer mass");
    double residual=0.0;
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        const double expected=s.f[i]*c[i]-
            (a.u_diffusion_flux_kg[s.ui(x+1,y,z)]-
             a.u_diffusion_flux_kg[s.ui(x,y,z)]+
             a.v_diffusion_flux_kg[s.vi(x,y+1,z)]-
             a.v_diffusion_flux_kg[s.vi(x,y,z)]+
             a.w_diffusion_flux_kg[s.wi(x,y,z+1)]-
             a.w_diffusion_flux_kg[s.wi(x,y,z)]);
        const double actual=s.f[i]*a.concentration_after_kg_m3[i];
        residual=std::max(residual,std::abs(expected-actual));
        near(actual,expected,1e-13,"anisotropic cellwise U/V/W mass balance");
    }
    // Scalar legacy and diagonal isotropic operator must match bitwise,
    // including with nonzero 3D prescribed MAC velocities.
    Setup moving(n);
    std::vector<double> initial(moving.f.size(),0.0);
    for(std::size_t i=0;i<initial.size();++i)
        if(moving.f[i]>0)initial[i]=0.5+0.001*i;
    const auto legacy=Diff::step(
        n,n,n,1.0,0.1,moving.box,moving.f,initial,
        moving.u,moving.v,moving.w,0.02);
    const auto diagonal=Diff::step_anisotropic(
        n,n,n,1.0,0.1,moving.box,moving.f,initial,
        moving.u,moving.v,moving.w,{0.02,0.02,0.02});
    check(legacy.concentration_after_kg_m3==
          diagonal.concentration_after_kg_m3 &&
          legacy.u_diffusion_flux_kg==diagonal.u_diffusion_flux_kg &&
          legacy.v_diffusion_flux_kg==diagonal.v_diffusion_flux_kg &&
          legacy.w_diffusion_flux_kg==diagonal.w_diffusion_flux_kg &&
          legacy.tracer_after_kg==diagonal.tracer_after_kg,
          "diagonal isotropic matches legacy scalar bitwise");
    const auto y_only=Diff::step_anisotropic(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,{0.0,0.02,0.0});
    near(y_only.concentration_after_kg_m3[s.id(1,1,1)],0.996,1e-14,
         "zero U/W diffusion axes leave only two Y transfers");
    near(y_only.concentration_after_kg_m3[s.id(1,2,1)],0.002,1e-14,
         "Y-only positive neighbor");
    for(double flux:y_only.u_diffusion_flux_kg)
        near(flux,0.0,0.0,"disabled U axis has zero diffusion flux");
    for(double flux:y_only.w_diffusion_flux_kg)
        near(flux,0.0,0.0,"disabled W axis has zero diffusion flux");
    const auto zero=Diff::step_anisotropic(
        n,n,n,1.0,0.1,moving.box,moving.f,initial,
        moving.u,moving.v,moving.w,{0.0,0.0,0.0});
    const auto passive=upc::P2StaticCutCellVof3D::step_passive_tracer(
        n,n,n,1.0,0.1,moving.box,moving.f,initial,
        moving.u,moving.v,moving.w);
    check(zero.concentration_after_kg_m3==
          passive.concentration_after_kg_m3 &&
          zero.tracer_after_kg==passive.tracer_after_kg,
          "all zero anisotropic coefficients preserve advection bitwise");
    rejects([&]{Diff::step_anisotropic(
        n,n,n,1,0.1,s.box,s.f,c,s.u,s.v,s.w,{0.01,-0.02,0.03});},
        "negative Y diffusivity rejected");
    rejects([&]{Diff::step_anisotropic(
        n,n,n,1,0.1,s.box,s.f,c,s.u,s.v,s.w,
        {0.01,0.02,std::numeric_limits<double>::quiet_NaN()});},
        "nonfinite Z diffusivity rejected");
    rejects([&]{Diff::step_anisotropic(
        n,n,n,1,0.1,s.box,s.f,c,s.u,s.v,s.w,
        {std::numeric_limits<double>::infinity(),0.02,0.03});},
        "infinite X diffusivity rejected");
    rejects([&]{Diff::step_anisotropic(
        n,n,n,1,1.0,s.box,s.f,c,s.u,s.v,s.w,{10.0,0.0,0.0});},
        "unstable anisotropic diffusion CFL rejected");
    std::cout<<"VOF_TRACER_ANISOTROPIC "<<n<<" "
             <<std::setprecision(17)
             <<a.concentration_after_kg_m3[s.id(1,1,1)]<<" "
             <<a.concentration_after_kg_m3[s.id(2,1,1)]<<" "
             <<a.concentration_after_kg_m3[s.id(1,2,1)]<<" "
             <<a.concentration_after_kg_m3[s.id(1,1,2)]<<" "
             <<a.tracer_after_kg<<" "<<a.max_diffusion_cfl<<" "
             <<residual<<"\n";
}
void diffusion_subcycling_reference(int n) {
    using Diff=upc::P2CutCellTracerDiffusion3D;
    Setup s(n);
    std::fill(s.u.begin(),s.u.end(),0.0);
    std::fill(s.v.begin(),s.v.end(),0.0);
    std::fill(s.w.begin(),s.w.end(),0.0);
    std::fill(s.f.begin(),s.f.end(),1.0);
    s.f[s.id(3,3,3)]=0.0;
    std::vector<double> c(s.f.size(),0.0);
    c[s.id(1,1,1)]=1.0;
    const std::array<double,3> d={0.1,0.1,0.1};
    const auto result=Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,2.5,s.box,s.f,c,s.u,s.v,s.w,d,0.8,8);
    check(result.diffusion_substeps==2,"auto diffusion chooses two stable substeps");
    near(result.max_diffusion_cfl,0.75,1e-13,
         "actual maximum subcycled 3D diffusion CFL");
    near(result.concentration_after_kg_m3[s.id(1,1,1)],0.15625,1e-13,
         "independent two-step 3D center stencil");
    near(result.concentration_after_kg_m3[s.id(2,1,1)],0.0625,1e-13,
         "independent two-step first neighbor");
    near(result.concentration_after_kg_m3[s.id(3,1,1)],0.015625,1e-13,
         "independent two-step axial second neighbor");
    near(result.concentration_after_kg_m3[s.id(2,2,1)],0.03125,1e-13,
         "independent two-step diagonal neighbor");
    near(result.tracer_after_kg,1.0,1e-12,"subcycled total tracer mass");
    near(result.advected.water.water_after_m3,double(n*n*n-1),1e-10,
         "subcycled water mass unchanged");
    std::vector<double> replay=c;
    std::vector<double> uflux(s.u.size(),0),vflux(s.v.size(),0),wflux(s.w.size(),0);
    for(int k=0;k<2;++k) {
        const auto part=Diff::step_anisotropic(
            n,n,n,1.0,1.25,s.box,s.f,replay,s.u,s.v,s.w,d);
        replay=part.concentration_after_kg_m3;
        for(std::size_t i=0;i<uflux.size();++i)
            uflux[i]+=part.u_diffusion_flux_kg[i];
        for(std::size_t i=0;i<vflux.size();++i)
            vflux[i]+=part.v_diffusion_flux_kg[i];
        for(std::size_t i=0;i<wflux.size();++i)
            wflux[i]+=part.w_diffusion_flux_kg[i];
    }
    double residual=0.0;
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        near(result.concentration_after_kg_m3[i],replay[i],1e-14,
             "manual diffusion replay cellwise");
        const double expected=s.f[i]*c[i]-
            (result.u_diffusion_flux_kg[s.ui(x+1,y,z)]-
             result.u_diffusion_flux_kg[s.ui(x,y,z)]+
             result.v_diffusion_flux_kg[s.vi(x,y+1,z)]-
             result.v_diffusion_flux_kg[s.vi(x,y,z)]+
             result.w_diffusion_flux_kg[s.wi(x,y,z+1)]-
             result.w_diffusion_flux_kg[s.wi(x,y,z)]);
        const double actual=s.f[i]*result.concentration_after_kg_m3[i];
        residual=std::max(residual,std::abs(expected-actual));
        near(actual,expected,1e-13,"independent subcycle cellwise face balance");
    }
    for(std::size_t i=0;i<uflux.size();++i)
        near(result.u_diffusion_flux_kg[i],uflux[i],1e-14,
             "U cumulative diffusion ledger replay");
    for(std::size_t i=0;i<vflux.size();++i)
        near(result.v_diffusion_flux_kg[i],vflux[i],1e-14,
             "V cumulative diffusion ledger replay");
    for(std::size_t i=0;i<wflux.size();++i)
        near(result.w_diffusion_flux_kg[i],wflux[i],1e-14,
             "W cumulative diffusion ledger replay");
    rejects([&]{Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,2.5,s.box,s.f,c,s.u,s.v,s.w,d,0.8,1);},
        "insufficient diffusion subcycle budget rejected");
    rejects([&]{Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,2.5,s.box,s.f,c,s.u,s.v,s.w,d,0.0,8);},
        "zero diffusion CFL target rejected");
    rejects([&]{Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,2.5,s.box,s.f,c,s.u,s.v,s.w,d,1.1,8);},
        "diffusion CFL target greater than one rejected");
    rejects([&]{Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,2.5,s.box,s.f,c,s.u,s.v,s.w,d,0.8,0);},
        "zero diffusion subcycle budget rejected");
    rejects([&]{Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,2.5,s.box,s.f,c,s.u,s.v,s.w,
        {0.1,std::numeric_limits<double>::quiet_NaN(),0.1},0.8,8);},
        "nonfinite anisotropic subcycle diffusivity rejected");
    const auto zero=Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,2.5,s.box,s.f,c,s.u,s.v,s.w,{0,0,0});
    check(zero.diffusion_substeps==1 &&
          zero.concentration_after_kg_m3==c,
          "zero diffusivity exactly preserves stationary tracer");
    std::cout<<"VOF_TRACER_DIFFUSION_SUBCYCLE "<<n<<" "
             <<result.diffusion_substeps<<" "<<std::setprecision(17)
             <<result.concentration_after_kg_m3[s.id(1,1,1)]<<" "
             <<result.tracer_after_kg<<" "<<result.max_diffusion_cfl<<" "
             <<residual<<"\n";
}
void diffusion_subcycling_moving_and_fractional(int n) {
    using Diff=upc::P2CutCellTracerDiffusion3D;
    Setup moving(n);
    std::vector<double> c(moving.f.size(),0.0);
    for(std::size_t i=0;i<c.size();++i)
        if(moving.f[i]>0.0)c[i]=0.2+0.001*i;
    const std::array<double,3> d={0.02,0.03,0.01};
    const auto legacy=Diff::step_anisotropic(
        n,n,n,1.0,0.1,moving.box,moving.f,c,
        moving.u,moving.v,moving.w,d);
    const auto adaptive=Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,0.1,moving.box,moving.f,c,
        moving.u,moving.v,moving.w,d);
    check(adaptive.diffusion_substeps==1,
          "low-CFL moving MAC requires only one diffusion substep");
    for(std::size_t i=0;i<c.size();++i)
        near(adaptive.concentration_after_kg_m3[i],
             legacy.concentration_after_kg_m3[i],1e-13,
             "nonzero MAC advection followed by one diffusion substep");
    near(adaptive.tracer_after_kg,legacy.tracer_after_kg,1e-12,
         "nonzero MAC tracer inventory parity");
    Setup s(n);
    std::fill(s.u.begin(),s.u.end(),0.0);
    std::fill(s.v.begin(),s.v.end(),0.0);
    std::fill(s.w.begin(),s.w.end(),0.0);
    std::fill(s.f.begin(),s.f.end(),1.0);
    const upc::P2Box3D partial_box{{3.2,3.2,3.2},{3.8,3.8,3.8}};
    std::vector<double> initial(s.f.size(),0.0);
    initial[s.id(3,3,3)]=1.0;
    const auto r=Diff::step_diffusion_subcycled_anisotropic(
        n,n,n,1.0,2.5,partial_box,s.f,initial,s.u,s.v,s.w,
        {0.1,0.1,0.1},0.8,8);
    check(r.diffusion_substeps>=2&&r.diffusion_substeps<=8,
          "fractional cut-cell diffusion subcycling");
    check(r.max_diffusion_cfl<=0.8+1e-12,
          "fractional cut-cell per-substep CFL bound");
    const auto geom=upc::P2MovingBoxFaceApertures3D::sample(
        n,n,n,1.0,2.5,partial_box,partial_box);
    const double initial_mass=geom.cell_geometry.alpha_old[s.id(3,3,3)];
    near(r.tracer_after_kg,initial_mass,1e-12,
         "fractional cut-cell dissolved mass conserved");
    double residual=0;
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        const double expected=geom.cell_geometry.alpha_old[i]*initial[i]-
            (r.u_diffusion_flux_kg[s.ui(x+1,y,z)]-
             r.u_diffusion_flux_kg[s.ui(x,y,z)]+
             r.v_diffusion_flux_kg[s.vi(x,y+1,z)]-
             r.v_diffusion_flux_kg[s.vi(x,y,z)]+
             r.w_diffusion_flux_kg[s.wi(x,y,z+1)]-
             r.w_diffusion_flux_kg[s.wi(x,y,z)]);
        const double actual=geom.cell_geometry.alpha_old[i]*
            r.concentration_after_kg_m3[i];
        residual=std::max(residual,std::abs(actual-expected));
        near(actual,expected,1e-12,
             "fractional cut-cell independent cumulative face balance");
    }
    std::cout<<"VOF_TRACER_DIFFUSION_SUBCYCLE_PARTIAL "<<n<<" "
             <<r.diffusion_substeps<<" "<<std::setprecision(17)
             <<initial_mass<<" "<<r.tracer_after_kg<<" "
             <<r.max_diffusion_cfl<<" "<<residual<<"\n";
}

void heterogeneous_diffusion_reference(int n) {
    using Diff=upc::P2CutCellTracerDiffusion3D;
    Setup s(n);
    std::fill(s.u.begin(),s.u.end(),0.0);
    std::fill(s.v.begin(),s.v.end(),0.0);
    std::fill(s.w.begin(),s.w.end(),0.0);
    std::fill(s.f.begin(),s.f.end(),1.0);
    s.f[s.id(3,3,3)]=0.0; // Exact stationary blocked voxel.
    std::vector<double> c(s.f.size(),0.0);
    const int x=1,y=1,z=1;
    c[s.id(x,y,z)]=1.0;
    std::array<std::vector<double>,3> d;
    for(auto& axis:d)axis.assign(s.f.size(),0.0);
    d[0][s.id(x-1,y,z)]=1.0;
    d[0][s.id(x,y,z)]=2.0;
    d[0][s.id(x+1,y,z)]=4.0;
    const auto a=Diff::step_heterogeneous(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,d);
    near(a.concentration_after_kg_m3[s.id(x,y,z)],0.6,1e-13,
         "independent harmonic-interface center mass");
    near(a.concentration_after_kg_m3[s.id(x-1,y,z)],2.0/15.0,1e-13,
         "harmonic left D=4/3, flux=2/15 kg");
    near(a.concentration_after_kg_m3[s.id(x+1,y,z)],4.0/15.0,1e-13,
         "harmonic right D=8/3, flux=4/15 kg");
    near(a.u_diffusion_flux_kg[s.ui(x,y,z)],-2.0/15.0,1e-13,
         "negative signed left heterogeneous flux");
    near(a.u_diffusion_flux_kg[s.ui(x+1,y,z)],4.0/15.0,1e-13,
         "positive signed right heterogeneous flux");
    near(a.tracer_after_kg,1.0,1e-13,"heterogeneous total mass");
    near(a.max_diffusion_cfl,0.4,1e-13,"heterogeneous local CFL");
    // Independently reconstruct every cell's signed finite-volume balance.
    double max_residual=0.0;
    for(int zz=0;zz<n;++zz)
    for(int yy=0;yy<n;++yy)
    for(int xx=0;xx<n;++xx) {
        const auto i=s.id(xx,yy,zz);
        const double signed_in=
            a.u_diffusion_flux_kg[s.ui(xx,yy,zz)]-
            a.u_diffusion_flux_kg[s.ui(xx+1,yy,zz)]+
            a.v_diffusion_flux_kg[s.vi(xx,yy,zz)]-
            a.v_diffusion_flux_kg[s.vi(xx,yy+1,zz)]+
            a.w_diffusion_flux_kg[s.wi(xx,yy,zz)]-
            a.w_diffusion_flux_kg[s.wi(xx,yy,zz+1)];
        const double expected=c[i]+signed_in;
        const double actual=a.concentration_after_kg_m3[i];
        max_residual=std::max(max_residual,std::abs(expected-actual));
        near(actual,expected,1e-12,"heterogeneous per-cell U/V/W ledger");
    }
    // Homogeneous per-cell D agrees with the existing axis operator.
    for(std::size_t axis=0;axis<3;++axis)
        std::fill(d[axis].begin(),d[axis].end(),0.01*(axis+1));
    const auto uniform=Diff::step_heterogeneous(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,d);
    const auto old=Diff::step_anisotropic(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,{0.01,0.02,0.03});
    for(std::size_t i=0;i<c.size();++i)
        near(uniform.concentration_after_kg_m3[i],
             old.concentration_after_kg_m3[i],1e-13,
             "uniform heterogeneous equals anisotropic reference");
    near(uniform.tracer_after_kg,old.tracer_after_kg,1e-12,
         "uniform heterogeneous total mass parity");
    d[0].pop_back();
    rejects([&]{Diff::step_heterogeneous(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,d);},
        "heterogeneous coefficient dimension mismatch");
    d[0].push_back(0.01);
    d[0][s.id(x,y,z)]=-0.1;
    rejects([&]{Diff::step_heterogeneous(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,d);},
        "negative heterogeneous coefficient");
    d[0][s.id(x,y,z)]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Diff::step_heterogeneous(
        n,n,n,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,d);},
        "nonfinite heterogeneous coefficient");
    d[0][s.id(x,y,z)]=2.0;
    d[0][s.id(x-1,y,z)]=1.0;
    d[0][s.id(x+1,y,z)]=4.0;
    std::fill(d[1].begin(),d[1].end(),0.0);
    std::fill(d[2].begin(),d[2].end(),0.0);
    rejects([&]{Diff::step_heterogeneous(
        n,n,n,1.0,0.5,s.box,s.f,c,s.u,s.v,s.w,d);},
        "heterogeneous CFL violation");
    std::cout<<std::setprecision(17)<<"VOF_TRACER_HETEROGENEOUS "
             <<n<<" "<<a.concentration_after_kg_m3[s.id(x,y,z)]
             <<" "<<a.concentration_after_kg_m3[s.id(x-1,y,z)]
             <<" "<<a.concentration_after_kg_m3[s.id(x+1,y,z)]
             <<" "<<a.tracer_after_kg<<" "<<a.max_diffusion_cfl
             <<" "<<max_residual<<"\n";
}

void heterogeneous_subcycle_reference(int n) {
    using Diff=upc::P2CutCellTracerDiffusion3D;
    Setup s(n);
    std::fill(s.u.begin(),s.u.end(),0.0);
    std::fill(s.v.begin(),s.v.end(),0.0);
    std::fill(s.w.begin(),s.w.end(),0.0);
    std::fill(s.f.begin(),s.f.end(),1.0);
    s.f[s.id(3,3,3)]=0.0;
    std::vector<double> c(s.f.size(),0.0);
    c[s.id(1,1,1)]=1.0;
    std::array<std::vector<double>,3> d;
    for(auto& axis:d)axis.assign(s.f.size(),0.0);
    d[0][s.id(0,1,1)]=1.0;
    d[0][s.id(1,1,1)]=2.0;
    d[0][s.id(2,1,1)]=4.0;
    rejects([&]{Diff::step_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,d);},
        "heterogeneous single-step CFL rejects");
    const auto r=Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,d);
    check(r.diffusion_substeps==3,"harmonic CFL requires 3 substeps");
    near(r.max_diffusion_cfl,2.0/3.0,1e-13,"harmonic substep CFL");
    near(r.tracer_after_kg,1.0,1e-12,"heterogeneous mass");
    std::vector<double> replay=c;
    std::vector<double> uf(r.u_diffusion_flux_kg.size(),0.0);
    std::vector<double> vf(r.v_diffusion_flux_kg.size(),0.0);
    std::vector<double> wf(r.w_diffusion_flux_kg.size(),0.0);
    for(int k=0;k<3;++k) {
        const auto part=Diff::step_heterogeneous(
            n,n,n,1,0.5/3,s.box,s.f,replay,s.u,s.v,s.w,d);
        replay=part.concentration_after_kg_m3;
        for(std::size_t i=0;i<uf.size();++i)uf[i]+=part.u_diffusion_flux_kg[i];
        for(std::size_t i=0;i<vf.size();++i)vf[i]+=part.v_diffusion_flux_kg[i];
        for(std::size_t i=0;i<wf.size();++i)wf[i]+=part.w_diffusion_flux_kg[i];
    }
    double residual=0.0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        near(r.concentration_after_kg_m3[i],replay[i],1e-14,
             "manual heterogeneous replay");
        const double expected=s.f[i]*c[i]-
            (r.u_diffusion_flux_kg[s.ui(x+1,y,z)]-
             r.u_diffusion_flux_kg[s.ui(x,y,z)]+
             r.v_diffusion_flux_kg[s.vi(x,y+1,z)]-
             r.v_diffusion_flux_kg[s.vi(x,y,z)]+
             r.w_diffusion_flux_kg[s.wi(x,y,z+1)]-
             r.w_diffusion_flux_kg[s.wi(x,y,z)]);
        const double actual=s.f[i]*r.concentration_after_kg_m3[i];
        residual=std::max(residual,std::abs(actual-expected));
        near(actual,expected,1e-12,"heterogeneous cell mass ledger");
    }
    for(std::size_t i=0;i<uf.size();++i)
        near(uf[i],r.u_diffusion_flux_kg[i],1e-14,"U replay ledger");
    for(std::size_t i=0;i<vf.size();++i)
        near(vf[i],r.v_diffusion_flux_kg[i],1e-14,"V replay ledger");
    for(std::size_t i=0;i<wf.size();++i)
        near(wf[i],r.w_diffusion_flux_kg[i],1e-14,"W replay ledger");
    rejects([&]{Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,d,0.8,2);},
        "heterogeneous subcycle insufficient budget");
    rejects([&]{Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,d,0.0);},
        "heterogeneous zero CFL target");
    rejects([&]{Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,d,1.1);},
        "heterogeneous CFL target over one");
    rejects([&]{Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,d,0.8,0);},
        "heterogeneous zero budget");
    auto invalid=d;
    invalid[1].pop_back();
    rejects([&]{Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,invalid);},
        "heterogeneous bad field size");
    invalid=d;invalid[0][s.id(1,1,1)]=-1;
    rejects([&]{Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,invalid);},
        "heterogeneous negative D");
    invalid=d;invalid[0][s.id(1,1,1)]=std::numeric_limits<double>::infinity();
    rejects([&]{Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,invalid);},
        "heterogeneous nonfinite D");
    for(auto& axis:d)std::fill(axis.begin(),axis.end(),0.0);
    const auto zero=Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,0.5,s.box,s.f,c,s.u,s.v,s.w,d);
    check(zero.diffusion_substeps==1&&zero.concentration_after_kg_m3==c,
          "zero D exact advection parity");
    std::cout<<"VOF_TRACER_HET_SUBCYCLE "<<n<<" "<<r.diffusion_substeps
             <<" "<<std::setprecision(17)<<r.tracer_after_kg<<" "
             <<r.max_diffusion_cfl<<" "<<residual<<"\n";
    s.box={{3.2,3.2,3.2},{4.8,4.8,4.8}};
    std::fill(s.f.begin(),s.f.end(),1.0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        c[i]=0.1+0.01*x+0.02*y+0.005*z;
        d[0][i]=0.1+0.02*x;
        d[1][i]=0.05+0.01*y;
        d[2][i]=0.03+0.005*z;
    }
    const auto fractional=Diff::step_diffusion_subcycled_heterogeneous(
        n,n,n,1,1.2,s.box,s.f,c,s.u,s.v,s.w,d,0.7);
    check(fractional.diffusion_substeps>=2,
          "fractional heterogeneous cut cells trigger subcycling");
    check(fractional.max_diffusion_cfl>0&&
          fractional.max_diffusion_cfl<=0.7+1e-12,
          "fractional heterogeneous CFL bound");
    near(fractional.tracer_after_kg,
         fractional.advected.tracer_after_kg,1e-10,
         "fractional heterogeneous solute mass");
    const auto geom=upc::P2MovingBoxFaceApertures3D::sample(
        n,n,n,1,1.2,s.box,s.box);
    double partial_residual=0.0;
    int partial_cells=0;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=s.id(x,y,z);
        const double alpha=geom.cell_geometry.alpha_old[i];
        if(alpha>0&&alpha<1)++partial_cells;
        const double expected=alpha*c[i]-
            (fractional.u_diffusion_flux_kg[s.ui(x+1,y,z)]-
             fractional.u_diffusion_flux_kg[s.ui(x,y,z)]+
             fractional.v_diffusion_flux_kg[s.vi(x,y+1,z)]-
             fractional.v_diffusion_flux_kg[s.vi(x,y,z)]+
             fractional.w_diffusion_flux_kg[s.wi(x,y,z+1)]-
             fractional.w_diffusion_flux_kg[s.wi(x,y,z)]);
        const double actual=alpha*fractional.concentration_after_kg_m3[i];
        partial_residual=std::max(partial_residual,std::abs(actual-expected));
        near(actual,expected,1e-11,"fractional heterogeneous cell ledger");
    }
    check(partial_cells>0,"actual fractional cut cells");
    std::cout<<"VOF_TRACER_HET_SUBCYCLE_PARTIAL "<<n<<" "
             <<fractional.diffusion_substeps<<" "<<std::setprecision(17)
             <<fractional.tracer_after_kg<<" "
             <<fractional.max_diffusion_cfl<<" "<<partial_residual<<" "
             <<partial_cells<<"\n";
}
void diffusion_analytic_and_partial() {
    using Diff=upc::P2CutCellTracerDiffusion3D;
    Setup s(8);
    std::fill(s.u.begin(),s.u.end(),0.0);
    std::fill(s.v.begin(),s.v.end(),0.0);
    std::fill(s.w.begin(),s.w.end(),0.0);
    std::fill(s.f.begin(),s.f.end(),1.0);
    s.f[s.id(3,3,3)]=0.0;
    std::vector<double> c(s.f.size(),0.0);
    c[s.id(1,1,1)]=1.0;
    const auto r=Diff::step(
        8,8,8,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,0.05);
    near(r.concentration_after_kg_m3[s.id(1,1,1)],0.97,1e-14,
         "independent six-face 3D Fick center 1-6*0.005");
    for(const auto& p:std::vector<std::vector<int>>{
        {0,1,1},{2,1,1},{1,0,1},{1,2,1},{1,1,0},{1,1,2}})
        near(r.concentration_after_kg_m3[s.id(p[0],p[1],p[2])],0.005,1e-14,
             "independent six-neighbor 3D Fick reference");
    near(r.u_diffusion_flux_kg[s.ui(2,1,1)],0.005,1e-14,
         "positive U-face Fick mass");
    near(r.v_diffusion_flux_kg[s.vi(1,2,1)],0.005,1e-14,
         "positive V-face Fick mass");
    near(r.w_diffusion_flux_kg[s.wi(1,1,2)],0.005,1e-14,
         "positive W-face Fick mass");
    near(r.tracer_after_kg,1.0,1e-13,"six-face Fick mass conserved");
    near(r.max_diffusion_cfl,0.03,1e-14,
         "six-neighbor analytic diffusion CFL");
    std::cout<<"VOF_TRACER_DIFFUSION_ANALYTIC "<<std::setprecision(17)
             <<r.concentration_after_kg_m3[s.id(1,1,1)]<<" "
             <<r.concentration_after_kg_m3[s.id(2,1,1)]<<" "
             <<r.tracer_after_kg<<" "<<r.max_diffusion_cfl<<"\n";
    // A partially blocked 3D box leaves fractional cell volumes/apertures.
    s.box={{3.2,3.2,3.2},{4.8,4.8,4.8}};
    std::fill(s.f.begin(),s.f.end(),1.0);
    for(int z=0;z<8;++z)
    for(int y=0;y<8;++y)
    for(int x=0;x<8;++x)c[s.id(x,y,z)]=0.5+0.02*x;
    const auto g=upc::P2MovingBoxFaceApertures3D::sample(
        8,8,8,1.0,0.1,s.box,s.box);
    int partial=0;
    for(double a:g.u_open)if(a>0&&a<1)++partial;
    for(double a:g.v_open)if(a>0&&a<1)++partial;
    for(double a:g.w_open)if(a>0&&a<1)++partial;
    check(partial>0,"partial 3D face apertures exist");
    const auto q=Diff::step(
        8,8,8,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,0.02);
    near(q.tracer_after_kg,q.advected.tracer_before_kg,1e-10,
         "fractional cut-cell Fick mass conserved");
    check(q.max_diffusion_cfl>0&&q.max_diffusion_cfl<1,
          "fractional cut-cell Fick CFL safe");
    int checked_partial=0,checked_sealed=0;
    for(int z=0;z<8;++z)
    for(int y=0;y<8;++y)
    for(int x=0;x<=8;++x) {
        const auto j=s.ui(x,y,z);
        if(x==0||x==8||g.u_open[j]==0.0) {
            near(q.u_diffusion_flux_kg[j],0.0,0.0,
                 "sealed U-face cannot carry diffusive tracer");
            ++checked_sealed;
        } else if(g.u_open[j]>0&&g.u_open[j]<1) {
            const double expected=0.02*0.1*g.u_open[j]*
                (c[s.id(x-1,y,z)]-c[s.id(x,y,z)]);
            near(q.u_diffusion_flux_kg[j],expected,1e-15,
                 "fractional U-face Fick conductance scales with aperture");
            ++checked_partial;
        }
    }
    check(checked_partial>0&&checked_sealed>0,
          "fractional Fick aperture and sealed walls independently tested");
    // A uniform dissolved concentration is invariant under no-flux Fick
    // diffusion, including at the stationary solid boundary.
    s.box={{3.0,3.0,3.0},{4.0,4.0,4.0}};
    std::fill(s.f.begin(),s.f.end(),1.0);
    s.f[s.id(3,3,3)]=0.0;
    std::fill(c.begin(),c.end(),2.0);
    c[s.id(3,3,3)]=0.0;
    const auto uniform=Diff::step(
        8,8,8,1.0,0.1,s.box,s.f,c,s.u,s.v,s.w,0.05);
    for(std::size_t i=0;i<c.size();++i)
        near(uniform.concentration_after_kg_m3[i],c[i],1e-14,
             "uniform concentration invariant at impermeable solid");
    std::cout<<"VOF_TRACER_DIFFUSION_PARTIAL "<<partial<<" "
             <<std::setprecision(17)<<q.advected.tracer_before_kg<<" "
             <<q.tracer_after_kg<<"\n";
    rejects([&]{Diff::step(
        8,8,8,1,0.1,s.box,s.f,c,s.u,s.v,s.w,-0.1);},
        "negative diffusivity rejected");
    rejects([&]{Diff::step(
        8,8,8,1,0.1,s.box,s.f,c,s.u,s.v,s.w,
        std::numeric_limits<double>::quiet_NaN());},
        "NaN diffusivity rejected");
    rejects([&]{Diff::step(
        8,8,8,1,1.0,s.box,s.f,c,s.u,s.v,s.w,10.0);},
        "unstable explicit diffusion CFL rejected");
    rejects([&]{Diff::step(
        8,8,8,1,0.1,s.box,s.f,c,s.u,s.v,s.w,
        std::numeric_limits<double>::infinity());},
        "infinite diffusivity rejected");
    auto bad=c;
    bad[s.id(1,1,1)]=-0.1;
    rejects([&]{Diff::step(
        8,8,8,1,0.1,s.box,s.f,bad,s.u,s.v,s.w,0.02);},
        "negative initial tracer rejected");
    std::cout<<"VOF_TRACER_DIFFUSION_NEGATIVE 5\n";
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
        diffusive_tracer(8);
        diffusive_tracer(12);
        diffusion_analytic_and_partial();
        anisotropic_diffusion_reference(8);
        anisotropic_diffusion_reference(12);
        diffusion_subcycling_reference(8);
        diffusion_subcycling_reference(12);
        diffusion_subcycling_moving_and_fractional(8);
        diffusion_subcycling_moving_and_fractional(12);
        heterogeneous_diffusion_reference(8);
        heterogeneous_diffusion_reference(12);
        heterogeneous_subcycle_reference(8);
        heterogeneous_subcycle_reference(12);
        negatives();
        std::cout<<"PASS: "<<assertions
                 <<" native static 3D cut-cell VOF assertions\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
