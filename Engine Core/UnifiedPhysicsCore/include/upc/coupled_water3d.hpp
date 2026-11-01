#pragma once
// UPC P2: Coupled 3D free-surface MAC pressure + bounded VOF transport.
// Experimental first-order CPU solver (optional masked viscosity; no surface tension).
// Pressure is solved on wet MAC cells. MAC face velocities transport material
// fill fractions in all three axes. Volume fluxes are pairwise conservative.
// Velocity self-advection is semi-Lagrangian; it dissipates energy numerically.
// No FLIP/APIC particle transfers, no 3D splashes below grid resolution.
#include "upc/mac3d.hpp"
#include "upc/p2_axis_vof_pcg_geometry3d.hpp"
#include "upc/p2_planar_vof_pcg_geometry3d.hpp"
#include "upc/p2_local_planar_normal_fit3d.hpp"
#include "upc/p2_vof_throughflow3d.hpp"
#include "upc/viscous_mac3d.hpp"
#include "upc/p2_vof_mechanical_inventory3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct TransportFlux3D {std::size_t from,to;double volume_m3;};
// Nominal-cell-mass-scaled staggered face-square diagnostic; NOT physical KE.
struct MacFaceEnergyStages3D {
    double initial_j=0,advection_delta_j=0,viscosity_delta_j=0;
    double gravity_delta_j=0,projection_delta_j=0;
    double vof_transport_delta_j=0,post_projection_delta_j=0;
    double final_j=0,closure_residual_j=0;
    std::size_t substeps=0;
};
class CoupledMacWater3D {
    struct Transfer {std::size_t from,to;double amount;};
    std::size_t nx_,ny_,nz_;
    double dx_,rho_;
    MacProjection3D mac_;
    std::vector<double> fill_;
    std::vector<std::uint8_t> solid_;
    double last_projection_divergence_=0;
    // Actual gravitational potential change from VOF face transfers only.
    // Does not include MAC kinetic energy, wall work or pressure forces.
    double last_vof_transport_potential_delta_j_=0;
    bool face_energy_stages_enabled_=false;
    MacFaceEnergyStages3D last_face_energy_stages_{};
    double kinematic_viscosity_m2_s_=0;
    // Prescribed tangential shear at static voxel obstacles (no solid motion).
    std::array<double,3> wall_tangential_velocity_m_s_{{0.0,0.0,0.0}};
    double last_viscous_energy_before_=0;
    double last_viscous_energy_after_=0;
    std::size_t last_viscosity_substeps_=0;
    // Accumulated over all viscosity stages of the most recent public step().
    // Raw MAC face-square/face-sum diagnostics, not continuum FSI energy.
    MaskedMacMomentumLedger3D last_viscous_diffusion_ledger_{};
    // Only the diffusion-stage MAC face-velocity-sum momentum proxy, not
    // continuum FSI impulse. Units after rho*dx^3 scaling: kg*m/s.
    std::array<double,3> last_viscous_face_momentum_delta_proxy_kg_m_s_{};
    std::array<double,3> last_viscous_solid_wall_source_proxy_kg_m_s_{};
    std::array<double,3> last_viscous_solid_wall_drive_proxy_kg_m_s_{};
    std::array<double,3> last_viscous_solid_wall_drag_proxy_kg_m_s_{};
    std::array<double,3> last_viscous_exterior_source_proxy_kg_m_s_{};
    std::array<double,3> last_viscous_closure_residual_proxy_kg_m_s_{};
    std::function<void(const TransportFlux3D&)> transport_callback_;
    std::size_t substeps_=0;
    bool conservative_throughflow_transport_=false;
    bool pcg_pressure_projection_enabled_=false;
    bool axis_vof_pcg_geometry_enabled_=false;
    bool planar_vof_pcg_geometry_enabled_=false;
    // Opt-in recomputation from the current VOF field at every projection.
    // Rejects nonplanar patches instead of silently falling back to gradients.
    bool auto_planar_vof_normals_enabled_=false;
    double auto_planar_vof_max_patch_error_=1e-7;
    std::size_t last_auto_planar_vof_fitted_cells_=0;
    double last_auto_planar_vof_patch_residual_=0;
    std::vector<std::array<double,3>> prescribed_planar_vof_normals_;
    std::size_t last_planar_vof_resolved_faces_=0;
    std::size_t last_planar_vof_oblique_faces_=0;
    std::size_t last_planar_vof_two_partial_faces_=0;
    std::size_t last_axis_vof_resolved_faces_=0;
    std::size_t last_axis_vof_unresolved_pure_faces_=0;
    std::size_t last_pcg_iterations_=0;
    double last_pcg_max_equation_residual_pa_=0;
    static constexpr double wet=1e-5;
    double pressure_wet_threshold()const{
        return (axis_vof_pcg_geometry_enabled_||planar_vof_pcg_geometry_enabled_)?0.5:wet;
    }
    void sync_axis_vof_pcg_geometry(){
        if(planar_vof_pcg_geometry_enabled_){
            // A fitted normal is accepted only when its entire local 3D
            // VOF patch agrees with a volume-matched plane. This preflight
            // runs BEFORE any interface distances are changed.
            P2LocalPlanarNormals3D fitted;
            if(auto_planar_vof_normals_enabled_)
                fitted=p2_fit_local_planar_vof_normals3d(
                    fill_,solid_,nx_,ny_,nz_,dx_,auto_planar_vof_max_patch_error_);
            const auto& normals=auto_planar_vof_normals_enabled_?
                fitted.normals:prescribed_planar_vof_normals_;
            const auto geometry=p2_planar_vof_pcg_geometry3d(
                fill_,solid_,normals,nx_,ny_,nz_,dx_);
            p2_apply_planar_vof_pcg_geometry3d(mac_,geometry);
            last_auto_planar_vof_fitted_cells_=fitted.fitted_cells;
            last_auto_planar_vof_patch_residual_=fitted.worst_patch_residual;
            last_planar_vof_resolved_faces_=geometry.resolved.size();
            last_planar_vof_oblique_faces_=geometry.oblique_faces;
            last_planar_vof_two_partial_faces_=geometry.two_partial_faces;
            return;
        }
        if(!axis_vof_pcg_geometry_enabled_)return;
        const auto geometry=p2_axis_vof_pcg_geometry3d(
            fill_,solid_,nx_,ny_,nz_,dx_);
        p2_apply_axis_vof_pcg_geometry3d(mac_,geometry);
        last_axis_vof_resolved_faces_=geometry.resolved.size();
        last_axis_vof_unresolved_pure_faces_=geometry.unresolved_pure_faces;
    }
    std::size_t cell(std::size_t x,std::size_t y,std::size_t z) const {
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("Coupled 3D index");
        return (z*ny_+y)*nx_+x;
    }
    std::size_t uidx(std::size_t x,std::size_t y,std::size_t z)const{
        return (z*ny_+y)*(nx_+1)+x;
    }
    std::size_t vidx(std::size_t x,std::size_t y,std::size_t z)const{
        return (z*(ny_+1)+y)*nx_+x;
    }
    std::size_t widx(std::size_t x,std::size_t y,std::size_t z)const{
        return (z*ny_+y)*nx_+x;
    }
    bool wet_or_empty(std::size_t x,std::size_t y,std::size_t z)const{
        return !solid_[cell(x,y,z)] && fill_[cell(x,y,z)]>wet;
    }
    void sync_occupancy(){
        for(std::size_t z=0;z<nz_;++z)
            for(std::size_t y=0;y<ny_;++y)
                for(std::size_t x=0;x<nx_;++x){
                    const auto i=cell(x,y,z);
                    mac_.set_cell(x,y,z,!solid_[i]&&fill_[i]>pressure_wet_threshold(),solid_[i]!=0);
                }
    }
    struct Grid3 {double x,y,z;};
    // Sample face velocity via trilinear interpolation on each staggered grid.
    double component(int axis,Grid3 p)const{
        const auto sx=axis==0?nx_+1:nx_;
        const auto sy=axis==1?ny_+1:ny_;
        const auto sz=axis==2?nz_+1:nz_;
        const double gx=p.x/dx_-(axis==0?0.0:0.5);
        const double gy=p.y/dx_-(axis==1?0.0:0.5);
        const double gz=p.z/dx_-(axis==2?0.0:0.5);
        auto prepare=[](double coord,std::size_t size){
            coord=std::clamp(coord,0.0,static_cast<double>(size-1));
            auto a=static_cast<std::size_t>(coord);
            auto b=std::min(a+1,size-1);
            return std::array<double,3>{static_cast<double>(a),
                                         static_cast<double>(b),coord-a};
        };
        const auto ax=prepare(gx,sx),ay=prepare(gy,sy),az=prepare(gz,sz);
        double result=0;
        for(int k=0;k<2;++k)for(int j=0;j<2;++j)for(int i=0;i<2;++i){
            const auto x=static_cast<std::size_t>(ax[i]),y=static_cast<std::size_t>(ay[j]);
            const auto z=static_cast<std::size_t>(az[k]);
            const double weight=(i?ax[2]:1-ax[2])*(j?ay[2]:1-ay[2])*
                                 (k?az[2]:1-az[2]);
            double v=0;
            if(axis==0)v=mac_.face_velocity_x(x,y,z);
            else if(axis==1)v=mac_.face_velocity_y(x,y,z);
            else v=mac_.face_velocity_z(x,y,z);
            result+=weight*v;
        }
        return result;
    }
    void advect_velocity(double dt){
        std::vector<double> next_u((nx_+1)*ny_*nz_);
        std::vector<double> next_v(nx_*(ny_+1)*nz_);
        std::vector<double> next_w(nx_*ny_*(nz_+1));
        auto trace=[&](Grid3 p,int axis){
            const Grid3 vel{component(0,p),component(1,p),component(2,p)};
            const Grid3 mid{p.x-0.5*dt*vel.x,p.y-0.5*dt*vel.y,p.z-0.5*dt*vel.z};
            const Grid3 vm{component(0,mid),component(1,mid),component(2,mid)};
            return component(axis,{p.x-dt*vm.x,p.y-dt*vm.y,p.z-dt*vm.z});
        };
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x){
            next_u[uidx(x,y,z)]=trace({x*dx_,(y+0.5)*dx_,(z+0.5)*dx_},0);
        }
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            next_v[vidx(x,y,z)]=trace({(x+0.5)*dx_,y*dx_,(z+0.5)*dx_},1);
        }
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            next_w[widx(x,y,z)]=trace({(x+0.5)*dx_,(y+0.5)*dx_,z*dx_},2);
        }
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x)
            mac_.set_face_velocity_x(x,y,z,next_u[uidx(x,y,z)]);
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            mac_.set_face_velocity_y(x,y,z,next_v[vidx(x,y,z)]);
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            mac_.set_face_velocity_z(x,y,z,next_w[widx(x,y,z)]);
    }
    double max_velocity_sum()const{
        double mx=0,my=0,mz=0;
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x)
            mx=std::max(mx,std::abs(mac_.face_velocity_x(x,y,z)));
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            my=std::max(my,std::abs(mac_.face_velocity_y(x,y,z)));
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            mz=std::max(mz,std::abs(mac_.face_velocity_z(x,y,z)));
        return mx+my+mz;
    }
    void advect_fraction(double dt){
        if(conservative_throughflow_transport_) {
            std::array<std::vector<double>,3> plus;
            for(auto& component:plus)component.assign(fill_.size(),0);
            for(std::size_t z=0;z<nz_;++z)
            for(std::size_t y=0;y<ny_;++y)
            for(std::size_t x=0;x<nx_;++x){
                const auto id=cell(x,y,z);
                if(x+1<nx_)plus[0][id]=mac_.face_velocity_x(x+1,y,z);
                if(y+1<ny_)plus[1][id]=mac_.face_velocity_y(x,y+1,z);
                if(z+1<nz_)plus[2][id]=mac_.face_velocity_z(x,y,z+1);
            }
            auto result=p2_vof_throughflow3d(
                fill_,solid_,plus,nx_,ny_,nz_,dx_,dt);
            const double cell_volume=dx_*dx_*dx_;
            double potential_change_j=0;
            std::vector<TransportFlux3D> reports;
            if(transport_callback_)reports.reserve(result.transfers.size());
            for(const auto& transfer:result.transfers) {
                const auto from_z=transfer.from/(nx_*ny_);
                const auto to_z=transfer.to/(nx_*ny_);
                potential_change_j+=rho_*cell_volume*9.81*transfer.fraction*
                    (static_cast<double>(to_z)-static_cast<double>(from_z))*dx_;
                if(transport_callback_)
                    reports.push_back({transfer.from,transfer.to,
                                       transfer.fraction*cell_volume});
            }
            fill_=std::move(result.fill);
            last_vof_transport_potential_delta_j_+=potential_change_j;
            for(const auto& report:reports)transport_callback_(report);
            return;
        }

        // Conservative face flux exchange, always between two existing cells.
        // No transfer through solid or external boundaries. Availability and
        // free-capacity limiters guarantee fractional occupation in [0,1].
        std::vector<Transfer> transfers;
        double transport_potential_delta_j=0;
        transfers.reserve(3*fill_.size());
        std::vector<double> wants_out(fill_.size(),0),wants_in(fill_.size(),0);
        auto offer=[&](std::size_t a,std::size_t b,double face_velocity){
            if(solid_[a]||solid_[b]||face_velocity==0)return;
            const auto donor=face_velocity>0?a:b;
            const auto receiver=face_velocity>0?b:a;
            const double amount=dt/dx_*std::abs(face_velocity)*
                std::min(fill_[donor],1-fill_[receiver]);
            if(amount<=0)return;
            transfers.push_back({donor,receiver,amount});
            wants_out[donor]+=amount;
            wants_in[receiver]+=amount;
        };
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=1;x<nx_;++x)
            offer(cell(x-1,y,z),cell(x,y,z),mac_.face_velocity_x(x,y,z));
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=1;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            offer(cell(x,y-1,z),cell(x,y,z),mac_.face_velocity_y(x,y,z));
        for(std::size_t z=1;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            offer(cell(x,y,z-1),cell(x,y,z),mac_.face_velocity_z(x,y,z));

        std::vector<double> out_scale(fill_.size(),1),in_scale(fill_.size(),1);
        for(std::size_t i=0;i<fill_.size();++i){
            if(wants_out[i]>fill_[i])out_scale[i]=fill_[i]/wants_out[i];
            if(wants_in[i]>1-fill_[i])
                in_scale[i]=(1-fill_[i])/wants_in[i];
        }
        auto next=fill_;
        std::vector<TransportFlux3D> transfer_report;
        if(transport_callback_)transfer_report.reserve(transfers.size());
        for(const auto& transfer:transfers){
            const double amount=transfer.amount*
                std::min(out_scale[transfer.from],in_scale[transfer.to]);
            next[transfer.from]-=amount;
            next[transfer.to]+=amount;
            // Mass moved across the cell-centre height difference (z is up).
            // Horizontal x/y transfers contribute exactly zero.
            const auto from_z=transfer.from/(nx_*ny_);
            const auto to_z=transfer.to/(nx_*ny_);
            transport_potential_delta_j+=rho_*dx_*dx_*dx_*9.81*amount*
                (static_cast<double>(to_z)-static_cast<double>(from_z))*dx_;
            if(transport_callback_&&amount>0)
                transfer_report.push_back({transfer.from,transfer.to,
                                           amount*dx_*dx_*dx_});
        }
        for(std::size_t i=0;i<next.size();++i){
            if(!std::isfinite(next[i])||next[i]<-1e-9||next[i]>1+1e-9)
                throw std::runtime_error("3D VOF stability failure");
            if(next[i]<0)next[i]=0;
            if(next[i]>1)next[i]=1;
            if(solid_[i]&&next[i]>1e-14)
                throw std::runtime_error("3D water entered a solid");
        }
        fill_.swap(next);
        last_vof_transport_potential_delta_j_+=transport_potential_delta_j;
        for(const auto& transfer:transfer_report)
            transport_callback_(transfer);
    }
    // Raw face-square diagnostic scaled by rho*dx^3/2, including dry faces.
    double face_energy_proxy_j()const{
        double sum=0;
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x){const double v=mac_.face_velocity_x(x,y,z);sum+=v*v;}
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x){const double v=mac_.face_velocity_y(x,y,z);sum+=v*v;}
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x){const double v=mac_.face_velocity_z(x,y,z);sum+=v*v;}
        return 0.5*rho_*dx_*dx_*dx_*sum;
    }
    void substep(double dt,int pressure_iterations,bool verified=false,double tolerance=1e-8,
                 bool reproject_after_transport=false){
        sync_occupancy();
        double previous=face_energy_stages_enabled_?face_energy_proxy_j():0;
        auto stage=[&](double& delta){
            if(face_energy_stages_enabled_){
                const double next=face_energy_proxy_j();
                delta+=next-previous;
                previous=next;
            }
        };
        advect_velocity(dt);
        stage(last_face_energy_stages_.advection_delta_j);
        if(kinematic_viscosity_m2_s_>0){
            // Snapshot actual liquid occupancy AFTER the previous VOF update.
            // No per-voxel viscosity copies: this is a temporary wet mask.
            std::vector<std::uint8_t> wet_mask(fill_.size(),0);
            for(std::size_t i=0;i<fill_.size();++i)
                wet_mask[i]=(!solid_[i]&&fill_[i]>wet)?1:0;
            ViscousMacDiffusion3D visc(nx_,ny_,nz_,dx_);
            last_viscous_energy_before_=visc.face_energy_proxy(mac_);
            MaskedMacMomentumLedger3D ledger;
            last_viscosity_substeps_+=visc.step_masked(
                mac_,dt,kinematic_viscosity_m2_s_,wet_mask,solid_,
                wall_tangential_velocity_m_s_,&ledger);
            const double face_mass_proxy=rho_*dx_*dx_*dx_;
            // Keep the unscaled, exact operator ledger across adaptive live
            // MAC/VOF substeps; pressure/gravity/advection are excluded.
            for(std::size_t axis=0;axis<3;++axis){
                last_viscous_diffusion_ledger_.active_face_delta_sum_m_s[axis]+=
                    ledger.active_face_delta_sum_m_s[axis];
                last_viscous_diffusion_ledger_.solid_boundary_source_sum_m_s[axis]+=
                    ledger.solid_boundary_source_sum_m_s[axis];
                last_viscous_diffusion_ledger_.solid_wall_drive_sum_m_s[axis]+=
                    ledger.solid_wall_drive_sum_m_s[axis];
                last_viscous_diffusion_ledger_.solid_wall_drag_sum_m_s[axis]+=
                    ledger.solid_wall_drag_sum_m_s[axis];
                last_viscous_diffusion_ledger_.exterior_boundary_source_sum_m_s[axis]+=
                    ledger.exterior_boundary_source_sum_m_s[axis];
                last_viscous_diffusion_ledger_.closure_residual_sum_m_s[axis]+=
                    ledger.closure_residual_sum_m_s[axis];
                last_viscous_diffusion_ledger_.face_energy_change_proxy_m2_s2[axis]+=
                    ledger.face_energy_change_proxy_m2_s2[axis];
                last_viscous_diffusion_ledger_.internal_gradient_loss_proxy_m2_s2[axis]+=
                    ledger.internal_gradient_loss_proxy_m2_s2[axis];
                last_viscous_diffusion_ledger_.solid_gradient_loss_proxy_m2_s2[axis]+=
                    ledger.solid_gradient_loss_proxy_m2_s2[axis];
                last_viscous_diffusion_ledger_.exterior_gradient_loss_proxy_m2_s2[axis]+=
                    ledger.exterior_gradient_loss_proxy_m2_s2[axis];
                last_viscous_diffusion_ledger_.prescribed_wall_work_proxy_m2_s2[axis]+=
                    ledger.prescribed_wall_work_proxy_m2_s2[axis];
                last_viscous_diffusion_ledger_.explicit_euler_correction_proxy_m2_s2[axis]+=
                    ledger.explicit_euler_correction_proxy_m2_s2[axis];
                last_viscous_diffusion_ledger_.energy_closure_residual_proxy_m2_s2[axis]+=
                    ledger.energy_closure_residual_proxy_m2_s2[axis];
            }
            last_viscous_diffusion_ledger_.substeps+=ledger.substeps;
            for(std::size_t axis=0;axis<3;++axis){
                last_viscous_face_momentum_delta_proxy_kg_m_s_[axis]+=
                    face_mass_proxy*ledger.active_face_delta_sum_m_s[axis];
                last_viscous_solid_wall_source_proxy_kg_m_s_[axis]+=
                    face_mass_proxy*ledger.solid_boundary_source_sum_m_s[axis];
                last_viscous_solid_wall_drive_proxy_kg_m_s_[axis]+=
                    face_mass_proxy*ledger.solid_wall_drive_sum_m_s[axis];
                last_viscous_solid_wall_drag_proxy_kg_m_s_[axis]+=
                    face_mass_proxy*ledger.solid_wall_drag_sum_m_s[axis];
                last_viscous_exterior_source_proxy_kg_m_s_[axis]+=
                    face_mass_proxy*ledger.exterior_boundary_source_sum_m_s[axis];
                last_viscous_closure_residual_proxy_kg_m_s_[axis]+=
                    face_mass_proxy*ledger.closure_residual_sum_m_s[axis];
            }
            last_viscous_energy_after_=visc.face_energy_proxy(mac_);
        }
        stage(last_face_energy_stages_.viscosity_delta_j);
        mac_.apply_gravity(dt);
        stage(last_face_energy_stages_.gravity_delta_j);
        if(pcg_pressure_projection_enabled_) {
            sync_axis_vof_pcg_geometry();
            const auto audit=mac_.project_pcg_verified(
                dt,pressure_iterations,tolerance,1e-10);
            last_pcg_iterations_+=static_cast<std::size_t>(audit.iterations);
            last_pcg_max_equation_residual_pa_=std::max(
                last_pcg_max_equation_residual_pa_,audit.max_equation_residual_pa);
            last_projection_divergence_=audit.max_divergence_s_inv;
        } else if(verified) {
            last_projection_divergence_=mac_.project_verified(
                dt,pressure_iterations,tolerance);
        } else {
            mac_.project(dt,pressure_iterations);
            last_projection_divergence_=mac_.max_divergence();
        }
        stage(last_face_energy_stages_.projection_delta_j);
        advect_fraction(dt);
        stage(last_face_energy_stages_.vof_transport_delta_j);
        // Optional second projection uses the NEW wet mask after VOF transport.
        // The pre-transport pressure solve alone does not constrain newly wet cells.
        if(reproject_after_transport){
            sync_occupancy();
            if(pcg_pressure_projection_enabled_) {
                sync_axis_vof_pcg_geometry();
                const auto audit=mac_.project_pcg_verified(
                    dt,pressure_iterations,tolerance,1e-10);
                last_pcg_iterations_+=static_cast<std::size_t>(audit.iterations);
                last_pcg_max_equation_residual_pa_=std::max(
                    last_pcg_max_equation_residual_pa_,audit.max_equation_residual_pa);
                last_projection_divergence_=audit.max_divergence_s_inv;
            } else {
                last_projection_divergence_=mac_.project_verified(
                    dt,pressure_iterations,tolerance);
            }
            stage(last_face_energy_stages_.post_projection_delta_j);
        }
    }
