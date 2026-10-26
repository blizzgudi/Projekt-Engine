#pragma once
// P3 CPU MVP: material-indexed porous voxels coupled to the same 3D MAC water
// domain, pressure loads on fixed rigid voxels, and a quantized moving voxel
// body. Water transfer is exactly mass-conservative, downward gravity energy
// change is tracked explicitly as dissipation in the porous path.
// Boundaries: Cartesian, uniform dx, constant rho and g, no elastic soil,
// fluid stresses beyond normal pressure, no high-fidelity moving-wall BC.
#include "upc/fluid3d_module.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <unordered_map>
#include <stdexcept>
#include <vector>

namespace upc {

struct MaterialForce3D {double x=0,y=0,z=0;};
struct PorousVoxel3D {
    std::uint32_t material_id=0;
    double porosity=0;               // void volume / bulk volume
    double conductivity_m_s=0;       // effective downward conductivity
    double pore_water_m3=0;
};
struct RigidVoxelBody3D {
    std::size_t x=0,y=0,z=0;
    std::uint32_t material_id=0;
    double mass_kg=1;
    double velocity_z_m_s=0;
    double center_z_m=0;
    double last_force_z_n=0;
    double last_mechanical_residual_j=0;
    bool active=false;
};
struct PoreTransfer3D {
    std::size_t source,to;
    double volume_m3;
    double potential_released_j;
    bool source_pore;
};
struct P3ExchangeReport {
    double volume_m3=0;
    double potential_released_j=0;
    double soil_to_soil_m3=0;
    double free_to_soil_m3=0;
    double mass_residual_kg=0;
    double gravitational_energy_residual_j=0;
};

class MaterialVolumeCoupler3D {
    UnifiedPhysicsRuntime& runtime_;
    CoupledMacWater3D& water_;
    VoxelGrid& grid_;
    std::size_t nx_,ny_,nz_;
    double dx_,rho_;
    std::vector<PorousVoxel3D> soil_;
    struct SoilDefinition {double porosity, conductivity;};
    std::unordered_map<std::uint32_t,SoilDefinition> porous_materials_;
    std::function<void(const PoreTransfer3D&)> pore_transfer_callback_;
    RigidVoxelBody3D body_;
    double dissipated_j_=0;
    double total_infiltrated_m3_=0;
    static constexpr double g=9.81;
    std::size_t at(std::size_t x,std::size_t y,std::size_t z)const {
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("3D material cell");
        return (z*ny_+y)*nx_+x;
    }
    double cell_volume()const{return dx_*dx_*dx_;}
    double pore_capacity(const PorousVoxel3D& c)const{return c.porosity*cell_volume();}
    bool soil_at(std::size_t x,std::size_t y,std::size_t z)const{
        return soil_[at(x,y,z)].material_id!=0;
    }
    void block_validation(std::size_t x,std::size_t y,std::size_t z,std::uint32_t id)const{
        at(x,y,z);
        if(water_.is_solid(x,y,z)||water_.water_fraction(x,y,z)>0||
           grid_.at(x,y,z).material_id!=0)
            throw std::invalid_argument("Material voxel overlaps existing occupancy");
        if(runtime_.core().materials().at(id).phase!=Phase::Solid)
            throw std::invalid_argument("Material voxel must use solid material ID");
    }
    void relocate_body(std::size_t new_z){
        if(new_z>=nz_||body_.x>=nx_||body_.y>=ny_)
            throw std::out_of_range("Body movement beyond domain");
        if(grid_.at(body_.x,body_.y,new_z).material_id ||
           water_.is_solid(body_.x,body_.y,new_z))
            throw std::runtime_error("Body cannot move into a material obstacle");
        const double target_water=water_.water_fraction(body_.x,body_.y,new_z);
        // Swap target water into the old body's grid slot (no volume loss).
        water_.set_water(body_.x,body_.y,new_z,0);
        water_.set_solid(body_.x,body_.y,new_z,true);
        water_.set_solid(body_.x,body_.y,body_.z,false);
        water_.set_water(body_.x,body_.y,body_.z,target_water);
        grid_.set(body_.x,body_.y,body_.z,{},runtime_.core().materials());
        grid_.set(body_.x,body_.y,new_z,{body_.material_id,1,293.15,0},
                  runtime_.core().materials());
        body_.z=new_z;
    }
public:
    MaterialVolumeCoupler3D(UnifiedPhysicsRuntime& rt,CoupledMacWater3D& fluid)
      :runtime_(rt),water_(fluid),grid_(rt.core().voxels()),
       nx_(fluid.nx()),ny_(fluid.ny()),nz_(fluid.nz()),
       dx_(fluid.cell_size_m()),rho_(fluid.density_kg_m3()) {
        if(nx_!=grid_.nx()||ny_!=grid_.ny()||nz_!=grid_.nz()||
           std::abs(dx_-grid_.cell_size_m())>1e-12)
            throw std::invalid_argument("P3 requires aligned 3D fluid and material grids");
        soil_.resize(nx_*ny_*nz_);
    }
    // Material-ID based physics defaults; per-voxel overrides remain possible.
    void register_porous_material(std::uint32_t id,double porosity,
                                  double conductivity_m_s){
        if(runtime_.core().materials().at(id).phase!=Phase::Solid ||
           !std::isfinite(porosity)||porosity<=0||porosity>=1||
           !std::isfinite(conductivity_m_s)||conductivity_m_s<0)
            throw std::invalid_argument("Invalid porous material definition");
        if(!porous_materials_.emplace(id,
            SoilDefinition{porosity,conductivity_m_s}).second)
            throw std::invalid_argument("Duplicate porous material properties");
    }
    void set_soil_from_material(std::size_t x,std::size_t y,std::size_t z,
                                std::uint32_t id,double initial_saturation=0){
        const auto it=porous_materials_.find(id);
        if(it==porous_materials_.end())
            throw std::out_of_range("Unregistered porous material physics");
        set_soil(x,y,z,id,it->second.porosity,it->second.conductivity,
                 initial_saturation);
    }
    void set_soil(std::size_t x,std::size_t y,std::size_t z,
                  std::uint32_t material_id,double porosity,
                  double conductivity_m_s,double initial_saturation=0){
        block_validation(x,y,z,material_id);
        if(!std::isfinite(porosity)||porosity<=0||porosity>=1 ||
           !std::isfinite(conductivity_m_s)||conductivity_m_s<0||
           !std::isfinite(initial_saturation)||initial_saturation<0||
           initial_saturation>1)
            throw std::invalid_argument("Invalid porous physics inputs");
        grid_.set(x,y,z,{material_id,1,293.15,0},runtime_.core().materials());
        water_.set_solid(x,y,z,true);
        soil_[at(x,y,z)]={material_id,porosity,conductivity_m_s,
                          porosity*cell_volume()*initial_saturation};
    }
    void set_fixed_rock(std::size_t x,std::size_t y,std::size_t z,
                        std::uint32_t material_id){
        block_validation(x,y,z,material_id);
        grid_.set(x,y,z,{material_id,1,293.15,0},runtime_.core().materials());
        water_.set_solid(x,y,z,true);
    }
    const PorousVoxel3D& soil(std::size_t x,std::size_t y,std::size_t z)const{
        return soil_.at(at(x,y,z));
    }
    double pore_water_volume_m3()const{
        double v=0;
        for(const auto& c:soil_)v+=c.pore_water_m3;
        return v;
    }
    double total_water_volume_m3()const{
        return water_.volume_m3()+pore_water_volume_m3();
    }
    double total_water_mass_kg()const{
        return rho_*total_water_volume_m3();
    }
    double gravity_potential_j()const{
        double v=0;
        for(std::size_t z=0;z<nz_;++z)
          for(std::size_t y=0;y<ny_;++y)
            for(std::size_t x=0;x<nx_;++x){
                const double height=(z+0.5)*dx_;
                v+= (soil_[at(x,y,z)].pore_water_m3+
                     water_.water_fraction(x,y,z)*cell_volume())*height;
            }
        return v*rho_*g;
    }
    double dissipated_energy_j()const{return dissipated_j_;}
    double total_infiltrated_volume_m3()const{return total_infiltrated_m3_;}

