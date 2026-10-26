#pragma once
// Bounded multi-body deformable volume proxies: each mass node is assigned
// a sphere. Pairwise nonbonded self-contact and body/body contact (3D), with
// impulse-based normal restitution and tangential Coulomb friction.
// NOT mesh/surface-intersection FEM contact, not swept collision detection.
#include "upc/solid_contact3d.hpp"
#include "upc/core.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
namespace upc {
struct DeformableContactReport3D {
    std::size_t contacts=0,substeps=0;
    double maximum_overlap_m=0;
    double inelastic_kinetic_loss_j=0;
};
class DeformableContact3D {
    struct Body {
        SpringSolid3D* solid;
        std::uint32_t material_id;
        double radius_m,restitution,friction;
    };
    const MaterialLibrary& materials_;
    std::vector<Body> bodies_;
    int iterations_=18;
    static double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
    static bool bonded(const SpringSolid3D& s,std::size_t a,std::size_t b){
        for(const auto& edge:s.bonds())
            if(edge.intact&&((edge.a==a&&edge.b==b)||(edge.a==b&&edge.b==a)))
                return true;
        return false;
    }
    void solve(MassNode& a,MassNode& b,double radius_sum,
               double restitution,double friction,
               DeformableContactReport3D& report){
        const Vec3 delta=b.position-a.position;
        const double len=norm(delta);
        if(len>=radius_sum)return;
        const double inv_a=a.fixed?0:1/a.mass_kg;
        const double inv_b=b.fixed?0:1/b.mass_kg;
        const double inv=inv_a+inv_b;
        if(inv<=0)return;
        const Vec3 normal=len>1e-12?delta/len:Vec3{1,0,0};
        const double penetration=radius_sum-len;
        report.maximum_overlap_m=std::max(report.maximum_overlap_m,penetration);
        ++report.contacts;
        // First project constraint, then resolve closing relative impulse.
        if(!a.fixed)a.position=a.position-normal*(penetration*inv_a/inv);
        if(!b.fixed)b.position=b.position+normal*(penetration*inv_b/inv);
        const Vec3 old_relative=b.velocity-a.velocity;
        const double vn=dot(old_relative,normal);
        if(vn>=0)return;
        const double initial_kinetic=
            (a.fixed?0:0.5*a.mass_kg*dot(a.velocity,a.velocity))+
            (b.fixed?0:0.5*b.mass_kg*dot(b.velocity,b.velocity));
        const double impulse=-(1+restitution)*vn/inv;
        if(!a.fixed)a.velocity=a.velocity-normal*(impulse*inv_a);
        if(!b.fixed)b.velocity=b.velocity+normal*(impulse*inv_b);
        const Vec3 v=b.velocity-a.velocity;
        const Vec3 tangential=v-normal*dot(v,normal);
        const double tangent_speed=norm(tangential);
        if(tangent_speed>0&&friction>0){
            const double jt=std::min(tangent_speed/inv,friction*impulse);
            const Vec3 j=tangential*(jt/tangent_speed);
            if(!a.fixed)a.velocity=a.velocity+j*inv_a;
            if(!b.fixed)b.velocity=b.velocity-j*inv_b;
        }
        const double final_kinetic=
            (a.fixed?0:0.5*a.mass_kg*dot(a.velocity,a.velocity))+
            (b.fixed?0:0.5*b.mass_kg*dot(b.velocity,b.velocity));
        const double loss=initial_kinetic-final_kinetic;
        if(loss<-1e-9)
            throw std::runtime_error("Deformable impact added kinetic energy");
        report.inelastic_kinetic_loss_j+=std::max(0.0,loss);
    }
    void contacts(DeformableContactReport3D& report){
        for(int iteration=0;iteration<iterations_;++iteration){
            for(std::size_t k=0;k<bodies_.size();++k){
                auto& a=bodies_[k];
                for(std::size_t j=k;j<bodies_.size();++j){
                    auto& b=bodies_[j];
                    const double e=std::min(a.restitution,b.restitution);
                    const double mu=std::sqrt(a.friction*b.friction);
                    for(std::size_t i=0;i<a.solid->node_count();++i){
                        const auto lower=(k==j)?i+1:std::size_t(0);
                        for(std::size_t m=lower;m<b.solid->node_count();++m){
                            if(k==j&&bonded(*a.solid,i,m))continue;
                            solve(a.solid->node(i),b.solid->node(m),
                                  a.radius_m+b.radius_m,e,mu,report);
                        }
                    }
                }
            }
        }
    }
public:
    explicit DeformableContact3D(const MaterialLibrary& materials)
        :materials_(materials){}
    void add_body(SpringSolid3D& solid,std::uint32_t id,double node_radius_m,
                  double restitution,double friction){
        if(materials_.at(id).phase!=Phase::Solid||
           !solid.node_count()||
           !std::isfinite(node_radius_m)||node_radius_m<=0||
           !std::isfinite(restitution)||restitution<0||restitution>1||
           !std::isfinite(friction)||friction<0)
            throw std::invalid_argument("Invalid deformable contact material");
        for(const auto& body:bodies_)
            if(body.solid==&solid)
                throw std::invalid_argument("Duplicate deformable body");
        bodies_.push_back({&solid,id,node_radius_m,restitution,friction});
    }
    std::size_t body_count()const{return bodies_.size();}
    DeformableContactReport3D step(double dt){
        if(!std::isfinite(dt)||dt<=0||dt>0.1)
            throw std::invalid_argument("Invalid deformable contact step");
        DeformableContactReport3D report;
        // Simple maximum-motion safeguard against deep discrete penetration.
        double maximum=0,smallest_radius=std::numeric_limits<double>::infinity();
        for(const auto& body:bodies_)
        for(std::size_t i=0;i<body.solid->node_count();++i){
            const auto& n=body.solid->node(i);
            maximum=std::max(maximum,norm(n.velocity)+9.81*dt);
            smallest_radius=std::min(smallest_radius,body.radius_m);
        }
        const auto count=std::max<std::size_t>(1,
            static_cast<std::size_t>(std::ceil(dt*maximum/(0.25*smallest_radius))));
        if(count>2000)throw std::runtime_error("Deformable contact substep limit");
        report.substeps=count;
        for(std::size_t s=0;s<count;++s){
            for(auto& body:bodies_)body.solid->step(dt/count);
            contacts(report);
        }
        return report;
    }
    double max_nonbonded_overlap_m()const{
        double overlap=0;
        for(std::size_t k=0;k<bodies_.size();++k)
        for(std::size_t j=k;j<bodies_.size();++j){
            const auto& a=bodies_[k],&b=bodies_[j];
            for(std::size_t i=0;i<a.solid->node_count();++i)
            for(std::size_t m=(k==j?i+1:0);m<b.solid->node_count();++m){
                if(k==j&&bonded(*a.solid,i,m))continue;
                overlap=std::max(overlap,a.radius_m+b.radius_m-
                         norm(a.solid->node(i).position-b.solid->node(m).position));
            }
        }
        return overlap;
    }
};
} // namespace upc
