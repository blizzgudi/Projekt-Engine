#include "upc/deformable_contact3d.hpp"
#include "upc/solid_lattice3d.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace upc;
static int checks=0;
void need(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
void near(double x,double y,double tol,const char* why){
    need(std::isfinite(x)&&std::abs(x-y)<=tol,why);
}
int main(){
try{
    MaterialLibrary lib;
    lib.add({5,Phase::Solid,2000,0});
    {
        // Two independent deformable spring objects collide through their
        // material-volume sphere proxies. Momentum conserved for e=1.
        SpringSolid3D left,right;
        auto a=left.add_node({{0.6,0,0},{1,0,0},1,false});
        auto b=right.add_node({{0.73,0,0},{-1,0,0},1,false});
        left.set_gravity({0,0,0});right.set_gravity({0,0,0});
        DeformableContact3D multi(lib);
        multi.add_body(left,5,0.1,1,0);
        multi.add_body(right,5,0.1,1,0);
        auto response=multi.step(0.001);
        need(response.contacts>0,"No deformable-deformable body impact");
        near(left.node(a).velocity.x,-1,1e-10,"Elastic body A response");
        near(right.node(b).velocity.x,1,1e-10,"Elastic body B response");
        need(multi.max_nonbonded_overlap_m()<1e-10,
             "Deformable bodies remain deeply overlapping");
        near(response.inelastic_kinetic_loss_j,0,1e-10,
             "Elastic contact released artificial heat");
    }
    {
        // A body can self-contact after losing a connecting edge.
        SpringSolid3D body;
        body.set_gravity({0,0,0});
        body.add_node({{0,0,0},{0,0,0},1,false});
        body.add_node({{1,0,0},{0,0,0},1,false});
        body.add_node({{0.02,0,0},{0,0,0},1,false});
        body.add_bond(0,1,100,0.25);
        DeformableContact3D multi(lib);
        multi.add_body(body,5,0.04,0,0.1);
        const auto result=multi.step(0.0001);
        need(result.contacts>0,"Nonbonded selfcontact not detected");
        need(multi.max_nonbonded_overlap_m()<1e-8,
             "Nonbonded selfcontact not separated");
        need(body.intact_bonds()==1,"Selfcontact destroyed unrelated bond");
    }
    {
        // Post-fracture fragments from one original spring volume collide
        // through their now-broken nonbonded interface. No double mass.
        SpringSolid3D body;
        body.set_gravity({0,0,0});
        body.add_node({{1,1,1},{0,0,0},2,false});
        body.add_node({{1.1,1,1},{0,0,0},2,false});
        body.add_bond(0,1,500,0.10);
        body.node(1).position.x=1.3;
        body.step(0.00001); // breaks stretched bond
        need(body.intact_bonds()==0,"Test fracture not produced");
        const double fracture=body.fracture_released_j();
        body.node(1).position.x=1.05;
        body.node(0).velocity={0.5,0,0};
        body.node(1).velocity={-0.5,0,0};
        DeformableContact3D multi(lib);
        multi.add_body(body,5,0.05,0.2,0);
        const double kinetic0=2*0.5*2*0.5*0.5;
        const double m0=body.node(0).mass_kg+body.node(1).mass_kg;
        const auto result=multi.step(0.001);
        need(result.contacts>0,"Fracture faces cannot collide");
        need(multi.max_nonbonded_overlap_m()<1e-9,"Broken fragment overlap");
        const double kinetic1=0.5*2*(body.node(0).velocity.x*
                                    body.node(0).velocity.x+
                                    body.node(1).velocity.x*
                                    body.node(1).velocity.x);
        near(kinetic1/kinetic0,0.2*0.2,1e-10,
             "Post-fracture restitution energy ratio incorrect");
        near(kinetic0-kinetic1,result.inelastic_kinetic_loss_j,1e-10,
             "Contact dissipation bookkeeping disagrees with kinetic energy");
        need(fracture>0,"Fracture energy missing");
        near(body.node(0).mass_kg+body.node(1).mass_kg,m0,1e-12,
             "Fragment contacts changed mass");
    }
    {
        SpringSolid3D body;
        body.add_node({{0,0,0},{0,0,0},1,false});
        DeformableContact3D multi(lib);
        multi.add_body(body,5,0.1,0.5,0.2);
        bool thrown=false;
        try{multi.add_body(body,5,0.1,0.5,0.2);}
        catch(const std::invalid_argument&){thrown=true;}
        need(thrown,"Duplicated deformable body accepted");
        thrown=false;
        try{multi.step(-1);}catch(const std::invalid_argument&){thrown=true;}
        need(thrown,"Invalid step accepted");
    }
    std::cout<<"PASS: P5 deformable multi-body/self/fragment collision and"
                " dissipated impact energy; checks="<<checks<<"\n";
    return 0;
}catch(const std::exception& e){
    std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;
}
}
