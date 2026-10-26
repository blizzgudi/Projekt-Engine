#pragma once
// P6 conservative voxel-local material mixtures and balanced reaction kinetics.
// Synthetic/registered species; no automatic chemistry, no combustion/CFD.
// All species belong to existing MaterialLibrary IDs. SI kg, mol, m, s, K, J.
#include "upc/core.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
struct ChemicalSpecies3D {
    std::uint32_t material_id=0;
    // synthetic or externally calibrated element IDs -> atoms per molecule
    std::map<std::uint32_t,double> composition;
    double molar_mass_kg_mol=0;
    double specific_heat_j_kg_k=0;
    double diffusivity_m2_s=0;
    double formation_enthalpy_j_mol=0;
};
struct StoichiometricTerm3D {
    std::uint32_t material_id=0;
    double coefficient=0; // negative reactants, positive products
};
struct ReactionRule3D {
    std::vector<StoichiometricTerm3D> terms;
    double rate_1_s=0;                  // limiting reagent pseudo-first-order
    double activation_energy_j_mol=0;   // temperature-dependent Arrhenius
};
struct MixtureCell3D {
    std::map<std::uint32_t,double> species_mol;
    // sensible energy relative to Tref=273.15 K. Chemical bond energy separate.
    double sensible_j=0;
};
struct ReactionReport3D {
    double extent_mol=0;
    double thermalized_j=0;
    double mass_residual_kg=0;
    double total_energy_residual_j=0;
};
struct DiffusionReport3D {
    double amount_mol=0;
    double mass_residual_kg=0;
    double total_energy_residual_j=0;
    std::size_t exchanges=0;
};
class ReactiveMixtureVolume3D {
    static constexpr double tref=273.15,gas_constant=8.31446261815324;
    const MaterialLibrary& material_library_;
    std::size_t nx_,ny_,nz_;
    double dx_,vol_;
    std::map<std::uint32_t,double> elements_; // custom atom masses kg/mol
    std::map<std::uint32_t,ChemicalSpecies3D> species_;
    std::vector<ReactionRule3D> rules_;
    std::vector<MixtureCell3D> cells_;
    std::size_t idx(std::size_t x,std::size_t y,std::size_t z)const{
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("Mixture voxel");
        return (z*ny_+y)*nx_+x;
    }
    static void valid(double value,const char* what,bool positive=false){
        if(!std::isfinite(value)||value<0||(positive&&value==0))
            throw std::invalid_argument(what);
    }
    const ChemicalSpecies3D& spec(std::uint32_t id)const{
        const auto it=species_.find(id);
        if(it==species_.end())throw std::out_of_range("Unregistered reactive species ID");
        return it->second;
    }
    double cell_mass(const MixtureCell3D& c)const{
        double m=0;
        for(const auto& [id,n]:c.species_mol)m+=n*spec(id).molar_mass_kg_mol;
        return m;
    }
    double heat_capacity(const MixtureCell3D& c)const{
        double cp=0;
        for(const auto& [id,n]:c.species_mol){
            const auto& s=spec(id);
            cp+=n*s.molar_mass_kg_mol*s.specific_heat_j_kg_k;
        }
        return cp;
    }
    double occupied_volume(const MixtureCell3D& c)const{
        double v=0;
        for(const auto& [id,n]:c.species_mol){
            const auto& s=spec(id);
            v+=n*s.molar_mass_kg_mol/
               material_library_.at(id).density_kg_m3;
        }
        return v;
    }
    static double number(const MixtureCell3D& c,std::uint32_t id){
        const auto it=c.species_mol.find(id);
        return it==c.species_mol.end()?0:it->second;
    }
    static void adjust(MixtureCell3D& c,std::uint32_t id,double amount){
        const double next=number(c,id)+amount;
        if(next < -1e-10)throw std::runtime_error("Negative constituent inventory");
        if(next<=0)c.species_mol.erase(id);
        else c.species_mol[id]=next;
    }
    double chemical(const MixtureCell3D& c)const{
        double energy=0;
        for(const auto& [id,n]:c.species_mol)
            energy+=n*spec(id).formation_enthalpy_j_mol;
        return energy;
    }
    void valid_cell(const MixtureCell3D& c)const{
        if(occupied_volume(c)>vol_*(1+1e-10))
            throw std::runtime_error("Mixture overfills a physical voxel");
        const double cp=heat_capacity(c);
        if(cp>0&&!std::isfinite(c.sensible_j) ||
           cp>0&&c.sensible_j<-tref*cp-1e-7)
            throw std::runtime_error("Nonphysical mixture temperature");
        if(cp==0&&std::abs(c.sensible_j)>1e-8)
            throw std::runtime_error("Empty voxel has residual sensible energy");
    }
    void pair_diffuse(std::size_t a,std::size_t b,double dt,
                      DiffusionReport3D& report){
        // Deterministic donor -> receiver per species; updates are
        // conservative even when a voxel is nearly full.
        for(const auto& [id,s]:species_){
            if(s.diffusivity_m2_s<=0 ||
               material_library_.at(id).phase==Phase::Solid)continue;
            const double an=number(cells_[a],id),bn=number(cells_[b],id);
            if(std::abs(an-bn)<1e-14)continue;
            const std::size_t donor=an>bn?a:b,target=an>bn?b:a;
            const double want=s.diffusivity_m2_s*dt/(dx_*dx_)*
                              std::abs(an-bn);
            auto& src=cells_[donor];
            auto& dst=cells_[target];
            const double mass_per_mol=s.molar_mass_kg_mol;
            const double molar_volume=mass_per_mol/
                material_library_.at(id).density_kg_m3;
            const double available=std::max(0.0,vol_-occupied_volume(dst));
            const double transfer=std::min({want,number(src,id),available/molar_volume});
            if(transfer<=1e-13)continue;
            const double old_mass=cell_mass(src);
            const double sensible=old_mass>0?
                src.sensible_j*(transfer*mass_per_mol/old_mass):0;
            adjust(src,id,-transfer);
            adjust(dst,id,+transfer);
            src.sensible_j-=sensible;
            dst.sensible_j+=sensible;
            valid_cell(src);valid_cell(dst);
            report.amount_mol+=transfer;
            ++report.exchanges;
        }
    }
public:
    ReactiveMixtureVolume3D(const MaterialLibrary& lib,
                            std::size_t nx,std::size_t ny,std::size_t nz,
                            double dx)
      :material_library_(lib),nx_(nx),ny_(ny),nz_(nz),
       dx_(dx),vol_(dx*dx*dx){
        if(!nx||!ny||!nz||!std::isfinite(dx)||dx<=0||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           !std::isfinite(vol_)||vol_<=0)
            throw std::invalid_argument("Invalid reactive volume dimensions");
        cells_.resize(nx*ny*nz);
    }
    void register_element(std::uint32_t id,double mass_kg_mol){
        valid(mass_kg_mol,"Invalid reference element mass",true);
        if(!id||!elements_.emplace(id,mass_kg_mol).second)
            throw std::invalid_argument("Invalid/duplicate element ID");
    }
    void register_species(ChemicalSpecies3D s){
        const auto& mat=material_library_.at(s.material_id);
        (void)mat;
        valid(s.molar_mass_kg_mol,"Invalid molar mass",true);
        valid(s.specific_heat_j_kg_k,"Invalid heat capacity",true);
        valid(s.diffusivity_m2_s,"Invalid diffusivity");
        if(!std::isfinite(s.formation_enthalpy_j_mol)||
           s.composition.empty())
            throw std::invalid_argument("Invalid chemical species");
        double summed=0;
        for(const auto& [element,amount]:s.composition){
            valid(amount,"Invalid atomic stoichiometry",true);
            auto it=elements_.find(element);
            if(it==elements_.end())throw std::invalid_argument("Unregistered element");
            summed+=amount*it->second;
        }
        if(std::abs(summed-s.molar_mass_kg_mol)>1e-9*
           std::max(1.0,s.molar_mass_kg_mol))
            throw std::invalid_argument("Species formula and molar mass disagree");
        if(!species_.emplace(s.material_id,std::move(s)).second)
            throw std::invalid_argument("Duplicate species material");
    }
    void register_reaction(ReactionRule3D r){
        valid(r.rate_1_s,"Invalid reaction rate");
        valid(r.activation_energy_j_mol,"Invalid activation energy");
        if(r.terms.size()<2)throw std::invalid_argument("Too few reaction terms");
        std::map<std::uint32_t,double> signed_atoms;
        double mass=0;
        bool input=false,output=false;
        std::map<std::uint32_t,bool> ids;
        for(const auto& term:r.terms){
            if(!std::isfinite(term.coefficient)||term.coefficient==0 ||
               !ids.emplace(term.material_id,true).second)
                throw std::invalid_argument("Invalid/duplicate reaction constituent");
            const auto& s=spec(term.material_id);
            input|=term.coefficient<0;
            output|=term.coefficient>0;
            mass+=term.coefficient*s.molar_mass_kg_mol;
            for(const auto& [atom,amount]:s.composition)
                signed_atoms[atom]+=term.coefficient*amount;
        }
        if(!input||!output||std::abs(mass)>1e-8)
            throw std::invalid_argument("Nonconservative reaction mass");
        for(const auto& [atom,v]:signed_atoms)
            if(std::abs(v)>1e-9)throw std::invalid_argument("Reaction violates element conservation");
        rules_.push_back(std::move(r));
    }
    const MixtureCell3D& at(std::size_t x,std::size_t y,std::size_t z)const{
        return cells_[idx(x,y,z)];
    }
    double mol(std::size_t x,std::size_t y,std::size_t z,std::uint32_t id)const{
        return number(at(x,y,z),id);
    }
    double occupied_volume_m3(std::size_t x,std::size_t y,std::size_t z)const{
        return occupied_volume(at(x,y,z));
    }
    double heat_capacity_j_k(std::size_t x,std::size_t y,std::size_t z)const{
        return heat_capacity(at(x,y,z));
    }
    double temperature_k(std::size_t x,std::size_t y,std::size_t z)const{
        const auto& c=at(x,y,z);
        const double cp=heat_capacity(c);
        return cp>0?tref+c.sensible_j/cp:tref;
    }
    void add_moles(std::size_t x,std::size_t y,std::size_t z,
                   std::uint32_t id,double n,double temp_k){
        valid(n,"Invalid species quantity",true);
        if(!std::isfinite(temp_k)||temp_k<0)throw std::invalid_argument("Bad temperature");
        const auto& s=spec(id);
        auto trial=cells_[idx(x,y,z)];
        if(!trial.species_mol.empty()&&
           std::abs(temperature_k(x,y,z)-temp_k)>1e-7)
            throw std::invalid_argument("Mixed cell temperatures require explicit thermal equilibration");
        adjust(trial,id,n);
        trial.sensible_j+=n*s.molar_mass_kg_mol*s.specific_heat_j_kg_k*(temp_k-tref);
        valid_cell(trial);
        cells_[idx(x,y,z)]=std::move(trial);
    }
    void add_heat_j(std::size_t x,std::size_t y,std::size_t z,double energy){
        if(!std::isfinite(energy))throw std::invalid_argument("Invalid external heat");
        auto& c=cells_[idx(x,y,z)];
        if(c.species_mol.empty())throw std::invalid_argument("Heating an empty mixture");
        const double heat=c.sensible_j+energy;
        if(heat < -tref*heat_capacity(c))
            throw std::invalid_argument("Reaction heat would cool below absolute zero");
        c.sensible_j=heat;
    }
    double total_mass_kg()const {
        double m=0;for(const auto& c:cells_)m+=cell_mass(c);return m;
    }
    double total_sensible_j()const {
        double s=0;for(const auto& c:cells_)s+=c.sensible_j;return s;
    }
    double total_chemical_j()const {
        double e=0;for(const auto& c:cells_)e+=chemical(c);return e;
    }
    double total_energy_j()const{return total_sensible_j()+total_chemical_j();}
    double total_mol(std::uint32_t id)const{
        double n=0;for(const auto& c:cells_)n+=number(c,id);return n;
    }
    double total_element_mol(std::uint32_t element)const{
        double n=0;
        for(const auto& c:cells_)for(const auto& [id,moles]:c.species_mol){
            const auto it=spec(id).composition.find(element);
            if(it!=spec(id).composition.end())n+=it->second*moles;
        }
        return n;
    }
    ReactionReport3D react(double dt){
        valid(dt,"Invalid reaction dt",true);
        if(dt>20)throw std::invalid_argument("Excessive kinetic timestep");
        const double m0=total_mass_kg(),energy0=total_energy_j();
        ReactionReport3D report;
        for(auto& c:cells_)if(!c.species_mol.empty())
          for(const auto& r:rules_){
            double limit=std::numeric_limits<double>::infinity();
            for(const auto& term:r.terms)if(term.coefficient<0)
                limit=std::min(limit,number(c,term.material_id)/(-term.coefficient));
            if(limit<=1e-14)continue;
            const double temp=tref+c.sensible_j/heat_capacity(c);
            if(temp<=0)continue;
            const double rate=r.rate_1_s*std::exp(-r.activation_energy_j_mol/
                                                 (gas_constant*temp));
            const double extent=limit*(-std::expm1(-rate*dt));
            if(extent<=0)continue;
            auto trial=c;
            for(const auto& term:r.terms)
                adjust(trial,term.material_id,term.coefficient*extent);
            const double heat_released=chemical(c)-chemical(trial);
            trial.sensible_j+=heat_released;
            valid_cell(trial); // reject unphysical volume/temperature before mutation
            c=std::move(trial);
            report.extent_mol+=extent;report.thermalized_j+=heat_released;
          }
        report.mass_residual_kg=total_mass_kg()-m0;
        report.total_energy_residual_j=total_energy_j()-energy0;
        if(std::abs(report.mass_residual_kg)>1e-8*std::max(1.0,m0)||
           std::abs(report.total_energy_residual_j)>1e-8*
                std::max(1.0,std::abs(energy0)))
            throw std::runtime_error("Reaction conservation audit failed");
        return report;
    }
    DiffusionReport3D diffuse(double dt){
        valid(dt,"Invalid diffusion dt",true);
        if(dt>1)throw std::invalid_argument("Excessive diffusion dt");
        const double mass0=total_mass_kg(),energy0=total_energy_j();
        DiffusionReport3D report;
        // Pair-wise finite-volume Fick diffusion. Target volume and donor
        // availability are bounded. Deterministic, 6 neighbors (3 positive).
        for(std::size_t z=0;z<nz_;++z)
          for(std::size_t y=0;y<ny_;++y)
            for(std::size_t x=0;x<nx_;++x){
                const auto i=idx(x,y,z);
                if(x+1<nx_)pair_diffuse(i,idx(x+1,y,z),dt,report);
                if(y+1<ny_)pair_diffuse(i,idx(x,y+1,z),dt,report);
                if(z+1<nz_)pair_diffuse(i,idx(x,y,z+1),dt,report);
            }
        report.mass_residual_kg=total_mass_kg()-mass0;
        report.total_energy_residual_j=total_energy_j()-energy0;
        if(std::abs(report.mass_residual_kg)>1e-8*std::max(1.0,mass0)||
           std::abs(report.total_energy_residual_j)>1e-8*
           std::max(1.0,std::abs(energy0)))
            throw std::runtime_error("Mixture diffusion conservation failed");
        return report;
    }
    // A projection into the existing MATERIAL voxel index: dominant material
    // ID for rendering/collision metadata. The mixture itself stays canonical.
    // Chemical composition must NEVER be recovered from this lossy projection.
    void write_material_proxy(VoxelGrid& grid,std::size_t x,std::size_t y,std::size_t z,
                              const MaterialLibrary& lib)const{
        if(grid.nx()!=nx_||grid.ny()!=ny_||grid.nz()!=nz_||
           std::abs(grid.cell_size_m()-dx_)>1e-12)
            throw std::invalid_argument("Mixture grid cannot map to voxel lattice");
        const auto& c=at(x,y,z);
        Voxel v=grid.at(x,y,z);
        if(c.species_mol.empty()){
            v.material_id=0;v.fill=0;
        }else{
            std::uint32_t dominant=0;
            double largest=-1;
            for(const auto& [id,n]:c.species_mol){
                const double mass=n*spec(id).molar_mass_kg_mol;
                if(mass>largest){largest=mass;dominant=id;}
            }
            v.material_id=dominant;v.fill=std::clamp(occupied_volume(c)/vol_,0.0,1.0);
            v.temperature_k=temperature_k(x,y,z);
        }
        grid.set(x,y,z,v,lib);
    }
    std::size_t nx()const{return nx_;}
    std::size_t ny()const{return ny_;}
    std::size_t nz()const{return nz_;}
    double cell_size_m()const{return dx_;}
};
} // namespace upc
