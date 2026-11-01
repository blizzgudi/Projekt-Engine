// P2 PARTIAL: automatic local volume-matched PLIC normals in LIVE MAC-PCG.
// Synthetic planar geometry, NOT stress-consistent two-phase CFD or MARIN acceptance.
#include "upc/coupled_water3d.hpp"
#include "upc/p2_oblique_plic_geometry3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using Vec=std::array<double,3>;
static int checks=0;
static void check(bool ok,const char* why) {
    ++checks;
    if(!ok)throw std::runtime_error(why);
}
template<class F>static void reject(F fn,const char* why) {
    bool failed=false;
    try{fn();}catch(const std::exception&){failed=true;}
    check(failed,why);
}
static double dot(Vec a,Vec b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
static std::vector<double> make_plane(int n,Vec normal,double cut) {
    std::vector<double> f(n*n*n);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x){
        const double c=cut-dot(normal,{double(x),double(y),double(z)});
        const double v=std::clamp(upc::p2_oblique_detail::clip_cube(normal,c).volume,0.,1.);
        f[(z*n+y)*n+x]=v<1e-12?0.:v>1.-1e-12?1.:v;
    }
    return f;
}
static void load(upc::CoupledMacWater3D& w,int n,const std::vector<double>& f){
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x)
        w.set_water(x,y,z,f[(z*n+y)*n+x]);
}
static void run(int n,Vec normal,double cut) {
    upc::CoupledMacWater3D water(n,n,n,.1,1000.);
    const auto initial=make_plane(n,normal,cut);
    load(water,n,initial);
    const double before=water.mass_kg();
    check(!water.auto_planar_vof_pcg_normals_enabled(),"auto must default OFF");
    reject([&]{water.set_planar_vof_pcg_geometry(true);},"no geometry without PCG/normals");
    water.set_pcg_pressure_projection(true);
    reject([&]{water.set_auto_planar_vof_pcg_normals(true,0);},"invalid zero fit tolerance");
    reject([&]{water.set_auto_planar_vof_pcg_normals(true,
        std::numeric_limits<double>::quiet_NaN());},"invalid NaN fit tolerance");
    water.set_auto_planar_vof_pcg_normals(true,1e-7);
    check(water.auto_planar_vof_pcg_normals_enabled(),"auto enabled");
    reject([&]{water.set_planar_vof_pcg_normals(std::vector<Vec>(n*n*n));},
           "manual normals rejected while auto active");
    water.set_planar_vof_pcg_geometry(true);
    reject([&]{water.set_auto_planar_vof_pcg_normals(false);},
           "source cannot switch under active geometry");
    reject([&]{water.step(1e-7);},"unverified geometry must reject");
    const double divergence=water.step_verified(1e-7,3000,1e-7);
    check(divergence<1e-7,"live auto PLIC PCG projection");
    check(water.last_auto_planar_vof_fitted_cells()>n*n/2,
          "live fitted more than half an interface layer");
    check(water.last_auto_planar_vof_patch_residual()<1e-7,
          "live local volume residual within strict tolerance");
    check(water.last_planar_vof_oblique_faces()>0,
          "live oblique ghost fluid faces");
    check(water.last_planar_vof_two_partial_faces()>0,
          "live two partial cell geometry");
    check(water.last_pcg_projection_iterations()>0,"live PCG iterations");
    check(std::abs(water.mass_kg()-before)<1e-7,"live VOF mass conservation");
    // The opt-in second projection must refit after actual VOF transport.
    // A microscopic step preserves this manufactured planar patch; a
    // general nonplanar transported patch is still correctly rejected.
    upc::CoupledMacWater3D post(n,n,n,.1,1000.);
    load(post,n,initial);
    post.set_pcg_pressure_projection(true);
    post.set_auto_planar_vof_pcg_normals(true,1e-7);
    post.set_planar_vof_pcg_geometry(true);
    const double post_mass=post.mass_kg();
    const double post_div=post.step_verified_reprojected(1e-10,3000,1e-7);
    check(post_div<1e-7,"post-transport automatic PLIC projection");
    check(post.last_auto_planar_vof_fitted_cells()>0,
          "post-transport PLIC refit executed");
    check(post.last_planar_vof_oblique_faces()>0,
          "post-transport oblique geometry applied");
    check(std::abs(post.mass_kg()-post_mass)<1e-7,
          "post-transport water mass conserved");
    const auto first_faces=water.last_planar_vof_resolved_faces();
    const auto first_fitted=water.last_auto_planar_vof_fitted_cells();
    // Replacing the full VOF plane must recompute normals from CURRENT fill.
    // The old fit cannot simply be cached or prescribed by the caller.
    const auto moved=make_plane(n,normal,cut+.2);
    load(water,n,moved);
    const double changed_mass=water.mass_kg();
    const double second_div=water.step_verified(1e-7,3000,1e-7);
    check(second_div<1e-7,"recomputed live plane PCG projection");
    check(water.last_auto_planar_vof_fitted_cells()>0,"second frame fitted");
    check(water.last_auto_planar_vof_patch_residual()<1e-7,
          "second frame geometric fit verified");
    check(std::abs(water.mass_kg()-changed_mass)<1e-7,
          "second frame water mass conserved");
    // Inject a nonplanar VOF patch: the verified step must roll back
    // both pressure and water state rather than accepting a false plane.
    auto bad=moved;
    bool changed=false;
    for(std::size_t i=0;i<bad.size();++i)if(bad[i]>1e-8&&bad[i]<1.-1e-8){
        bad[i]=std::min(1.,bad[i]+.08);changed=true;break;
    }
    check(changed,"a real partial cell was corrupted");
    load(water,n,bad);
    const double failure_mass=water.mass_kg();
    const double failure_pressure=water.pressure_pa(2,2,2);
    const auto failure_iterations=water.last_pcg_projection_iterations();
    reject([&]{(void)water.step_verified(1e-7,3000,1e-7);},
           "nonplanar live patch must fail closed");
    check(water.mass_kg()==failure_mass,"failure leaves water unchanged");
    check(water.pressure_pa(2,2,2)==failure_pressure,
          "failure leaves pressure unchanged");
    check(water.last_pcg_projection_iterations()==failure_iterations,
          "failure leaves diagnostics unchanged");
    water.set_planar_vof_pcg_geometry(false);
    check(water.last_auto_planar_vof_fitted_cells()==0,
          "disabling geometry clears fit diagnostics");
    water.set_auto_planar_vof_pcg_normals(false);
    check(!water.auto_planar_vof_pcg_normals_enabled(),
          "can disable auto source after geometry");
    std::cout<<"LIVE_AUTO_PLIC n="<<n
             <<" first_fitted="<<first_fitted
             <<" first_faces="<<first_faces
             <<" first_div="<<divergence
             <<" second_div="<<second_div
             <<" rollback=PASS\n";
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        run(8,{1./std::sqrt(2.),1./std::sqrt(2.),0.},6.17/std::sqrt(2.));
        run(12,{1./std::sqrt(3.),1./std::sqrt(3.),1./std::sqrt(3.)},
            10.81/std::sqrt(3.));
        std::cout<<"PASS P2_LIVE_AUTO_PLANAR assertions="<<checks
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"FAIL P2_LIVE_AUTO_PLANAR assertions="<<checks
                 <<" reason="<<e.what()<<"\n";
        return 1;
    }
}
