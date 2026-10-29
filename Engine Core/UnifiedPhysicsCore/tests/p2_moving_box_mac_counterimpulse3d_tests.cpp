#include "upc/p2_moving_box_mac_counterimpulse3d.hpp"
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
using Step=upc::P2MovingBoxMacCounterImpulse3D;
using Faces=Step::Faces;
using Box=upc::P2Box3D;
using State=upc::RigidPressureState3D;
std::size_t checks=0;
void require(bool yes,const char* what) {
    ++checks;if(!yes)throw std::runtime_error(what);
}
void near(double x,double y,double eps,const char* what) {
    require(std::isfinite(x)&&std::abs(x-y)<=eps,what);
}
template<class F>void rejects(F f,const char* what) {
    bool failed=false;try{f();}catch(const std::exception&){failed=true;}
    require(failed,what);
}
Faces zero_faces(int n) {
    return {{std::vector<double>((n+1)*n*n,0.0),
             std::vector<double>(n*(n+1)*n,0.0),
             std::vector<double>(n*n*(n+1),0.0)}};
}
std::vector<double> gradient(int n,int axis,int sign) {
    std::vector<double> p(n*n*n);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const int xyz[3]={x,y,z};
        p[(z*n+y)*n+x]=100+10*sign*(xyz[axis]+0.5);
    }
    return p;
}
void analytic(int n,int axis,int sign) {
    Box box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    State s;s.mass_kg=10.0;s.centre_m={{4,4,4}};
    const auto pressure=gradient(n,axis,sign);
    Faces velocities=zero_faces(n);
    const auto initial=velocities;
    double total_body=0.0,total_fluid=0.0;
    for(int k=1;k<=2;++k) {
        const auto r=Step::step(n,n,n,1.0,0.1,1000.0,
                                box,s,pressure,velocities);
        const double expected_body=-2.56*sign;
        const double expected_fluid=2.56*sign;
        near(r.body.rigid.body_linear_impulse_Ns[axis],
             expected_body,1e-12,"analytic body impulse");
        near(r.applied_fluid_impulse_Ns[axis],
             expected_fluid,1e-10,"applied fluid impulse");
        for(int a=0;a<3;++a) {
            require(r.updated_faces[a]>0,"nonempty 3D open dual shell");
            require(r.selected_dual_mass_kg[a]>0,"positive dual masses");
            near(r.total_impulse_residual_Ns[a],0,1e-10,
                 "body and MAC impulse close");
            near(r.applied_fluid_impulse_Ns[a],
                 -r.body.rigid.body_linear_impulse_Ns[a],1e-10,
                 "equal opposite physical units");
            near(r.face_delta_velocity_m_s[a]*
                 r.selected_dual_mass_kg[a],
                 r.applied_fluid_impulse_Ns[a],1e-10,
                 "independent mass times velocity reference");
            if(a!=axis)near(r.applied_fluid_impulse_Ns[a],0,1e-10,
                            "no transverse reaction");
            require(r.velocity_after_m_s[a].size()==velocities[a].size(),
                    "MAC shape preserved");
        }
        require(r.max_geometric_continuity_residual_m3>1e-8,
                "missing projection honestly reported");
        require(std::isfinite(r.selected_face_energy_change_proxy_J),
                "finite face energy proxy");
        total_body+=r.body.rigid.body_linear_impulse_Ns[axis];
        total_fluid+=r.applied_fluid_impulse_Ns[axis];
        box=r.body.box_after;s=r.body.rigid.after;
        velocities=r.velocity_after_m_s;
    }
    near(total_body,-5.12*sign,1e-10,"two-step body impulse");
    near(total_fluid,5.12*sign,1e-10,"two-step applied fluid impulse");
    near(s.velocity_m_s[axis],-0.512*sign,1e-12,
         "two-step rigid velocity");
    near(s.centre_m[axis],4.0-0.0512*sign,1e-12,
         "two-step rigid centre");
    require(initial==zero_faces(n),"input snapshot unchanged");
    require(pressure==gradient(n,axis,sign),"pressure unchanged");
    std::cout<<"COUNTER "<<n<<" "<<axis<<" "<<sign<<" "
             <<std::setprecision(17)<<total_body<<" "<<total_fluid<<" "
             <<s.velocity_m_s[axis]<<" "<<s.centre_m[axis]<<"\n";
}
void gauge_and_kinetic() {
    Box box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    State s;s.mass_kg=10;s.centre_m={{4,4,4}};
    auto p=gradient(8,0,1);
    auto v=zero_faces(8);
    const auto a=Step::step(8,8,8,1,0.1,1000,box,s,p,v);
    for(auto& x:p)x+=73;
    const auto b=Step::step(8,8,8,1,0.1,1000,box,s,p,v);
    for(int i=0;i<3;++i) {
        near(a.applied_fluid_impulse_Ns[i],
             b.applied_fluid_impulse_Ns[i],1e-10,
             "gauge invariant reaction");
        near(a.body.rigid.after.velocity_m_s[i],
             b.body.rigid.after.velocity_m_s[i],1e-10,
             "gauge invariant body");
    }
    // Direct independent weighted kinetic-energy check at zero initial velocity.
    near(a.selected_face_energy_change_proxy_J,
         0.5*a.applied_fluid_impulse_Ns[0]*
         a.face_delta_velocity_m_s[0],1e-10,
         "kinetic proxy is exact on selected dual masses");
    require(a.selected_face_energy_change_proxy_J>0,
            "fluid kinetic proxy positive for impulse at rest");
    require(a.max_geometric_continuity_residual_m3>0,
            "unprojected continuity violation exposed");
}
void negatives() {
    Box box{{3.2,3.2,3.2},{4.8,4.8,4.8}};
    State s;s.mass_kg=10;s.centre_m={{4,4,4}};
    auto p=gradient(8,0,1);
    auto v=zero_faces(8);
    const auto original=v;
    rejects([&]{Step::step(8,8,8,1,0.1,0,box,s,p,v);},
            "reject zero density");
    rejects([&]{Step::step(8,8,8,1,0.1,1000,box,s,p,{});},
            "reject MAC shape");
    v[0][0]=0.2;
    rejects([&]{Step::step(8,8,8,1,0.1,1000,box,s,p,v);},
            "reject nonzero exterior normal face");
    v=original;
    v[1][20]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Step::step(8,8,8,1,0.1,1000,box,s,p,v);},
            "reject nonfinite MAC");
    v=original;
    p[0]=std::numeric_limits<double>::infinity();
    rejects([&]{Step::step(8,8,8,1,0.1,1000,box,s,p,v);},
            "reject nonfinite pressure");
    require(v==original,"failure does not modify MAC");
}
}
int main() {
    try {
        for(int n:{8,12})for(int a=0;a<3;++a)
            for(int sign:{-1,1})analytic(n,a,sign);
        gauge_and_kinetic();
        negatives();
        std::cout<<"PASS: "<<checks
                 <<" native moving MAC counterimpulse assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
