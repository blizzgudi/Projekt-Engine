#pragma once
// P2 PARTIAL: piecewise-constant cut-cell pressure traction on a prescribed
// translating box. Diagnostic equal/opposite impulse, NOT applied two-way FSI.
// Exact voxel-swept pressure work is kept separate from midpoint surface work.
#include "upc/p2_moving_box_open_volume3d.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2MovingBoxPressureTractionResult3D {
    std::array<double,3> solid_force_n{},solid_torque_nm{};
    std::array<double,3> fluid_counter_impulse_kg_m_s{};
    double fluid_geometric_pressure_work_j=0.0;
    double solid_geometric_pressure_work_j=0.0;
    double midpoint_traction_work_j=0.0;
    double midpoint_work_difference_j=0.0;
    double integrated_surface_area_m2=0.0;
};
class P2MovingBoxPressureTraction3D {
public:
    // p is cell-centered Pa on the primal (nx,ny,nz) grid. The solid
    // surface is evaluated at the midpoint of the prescribed translation.
    // Each surface tile samples pressure from the OUTSIDE cut cell.
    // Force on solid is -p*n_solid*dA, torque about midpoint box center.
    // The fluid counter-impulse is only a proposed action/reaction ledger:
    // it is NOT fed back to the fluid or a dynamic body.
    static P2MovingBoxPressureTractionResult3D audit(
        std::size_t nx,std::size_t ny,std::size_t nz,
        double dx,double dt,const P2Box3D& old_box,
        const P2Box3D& new_box,const std::vector<double>& pressure_pa) {
        const auto g=P2MovingBoxOpenVolumes3D::sample(
            nx,ny,nz,dx,dt,old_box,new_box);
        const std::array<std::size_t,3> dims{{nx,ny,nz}};
        if(pressure_pa.size()!=g.alpha_old.size())
            throw std::invalid_argument("Pressure traction shape mismatch");
        for(double p:pressure_pa)
            if(!std::isfinite(p))
                throw std::invalid_argument("Nonfinite pressure traction input");
        P2Box3D mid;
        std::array<double,3> center{};
        for(int a=0;a<3;++a) {
            mid.min_m[a]=0.5*(old_box.min_m[a]+new_box.min_m[a]);
            mid.max_m[a]=0.5*(old_box.max_m[a]+new_box.max_m[a]);
            center[a]=0.5*(mid.min_m[a]+mid.max_m[a]);
        }
        const auto index=[&](const std::array<std::size_t,3>& ijk) {
            return (ijk[2]*ny+ijk[1])*nx+ijk[0];
        };
        P2MovingBoxPressureTractionResult3D result;
        const double cell_volume=dx*dx*dx;
        for(std::size_t i=0;i<pressure_pa.size();++i)
            result.fluid_geometric_pressure_work_j+=
                pressure_pa[i]*(g.alpha_old[i]-g.alpha_new[i])*cell_volume;
        result.solid_geometric_pressure_work_j=
            -result.fluid_geometric_pressure_work_j;
        for(int axis=0;axis<3;++axis)
        for(int side:{-1,1}) {
            const double face=side<0?mid.min_m[axis]:mid.max_m[axis];
            const double toward=side<0?
                -std::numeric_limits<double>::infinity():
                 std::numeric_limits<double>::infinity();
            const double sample=std::nextafter(face/dx,toward);
            const auto normal_cell=static_cast<long long>(std::floor(sample));
            if(normal_cell<0||
               normal_cell>=static_cast<long long>(dims[axis]))
                throw std::invalid_argument("Pressure face outside grid");
            const int b=(axis+1)%3,c=(axis+2)%3;
            for(std::size_t j=0;j<dims[b];++j)
            for(std::size_t k=0;k<dims[c];++k) {
                const double bj=static_cast<double>(j)*dx;
                const double ck=static_cast<double>(k)*dx;
                const double len_b=std::max(0.0,std::min(bj+dx,mid.max_m[b])-
                                                    std::max(bj,mid.min_m[b]));
                const double len_c=std::max(0.0,std::min(ck+dx,mid.max_m[c])-
                                                    std::max(ck,mid.min_m[c]));
                const double area=len_b*len_c;
                if(area==0.0)continue;
                std::array<std::size_t,3> ijk{};
                ijk[axis]=static_cast<std::size_t>(normal_cell);
                ijk[b]=j;ijk[c]=k;
                const double f=-static_cast<double>(side)*pressure_pa[index(ijk)]*area;
                std::array<double,3> r{};
                r[axis]=face-center[axis];
                r[b]=0.5*(std::max(bj,mid.min_m[b])+
                          std::min(bj+dx,mid.max_m[b]))-center[b];
                r[c]=0.5*(std::max(ck,mid.min_m[c])+
                          std::min(ck+dx,mid.max_m[c]))-center[c];
                std::array<double,3> force{};
                force[axis]=f;
                result.solid_force_n[axis]+=f;
                result.solid_torque_nm[0]+=r[1]*force[2]-r[2]*force[1];
                result.solid_torque_nm[1]+=r[2]*force[0]-r[0]*force[2];
                result.solid_torque_nm[2]+=r[0]*force[1]-r[1]*force[0];
                result.integrated_surface_area_m2+=area;
            }
        }
        for(int a=0;a<3;++a) {
            result.fluid_counter_impulse_kg_m_s[a]=
                -dt*result.solid_force_n[a];
            result.midpoint_traction_work_j+=
                result.solid_force_n[a]*g.translation_m[a];
        }
        result.midpoint_work_difference_j=
            result.midpoint_traction_work_j-
            result.solid_geometric_pressure_work_j;
        const auto finite=[&](double x) {
            if(!std::isfinite(x))
                throw std::runtime_error("Nonfinite pressure traction accumulation");
        };
        finite(result.fluid_geometric_pressure_work_j);
        finite(result.solid_geometric_pressure_work_j);
        finite(result.midpoint_traction_work_j);
        finite(result.midpoint_work_difference_j);
        finite(result.integrated_surface_area_m2);
        for(int a=0;a<3;++a) {
            finite(result.solid_force_n[a]);
            finite(result.solid_torque_nm[a]);
            finite(result.fluid_counter_impulse_kg_m_s[a]);
        }
        return result;
    }
};
} // namespace upc
