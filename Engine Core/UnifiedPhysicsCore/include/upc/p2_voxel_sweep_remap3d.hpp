#pragma once
// P2 PARTIAL: exact one-voxel rigid-mask translation and conservative VOF
// reclassification. This is a geometry/mass bookkeeping kernel, NOT FSI:
// no MAC-face remap, pressure impulse, swept-boundary force or fluid momentum.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace upc {
struct VoxelSweepRemapResult3D {
    std::vector<std::uint8_t> body_mask;
    std::vector<double> water_fill;
    std::size_t entering_cells=0;
    std::size_t vacated_cells=0;
    double displaced_water_m3=0.0;
    double mass_residual_m3=0.0;
};

class VoxelSweepRemap3D {
public:
    // A signed unit axis translation, never diagonal or sub-voxel.
    // Each entering cell's VOF is transferred to the vacated cell at the
    // trailing end of the same contiguous OLD body run. This artificial
    // instantaneous through-body transfer is only a conservative remap,
    // NOT a physical local advection or hydrodynamic swept-volume solve.
    // No caller data is changed, including on failure.
    static VoxelSweepRemapResult3D translate(
        std::size_t nx,std::size_t ny,std::size_t nz,double dx_m,
        const std::vector<std::uint8_t>& body,
        const std::vector<std::uint8_t>& fixed_solid,
        const std::vector<double>& fill,
        const std::array<int,3>& delta) {
        if(nx==0||ny==0||nz==0||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           !std::isfinite(dx_m)||dx_m<=0||
           !std::isfinite(dx_m*dx_m*dx_m))
            throw std::invalid_argument("Invalid sweep domain or cell size");
        const std::size_t n=nx*ny*nz;
        if(body.size()!=n||fixed_solid.size()!=n||fill.size()!=n)
            throw std::invalid_argument("Sweep array size mismatch");
        int axis=-1,step=0;
        for(int a=0;a<3;++a) {
            if(delta[a]==0)continue;
            if(axis>=0|| (delta[a]!=1&&delta[a]!=-1))
                throw std::invalid_argument("Sweep requires one signed axis cell");
            axis=a;step=delta[a];
        }
        if(axis<0)throw std::invalid_argument("Zero sweep is not a movement");
        std::size_t body_count=0;
        double initial=0.0;
        for(std::size_t i=0;i<n;++i) {
            if(body[i]>1||fixed_solid[i]>1||
               (body[i]&&fixed_solid[i])||
               !std::isfinite(fill[i])||fill[i]<0||fill[i]>1||
               ((body[i]||fixed_solid[i])&&fill[i]!=0))
                throw std::invalid_argument("Invalid solid or water occupancy");
            body_count+=body[i];
            initial+=fill[i];
        }
        if(body_count==0)throw std::invalid_argument("Empty moving body");
        const std::array<std::size_t,3> dims{{nx,ny,nz}};
        auto index=[&](std::size_t x,std::size_t y,std::size_t z) {
            return (z*ny+y)*nx+x;
        };
        VoxelSweepRemapResult3D out;
        out.body_mask.assign(n,0);
        out.water_fill=fill;
        // Construct the entire new mask before touching water fractions.
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const std::size_t old=index(x,y,z);
            if(!body[old])continue;
            std::array<std::size_t,3> c{{x,y,z}};
            if((step<0&&c[axis]==0)||
               (step>0&&c[axis]+1>=dims[axis]))
                throw std::invalid_argument("Body sweep exits domain");
            if(step<0)--c[axis];else ++c[axis];
            const std::size_t dest=index(c[0],c[1],c[2]);
            if(fixed_solid[dest])
                throw std::invalid_argument("Body sweep hits fixed solid");
            out.body_mask[dest]=1;
        }
        for(std::size_t z=0;z<nz;++z)for(std::size_t y=0;y<ny;++y)
        for(std::size_t x=0;x<nx;++x) {
            const std::size_t entering=index(x,y,z);
            if(!out.body_mask[entering]||body[entering])continue;
            ++out.entering_cells;
            // Walk backward along the OLD solid run until its trailing cell.
            std::array<std::size_t,3> c{{x,y,z}};
            std::size_t trailing=entering;
            while(true) {
                if((step>0&&c[axis]==0)||
                   (step<0&&c[axis]+1>=dims[axis]))
                    throw std::logic_error("Missing trailing sweep cell");
                if(step>0)--c[axis];else ++c[axis];
                const std::size_t candidate=index(c[0],c[1],c[2]);
                if(!body[candidate])
                    throw std::logic_error("Broken old-body run during sweep");
                trailing=candidate;
                if(!out.body_mask[trailing])break;
            }
            if(!body[trailing]||out.body_mask[trailing])
                throw std::logic_error("Invalid sweep pairing");
            const double displaced=fill[entering];
            out.water_fill[trailing]=displaced;
            out.water_fill[entering]=0.0;
            out.displaced_water_m3+=displaced*dx_m*dx_m*dx_m;
        }
        double final=0.0;
        for(std::size_t i=0;i<n;++i) {
            if(body[i]&&!out.body_mask[i])++out.vacated_cells;
            if((out.body_mask[i]||fixed_solid[i])&&out.water_fill[i]!=0)
                throw std::logic_error("Water/solid overlap after sweep");
            final+=out.water_fill[i];
        }
        if(out.entering_cells!=out.vacated_cells)
            throw std::logic_error("Sweep entering/vacated count mismatch");
        out.mass_residual_m3=(final-initial)*dx_m*dx_m*dx_m;
        return out;
    }
};
} // namespace upc