public:
    CoupledMacWater3D(std::size_t nx,std::size_t ny,std::size_t nz,double dx,
                      double density=1000)
       :nx_(nx),ny_(ny),nz_(nz),dx_(dx),rho_(density),
        mac_(nx,ny,nz,dx,density),fill_(nx*ny*nz,0),solid_(nx*ny*nz,0) {
        if(!nx||!ny||!nz||!std::isfinite(dx)||dx<=0||
           !std::isfinite(density)||density<=0||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz)
            throw std::invalid_argument("Invalid coupled domain");
    }
    void set_water(std::size_t x,std::size_t y,std::size_t z,double fraction){
        if(!std::isfinite(fraction)||fraction<0||fraction>1)
            throw std::invalid_argument("Bad 3D water fraction");
        const auto i=cell(x,y,z);
        if(solid_[i]&&fraction>0)throw std::invalid_argument("Solid water overlap");
        fill_[i]=fraction;
    }
    void set_solid(std::size_t x,std::size_t y,std::size_t z,bool s=true){
        const auto i=cell(x,y,z);
        if(s&&fill_[i]>0)throw std::invalid_argument("Solid water overlap");
        solid_[i]=s?1:0;
    }
    double water_fraction(std::size_t x,std::size_t y,std::size_t z)const{
        return fill_[cell(x,y,z)];
    }
    bool is_solid(std::size_t x,std::size_t y,std::size_t z)const{
        return solid_[cell(x,y,z)]!=0;
    }
    double pressure_pa(std::size_t x,std::size_t y,std::size_t z)const{
        return mac_.pressure_pa(x,y,z);
    }
    double volume_m3()const{
        double v=0;
        for(double f:fill_)v+=f;
        return v*dx_*dx_*dx_;
    }
    double mass_kg()const{return rho_*volume_m3();}
    // Cell-centre gravitational potential relative to the z=0 floor.
    // This is a VOF mass-height diagnostic, not total mechanical energy.
    double water_gravitational_potential_j()const{
        double sum=0;
        const double cell_mass=rho_*dx_*dx_*dx_;
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            sum+=fill_[cell(x,y,z)]*cell_mass*9.81*(z+0.5)*dx_;
        return sum;
    }
    // VOF-weighted cell-centred mass/momentum/kinetic/potential snapshot.
    // Samples the two adjacent MAC faces per axis. This is an approximation
    // to the water inventory, not exact continuum energy or FSI closure.
    VofMechanicalInventory3D water_mechanical_inventory()const{
        return vof_mechanical_inventory3d(
            mac_,fill_,solid_,nx_,ny_,nz_,dx_,rho_);
    }
    // Sum of the discrete VOF transport-stage potential changes over the
    // last successful step, including all adaptive substeps. A failed
    // transactional verified step leaves this diagnostic unchanged.
    double last_vof_transport_potential_delta_j()const{
        return last_vof_transport_potential_delta_j_;
    }
    // Opt-in only: six stage contributions are numerical face proxies.
    void enable_face_energy_stages(bool enabled){face_energy_stages_enabled_=enabled;}
    bool face_energy_stages_enabled()const{return face_energy_stages_enabled_;}
    MacFaceEnergyStages3D last_face_energy_stages()const{return last_face_energy_stages_;}
    double current_face_energy_proxy_j()const{return face_energy_proxy_j();}
    double last_projection_divergence()const{return last_projection_divergence_;}
    // Read-only diagnostic using CURRENT VOF occupancy, not the wet mask
    // cached before transport by the original verified step.
    double current_wet_divergence_s_inv()const{
        MacProjection3D probe=mac_;
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x){
            const auto i=cell(x,y,z);
            probe.set_cell(x,y,z,!solid_[i]&&fill_[i]>pressure_wet_threshold(),solid_[i]!=0);
        }
        return probe.max_divergence();
    }
    // Opt-in: nu=0 preserves the legacy inviscid MAC/VOF path exactly.
    // Positive nu requires a 3D grid with at least two cells per axis.
    // Stationary solids use approximate no-slip; free surface uses a
    // zero-gradient proxy. No moving-body impulse coupling is implied.
    // P2 opt-in: simultaneous conservative donor-cell flux through completely
    // occupied cells. Original transport remains the default. This is not
    // geometric PLIC and does not guarantee experimental CFD accuracy.
    // P2 opt-in: solve the actual MAC pressure system using a Jacobi-PCG
    // residual check and transactional divergence gate. This is not a
    // two-phase air/liquid stress model or wall-transducer validation.
    void set_pcg_pressure_projection(bool enabled){
        if(!enabled&&(axis_vof_pcg_geometry_enabled_||planar_vof_pcg_geometry_enabled_))
            throw std::invalid_argument("Axis VOF geometry requires PCG");
        pcg_pressure_projection_enabled_=enabled;
    }
    // P2 PARTIAL: externally prescribed liquid-side normal stress pressure
    // (Pa, gauge) at every live water/air interface. Only normal pressure
    // is imposed: no curvature model, air momentum or tangential viscous
    // traction. Zero is the historical default. Requires verified PCG.
    void set_uniform_air_interface_pressure_pa(double pressure){
        if(!pcg_pressure_projection_enabled_)
            throw std::invalid_argument("Enable PCG before air interface pressure");
        mac_.set_uniform_interface_pressure_pa(pressure);
    }
    // P2 experimental opt-in: exact axis-aligned VOF cuts at the PCG
    // liquid/air pressure boundary. This changes the wet mask to f>0.5
    // (the geometry threshold); legacy uses f>1e-5. Oblique or ambiguous
    // partial cuts throw transactionally. No stress-continuous air solver.
    void set_axis_vof_pcg_geometry(bool enabled){
        if(enabled&&planar_vof_pcg_geometry_enabled_)
            throw std::invalid_argument("Planar and axis VOF PCG modes are exclusive");
        if(enabled&&!pcg_pressure_projection_enabled_)
            throw std::invalid_argument("Enable PCG before axis VOF geometry");
        axis_vof_pcg_geometry_enabled_=enabled;
        if(!enabled){
            mac_.clear_interface_distances();
            last_axis_vof_resolved_faces_=0;
            last_axis_vof_unresolved_pure_faces_=0;
        }
    }

    // P2 PARTIAL: the caller prescribes independently known normalized or
    // unnormalized outward normals for each VOF cell. Unknown/default
    // reconstructed normals do not imply a valid continuous interface.
    // Every next verified projection rechecks all volume-matched planes;
    // stale normals or impossible cuts are rejected atomically.
    void set_planar_vof_pcg_normals(
        std::vector<std::array<double,3>> normals) {
        if(auto_planar_vof_normals_enabled_)
            throw std::invalid_argument("Disable automatic planar normals before prescribing normals");
        if(normals.size()!=fill_.size())
            throw std::invalid_argument("Prescribed planar normal count mismatch");
        for(const auto& n:normals)
            for(double a:n)
                if(!std::isfinite(a))
                    throw std::invalid_argument("Nonfinite prescribed planar normal");
        prescribed_planar_vof_normals_=std::move(normals);
    }
    // P2 PARTIAL: strict local PLIC normal fitting on the CURRENT 3D VOF
    // field before each live PCG projection, including post-transport.
    // Geometry mode must be off while switching sources; no fallback on
    // noisy/curved patches and no two-phase stress balance is claimed.
    void set_auto_planar_vof_pcg_normals(bool enabled,double max_patch_error=1e-7){
        if(planar_vof_pcg_geometry_enabled_)
            throw std::invalid_argument("Disable planar geometry before switching normal source");
        if(!std::isfinite(max_patch_error)||max_patch_error<=0||max_patch_error>=.5)
            throw std::invalid_argument("Invalid automatic planar fit tolerance");
        auto_planar_vof_normals_enabled_=enabled;
        auto_planar_vof_max_patch_error_=max_patch_error;
    }
    bool auto_planar_vof_pcg_normals_enabled()const{
        return auto_planar_vof_normals_enabled_;
    }
    std::size_t last_auto_planar_vof_fitted_cells()const{
        return last_auto_planar_vof_fitted_cells_;
    }
    double last_auto_planar_vof_patch_residual()const{
        return last_auto_planar_vof_patch_residual_;
    }
    void set_planar_vof_pcg_geometry(bool enabled) {
        if(enabled&&(!pcg_pressure_projection_enabled_||
                     axis_vof_pcg_geometry_enabled_||
                     (!auto_planar_vof_normals_enabled_&&
                      prescribed_planar_vof_normals_.size()!=fill_.size())))
            throw std::invalid_argument("Planar VOF mode requires PCG and valid normal source");
        planar_vof_pcg_geometry_enabled_=enabled;
        if(!enabled) {
            mac_.clear_interface_distances();
            last_planar_vof_resolved_faces_=0;
            last_planar_vof_oblique_faces_=0;
            last_planar_vof_two_partial_faces_=0;
            last_auto_planar_vof_fitted_cells_=0;
            last_auto_planar_vof_patch_residual_=0;
        }
    }
    bool planar_vof_pcg_geometry_enabled()const{
        return planar_vof_pcg_geometry_enabled_;
    }
    std::size_t last_planar_vof_resolved_faces()const{
        return last_planar_vof_resolved_faces_;
    }
    std::size_t last_planar_vof_oblique_faces()const{
        return last_planar_vof_oblique_faces_;
    }
    std::size_t last_planar_vof_two_partial_faces()const{
        return last_planar_vof_two_partial_faces_;
    }
    bool axis_vof_pcg_geometry_enabled()const{return axis_vof_pcg_geometry_enabled_;}
    std::size_t last_axis_vof_resolved_faces()const{return last_axis_vof_resolved_faces_;}
    std::size_t last_axis_vof_unresolved_pure_faces()const{return last_axis_vof_unresolved_pure_faces_;}
    bool pcg_pressure_projection_enabled()const{
        return pcg_pressure_projection_enabled_;
    }
    std::size_t last_pcg_projection_iterations()const{
        return last_pcg_iterations_;
    }
    double last_pcg_max_equation_residual_pa()const{
        return last_pcg_max_equation_residual_pa_;
    }
    void set_conservative_throughflow_transport(bool enabled){
        conservative_throughflow_transport_=enabled;
    }
    bool conservative_throughflow_transport_enabled()const{
        return conservative_throughflow_transport_;
    }
    void set_kinematic_viscosity(double nu_m2_s){
        if(!std::isfinite(nu_m2_s)||nu_m2_s<0||
           (nu_m2_s>0&&(nx_<2||ny_<2||nz_<2)))
            throw std::invalid_argument("Invalid coupled kinematic viscosity");
        kinematic_viscosity_m2_s_=nu_m2_s;
    }
    double kinematic_viscosity_m2_s()const{return kinematic_viscosity_m2_s_;}
    // Experimental fixed-geometry Couette boundary, opt-in and effective
    // only when nu>0. This prescribes tangential velocity at *static*
    // solid interfaces; it does NOT move solids or exchange body impulse.
    // The operator prevents normal flow at solid faces. Wall-driven shear
    // may ADD kinetic energy: viscous face-energy is not monotone in this mode.
    void set_wall_tangential_velocity(const std::array<double,3>& v_m_s){
        for(double v:v_m_s)
            if(!std::isfinite(v))
                throw std::invalid_argument("Nonfinite coupled wall velocity");
        wall_tangential_velocity_m_s_=v_m_s;
    }
    std::array<double,3> wall_tangential_velocity_m_s()const{
        return wall_tangential_velocity_m_s_;
    }
    std::size_t last_viscosity_substeps()const{return last_viscosity_substeps_;}
    // Returns a copy of the *last public step* diffusion-only ledger. It
    // resets each step and includes every internal adaptive substep.
    MaskedMacMomentumLedger3D last_viscous_diffusion_ledger()const{
        return last_viscous_diffusion_ledger_;
    }
    // Diagnostic only: the sums are staggered-face momentum proxies for
    // diffusion, NOT a conservative physical solid-body reaction impulse.
    std::array<double,3> last_viscous_face_momentum_delta_proxy_kg_m_s()const{
        return last_viscous_face_momentum_delta_proxy_kg_m_s_;
    }
    std::array<double,3> last_viscous_solid_wall_source_proxy_kg_m_s()const{
        return last_viscous_solid_wall_source_proxy_kg_m_s_;
    }
    // Drive/drag are numeric source proxies, not force/impulse on a body.
    std::array<double,3> last_viscous_solid_wall_drive_proxy_kg_m_s()const{
        return last_viscous_solid_wall_drive_proxy_kg_m_s_;
    }
    std::array<double,3> last_viscous_solid_wall_drag_proxy_kg_m_s()const{
        return last_viscous_solid_wall_drag_proxy_kg_m_s_;
    }
    std::array<double,3> last_viscous_exterior_source_proxy_kg_m_s()const{
        return last_viscous_exterior_source_proxy_kg_m_s_;
    }
    std::array<double,3> last_viscous_closure_residual_proxy_kg_m_s()const{
        return last_viscous_closure_residual_proxy_kg_m_s_;
    }
    // Before/after *diffusion only*, prior to gravity/projection.
    // Face-square proxy is NOT the full physical fluid energy.
    double last_viscous_energy_before()const{return last_viscous_energy_before_;}
    double last_viscous_energy_after()const{return last_viscous_energy_after_;}
    std::size_t last_substeps()const{return substeps_;}
    std::size_t cell_count()const{return fill_.size();}
    std::size_t nx()const{return nx_;}
    std::size_t ny()const{return ny_;}
    std::size_t nz()const{return nz_;}
    double cell_size_m()const{return dx_;}
    double density_kg_m3()const{return rho_;}
    void set_transport_listener(std::function<void(const TransportFlux3D&)> listener){
        transport_callback_=std::move(listener);
    }

    // Exact resident capacity for owned grid arrays; excludes transient vectors,
    // allocators, stack, and instrumentation.
    std::size_t resident_bytes_model()const{
        const std::size_t n=fill_.size();
        const std::size_t faces=(nx_+1)*ny_*nz_+nx_*(ny_+1)*nz_+nx_*ny_*(nz_+1);
        return n*(sizeof(double)+sizeof(std::uint8_t))+
               n*(sizeof(double)+sizeof(std::uint8_t)*2)+faces*sizeof(double);
    }
