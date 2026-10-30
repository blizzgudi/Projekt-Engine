#include "upc/p2_frozen_wrench_rigid_step3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t checks=0;
void check(bool v,const char* name) {
    ++checks;
    if(!v)throw std::runtime_error(name);
}
void near(double a,double b,double tolerance,const char* name) {
    check(std::isfinite(a)&&std::abs(a-b)<=tolerance,name);
}
template<class F> void rejects(F fn,const char* name) {
    bool rejected=false;
    try {fn();} catch(const std::exception&) {rejected=true;}
    check(rejected,name);
}
std::vector<double> pressure(int n) {
    std::vector<double> p(static_cast<std::size_t>(n)*n*n);
    for(int z=0;z<n;++z)
    for(int y=0;y<n;++y)
    for(int x=0;x<n;++x) {
        const double X=x+0.5,Y=y+0.5,Z=z+0.5;
        p[(static_cast<std::size_t>(z)*n+y)*n+x]=
            100+3*X-2*Y+4*Z+0.25*std::pow(X-4.5,3)*(Y-4.5);
    }
    return p;
}
void run(int n,double dx,int sign) {
    using Step=upc::P2FrozenWrenchRigidStep3D;
    using Box=upc::P2Box3D;
    const double dt=0.05;
    const Box old{{3.2*dx,3.1*dx,3.3*dx},{5.7*dx,5.6*dx,5.8*dx}};
    Box next=old;
    for(int a=0;a<3;++a) {
        const double d=sign*(a==0?0.11:(a==1?-0.08:0.09))*dx;
        next.min_m[a]+=d;
        next.max_m[a]+=d;
    }
    upc::RigidPressureState3D body;
    body.mass_kg=12;
    body.inertia_world_kg_m2={{0.45,0.63,0.78}};
    body.velocity_m_s={{0.15,-0.07,0.08}};
    body.angular_velocity_rad_s={{0.2,-0.1,0.15}};
    for(int a=0;a<3;++a)
        body.centre_m[a]=0.5*(old.min_m[a]+old.max_m[a]);
    const std::array<double,3> ref{{3.4*dx,4.9*dx,4.1*dx}};
    auto p=pressure(n);
    const auto unchanged=p;
    const auto r=Step::step(n,n,n,dx,dt,old,next,p,ref,body);
    check(p==unchanged,"pressure input unchanged");
    check(std::abs(r.measured.traction.solid_torque_nm[2])>1e-8,
          "intrinsic torque exists");
    check(r.prescribed_translation_mismatch_m>1e-6,
          "prescribed path differs from ODE response");
    double dE=0.0,work=0.0,mismatch_sq=0.0;
    for(int a=0;a<3;++a) {
        const double impulse=dt*r.load_about_body_centre.force_N[a];
        const double spin_impulse=dt*r.load_about_body_centre.torque_Nm[a];
        const double v=body.velocity_m_s[a]+impulse/body.mass_kg;
        const double omega=body.angular_velocity_rad_s[a]+
            spin_impulse/body.inertia_world_kg_m2[a];
        const double d=0.5*dt*(v+body.velocity_m_s[a]);
        const double theta=0.5*dt*(omega+body.angular_velocity_rad_s[a]);
        near(r.response.after.velocity_m_s[a],v,1e-13,"ODE velocity");
        near(r.response.after.angular_velocity_rad_s[a],omega,1e-13,"ODE spin");
        near(r.response.after.centre_m[a],body.centre_m[a]+d,1e-13,
             "ODE translation");
        near(r.response.body_linear_impulse_Ns[a],impulse,1e-13,
             "body linear impulse");
        near(r.response.body_angular_impulse_Nms[a],spin_impulse,1e-13,
             "body angular impulse");
        near(r.response.body_linear_impulse_Ns[a]+
             r.response.fluid_reaction_impulse_Ns[a],0,1e-14,
             "unapplied linear reaction");
        near(r.response.body_angular_impulse_Nms[a]+
             r.response.fluid_reaction_angular_impulse_Nms[a],0,1e-14,
             "unapplied angular reaction");
        dE+=0.5*body.mass_kg*(v*v-body.velocity_m_s[a]*body.velocity_m_s[a])+
            0.5*body.inertia_world_kg_m2[a]*
            (omega*omega-body.angular_velocity_rad_s[a]*body.angular_velocity_rad_s[a]);
        work+=r.load_about_body_centre.force_N[a]*d+
              r.load_about_body_centre.torque_Nm[a]*theta;
        const double prescribed_c=0.5*(next.min_m[a]+next.max_m[a]);
        mismatch_sq+=std::pow(body.centre_m[a]+d-prescribed_c,2);
    }
    near(r.response.kinetic_energy_change_J,dE,1e-12,"kinetic delta");
    near(r.response.frozen_pressure_work_J,work,1e-12,"frozen work");
    near(r.response.energy_residual_J,0,1e-12,"frozen ODE energy identity");
    near(r.prescribed_translation_mismatch_m,std::sqrt(mismatch_sq),1e-13,
         "prescribed displacement mismatch");
    // Changing the arbitrary reference cannot change body-centred torque.
    const std::array<double,3> ref2{{6.2*dx,2.3*dx,6.4*dx}};
    const auto r2=Step::step(n,n,n,dx,dt,old,next,p,ref2,body);
    for(int a=0;a<3;++a) {
        near(r2.load_about_body_centre.torque_Nm[a],
             r.load_about_body_centre.torque_Nm[a],1e-12,
             "reference-independent physical torque");
        near(r2.response.after.angular_velocity_rad_s[a],
             r.response.after.angular_velocity_rad_s[a],1e-12,
             "reference-independent angular response");
    }
    auto gauge=p;
    for(auto& v:gauge)v+=500;
    const auto shifted=Step::step(n,n,n,dx,dt,old,next,gauge,ref,body);
    for(int a=0;a<3;++a) {
        near(shifted.load_about_body_centre.force_N[a],
             r.load_about_body_centre.force_N[a],1e-10,
             "pressure gauge invariant force");
        near(shifted.load_about_body_centre.torque_Nm[a],
             r.load_about_body_centre.torque_Nm[a],1e-10,
             "pressure gauge invariant torque");
    }
    auto bad=body;
    bad.centre_m[0]+=dx;
    rejects([&]{Step::step(n,n,n,dx,dt,old,next,p,ref,bad);},
            "reject misregistered body centre");
    bad=body;
    bad.inertia_world_kg_m2[1]=0;
    rejects([&]{Step::step(n,n,n,dx,dt,old,next,p,ref,bad);},
            "reject zero inertia");
    bad=body;
    bad.angular_velocity_rad_s[0]=std::numeric_limits<double>::infinity();
    rejects([&]{Step::step(n,n,n,dx,dt,old,next,p,ref,bad);},
            "reject nonfinite angular velocity");
    rejects([&]{Step::step(n,n,n,dx,-dt,old,next,p,ref,body);},
            "reject negative dt");
    std::cout<<std::setprecision(17)<<"CASE n="<<n<<" dx="<<dx
             <<" sign="<<sign;
    for(int a=0;a<3;++a)
        std::cout<<" f"<<a<<"="<<r.load_about_body_centre.force_N[a];
    for(int a=0;a<3;++a)
        std::cout<<" t"<<a<<"="<<r.load_about_body_centre.torque_Nm[a];
    for(int a=0;a<3;++a)
        std::cout<<" v"<<a<<"="<<r.response.after.velocity_m_s[a];
    for(int a=0;a<3;++a)
        std::cout<<" w"<<a<<"="<<r.response.after.angular_velocity_rad_s[a];
    for(int a=0;a<3;++a)
        std::cout<<" c"<<a<<"="<<r.response.after.centre_m[a];
    std::cout<<" deltaE="<<r.response.kinetic_energy_change_J
             <<" work="<<r.response.frozen_pressure_work_J
             <<" residual="<<r.response.energy_residual_J
             <<" mismatch="<<r.prescribed_translation_mismatch_m<<"\n";
}
}
int main() {
    try {
        for(int n:{8,12})
            for(int sign:{-1,1})
                run(n,n==8?0.1:0.125,sign);
        std::cout<<"PASS: "<<checks
                 <<" native frozen-wrench rigid-body assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
