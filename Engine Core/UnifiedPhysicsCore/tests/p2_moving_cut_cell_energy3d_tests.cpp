// P2 PARTIAL: independent moving-wall pressure/kinetic identities.
// Closed, fully flooded, prescribed translating box. NOT physical FSI.
#include "upc/p2_moving_cut_cell_energy3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool good,const char* why) {
    ++assertions;
    if(!good)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    check(std::isfinite(a)&&std::abs(a-b)<=tol,why);
}
using Box=upc::P2Box3D;
using Energy=upc::P2MovingCutCellPressureEnergy3D;
using Projection=upc::P2MovingBoxCutCellProjection3DBuilder;
std::size_t ui(int n,int x,int y,int z){return (z*n+y)*(n+1)+x;}
std::size_t vi(int n,int x,int y,int z){return (z*(n+1)+y)*n+x;}
std::size_t wi(int n,int x,int y,int z){return (z*n+y)*n+x;}
std::size_t ci(int n,int x,int y,int z){return (z*n+y)*n+x;}
struct Predictor {
    std::vector<double> u,v,w;
    explicit Predictor(int n):u((n+1)*n*n,0.0),
        v(n*(n+1)*n,0.0),w(n*n*(n+1),0.0) {}
};
Predictor manufactured(int n) {
    Predictor p(n);
    constexpr double dt=0.25,chi=0.03,curl=0.02;
    // Discrete curl plus a compact six-face gradient far from the box.
    p.u[ui(n,2,1,1)]+=curl/dt;
    p.v[vi(n,2,2,1)]+=curl/dt;
    p.u[ui(n,2,2,1)]-=curl/dt;
    p.v[vi(n,1,2,1)]-=curl/dt;
    p.u[ui(n,1,1,1)]-=chi/dt;
    p.u[ui(n,2,1,1)]+=chi/dt;
    p.v[vi(n,1,1,1)]-=chi/dt;
    p.v[vi(n,1,2,1)]+=chi/dt;
    p.w[wi(n,1,1,1)]-=chi/dt;
    p.w[wi(n,1,1,2)]+=chi/dt;
    return p;
}
void run(int n,int axis,int sign,bool manufactured_predictor) {
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    Box next=b;
    next.min_m[axis]+=sign*0.25;
    next.max_m[axis]+=sign*0.25;
    const auto p=manufactured_predictor?manufactured(n):Predictor(n);
    auto result=Projection::sample_with_predictor(
        n,n,n,1.0,0.25,1000.0,b,next,p.u,p.v,p.w);
    const auto e=Energy::audit(n,n,n,1.0,0.25,1000.0,
                               p.u,p.v,p.w,result);
    near(e.predicted_identity_residual_j,0.0,2e-7,
         "predictor pressure-work identity");
    near(e.moving_wall_identity_residual_j,0.0,2e-7,
         "moving wall pressure-work identity");
    near(e.projected_work_geometry_residual_j,0.0,2e-7,
         "projected pressure work equals geometric source");
    near(e.max_pressure_gradient_residual_m_s,0.0,2e-10,
         "independent pressure gradient correction");
    near(e.max_continuity_residual_m3,0.0,2e-10,
         "independent cut-cell continuity");
    check(e.projected_face_kinetic_j>0.0,"moving box produces fluid kinetic proxy");
    check(e.correction_energy_j>0.0,"pressure correction nonzero");
    if(!manufactured_predictor) {
        near(e.predicted_face_kinetic_j,0.0,0.0,"zero predictor kinetic");
        near(e.projected_face_kinetic_j,e.correction_energy_j,2e-8,
             "zero predictor exact kinetic reference");
        near(e.geometric_pressure_work_j,2.0*e.correction_energy_j,2e-7,
             "independent moving-wall pressure work = twice correction energy");
    } else {
        check(e.predicted_face_kinetic_j>0.0,"manufactured predictor kinetic");
    }
    // Pressure is gauge-free: adding a constant cannot change the energy work.
    for(auto& pressure:result.pressure_pa)pressure+=175.0;
    const auto shifted=Energy::audit(n,n,n,1.0,0.25,1000.0,
                                    p.u,p.v,p.w,result);
    near(shifted.geometric_pressure_work_j,e.geometric_pressure_work_j,
         2e-7,"gauge invariance of geometric work");
    near(shifted.projected_face_kinetic_j,e.projected_face_kinetic_j,
         1e-10,"gauge invariance of kinetic energy");
    std::cout<<std::setprecision(17)<<"CUTENERGY "<<n<<" "<<axis<<" "<<sign
             <<" "<<(manufactured_predictor?1:0)<<" "
             <<e.predicted_face_kinetic_j<<" "<<e.projected_face_kinetic_j
             <<" "<<e.geometric_pressure_work_j<<" "<<e.correction_energy_j
             <<" "<<e.moving_wall_identity_residual_j<<" "
             <<e.max_continuity_residual_m3<<"\n";
}
void sequential(int n,int axis,int sign) {
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    upc::P2AuditedPrescribedBoxMacVof3D sim(n,n,n,1.0,1000.0,b);
    const auto p=manufactured(n);
    for(int step=1;step<=2;++step) {
        Box next=sim.state().box();
        next.min_m[axis]+=sign*0.25;
        next.max_m[axis]+=sign*0.25;
        const auto result=sim.advance(next,0.25,p.u,p.v,p.w);
        check(result.state.step_index==static_cast<std::size_t>(step),
              "audited step index");
        check(sim.state().step_count()==static_cast<std::size_t>(step),
              "audited committed state");
        near(result.energy.moving_wall_identity_residual_j,0.0,2e-7,
             "sequential moving pressure energy");
        double water=0.0;
        for(std::size_t i=0;i<sim.state().water_fraction().size();++i) {
            water+=sim.state().water_fraction()[i];
            near(sim.state().water_fraction()[i],sim.state().open_fraction()[i],
                 5e-10,"saturated VOF follows open geometry");
        }
        near(water,n*n*n-3.375,2e-9,"independent water volume");
    }
    const auto previous=sim.state().water_fraction();
    const auto previous_box=sim.state().box();
    const auto previous_mac=sim.state().corrected_u_m_s();
    auto bad=p;
    bad.u[ui(n,0,1,1)]=1.0;
    bool rejected=false;
    try{sim.advance(previous_box,0.25,bad.u,bad.v,bad.w);}
    catch(const std::exception&){rejected=true;}
    check(rejected,"bad audited step rejected");
    check(sim.state().water_fraction()==previous &&
          sim.state().corrected_u_m_s()==previous_mac &&
          sim.state().box().min_m==previous_box.min_m &&
          sim.state().step_count()==2,"audited transaction rollback");
    std::cout<<"AUDITED "<<n<<" "<<axis<<" "<<sign<<" "<<2<<" "
             <<std::setprecision(17)<<n*n*n-3.375<<"\n";
}
template<typename Fn> void reject(Fn fn,const char* why) {
    bool failed=false;
    try{fn();}catch(const std::exception&){failed=true;}
    check(failed,why);
}
void negative() {
    constexpr int n=8;
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    Box next=b;next.min_m[0]+=0.25;next.max_m[0]+=0.25;
    const auto p=manufactured(n);
    const auto good=Projection::sample_with_predictor(
        n,n,n,1.0,0.25,1000.0,b,next,p.u,p.v,p.w);
    auto audit=[&](const Predictor& q,
                   const upc::P2MovingBoxCutCellProjection3D& projection) {
        return Energy::audit(n,n,n,1.0,0.25,1000.0,
                             q.u,q.v,q.w,projection);
    };
    {
        auto x=good;x.pressure_pa[ci(n,2,2,2)]+=100.0;
        reject([&]{audit(p,x);},"pressure tampering");
    }
    {
        auto x=good;x.u_fluid_m3[ui(n,2,1,1)]+=0.1;
        reject([&]{audit(p,x);},"flux tampering");
    }
    {
        auto x=good;x.wall.geometry.cell_geometry.alpha_old[ci(n,1,1,1)]+=0.2;
        reject([&]{audit(p,x);},"geometry continuity tampering");
    }
    {
        auto x=good;x.wall.geometry.u_open[ui(n,1,1,1)]=2.0;
        reject([&]{audit(p,x);},"invalid aperture");
    }
    {
        auto x=good;x.pressure_pa[0]=std::numeric_limits<double>::quiet_NaN();
        reject([&]{audit(p,x);},"nonfinite pressure");
    }
    {
        auto x=good;x.u_fluid_m3[ui(n,0,1,1)]=0.2;
        reject([&]{audit(p,x);},"outer boundary flux");
    }
    {
        auto x=p;x.u.pop_back();
        reject([&]{audit(x,good);},"predictor shape");
    }
    {
        auto x=p;x.v[vi(n,0,0,0)]=std::numeric_limits<double>::infinity();
        reject([&]{audit(x,good);},"nonfinite predictor");
    }
    {
        auto x=p;x.u[ui(n,0,1,1)]=1.0;
        reject([&]{audit(x,good);},"outer boundary predictor");
    }
    reject([&]{Energy::audit(n,n,n,1.0,0.0,1000.0,p.u,p.v,p.w,good);},
           "invalid dt");
}
}
int main() {
    try {
        for(int n:{8,12,16})
            for(int axis=0;axis<3;++axis)
                for(int sign:{-1,1}) {
                    run(n,axis,sign,false);
                    run(n,axis,sign,true);
                    sequential(n,axis,sign);
                }
        negative();
        std::cout<<"PASS: "<<assertions
                 <<" native moving cut-cell pressure energy assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
