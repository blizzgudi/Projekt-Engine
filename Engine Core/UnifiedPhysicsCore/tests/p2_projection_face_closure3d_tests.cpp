// P2 partial: complete staggered-face projection partition and fault detection.
// Numerical face-mass proxy only; NOT a physical FSI conservation claim.
#include "upc/p2_projection_face_closure3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::size_t assertions=0;
void check(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
void near(double a,double b,double tol,const char* why) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,why);
}
struct FaceChange {int axis,x,y,z;double dv;};
void run(int n,double dx,double rho,double dt) {
    const int c=n/2;
    const auto idx=[&](int x,int y,int z) {
        return static_cast<std::size_t>((z*n+y)*n+x);
    };
    std::vector<std::uint8_t> wet(static_cast<std::size_t>(n)*n*n,0),solid(wet);
    wet[idx(c,c,c)]=1;
    solid[idx(c+1,c,c)]=1;
    upc::MacProjection3D mac(n,n,n,dx,rho);
    mac.set_cell(c,c,c,true);
    mac.set_cell(c+1,c,c,false,true);
    mac.set_face_velocity_x(c,c,c,-0.06);
    mac.set_face_velocity_x(c+1,c,c,0.25);
    mac.set_face_velocity_y(c,c,c,0.03);
    mac.set_face_velocity_y(c,c+1,c,-0.11);
    mac.set_face_velocity_z(c,c,c,0.05);
    mac.set_face_velocity_z(c,c,c+1,0.08);
    mac.set_face_velocity_y(c+1,c+1,c,-0.17);
    mac.set_face_velocity_z(c,c,0,0.31);
    mac.set_face_velocity_x(1,1,1,0.12); // dry-dry: must NOT be projected
    const auto before=mac;
    mac.project(dt,4000);
    const std::array<double,3> centre{{
        (c+0.2)*dx,(c-0.1)*dx,(c+0.15)*dx
    }};
    const auto audit=[&](const upc::MacProjection3D& state) {
        return upc::ProjectionFaceClosureAudit3D::analyze(
            before,state,n,n,n,dx,rho,dt,wet,solid,centre);
    };
    const auto b=audit(mac);
    const double mass=rho*dx*dx*dx;
    const double pressure=rho*dx*0.005/dt;
    near(mac.pressure_pa(c,c,c),pressure,1e-10,"independent five-face Poisson pressure");
    near(mac.face_velocity_x(c,c,c),-0.07,1e-12,"x negative pressure gradient");
    near(mac.face_velocity_x(c+1,c,c),0,0,"x positive solid clamp");
    near(mac.face_velocity_y(c,c,c),0.02,1e-12,"y negative pressure gradient");
    near(mac.face_velocity_y(c,c+1,c),-0.10,1e-12,"y positive pressure gradient");
    near(mac.face_velocity_z(c,c,c),0.04,1e-12,"z negative pressure gradient");
    near(mac.face_velocity_z(c,c,c+1),0.09,1e-12,"z positive pressure gradient");
    near(mac.face_velocity_y(c+1,c+1,c),0,0,"solid-air clamp");
    near(mac.face_velocity_z(c,c,0),0,0,"exterior clamp");
    near(mac.face_velocity_x(1,1,1),0.12,0,"dry-dry velocity preserved");
    check(b.pressure_faces==5,"five wet-air gradient faces");
    check(b.clamped_faces==static_cast<std::size_t>(6*n*n+6),
          "all exterior and solid faces");
    check(b.dry_dry_faces==static_cast<std::size_t>(3*n*n*n-3*n*n-11),
          "all dry-dry faces enumerated");
    check(b.pressure_faces+b.clamped_faces+b.dry_dry_faces==
          static_cast<std::size_t>(3*(n+1)*n*n),"entire MAC face partition");
    check(b.changed_dry_dry_faces==0&&b.max_dry_dry_delta_m_s==0,
          "dry-dry velocity unchanged");
    check(b.nonzero_clamped_after_faces==0,"all clamped velocities zero");
    check(b.components.wet_air_faces==5&&b.components.wet_wet_faces==0,
          "gradient face classification");
    check(b.components.clamp_solid_faces==6&&
          b.components.clamp_exterior_faces==static_cast<std::size_t>(6*n*n),
          "clamp face classification");
    const std::array<double,3> expected_linear{{-0.26*mass,0.17*mass,-0.31*mass}};
    for(int a=0;a<3;++a)
        near(b.all_active_linear_Ns[a],expected_linear[a],1e-12,
             "independent total face impulse");
    const double expected_energy=-0.094*mass;
    near(b.all_active_energy_delta_proxy_J,expected_energy,1e-12,
         "independent total face energy");
    // Independent angular reference from the eight nonzero face deltas.
    const std::array<FaceChange,8> changes{{
        {0,c,c,c,-0.01}, {1,c,c,c,-0.01},
        {1,c,c+1,c,0.01}, {2,c,c,c,-0.01},
        {2,c,c,c+1,0.01}, {0,c+1,c,c,-0.25},
        {1,c+1,c+1,c,0.17}, {2,c,c,0,-0.31}
    }};
    std::array<double,3> expected_angular{};
    for(const auto& f:changes) {
        const std::array<double,3> r{{
            (f.x+(f.axis!=0?0.5:0.0))*dx-centre[0],
            (f.y+(f.axis!=1?0.5:0.0))*dx-centre[1],
            (f.z+(f.axis!=2?0.5:0.0))*dx-centre[2]
        }};
        const double impulse=mass*f.dv;
        expected_angular[(f.axis+1)%3]+=r[(f.axis+2)%3]*impulse;
        expected_angular[(f.axis+2)%3]-=r[(f.axis+1)%3]*impulse;
    }
    for(int a=0;a<3;++a) {
        near(b.all_active_angular_Nms[a],expected_angular[a],1e-12,
             "independent angular impulse");
        near(b.decomposition_linear_residual_Ns[a],0,1e-12,
             "full-face gradient+clamp linear closure");
        near(b.decomposition_angular_residual_Nms[a],0,1e-12,
             "full-face gradient+clamp angular closure");
    }
    near(b.decomposition_energy_residual_proxy_J,0,1e-12,
         "full-face gradient+clamp energy closure");
    check(b.components.max_gradient_velocity_residual_m_s<1e-12,
          "actual gradient matches pressure");
    check(b.components.max_divergence_after_s_inv<1e-10,
          "3D wet-cell divergence after projection");
    // Fault injection 1: previously unaccounted dry-dry face must be flagged.
    auto corrupted=mac;
    corrupted.set_face_velocity_x(1,1,1,0.13);
    const auto dry_fault=audit(corrupted);
    check(dry_fault.changed_dry_dry_faces==1,"detect dry-dry corruption");
    near(dry_fault.max_dry_dry_delta_m_s,0.01,1e-14,
         "quantify dry-dry corruption");
    near(dry_fault.all_active_energy_delta_proxy_J,expected_energy,1e-12,
         "dry-dry face not counted as liquid mass");
    // Fault injection 2: a clamped face erroneously left moving.
    corrupted=mac;
    corrupted.set_face_velocity_x(c+1,c,c,0.04);
    const auto clamp_fault=audit(corrupted);
    check(clamp_fault.nonzero_clamped_after_faces==1,
          "detect failed solid clamp");
    near(clamp_fault.components.max_clamp_after_velocity_m_s,0.04,1e-14,
         "quantify failed solid clamp");
    // Fault injection 3: an incorrect pressure-gradient writeback.
    corrupted=mac;
    corrupted.set_face_velocity_y(c,c,c,0.05);
    const auto gradient_fault=audit(corrupted);
    check(gradient_fault.components.max_gradient_velocity_residual_m_s>0.029,
          "detect incorrect gradient writeback");
    // Bad masks must never be accepted as a valid projection audit.
    auto invalid_wet=wet;
    invalid_wet[idx(c+1,c,c)]=1;
    bool rejected=false;
    try { (void)upc::ProjectionFaceClosureAudit3D::analyze(
        before,mac,n,n,n,dx,rho,dt,invalid_wet,solid,centre);
    } catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"reject overlapping fluid and solid masks");
    double max_residual=std::abs(b.decomposition_energy_residual_proxy_J);
    for(int a=0;a<3;++a)
        max_residual=std::max({max_residual,
            std::abs(b.decomposition_linear_residual_Ns[a]),
            std::abs(b.decomposition_angular_residual_Nms[a])});
    std::cout<<"case,"<<n<<","<<dx<<","<<rho<<","<<dt<<","<<pressure;
    for(double v:b.all_active_linear_Ns)std::cout<<","<<v;
    for(double v:b.all_active_angular_Nms)std::cout<<","<<v;
    std::cout<<","<<b.all_active_energy_delta_proxy_J
             <<","<<b.pressure_faces<<","<<b.clamped_faces
             <<","<<b.dry_dry_faces<<","<<b.changed_dry_dry_faces
             <<","<<b.nonzero_clamped_after_faces<<","<<max_residual
             <<","<<b.components.max_gradient_velocity_residual_m_s<<"\n";
}
}
int main() {
    try {
        std::cout<<std::setprecision(17);
        std::cout<<"mode,n,dx,rho,dt,pressure,px,py,pz,Lx,Ly,Lz,energy,"
                    "pressure_faces,clamped_faces,dry_faces,dry_changed,"
                    "clamp_nonzero,max_residual,gradient_error\n";
        run(8,0.1,1000.0,0.02);
        run(12,0.08,1100.0,0.015);
        std::cout<<"PASS: P2 all-face projection closure assertions="
                 <<assertions<<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" after "<<assertions<<" assertions\n";
        return 1;
    }
}
