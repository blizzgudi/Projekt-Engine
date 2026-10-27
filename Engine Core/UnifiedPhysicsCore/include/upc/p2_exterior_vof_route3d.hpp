#pragma once
// P2 PARTIAL: exterior-only, face-connected VOF permutation for a one-cell
// translating voxel body. Topological mass bookkeeping, NOT a physical
// VOF flux/pressure solve, MAC remap, or two-way fluid-solid interaction.
#include "upc/p2_voxel_sweep_remap3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct ExteriorVofRoute3D {
    std::vector<std::size_t> cell_indices; // entering -> exterior -> vacated
    double leading_fraction=0.0;
};
struct ExteriorVoxelSweep3D {
    std::vector<std::uint8_t> body_mask;
    std::vector<double> water_fill;
    std::vector<ExteriorVofRoute3D> routes;
    std::size_t entering_cells=0, vacated_cells=0, face_steps=0;
    double leading_displaced_m3=0.0, mass_residual_m3=0.0;
};

class ExteriorVoxelSweep3DRemap {
public:
    // A bounded proof-of-concept. Each wet leading cell is paired with the
    // trailing vacated cell of the SAME old contiguous body run. A shortest
    // six-neighbor route must exist through cells free of BOTH old and new
    // body masks, with no fixed-solid cells. Direct leading->trailing transfer
    // is forbidden: at least one exterior cell is required. Fractions are
    // permuted one cell per route edge, so each stays in [0,1] and the total
    // remains invariant. This is NOT time-resolved physical advection.
    // All inputs are const and errors leave the caller's state untouched.
    static ExteriorVoxelSweep3D translate(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,
        const std::vector<std::uint8_t>& body,
        const std::vector<std::uint8_t>& fixed,
        const std::vector<double>& fill,
        const std::array<int,3>& delta) {
        const auto base=VoxelSweepRemap3D::translate(
            nx,ny,nz,dx_m,body,fixed,fill,delta);
        const std::size_t n=body.size();
        if(n>2000000)
            throw std::invalid_argument("Exterior route prototype grid limit");
        int axis=-1,sign=0;
        for(int a=0;a<3;++a)if(delta[a]){axis=a;sign=delta[a];}
        const std::array<std::size_t,3> dims{{nx,ny,nz}};
        auto index=[&](const std::array<std::size_t,3>& c) {
            return (c[2]*ny+c[1])*nx+c[0];
        };
        auto coords=[&](std::size_t i) {
            return std::array<std::size_t,3>{{i%nx,(i/nx)%ny,i/(nx*ny)}};
        };
        ExteriorVoxelSweep3D out;
        out.body_mask=base.body_mask;
        out.water_fill=fill;
        out.entering_cells=base.entering_cells;
        out.vacated_cells=base.vacated_cells;
        const double cell_m3=dx_m*dx_m*dx_m;
        for(std::size_t source=0;source<n;++source) {
            if(!out.body_mask[source]||body[source])continue;
            // Pair with the newly vacated trailing cell on this body run.
            auto c=coords(source);
            std::size_t target=n;
            while(true) {
                if(sign>0)--c[axis];else ++c[axis];
                const std::size_t k=index(c);
                if(!body[k])throw std::logic_error("Broken old body run");
                if(!out.body_mask[k]){target=k;break;}
            }
            const double fraction=fill[source];
            if(fraction==0.0)continue; // dry motion needs no fluid route
            std::vector<std::size_t> parent(n,n),queue;
            queue.reserve(n);
            parent[source]=source;
            queue.push_back(source);
            std::size_t head=0;
            while(head<queue.size()&&parent[target]==n) {
                const auto at=queue[head++];
                const auto pos=coords(at);
                for(int a=0;a<3;++a)for(int s:{-1,1}) {
                    if((s<0&&pos[a]==0)||(s>0&&pos[a]+1>=dims[a]))
                        continue;
                    auto next=pos;
                    if(s<0)--next[a];else ++next[a];
                    const auto k=index(next);
                    if(parent[k]!=n||fixed[k])continue;
                    if(k==target) {
                        // No direct path through the solid's old footprint.
                        if(at==source)continue;
                    } else if(body[k]||out.body_mask[k])continue;
                    parent[k]=at;
                    queue.push_back(k);
                }
            }
            if(parent[target]==n)
                throw std::invalid_argument(
                    "No exterior face-connected fluid displacement route");
            std::vector<std::size_t> path;
            for(std::size_t k=target;k!=source;k=parent[k])
                path.push_back(k);
            path.push_back(source);
            std::reverse(path.begin(),path.end());
            if(path.size()<3)
                throw std::logic_error("Exterior path must avoid body");
            // Move the existing fractions along the exterior path toward
            // the vacated cell; no through-body transfer and no >1 fill.
            for(std::size_t j=path.size()-1;j>0;--j)
                out.water_fill[path[j]]=out.water_fill[path[j-1]];
            out.water_fill[source]=0.0;
            out.face_steps+=path.size()-1;
            out.leading_displaced_m3+=fraction*cell_m3;
            out.routes.push_back({std::move(path),fraction});
        }
        double before=0.0,after=0.0;
        for(std::size_t i=0;i<n;++i) {
            before+=fill[i];
            after+=out.water_fill[i];
            if((out.body_mask[i]||fixed[i])&&out.water_fill[i]!=0.0)
                throw std::logic_error("Exterior route left water in solid");
            if(!std::isfinite(out.water_fill[i])||
               out.water_fill[i]<0||out.water_fill[i]>1)
                throw std::logic_error("Exterior route invalid VOF fraction");
        }
        out.mass_residual_m3=(after-before)*cell_m3;
        if(std::abs(after-before)>1e-10*std::max(1.0,before))
            throw std::logic_error("Exterior route mass conservation failed");
        return out;
    }
};
} // namespace upc