private:
    void step_impl(double seconds,int pressure_iterations,bool verified,double tolerance,
                   bool reproject_after_transport=false){
        if(!std::isfinite(seconds)||seconds<=0||seconds>2 ||
           pressure_iterations<1||pressure_iterations>10000)
            throw std::invalid_argument("Invalid coupled step");
        if((axis_vof_pcg_geometry_enabled_||planar_vof_pcg_geometry_enabled_)&&!verified)
            throw std::invalid_argument("Geometric VOF pressure requires transactional verified step");
        double remaining=seconds;
        substeps_=0;
        last_pcg_iterations_=0;
        last_axis_vof_resolved_faces_=0;
        last_axis_vof_unresolved_pure_faces_=0;
        last_pcg_max_equation_residual_pa_=0;
        last_vof_transport_potential_delta_j_=0;
        last_face_energy_stages_={};
        if(face_energy_stages_enabled_)
            last_face_energy_stages_.initial_j=face_energy_proxy_j();
        last_viscosity_substeps_=0;
        last_viscous_diffusion_ledger_=MaskedMacMomentumLedger3D{};
        last_viscous_face_momentum_delta_proxy_kg_m_s_={};
        last_viscous_solid_wall_source_proxy_kg_m_s_={};
        last_viscous_solid_wall_drive_proxy_kg_m_s_={};
        last_viscous_solid_wall_drag_proxy_kg_m_s_={};
        last_viscous_exterior_source_proxy_kg_m_s_={};
        last_viscous_closure_residual_proxy_kg_m_s_={};
        last_viscous_energy_before_=0;
        last_viscous_energy_after_=0;
        while(remaining>1e-13){
            if(++substeps_>4000)throw std::runtime_error("Coupled solver substep cap");
            const double peak=max_velocity_sum();
            const double dt=std::min(remaining,peak>0?0.18*dx_/peak:remaining);
            substep(dt,pressure_iterations,verified,tolerance,reproject_after_transport);
            remaining-=dt;
        }
        if(face_energy_stages_enabled_){
            auto& e=last_face_energy_stages_;
            e.final_j=face_energy_proxy_j();
            e.substeps=substeps_;
            const double delta=e.advection_delta_j+e.viscosity_delta_j+
                e.gravity_delta_j+e.projection_delta_j+
                e.vof_transport_delta_j+e.post_projection_delta_j;
            e.closure_residual_j=(e.final_j-e.initial_j)-delta;
        }
    }