    // Conservative one-way gravity-driven Darcy-style exchange:
    // 1) free fluid just above each porous voxel -> its pore storage.
    // 2) saturated/partially wet upper porous voxel -> lower porous voxel.
    // This does not implement full Richards/Stokes-Darcy, capillary rise,
    // or lateral subsurface pressure diffusion.
    void set_pore_transfer_listener(std::function<void(const PoreTransfer3D&)> observer){
        pore_transfer_callback_=std::move(observer);
    }
    P3ExchangeReport exchange(double dt){
        if(!std::isfinite(dt)||dt<=0||dt>2)
            throw std::invalid_argument("Invalid coupled exchange dt");
        const double mass0=total_water_mass_kg();
        const double potential0=gravity_potential_j();
        P3ExchangeReport report;
        const double area=dx_*dx_;
        // Descending z order allows soil below to be filled immediately.
        for(std::size_t z=nz_;z-- > 0;)
         for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            auto& dest=soil_[at(x,y,z)];
            if(!dest.material_id||z+1>=nz_||dest.conductivity_m_s==0)continue;
            const double capacity=pore_capacity(dest)-dest.pore_water_m3;
            if(capacity<=1e-14)continue;
            const auto src_index=at(x,y,z+1);
            const auto& above=soil_[src_index];
            double source_volume=0,src_pressure=0;
            if(above.material_id){
                source_volume=above.pore_water_m3;
                if(source_volume<=0)continue;
                // Effective wet connectivity for simple unsaturated soil,
                // not a calibrated capillary pressure relationship.
                const double saturation=source_volume/pore_capacity(above);
                src_pressure=std::max(0.0,saturation);
            }else if(!water_.is_solid(x,y,z+1)){
                source_volume=water_.water_fraction(x,y,z+1)*cell_volume();
                src_pressure=std::max(0.0,
                    water_.pressure_pa(x,y,z+1)/(rho_*g*dx_));
            }else continue;
            const double permeability=above.material_id?
                std::min(above.conductivity_m_s,dest.conductivity_m_s):
                dest.conductivity_m_s;
            const double flux=std::min({source_volume,capacity,
                permeability*area*dt*(1.0+std::min(src_pressure,20.0))});
            if(flux<=0)continue;
            if(above.material_id) {
                soil_[src_index].pore_water_m3-=flux;
                report.soil_to_soil_m3+=flux;
            }else{
                const double original=water_.water_fraction(x,y,z+1);
                water_.set_water(x,y,z+1,
                    std::max(0.0,original-flux/cell_volume()));
                report.free_to_soil_m3+=flux;
            }
            dest.pore_water_m3+=flux;
            report.volume_m3+=flux;
            // Water drops by exactly one dx between the two cell centres.
            const double grav_loss=rho_*g*flux*dx_;
            dissipated_j_+=grav_loss;
            report.potential_released_j+=grav_loss;
            if(pore_transfer_callback_)
                pore_transfer_callback_({src_index,at(x,y,z),flux,
                                         grav_loss,above.material_id!=0});
        }
        total_infiltrated_m3_+=report.free_to_soil_m3;
        report.mass_residual_kg=total_water_mass_kg()-mass0;
        report.gravitational_energy_residual_j=
            (gravity_potential_j()+dissipated_j_)-
            (potential0+dissipated_j_-report.potential_released_j);
        if(std::abs(report.mass_residual_kg)>1e-8 ||
           std::abs(report.gravitational_energy_residual_j)>1e-6)
            throw std::runtime_error("P3 mass/transfer-energy audit failure");
        return report;
    }
    // Includes MAC motion + downward porous transfer. Kinetic/pressure solver
    // energy is NOT covered by the Darcy potential+dissipation ledger.
    P3ExchangeReport step(double dt,int pressure_iterations=250){
        runtime_.step(dt,pressure_iterations);
        return exchange(dt);
    }
    // Pressure traction on a grid-locked rigid cell: each adjacent free-water
    // pressure contributes p*dA toward the solid; soil contacts excluded.
    MaterialForce3D wall_force(std::size_t x,std::size_t y,std::size_t z)const{
        if(!water_.is_solid(x,y,z)||soil_[at(x,y,z)].material_id)
            throw std::invalid_argument("Pressure traction requires rigid solid voxel");
        MaterialForce3D f;
        const double area=dx_*dx_;
        if(x>0&&!water_.is_solid(x-1,y,z))
            f.x+=water_.pressure_pa(x-1,y,z)*area;
        if(x+1<nx_&&!water_.is_solid(x+1,y,z))
            f.x-=water_.pressure_pa(x+1,y,z)*area;
        if(y>0&&!water_.is_solid(x,y-1,z))
            f.y+=water_.pressure_pa(x,y-1,z)*area;
        if(y+1<ny_&&!water_.is_solid(x,y+1,z))
            f.y-=water_.pressure_pa(x,y+1,z)*area;
        // MAC pressure samples sit one *full cell* apart across the rock
        // (liquid below z-1, rock z, liquid above z+1).
        // Hydrostatic face reconstruction subtracts/adds half-cell rho*g*dx.
        if(z>0&&!water_.is_solid(x,y,z-1) &&
           water_.water_fraction(x,y,z-1)>1e-5)
            f.z+=std::max(0.0,water_.pressure_pa(x,y,z-1)-
                rho_*g*dx_*0.5)*area;
        if(z+1<nz_&&!water_.is_solid(x,y,z+1) &&
           water_.water_fraction(x,y,z+1)>1e-5)
            f.z-=std::max(0.0,water_.pressure_pa(x,y,z+1)+
                rho_*g*dx_*0.5)*area;
        return f;
    }
    void set_rigid_body(std::size_t x,std::size_t y,std::size_t z,
                        std::uint32_t id,double mass_kg){
        if(body_.active)throw std::invalid_argument("Only one voxel body in P3 MVP");
        block_validation(x,y,z,id);
        if(!std::isfinite(mass_kg)||mass_kg<=0)
            throw std::invalid_argument("Invalid body mass");
        grid_.set(x,y,z,{id,1,293.15,0},runtime_.core().materials());
        water_.set_solid(x,y,z,true);
        body_={x,y,z,id,mass_kg,0.0,(z+0.5)*dx_,0,0,true};
    }
    const RigidVoxelBody3D& rigid_body()const{
        if(!body_.active)throw std::runtime_error("Rigid body not set");
        return body_;
    }
    // Grid-quantised single-body translation, pressure/weight net force.
    // Exact body mechanical work balance under trapezoidal position update.
    // Conserves WATER VOLUME by moving displaced destination water into the
    // vacated voxel; fluid momentum/energy during remap is NOT conserved.
    void step_body(double dt){
        if(!body_.active||!std::isfinite(dt)||dt<=0||dt>0.1)
            throw std::invalid_argument("Invalid moving rigid-body step");
        const auto traction=wall_force(body_.x,body_.y,body_.z);
        const double force=traction.z-body_.mass_kg*g;
        const double v0=body_.velocity_z_m_s;
        const double v1=v0+dt*force/body_.mass_kg;
        const double z0=body_.center_z_m;
        double z1=z0+0.5*(v0+v1)*dt;
        // Keep the rigid voxel inside the world and prevent a jump over more
        // than a single voxel each time step. No hidden wall tunneling.
        if(std::abs(z1-z0)>0.49*dx_)
            throw std::runtime_error("Rigid voxel dt too large for single-step collision");
        if(z1<0.5*dx_||z1>(nz_-0.5)*dx_)
            throw std::runtime_error("Moving body crosses world boundary");
        auto grid_pos=static_cast<std::size_t>(std::floor(z1/dx_));
        grid_pos=std::min(grid_pos,nz_-1);
        if(grid_pos!=body_.z){
            if(grid_pos+1<body_.z||grid_pos>body_.z+1)
                throw std::runtime_error("Body jumped over cell");
            relocate_body(grid_pos);
        }
        body_.center_z_m=z1;
        body_.velocity_z_m_s=v1;
        body_.last_force_z_n=traction.z;
        const double kinetic_change=0.5*body_.mass_kg*(v1*v1-v0*v0);
        const double gravity_change=body_.mass_kg*g*(z1-z0);
        const double fluid_pressure_work=traction.z*(z1-z0);
        body_.last_mechanical_residual_j=
            kinetic_change+gravity_change-fluid_pressure_work;
        if(std::abs(body_.last_mechanical_residual_j)>1e-8)
            throw std::runtime_error("Body work-energy discrepancy");
    }
};
} // namespace upc
