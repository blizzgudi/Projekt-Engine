#include "upc/solid_contact3d.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace upc;

static int checks=0;
static void check(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
static void near(double actual,double expected,double tolerance,const char* message) {
    check(std::isfinite(actual)&&std::abs(actual-expected)<=tolerance,message);
}
static SpringSolid3D particle(Vec3 p,Vec3 v,double mass=1) {
    SpringSolid3D s;
    s.add_node({p,v,mass,false});
    s.set_gravity({0,0,0});
    return s;
}
static void invalid(SolidPlaneContactSettings3D cfg,const char* message) {
    bool threw=false;
    try { SolidPlaneContact3D plane(cfg); (void)plane; }
    catch(const std::invalid_argument&) { threw=true; }
    check(threw,message);
}
int main() {
    try {
        {
            auto s=particle({0,0,0},{0,0,0});
            s.set_gravity({0,0,-9.81});
            SolidPlaneContact3D ground({});
            for(int i=0;i<10000;++i) {
                ground.step(s,0.001);
                check(s.node(0).position.z>=-1e-12,"Resting node penetrated floor");
            }
            near(s.node(0).position.z,0,1e-12,"Resting contact position drift");
            near(s.node(0).velocity.z,0,1e-12,"Resting contact velocity drift");
        }
        {
            auto s=particle({0,0,0.1},{0,0,-2});
            SolidPlaneContact3D ground({{0,0,1},0,0.5,0,0});
            ground.step(s,0.1);
            near(s.node(0).position.z,0,1e-12,"Impact penetration not projected");
            near(s.node(0).velocity.z,1,1e-12,"Normal restitution impulse incorrect");
            near(0.5*s.node(0).velocity.z*s.node(0).velocity.z,0.5,1e-12,
                 "Rebound kinetic energy incorrect");
        }
        {
            auto s=particle({0,0,0},{3,0,-2});
            SolidPlaneContact3D ground({{0,0,1},0,0,0.3,0});
            ground.step(s,0.01);
            near(s.node(0).velocity.x,2.4,1e-12,"Coulomb tangential impulse incorrect");
            near(s.node(0).velocity.z,0,1e-12,"Inelastic normal impulse incorrect");
            near(s.node(0).position.z,0,1e-12,"Oblique impact penetration");
            check(s.node(0).velocity.x<=3,"Friction added kinetic energy");
        }
        {
            auto s=particle({0,0,0},{0.01,0,-2});
            SolidPlaneContact3D ground({{0,0,1},0,0,0.3,0});
            ground.step(s,0.01);
            near(s.node(0).velocity.x,0,1e-12,"Static-limit friction overshot zero");
        }
        {
            auto s=particle({0,0,-1},{0,0,0});
            SolidPlaneContact3D ground({{0,0,2},0,0,0,0});
            ground.step(s,0.001);
            near(s.node(0).position.z,0,1e-12,"Non-unit normal not normalized");
        }
        {
            const double inv=1/std::sqrt(3.0);
            const Vec3 n{inv,inv,inv};
            auto s=particle({0,0,0},n*(-2));
            SolidPlaneContact3D plane({{1,1,1},0,0.5,0,0});
            plane.step(s,0.01);
            near(solid_dot(n,s.node(0).velocity),1,1e-12,
                 "Rotated-plane restitution incorrect");
            near(solid_dot(n,s.node(0).position),0,1e-12,
                 "Rotated-plane penetration");
        }
        {
            auto s=particle({0,0,0},{0,0,-2});
            s.add_node({{0,0,-1},{0,0,-2},5,true});
            SolidPlaneContact3D ground({});
            ground.step(s,0.01);
            near(s.node(1).position.z,-1,1e-12,"Fixed node moved");
            near(s.node(1).velocity.z,-2,1e-12,"Fixed node velocity modified");
        }
        {
            for(double mass : {0.25,1.0,100.0}) {
                auto s=particle({0,0,0},{3,0,-2},mass);
                SolidPlaneContact3D ground({{0,0,1},0,0,0.3,0});
                ground.step(s,0.01);
                near(s.node(0).velocity.x,2.4,1e-12,
                     "Plane impulse incorrectly depends on mass");
            }
        }
        {
            auto s=particle({0,0,0},{0,0,-0.01});
            SolidPlaneContact3D ground({{0,0,1},0,1,0,0.05});
            ground.step(s,0.001);
            near(s.node(0).velocity.z,0,1e-12,
                 "Sub-threshold resting collision should not bounce");
        }
        {
            // Deterministic property sweep: stationary plane cannot create
            // kinetic energy for any restitution in [0,1] or Coulomb friction.
            for(double e : {0.0,0.25,0.9,1.0})
                for(double mu : {0.0,0.2,1.0})
                    for(int k=1;k<=40;++k) {
                        const Vec3 v{0.05*k,-0.03*k,-0.1*k};
                        const double initial=solid_dot(v,v);
                        auto s=particle({0,0,0},v);
                        SolidPlaneContact3D ground({{0,0,1},0,e,mu,0});
                        ground.step(s,0.001);
                        const auto after=s.node(0);
                        check(solid_dot(after.velocity,after.velocity)<=initial+1e-10,
                              "Contact created kinetic energy");
                        check(after.position.z>=-1e-12,
                              "Contact property sweep penetration");
                    }
        }
        {
            auto cfg=SolidPlaneContactSettings3D{};
            cfg.normal={0,0,0}; invalid(cfg,"Zero normal accepted");
            cfg.normal={0,0,1}; cfg.restitution=1.01;
            invalid(cfg,"Restitution above one accepted");
            cfg.restitution=0; cfg.friction=-0.1;
            invalid(cfg,"Negative friction accepted");
            cfg.friction=0; cfg.bounce_threshold_m_s=-1;
            invalid(cfg,"Negative bounce threshold accepted");
            cfg.bounce_threshold_m_s=0;
            cfg.offset_m=std::numeric_limits<double>::infinity();
            invalid(cfg,"Infinite plane offset accepted");
        }
        std::cout<<"PASS: P5 bounded unilateral plane contact, friction, restitution, resting, "
                 <<"rotated normals, fixed nodes, validation; checks="<<checks<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<" (checks="<<checks<<")\n";
        return 1;
    }
}
