#pragma once
// P5 bounded DEM particle solver: 3D spherical grains, pairwise impulses,
// Coulomb tangential friction, six box planes and fixed spherical obstacles.
// CPU O(N^2); single-body point/translation only. No rotation, cohesion,
// continuum sand stresses, adhesion, GPU, or continuous collision detection.
// Material IDs and grain mass use the existing MaterialLibrary's density.
#include "upc/core.hpp"
#include "upc/solid_contact3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>
namespace upc {
struct Grain3D {
    Vec3 position,velocity;
    double radius_m=0, mass_kg=0;
    std::uint32_t material_id=0;
    bool fixed=false;
};
struct GrainContactParameters {
    double restitution=0,friction=0.5;
};
struct GranularStepReport {
    std::size_t substeps=0,particle_contacts=0,boundary_contacts=0;
    double max_overlap_m=0, kinetic_energy_before_j=0,kinetic_energy_after_j=0;
};
class GranularWorld3D {
    const MaterialLibrary& lib_;
    Vec3 minimum_,maximum_,gravity_{0,0,-9.81};
    std::vector<Grain3D> grains_;
    std::unordered_map<std::uint32_t,GrainContactParameters> parameters_;
    static bool finite3(Vec3 v){
        return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
    }
    static double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
    static void resolve(Grain3D& a,Grain3D& b,
                        double e,double mu,GranularStepReport& report){
        Vec3 delta=b.position-a.position;
        const double distance=norm(delta);
        const double diameter=a.radius_m+b.radius_m;
        if(distance>=diameter)return;
        // Coincident grains need a deterministic normal independent of time.
        const Vec3 n=distance>1e-14?delta/distance:Vec3{1,0,0};
        const double inv_a=a.fixed?0:1.0/a.mass_kg;
        const double inv_b=b.fixed?0:1.0/b.mass_kg;
        const double inv=inv_a+inv_b;
        if(inv==0)return;
        const double overlap=diameter-distance;
        report.max_overlap_m=std::max(report.max_overlap_m,overlap);
        ++report.particle_contacts;
        const Vec3 separation=n*(overlap/inv);
        if(!a.fixed)a.position=a.position-separation*inv_a;
        if(!b.fixed)b.position=b.position+separation*inv_b;
        const Vec3 relative=b.velocity-a.velocity;
        const double normal_speed=dot(relative,n);
        if(normal_speed>=0)return;
        const double normal_impulse=-(1+e)*normal_speed/inv;
        const Vec3 jn=n*normal_impulse;
        if(!a.fixed)a.velocity=a.velocity-jn*inv_a;
        if(!b.fixed)b.velocity=b.velocity+jn*inv_b;
        // A scalar Coulomb tangential impulse dampens slip without energy gain.
        const Vec3 vnew=b.velocity-a.velocity;
        const Vec3 tangent=vnew-n*dot(vnew,n);
        const double vt=norm(tangent);
        if(vt>1e-14 && mu>0){
            const double jt=std::min(vt/inv,mu*normal_impulse);
            const Vec3 tangential_impulse=tangent*(jt/vt);
            if(!a.fixed)a.velocity=a.velocity+tangential_impulse*inv_a;
            if(!b.fixed)b.velocity=b.velocity-tangential_impulse*inv_b;
        }
    }
    void wall(Grain3D& a,int axis,bool upper,double e,double mu,
              GranularStepReport& report){
        const double pos=axis==0?a.position.x:axis==1?a.position.y:a.position.z;
        const double minimum=axis==0?minimum_.x:axis==1?minimum_.y:minimum_.z;
        const double maximum=axis==0?maximum_.x:axis==1?maximum_.y:maximum_.z;
        const double gap=upper?maximum-a.radius_m-pos:pos-a.radius_m-minimum;
        if(gap>=0)return;
        ++report.boundary_contacts;
        report.max_overlap_m=std::max(report.max_overlap_m,-gap);
        Vec3 n=axis==0?Vec3{1,0,0}:axis==1?Vec3{0,1,0}:Vec3{0,0,1};
        if(upper)n=n*(-1);
        a.position=a.position+n*(-gap);
        const double vn=dot(a.velocity,n);
        if(vn>=0)return;
        const double restitution= -vn>0.05?e:0.0;
        const double normal_dv=-(1+restitution)*vn;
        a.velocity=a.velocity+n*normal_dv;
        const Vec3 tangent=a.velocity-n*dot(a.velocity,n);
        const double vt=norm(tangent);
        if(vt>0 && mu>0){
            const double amount=std::min(vt,mu*normal_dv);
            a.velocity=a.velocity-tangent*(amount/vt);
        }
    }
    void contacts(GranularStepReport& report){
        for(int iteration=0;iteration<12;++iteration){
            for(std::size_t i=0;i<grains_.size();++i)
              for(std::size_t j=i+1;j<grains_.size();++j){
                const auto p=parameters_.at(grains_[i].material_id);
                const auto q=parameters_.at(grains_[j].material_id);
                resolve(grains_[i],grains_[j],
                        std::min(p.restitution,q.restitution),
                        std::sqrt(p.friction*q.friction),report);
              }
            // Boundary projection LAST ensures non-penetration at step end.
            for(auto& grain:grains_)if(!grain.fixed){
                const auto spec=parameters_.at(grain.material_id);
                for(int axis=0;axis<3;++axis){
                    wall(grain,axis,false,spec.restitution,spec.friction,report);
                    wall(grain,axis,true,spec.restitution,spec.friction,report);
                }
            }
        }
    }
public:
    GranularWorld3D(const MaterialLibrary& lib,Vec3 minimum,Vec3 maximum)
       :lib_(lib),minimum_(minimum),maximum_(maximum){
        if(!finite3(minimum)||!finite3(maximum)||
           minimum.x>=maximum.x||minimum.y>=maximum.y||minimum.z>=maximum.z)
            throw std::invalid_argument("Invalid DEM domain");
    }
    void define_material(std::uint32_t id,GrainContactParameters params){
        if(lib_.at(id).phase!=Phase::Solid||
           !std::isfinite(params.restitution)||params.restitution<0||
           params.restitution>1||!std::isfinite(params.friction)||params.friction<0||
           !parameters_.emplace(id,params).second)
            throw std::invalid_argument("Invalid/duplicate grain material");
    }
    void set_gravity(Vec3 g){
        if(!finite3(g))throw std::invalid_argument("Invalid DEM gravity");
        gravity_=g;
    }
    std::size_t add_grain(std::uint32_t id,Vec3 position,double radius,
                          Vec3 velocity={0,0,0},bool fixed=false){
        if(parameters_.count(id)!=1||!finite3(position)||!finite3(velocity)||
           !std::isfinite(radius)||radius<=0)
            throw std::invalid_argument("Invalid grain parameters");
        if(position.x-radius<minimum_.x||position.x+radius>maximum_.x||
           position.y-radius<minimum_.y||position.y+radius>maximum_.y||
           position.z-radius<minimum_.z||position.z+radius>maximum_.z)
            throw std::out_of_range("Grain center/radius outside box");
        const double mass=lib_.at(id).density_kg_m3*(4.0/3.0)*3.14159265358979323846*
                          radius*radius*radius;
        if(!std::isfinite(mass)||mass<=0)
            throw std::invalid_argument("Invalid grain mass");
        grains_.push_back({position,velocity,radius,mass,id,fixed});
        return grains_.size()-1;
    }
    const Grain3D& grain(std::size_t i)const{return grains_.at(i);}
    std::size_t count()const{return grains_.size();}
    double total_mass_kg()const{
        double mass=0;for(const auto& g:grains_)mass+=g.mass_kg;return mass;
    }
    Vec3 momentum()const{
        Vec3 p;for(const auto& g:grains_)p=p+g.velocity*g.mass_kg;return p;
    }
    double kinetic_energy_j()const{
        double e=0;
        for(const auto& g:grains_)e+=0.5*g.mass_kg*dot(g.velocity,g.velocity);
        return e;
    }
    double max_pair_overlap_m()const{
        double max=0;
        for(std::size_t i=0;i<grains_.size();++i)
        for(std::size_t j=i+1;j<grains_.size();++j){
            const double d=norm(grains_[i].position-grains_[j].position);
            max=std::max(max,grains_[i].radius_m+grains_[j].radius_m-d);
        }
        return max;
    }
    GranularStepReport step(double dt){
        if(!std::isfinite(dt)||dt<=0||dt>0.5)
            throw std::invalid_argument("Invalid DEM dt");
        GranularStepReport report;
        report.kinetic_energy_before_j=kinetic_energy_j();
        double max_speed=norm(gravity_)*dt;
        double min_radius=std::numeric_limits<double>::infinity();
        for(const auto& g:grains_)if(!g.fixed){
            max_speed=std::max(max_speed,norm(g.velocity)+norm(gravity_)*dt);
            min_radius=std::min(min_radius,g.radius_m);
        }
        if(!std::isfinite(min_radius)){report.substeps=1;return report;}
        const auto substeps=std::max<std::size_t>(1,
            static_cast<std::size_t>(std::ceil(dt*max_speed/(0.3*min_radius))));
        if(substeps>3000)throw std::runtime_error("DEM substep cap");
        report.substeps=substeps;
        const double h=dt/substeps;
        for(std::size_t s=0;s<substeps;++s){
            for(auto& g:grains_)if(!g.fixed){
                g.velocity=g.velocity+gravity_*h;
                g.position=g.position+g.velocity*h;
            }
            contacts(report);
            for(const auto& g:grains_)
                if(!finite3(g.position)||!finite3(g.velocity))
                    throw std::runtime_error("Nonfinite granular state");
        }
        report.kinetic_energy_after_j=kinetic_energy_j();
        return report;
    }
};
} // namespace upc
