#pragma once
// Experimental P5 mass-spring bond model, not continuum FEM/MPM.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace upc {
struct Vec3 {
    double x=0,y=0,z=0;
    Vec3 operator+(Vec3 b)const{return {x+b.x,y+b.y,z+b.z};}
    Vec3 operator-(Vec3 b)const{return {x-b.x,y-b.y,z-b.z};}
    Vec3 operator*(double s)const{return {x*s,y*s,z*s};}
    Vec3 operator/(double s)const{return {x/s,y/s,z/s};}
};
inline double norm(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
struct MassNode {
    Vec3 position,velocity;
    double mass_kg=1;
    bool fixed=false;
};
struct ElasticBond {
    std::size_t a,b;
    double rest_m=0;
    double stiffness_n_per_m=0;
    double max_strain=0;
    bool intact=true;
};
class SpringSolid3D {
    std::vector<MassNode> nodes_;
    std::vector<ElasticBond> bonds_;
    Vec3 gravity_{0,0,-9.81};
    double fracture_released_j_=0;
public:
    std::size_t add_node(MassNode n){
        if(!std::isfinite(n.mass_kg)||n.mass_kg<=0||
           !std::isfinite(n.position.x)||!std::isfinite(n.position.y)||
           !std::isfinite(n.position.z)||!std::isfinite(n.velocity.x)||
           !std::isfinite(n.velocity.y)||!std::isfinite(n.velocity.z))
           throw std::invalid_argument("Invalid mass node");
        nodes_.push_back(n);
        return nodes_.size()-1;
    }
    void add_bond(std::size_t a,std::size_t b,double stiffness,double break_strain){
        if(a>=nodes_.size()||b>=nodes_.size()||a==b||
           !std::isfinite(stiffness)||stiffness<=0||
           !std::isfinite(break_strain)||break_strain<=0)
            throw std::invalid_argument("Invalid elastic bond");
        double length=norm(nodes_[a].position-nodes_[b].position);
        if(length<=0)throw std::invalid_argument("Coincident bond nodes");
        bonds_.push_back({a,b,length,stiffness,break_strain,true});
    }
    void set_gravity(Vec3 g){
        if(!std::isfinite(g.x)||!std::isfinite(g.y)||!std::isfinite(g.z))
            throw std::invalid_argument("Non-finite gravity");
        gravity_=g;
    }
    MassNode& node(std::size_t i){return nodes_.at(i);}
    const MassNode& node(std::size_t i)const{return nodes_.at(i);}
    std::size_t node_count()const{return nodes_.size();}
    const std::vector<ElasticBond>& bonds()const{return bonds_;}
    double fracture_released_j()const{return fracture_released_j_;}
    std::size_t intact_bonds()const{
        std::size_t n=0;for(const auto& b:bonds_)n+=b.intact?1:0;return n;
    }
    double elastic_energy_j()const {
        double e=0;
        for(const auto& b:bonds_)if(b.intact){
            const double extension=norm(nodes_[b.a].position-nodes_[b.b].position)-b.rest_m;
            e+=0.5*b.stiffness_n_per_m*extension*extension;
        }
        return e;
    }
    void step(double dt){
        if(!std::isfinite(dt)||dt<=0||dt>0.1)throw std::invalid_argument("Invalid solid dt");
        std::vector<Vec3> force(nodes_.size());
        for(std::size_t i=0;i<nodes_.size();++i)
            force[i]=gravity_*nodes_[i].mass_kg;
        for(auto& b:bonds_)if(b.intact){
            Vec3 delta=nodes_[b.b].position-nodes_[b.a].position;
            const double len=norm(delta);
            if(len<1e-12)continue;
            const double extension=len-b.rest_m;
            if(std::abs(extension)/b.rest_m > b.max_strain){
                fracture_released_j_+=0.5*b.stiffness_n_per_m*
                                       extension*extension;
                b.intact=false;continue;
            }
            Vec3 f=delta*(b.stiffness_n_per_m*extension/len);
            force[b.a]=force[b.a]+f;
            force[b.b]=force[b.b]-f;
        }
        for(std::size_t i=0;i<nodes_.size();++i)if(!nodes_[i].fixed){
            nodes_[i].velocity=nodes_[i].velocity+force[i]*(dt/nodes_[i].mass_kg);
            nodes_[i].position=nodes_[i].position+nodes_[i].velocity*dt;
        }
    }
};
}