public:
    // Preserve the original, unverified solver path for existing callers.
    void step(double seconds,int pressure_iterations=250){
        step_impl(seconds,pressure_iterations,false,1e-8);
    }
    // Opt-in transactional LIVE MAC/VOF step. Every adaptive substep uses
    // residual-verified projection. On numerical failure the complete fluid
    // state, diagnostics and external transport notifications remain unchanged.
    // Notifications are replayed only after commit; exceptions thrown by a
    // user callback after commit cannot undo external side effects.
    double step_verified(double seconds,int pressure_iterations=250,
                         double max_divergence_s_inv=1e-8){
        if(!std::isfinite(max_divergence_s_inv)||max_divergence_s_inv<=0)
            throw std::invalid_argument("Invalid coupled projection tolerance");
        CoupledMacWater3D candidate=*this;
        std::vector<TransportFlux3D> deferred;
        if(transport_callback_)
            candidate.transport_callback_=[&deferred](const TransportFlux3D& e){
                deferred.push_back(e);
            };
        candidate.step_impl(seconds,pressure_iterations,true,max_divergence_s_inv);
        const double residual=candidate.last_projection_divergence_;
        candidate.transport_callback_=transport_callback_;
        *this=std::move(candidate);
        if(transport_callback_)
            for(const auto& event:deferred)transport_callback_(event);
        return residual;
    }
    // Opt-in P2 post-VOF projection: the ordinary verified step checks the
    // pre-transport wet mask. Newly wetted cells can violate incompressibility
    // until the next step. This variant projects AGAIN on the transported
    // wet mask after every substep and commits only the fully verified result.
    // It does not model moving-solid FSI or guarantee energy conservation.
    double step_verified_reprojected(double seconds,int pressure_iterations=250,
                                     double max_divergence_s_inv=1e-8){
        if(!std::isfinite(max_divergence_s_inv)||max_divergence_s_inv<=0)
            throw std::invalid_argument("Invalid post-VOF projection tolerance");
        const double before=mass_kg();
        CoupledMacWater3D candidate=*this;
        std::vector<TransportFlux3D> deferred;
        if(transport_callback_)
            candidate.transport_callback_=[&deferred](const TransportFlux3D& e){
                deferred.push_back(e);
            };
        candidate.step_impl(seconds,pressure_iterations,true,
                            max_divergence_s_inv,true);
        const double after=candidate.mass_kg();
        if(!std::isfinite(after)||
           std::abs(after-before)>1e-10*std::max(1.0,std::abs(before)))
            throw std::runtime_error("Post-VOF water mass budget failed");
        for(std::size_t i=0;i<candidate.fill_.size();++i)
            if(!std::isfinite(candidate.fill_[i])||
               candidate.fill_[i]<0||candidate.fill_[i]>1||
               (candidate.solid_[i]&&candidate.fill_[i]>0))
                throw std::runtime_error("Post-VOF occupancy invalid");
        const double residual=candidate.last_projection_divergence_;
        candidate.transport_callback_=transport_callback_;
        *this=std::move(candidate);
        if(transport_callback_)
            for(const auto& event:deferred)transport_callback_(event);
        return residual;
    }
};
} // namespace upc
