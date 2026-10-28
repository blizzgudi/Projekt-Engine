// P2 PARTIAL: moving cut-cell pressure projection of nonzero predicted
// MAC velocities. Independent curl/gradient decomposition and VOF checks.
// This is NOT a full free-surface or two-way FSI validation.
#include "upc/p2_moving_box_cut_cell_projection3d.hpp"
#include "upc/p2_fractional_boundary_vof_flux3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using Box=upc::P2Box3D;
using Solver=upc::P2MovingBoxCutCellProjection3DBuilder;
std::size_t assertions=0;
void check(bool condition,const char* message) {
    ++assertions;
    if(!condition)throw std::runtime_error(message);
}
void near(double a,double b,double tol,const char* message) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,message);
}
std::size_t ci(int n,int x,int y,int z) {return (z*n+y)*n+x;}
std::size_t ui(int n,int x,int y,int z) {return (z*n+y)*(n+1)+x;}
std::size_t vi(int n,int x,int y,int z) {return (z*(n+1)+y)*n+x;}
std::size_t wi(int n,int x,int y,int z) {return (z*n+y)*n+x;}
struct Pred {
    std::vector<double> u,v,w;
    explicit Pred(int n):u((n+1)*n*n,0.0),
                         v(n*(n+1)*n,0.0),
                         w(n*n*(n+1),0.0) {}
};
void verify_divergence(int n,const upc::P2MovingBoxCutCellProjection3D& r) {
    const auto& g=r.wall.geometry.cell_geometry;
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=ci(n,x,y,z);
        const double div=r.u_fluid_m3[ui(n,x+1,y,z)]-r.u_fluid_m3[ui(n,x,y,z)]
            +r.v_fluid_m3[vi(n,x,y+1,z)]-r.v_fluid_m3[vi(n,x,y,z)]
            +r.w_fluid_m3[wi(n,x,y,z+1)]-r.w_fluid_m3[wi(n,x,y,z)];
        near(div,g.alpha_old[i]-g.alpha_new[i],2e-10,
             "predicted moving cut-cell local GCL");
    }
}
void run(int n,int axis,int sign,double shift) {
    Box old{{3.2,3.1,3.3},{4.7,4.6,4.8}}, moved=old;
    moved.min_m[axis]+=sign*shift;
    moved.max_m[axis]+=sign*shift;
    constexpr double dt=0.25, rho=1000.0, circulation=0.02, chi=0.03;
    const auto base=Solver::sample(n,n,n,1.0,dt,rho,old,moved);
    // Independent exact discrete curl: one closed 4-cell loop.
    Pred curl(n);
    curl.u[ui(n,2,1,1)]=circulation/dt;
    curl.v[vi(n,2,2,1)]=circulation/dt;
    curl.u[ui(n,2,2,1)]=-circulation/dt;
    curl.v[vi(n,1,2,1)]=-circulation/dt;
    // Independent discrete gradient: chi=0.03 at cell (1,1,1).
    // Pressure projection must remove its 6 face fluxes.
    Pred gradient(n);
    gradient.u[ui(n,1,1,1)]=-chi/dt;
    gradient.u[ui(n,2,1,1)]=chi/dt;
    gradient.v[vi(n,1,1,1)]=-chi/dt;
    gradient.v[vi(n,1,2,1)]=chi/dt;
    gradient.w[wi(n,1,1,1)]=-chi/dt;
    gradient.w[wi(n,1,1,2)]=chi/dt;
    Pred combined(n);
    for(std::size_t i=0;i<combined.u.size();++i)
        combined.u[i]=curl.u[i]+gradient.u[i];
    for(std::size_t i=0;i<combined.v.size();++i)
        combined.v[i]=curl.v[i]+gradient.v[i];
    for(std::size_t i=0;i<combined.w.size();++i)
        combined.w[i]=curl.w[i]+gradient.w[i];
    const auto corrected=Solver::sample_with_predictor(
        n,n,n,1.0,dt,rho,old,moved,combined.u,combined.v,combined.w);
    verify_divergence(n,corrected);
    near(corrected.max_continuity_residual_m3,0.0,2e-10,
         "predictor pressure convergence");
    double max_flux_error=0.0;
    const auto compare=[&](const std::vector<double>& actual,
                           const std::vector<double>& original,
                           const std::vector<double>& curl_velocity) {
        for(std::size_t i=0;i<actual.size();++i) {
            const double expected=original[i]+curl_velocity[i]*dt;
            max_flux_error=std::max(max_flux_error,std::abs(actual[i]-expected));
            near(actual[i],expected,3e-10,
                 "discrete curl survives and gradient is removed");
        }
    };
    compare(corrected.u_fluid_m3,base.u_fluid_m3,curl.u);
    compare(corrected.v_fluid_m3,base.v_fluid_m3,curl.v);
    compare(corrected.w_fluid_m3,base.w_fluid_m3,curl.w);
    // Pressure differences must cancel exactly the independent
    // gradient; gauge offsets are intentionally ignored.
    const auto a=ci(n,1,1,1),b=ci(n,0,1,1);
    const double p_diff=(corrected.pressure_pa[a]-corrected.pressure_pa[b])
                        -(base.pressure_pa[a]-base.pressure_pa[b]);
    near(p_diff,-rho*chi/(dt*dt),2e-6,
         "independent pressure gradient scale");
    // Saturated VOF must still fill the exact new fluid-open volumes.
    const auto& geom=corrected.wall.geometry.cell_geometry;
    auto vel=[&](const std::vector<double>& flux) {
        auto out=flux;
        for(auto& q:out)q/=dt;
        return out;
    };
    const auto water=upc::FractionalBoundaryVofFlux3D::step(
        n,n,n,1.0,dt,geom.alpha_old,geom.alpha_new,geom.alpha_old,
        vel(corrected.u_fluid_m3),vel(corrected.v_fluid_m3),
        vel(corrected.w_fluid_m3));
    near(water.mass_residual_m3,0.0,1e-9,"saturated VOF mass");
    for(std::size_t i=0;i<geom.alpha_new.size();++i)
        near(water.water_cell_fraction[i],geom.alpha_new[i],3e-10,
             "saturated moving cut-cell VOF");
    std::cout<<std::setprecision(17)<<"PRED "<<n<<" "<<axis<<" "<<sign
             <<" "<<shift<<" "<<max_flux_error<<" "<<p_diff
             <<" "<<corrected.max_continuity_residual_m3<<"\n";
}
void invalid_inputs() {
    const int n=8;
    // Aligned 2x2x2 solid guarantees a truly closed internal face.
    Box b{{3.0,3.0,3.0},{5.0,5.0,5.0}};
    Pred p(n);
    const auto reject=[&](const Pred& x,std::size_t budget=4000) {
        bool threw=false;
        try {
            (void)Solver::sample_with_predictor(n,n,n,1.0,0.25,1000.0,
                                                b,b,x.u,x.v,x.w,budget);
        }catch(const std::exception&){threw=true;}
        check(threw,"invalid predicted MAC velocity must be rejected");
    };
    p.u[ui(n,0,1,1)]=0.1;
    reject(p);
    p.u[ui(n,0,1,1)]=0.0;
    p.u[ui(n,4,4,4)]=0.1; // box interior is closed for this face
    reject(p);
    p.u[ui(n,4,4,4)]=0.0;
    p.v[vi(n,1,1,1)]=std::numeric_limits<double>::quiet_NaN();
    reject(p);
    p.v[vi(n,1,1,1)]=0.0;
    p.w.pop_back();
    reject(p);
    // Invalid iteration budget must not alter any input predictor.
    p.w.push_back(0.0);
    reject(p,0);
    check(std::all_of(p.u.begin(),p.u.end(),[](double q){return q==0.0;}),
          "predictor input remains unmodified");
}
}
int main() {
    try {
        for(int n:{8,12,16})for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})for(double shift:{0.25,0.5})
                run(n,axis,sign,shift);
        invalid_inputs();
        std::cout<<"PASS: "<<assertions
                 <<" native predicted MAC cut-cell assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
