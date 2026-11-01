#pragma once
// MAC-grid pressure projection building block (P2 research).
// Face-centred velocities, cell-centred pressure. Liquid occupancy fixed
// during project(); free surface: zero pressure in air; impermeable solids.
// No velocity advection or fluid volume transport integrated here.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace upc {
class MacProjection3D {
    std::size_t nx_,ny_,nz_;
    double dx_,rho_;
    std::vector<unsigned char> fluid_,solid_;
    std::vector<double> pressure_,u_,v_,w_;
    // Opt-in geometric distance from the wet cell centre to the p=0
    // interface, in grid spacings. Defaults to the historical half-cell.
    // These distances affect ONLY project_pcg_verified(), not project().
    std::vector<double> interface_x_,interface_y_,interface_z_;
    // Prescribed liquid-side normal-stress boundary pressure at liquid/air
    // faces (Pa, gauge). An externally computed curvature/air/viscous jump
    // may be supplied; this class does NOT calculate any of those stresses.
    // Opt-in values are used only by the verified PCG projection.
    double uniform_interface_pressure_pa_=0;
    std::vector<double> interface_pressure_x_,interface_pressure_y_,interface_pressure_z_;
    std::size_t cell(std::size_t x,std::size_t y,std::size_t z)const{return (z*ny_+y)*nx_+x;}
    std::size_t fu(std::size_t x,std::size_t y,std::size_t z)const{return (z*ny_+y)*(nx_+1)+x;}
    std::size_t fv(std::size_t x,std::size_t y,std::size_t z)const{return (z*(ny_+1)+y)*nx_+x;}
    std::size_t fw(std::size_t x,std::size_t y,std::size_t z)const{return (z*ny_+y)*nx_+x;}
    bool within(int x,int y,int z)const{
        return x>=0&&y>=0&&z>=0&&x<(int)nx_&&y<(int)ny_&&z<(int)nz_;
    }
    bool is_liquid(int x,int y,int z)const{
        return within(x,y,z)&&fluid_[cell(x,y,z)]!=0;
    }
    bool is_wall(int x,int y,int z)const{
        return !within(x,y,z)||solid_[cell(x,y,z)]!=0;
    }
    double p(int x,int y,int z)const{
        return is_liquid(x,y,z)?pressure_[cell(x,y,z)]:0.0;
    }
public:
    MacProjection3D(std::size_t nx,std::size_t ny,std::size_t nz,
                    double dx,double density=1000.0)
     :nx_(nx),ny_(ny),nz_(nz),dx_(dx),rho_(density){
        if(!nx||!ny||!nz||nx>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           ny>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nz>static_cast<std::size_t>(std::numeric_limits<int>::max())||
           nx>std::numeric_limits<std::size_t>::max()/ny||
           nx*ny>std::numeric_limits<std::size_t>::max()/nz||
           !std::isfinite(dx)||dx<=0||!std::isfinite(density)||density<=0)
            throw std::invalid_argument("Invalid MAC dimensions or physical parameters");
        const std::size_t n=nx*ny*nz;
        fluid_.resize(n,0);solid_.resize(n,0);pressure_.resize(n,0);
        u_.resize((nx+1)*ny*nz);v_.resize(nx*(ny+1)*nz);w_.resize(nx*ny*(nz+1));
        // Geometry arrays remain unallocated until a caller opts in.
    }
    void set_cell(std::size_t x,std::size_t y,std::size_t z,bool fluid,bool solid=false){
        if(x>=nx_||y>=ny_||z>=nz_|| (fluid&&solid))throw std::invalid_argument("Bad MAC cell");
        const auto i=cell(x,y,z);fluid_[i]=fluid;solid_[i]=solid;
    }
    // Explicit geometry supplied by a separate interface reconstruction.
    // A VOF fill fraction is NOT automatically a distance to the surface.
    // Solid and liquid-liquid faces ignore the stored value.
    void set_interface_distance_x(std::size_t x,std::size_t y,std::size_t z,double theta){
        if(x==0||x>=nx_||y>=ny_||z>=nz_||!std::isfinite(theta)||
           theta<=0||theta>1)throw std::invalid_argument("Bad x interface distance");
        if(interface_x_.empty())interface_x_.assign(u_.size(),0.5);
        interface_x_[fu(x,y,z)]=theta;
    }
    void set_interface_distance_y(std::size_t x,std::size_t y,std::size_t z,double theta){
        if(x>=nx_||y==0||y>=ny_||z>=nz_||!std::isfinite(theta)||
           theta<=0||theta>1)throw std::invalid_argument("Bad y interface distance");
        if(interface_y_.empty())interface_y_.assign(v_.size(),0.5);
        interface_y_[fv(x,y,z)]=theta;
    }
    void set_interface_distance_z(std::size_t x,std::size_t y,std::size_t z,double theta){
        if(x>=nx_||y>=ny_||z==0||z>=nz_||!std::isfinite(theta)||
           theta<=0||theta>1)throw std::invalid_argument("Bad z interface distance");
        if(interface_z_.empty())interface_z_.assign(w_.size(),0.5);
        interface_z_[fw(x,y,z)]=theta;
    }
    // Drop prior geometric pressure cuts before reconstructing a new
    // VOF frame. An empty geometry must NOT inherit a previous frame.
    void clear_interface_distances(){
        interface_x_.clear();interface_y_.clear();interface_z_.clear();
    }
    // Constant liquid-side boundary pressure (Pa, gauge). Does not affect
    // the legacy Gauss-Seidel solver. Individual face overrides must be
    // cleared before changing the uniform value.
    void set_uniform_interface_pressure_pa(double value){
        if(!std::isfinite(value)||!interface_pressure_x_.empty()||
           !interface_pressure_y_.empty()||!interface_pressure_z_.empty())
            throw std::invalid_argument("Invalid uniform interface pressure or active overrides");
        uniform_interface_pressure_pa_=value;
    }
    void set_interface_pressure_x(std::size_t x,std::size_t y,std::size_t z,double value){
        if(x==0||x>=nx_||y>=ny_||z>=nz_||!std::isfinite(value))
            throw std::invalid_argument("Bad x interface pressure");
        if(interface_pressure_x_.empty())
            interface_pressure_x_.assign(u_.size(),uniform_interface_pressure_pa_);
        interface_pressure_x_[fu(x,y,z)]=value;
    }
    void set_interface_pressure_y(std::size_t x,std::size_t y,std::size_t z,double value){
        if(x>=nx_||y==0||y>=ny_||z>=nz_||!std::isfinite(value))
            throw std::invalid_argument("Bad y interface pressure");
        if(interface_pressure_y_.empty())
            interface_pressure_y_.assign(v_.size(),uniform_interface_pressure_pa_);
        interface_pressure_y_[fv(x,y,z)]=value;
    }
    void set_interface_pressure_z(std::size_t x,std::size_t y,std::size_t z,double value){
        if(x>=nx_||y>=ny_||z==0||z>=nz_||!std::isfinite(value))
            throw std::invalid_argument("Bad z interface pressure");
        if(interface_pressure_z_.empty())
            interface_pressure_z_.assign(w_.size(),uniform_interface_pressure_pa_);
        interface_pressure_z_[fw(x,y,z)]=value;
    }
    // Required when prescribed interface pressure varies with moving VOF
    // geometry: stale face values must never be reused on the next frame.
    void clear_interface_pressures(){
        uniform_interface_pressure_pa_=0;
        interface_pressure_x_.clear();interface_pressure_y_.clear();interface_pressure_z_.clear();
    }
    void set_face_velocity_x(std::size_t x,std::size_t y,std::size_t z,double v){
        if(x>nx_||y>=ny_||z>=nz_||!std::isfinite(v))throw std::invalid_argument("Bad face");
        u_[fu(x,y,z)]=v;
    }
    void set_face_velocity_y(std::size_t x,std::size_t y,std::size_t z,double v){
        if(x>=nx_||y>ny_||z>=nz_||!std::isfinite(v))throw std::invalid_argument("Bad face");
        v_[fv(x,y,z)]=v;
    }
    void set_face_velocity_z(std::size_t x,std::size_t y,std::size_t z,double v){
        if(x>=nx_||y>=ny_||z>nz_||!std::isfinite(v))throw std::invalid_argument("Bad face");
        w_[fw(x,y,z)]=v;
    }
    double face_velocity_x(std::size_t x,std::size_t y,std::size_t z)const{
        if(x>nx_||y>=ny_||z>=nz_)throw std::out_of_range("Face x");
        return u_[fu(x,y,z)];
    }
    double face_velocity_y(std::size_t x,std::size_t y,std::size_t z)const{
        if(x>=nx_||y>ny_||z>=nz_)throw std::out_of_range("Face y");
        return v_[fv(x,y,z)];
    }
    double face_velocity_z(std::size_t x,std::size_t y,std::size_t z)const{
        if(x>=nx_||y>=ny_||z>nz_)throw std::out_of_range("Face z");
        return w_[fw(x,y,z)];
    }
    double pressure_pa(std::size_t x,std::size_t y,std::size_t z)const{
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("Pressure");
        return pressure_[cell(x,y,z)];
    }
    double divergence(std::size_t x,std::size_t y,std::size_t z)const{
        if(x>=nx_||y>=ny_||z>=nz_)throw std::out_of_range("divergence");
        if(!fluid_[cell(x,y,z)])return 0;
        return (u_[fu(x+1,y,z)]-u_[fu(x,y,z)]
               +v_[fv(x,y+1,z)]-v_[fv(x,y,z)]
               +w_[fw(x,y,z+1)]-w_[fw(x,y,z)])/dx_;
    }
    double max_divergence()const{
        double m=0;
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            m=std::max(m,std::abs(divergence(x,y,z)));
        return m;
    }
    void apply_gravity(double dt,double gravity_m_s2=9.81){
        if(!std::isfinite(dt)||dt<=0||!std::isfinite(gravity_m_s2))
            throw std::invalid_argument("Bad gravity step");
        for(std::size_t z=1;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
          for(std::size_t x=0;x<nx_;++x){
            if((fluid_[cell(x,y,z-1)]||fluid_[cell(x,y,z)]) &&
               !solid_[cell(x,y,z-1)]&&!solid_[cell(x,y,z)])
               w_[fw(x,y,z)]-=gravity_m_s2*dt;
        }
    }
    void project(double dt,int iterations=500){
        if(!std::isfinite(dt)||dt<=0||iterations<1||iterations>10000)
            throw std::invalid_argument("Bad MAC projection");
        // Enforce solid boundaries before computing divergence.
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x)
            if(x==0||x==nx_||is_wall((int)x-1,(int)y,(int)z)||
               is_wall((int)x,(int)y,(int)z))u_[fu(x,y,z)]=0;
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(y==0||y==ny_||is_wall((int)x,(int)y-1,(int)z)||
               is_wall((int)x,(int)y,(int)z))v_[fv(x,y,z)]=0;
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(z==0||z==nz_||is_wall((int)x,(int)y,(int)z-1)||
               is_wall((int)x,(int)y,(int)z))w_[fw(x,y,z)]=0;
        std::vector<double> rhs(fluid_.size(),0);
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)if(fluid_[cell(x,y,z)])
            rhs[cell(x,y,z)]=rho_*dx_*dx_/dt*divergence(x,y,z);
        std::fill(pressure_.begin(),pressure_.end(),0);
        constexpr int dir[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for(int iter=0;iter<iterations;++iter){
            double max_delta=0;
            for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
            for(std::size_t x=0;x<nx_;++x)if(fluid_[cell(x,y,z)]){
                double sum=0;int coefficient=0;
                for(const auto& d:dir) {
                    const int xx=(int)x+d[0],yy=(int)y+d[1],zz=(int)z+d[2];
                    if(is_wall(xx,yy,zz))continue;
                    // Air pressure is zero on the liquid/air face, only half
                    // a cell from the liquid center: ghost-fluid factor 2.
                    if(is_liquid(xx,yy,zz)){++coefficient;sum+=p(xx,yy,zz);}
                    else coefficient+=2;
                }
                if(coefficient==0)continue;
                const std::size_t i=cell(x,y,z);
                const double next=(sum-rhs[i])/coefficient;
                max_delta=std::max(max_delta,std::abs(next-pressure_[i]));
                pressure_[i]=next;
            }
            if(max_delta<1e-8)break;
        }
        const double scale=dt/(rho_*dx_);
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=1;x<nx_;++x)
            if(!is_wall((int)x-1,(int)y,(int)z)&&!is_wall((int)x,(int)y,(int)z)){
                const double ghost=is_liquid((int)x,(int)y,(int)z)!=is_liquid((int)x-1,(int)y,(int)z)?2.0:1.0;
                u_[fu(x,y,z)]-=scale*ghost*(p((int)x,(int)y,(int)z)-p((int)x-1,(int)y,(int)z));
            }
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=1;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(!is_wall((int)x,(int)y-1,(int)z)&&!is_wall((int)x,(int)y,(int)z)){
                const double ghost=is_liquid((int)x,(int)y,(int)z)!=is_liquid((int)x,(int)y-1,(int)z)?2.0:1.0;
                v_[fv(x,y,z)]-=scale*ghost*(p((int)x,(int)y,(int)z)-p((int)x,(int)y-1,(int)z));
            }
        for(std::size_t z=1;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(!is_wall((int)x,(int)y,(int)z-1)&&!is_wall((int)x,(int)y,(int)z)){
                const double ghost=is_liquid((int)x,(int)y,(int)z)!=is_liquid((int)x,(int)y,(int)z-1)?2.0:1.0;
                w_[fw(x,y,z)]-=scale*ghost*(p((int)x,(int)y,(int)z)-p((int)x,(int)y,(int)z-1));
            }
    }

    // P2 PARTIAL: independent opt-in preconditioned conjugate-gradient solve
    // of the SAME MAC / ghost-fluid free-surface Poisson stencil as project().
    // Transactional, and the actual wet-cell divergence is the final gate.
    // This is NOT a two-phase stress model or measured CFD validation.
    // A pure-Neumann liquid component is given a mean-zero pressure gauge.
    struct PcgProjectionAudit {
        int iterations=0;
        double max_equation_residual_pa=0;
        double max_divergence_s_inv=0;
        std::size_t liquid_cells=0;
        std::size_t closed_neumann_components=0;
    };
    PcgProjectionAudit project_pcg_verified(
        double dt,int max_iterations=2000,
        double max_divergence_s_inv=1e-7,
        double relative_equation_tolerance=1e-9) {
        if(!std::isfinite(dt)||dt<=0||max_iterations<1||max_iterations>20000||
           !std::isfinite(max_divergence_s_inv)||max_divergence_s_inv<=0||
           !std::isfinite(relative_equation_tolerance)||
           relative_equation_tolerance<=0||relative_equation_tolerance>0.1)
            throw std::invalid_argument("Invalid MAC PCG projection parameters");
        MacProjection3D work=*this;
        // Enforce zero normal velocity at the physical box / solids.
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<=nx_;++x)
            if(x==0||x==nx_||work.is_wall((int)x-1,(int)y,(int)z)||
               work.is_wall((int)x,(int)y,(int)z))
                work.u_[work.fu(x,y,z)]=0;
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<=ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(y==0||y==ny_||work.is_wall((int)x,(int)y-1,(int)z)||
               work.is_wall((int)x,(int)y,(int)z))
                work.v_[work.fv(x,y,z)]=0;
        for(std::size_t z=0;z<=nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(z==0||z==nz_||work.is_wall((int)x,(int)y,(int)z-1)||
               work.is_wall((int)x,(int)y,(int)z))
                work.w_[work.fw(x,y,z)]=0;

        const auto n=work.fluid_.size();
        std::vector<double> diagonal(n,0),b(n,0),pressure(n,0);
        std::vector<int> component(n,-1);
        PcgProjectionAudit audit;
        constexpr int directions[6][3]={
            {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        const auto air_weight=[&](std::size_t x,std::size_t y,std::size_t z,
                                  const int* d)->double {
            if(d[0])return (work.interface_x_.empty()?2.0:1.0/work.interface_x_[work.fu(x+(d[0]>0),y,z)]);
            if(d[1])return (work.interface_y_.empty()?2.0:1.0/work.interface_y_[work.fv(x,y+(d[1]>0),z)]);
            return (work.interface_z_.empty()?2.0:1.0/work.interface_z_[work.fw(x,y,z+(d[2]>0))]);
        };
        const auto air_pressure=[&](std::size_t x,std::size_t y,std::size_t z,
                                    const int* d)->double {
            if(d[0])return work.interface_pressure_x_.empty()?work.uniform_interface_pressure_pa_:
                work.interface_pressure_x_[work.fu(x+(d[0]>0),y,z)];
            if(d[1])return work.interface_pressure_y_.empty()?work.uniform_interface_pressure_pa_:
                work.interface_pressure_y_[work.fv(x,y+(d[1]>0),z)];
            return work.interface_pressure_z_.empty()?work.uniform_interface_pressure_pa_:
                work.interface_pressure_z_[work.fw(x,y,z+(d[2]>0))];
        };
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x) {
            const auto id=work.cell(x,y,z);
            if(!work.fluid_[id])continue;
            ++audit.liquid_cells;
            b[id]=-rho_*dx_*dx_/dt*work.divergence(x,y,z);
            for(const auto& d:directions) {
                const int xx=(int)x+d[0], yy=(int)y+d[1], zz=(int)z+d[2];
                if(work.is_wall(xx,yy,zz))continue;
                if(work.is_liquid(xx,yy,zz))diagonal[id]+=1.0;
                else {
                    const double weight=air_weight(x,y,z,d);
                    diagonal[id]+=weight;
                    // Inhomogeneous Dirichlet pressure on the actual
                    // fractional interface; zero is the legacy default.
                    b[id]+=weight*air_pressure(x,y,z,d);
                }
            }
        }
        // Partition the liquid graph; check each isolated Neumann
        // cavity is algebraically solvable before running the Krylov solver.
        std::vector<std::vector<std::size_t>> neumann_groups;
        std::vector<std::size_t> queue;
        queue.reserve(audit.liquid_cells);
        int label=0;
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x) {
            const auto seed=work.cell(x,y,z);
            if(!work.fluid_[seed]||component[seed]>=0)continue;
            queue.clear();
            queue.push_back(seed);
            component[seed]=label;
            bool has_air=false;
            long double source_sum=0,abs_sum=0;
            for(std::size_t qi=0;qi<queue.size();++qi) {
                const auto at=queue[qi];
                source_sum+=b[at];abs_sum+=std::abs(b[at]);
                const auto zz=at/(nx_*ny_);
                const auto yy=(at/nx_)%ny_;
                const auto xx=at%nx_;
                for(const auto& d:directions) {
                    const int tx=(int)xx+d[0],ty=(int)yy+d[1],tz=(int)zz+d[2];
                    if(work.is_wall(tx,ty,tz))continue;
                    if(!work.is_liquid(tx,ty,tz)){has_air=true;continue;}
                    const auto next=work.cell((std::size_t)tx,(std::size_t)ty,
                                              (std::size_t)tz);
                    if(component[next]<0) {
                        component[next]=label;
                        queue.push_back(next);
                    }
                }
            }
            if(!has_air) {
                // Closed incompressible cavity: sum(divergence)=0, not
                // merely a potentially arbitrary pressure normalization.
                if(std::abs(source_sum)>1e-9L*std::max(1.0L,abs_sum))
                    throw std::runtime_error("Unsatisfiable closed MAC Neumann source");
                neumann_groups.push_back(queue);
                ++audit.closed_neumann_components;
            }
            ++label;
        }
        const auto orthogonalize=[&](std::vector<double>& values) {
            for(const auto& group:neumann_groups) {
                long double sum=0;
                for(auto cellid:group)sum+=values[cellid];
                const double mean=(double)(sum/group.size());
                for(auto cellid:group)values[cellid]-=mean;
            }
        };
        const auto apply_A=[&](const std::vector<double>& values,
                               std::vector<double>& output) {
            std::fill(output.begin(),output.end(),0.0);
            for(std::size_t z=0;z<nz_;++z)
            for(std::size_t y=0;y<ny_;++y)
            for(std::size_t x=0;x<nx_;++x) {
                const auto id=work.cell(x,y,z);
                if(!work.fluid_[id])continue;
                double a=diagonal[id]*values[id];
                for(const auto& d:directions) {
                    const int xx=(int)x+d[0],yy=(int)y+d[1],zz=(int)z+d[2];
                    if(work.is_liquid(xx,yy,zz))
                        a-=values[work.cell((std::size_t)xx,
                                            (std::size_t)yy,(std::size_t)zz)];
                }
                output[id]=a;
            }
        };
        double norm_b=0;
        for(std::size_t i=0;i<n;++i)
            if(work.fluid_[i])norm_b=std::max(norm_b,std::abs(b[i]));
        const double allowed_residual=std::max(
            1e-10,norm_b*relative_equation_tolerance);
        std::vector<double> residual=b,z(n,0),direction(n,0),Ap(n,0);
        orthogonalize(residual);
        auto max_residual=[&]() {
            double maximum=0;
            for(std::size_t i=0;i<n;++i)
                if(work.fluid_[i])maximum=std::max(maximum,std::abs(residual[i]));
            return maximum;
        };
        double rz=0;
        for(std::size_t i=0;i<n;++i)if(work.fluid_[i]) {
            if(diagonal[i]<=0) {
                // A sealed single-cell cavity has no pressure gradient
                // degree of freedom. Reject an inconsistent source.
                if(std::abs(residual[i])>allowed_residual)
                    throw std::runtime_error("Inconsistent isolated MAC cell source");
                continue;
            }
            z[i]=residual[i]/diagonal[i];
            direction[i]=z[i];
            rz+=residual[i]*z[i];
        }
        orthogonalize(direction);
        auto residual_max=max_residual();
        for(int iter=0;iter<max_iterations&&residual_max>allowed_residual;++iter) {
            apply_A(direction,Ap);
            double denom=0;
            for(std::size_t i=0;i<n;++i)
                if(work.fluid_[i])denom+=direction[i]*Ap[i];
            if(!std::isfinite(denom)||!std::isfinite(rz)||denom<=0||rz<=0)
                throw std::runtime_error("MAC PCG nonpositive curvature / breakdown");
            const double alpha=rz/denom;
            if(!std::isfinite(alpha))
                throw std::runtime_error("Nonfinite MAC PCG step");
            for(std::size_t i=0;i<n;++i)if(work.fluid_[i]) {
                pressure[i]+=alpha*direction[i];
                residual[i]-=alpha*Ap[i];
            }
            orthogonalize(pressure);
            orthogonalize(residual);
            residual_max=max_residual();
            audit.iterations=iter+1;
            if(residual_max<=allowed_residual)break;
            double next_rz=0;
            for(std::size_t i=0;i<n;++i)if(work.fluid_[i]) {
                if(diagonal[i]>0) {
                    z[i]=residual[i]/diagonal[i];
                    next_rz+=residual[i]*z[i];
                }
            }
            if(!std::isfinite(next_rz)||next_rz<0)
                throw std::runtime_error("Invalid MAC PCG preconditioned norm");
            const double beta=next_rz/rz;
            for(std::size_t i=0;i<n;++i)if(work.fluid_[i])
                direction[i]=z[i]+beta*direction[i];
            orthogonalize(direction);
            rz=next_rz;
        }
        // Validate actual matrix residual; recursively-updated CG
        // residuals alone may drift due to finite precision.
        apply_A(pressure,Ap);
        audit.max_equation_residual_pa=0;
        for(std::size_t i=0;i<n;++i)if(work.fluid_[i]) {
            audit.max_equation_residual_pa=std::max(
                audit.max_equation_residual_pa,std::abs(Ap[i]-b[i]));
        }
        if(!std::isfinite(audit.max_equation_residual_pa)||
           audit.max_equation_residual_pa>allowed_residual)
            throw std::runtime_error("MAC PCG equation residual did not converge");
        work.pressure_=std::move(pressure);
        const double scale=dt/(rho_*dx_);
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=1;x<nx_;++x)
            if(!work.is_wall((int)x-1,(int)y,(int)z)&&
               !work.is_wall((int)x,(int)y,(int)z)) {
                const double ghost=work.is_liquid((int)x,(int)y,(int)z)!=
                                   work.is_liquid((int)x-1,(int)y,(int)z)?
                                   (work.interface_x_.empty()?2.0:1.0/work.interface_x_[work.fu(x,y,z)]):1.0;
                const double pb=work.interface_pressure_x_.empty()?
                    work.uniform_interface_pressure_pa_:work.interface_pressure_x_[work.fu(x,y,z)];
                const double pr=work.is_liquid((int)x,(int)y,(int)z)?
                    work.p((int)x,(int)y,(int)z):pb;
                const double pl=work.is_liquid((int)x-1,(int)y,(int)z)?
                    work.p((int)x-1,(int)y,(int)z):pb;
                work.u_[work.fu(x,y,z)]-=scale*ghost*(pr-pl);
            }
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=1;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(!work.is_wall((int)x,(int)y-1,(int)z)&&
               !work.is_wall((int)x,(int)y,(int)z)) {
                const double ghost=work.is_liquid((int)x,(int)y,(int)z)!=
                                   work.is_liquid((int)x,(int)y-1,(int)z)?
                                   (work.interface_y_.empty()?2.0:1.0/work.interface_y_[work.fv(x,y,z)]):1.0;
                const double pb=work.interface_pressure_y_.empty()?
                    work.uniform_interface_pressure_pa_:work.interface_pressure_y_[work.fv(x,y,z)];
                const double pr=work.is_liquid((int)x,(int)y,(int)z)?
                    work.p((int)x,(int)y,(int)z):pb;
                const double pl=work.is_liquid((int)x,(int)y-1,(int)z)?
                    work.p((int)x,(int)y-1,(int)z):pb;
                work.v_[work.fv(x,y,z)]-=scale*ghost*(pr-pl);
            }
        for(std::size_t z=1;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(!work.is_wall((int)x,(int)y,(int)z-1)&&
               !work.is_wall((int)x,(int)y,(int)z)) {
                const double ghost=work.is_liquid((int)x,(int)y,(int)z)!=
                                   work.is_liquid((int)x,(int)y,(int)z-1)?
                                   (work.interface_z_.empty()?2.0:1.0/work.interface_z_[work.fw(x,y,z)]):1.0;
                const double pb=work.interface_pressure_z_.empty()?
                    work.uniform_interface_pressure_pa_:work.interface_pressure_z_[work.fw(x,y,z)];
                const double pr=work.is_liquid((int)x,(int)y,(int)z)?
                    work.p((int)x,(int)y,(int)z):pb;
                const double pl=work.is_liquid((int)x,(int)y,(int)z-1)?
                    work.p((int)x,(int)y,(int)z-1):pb;
                work.w_[work.fw(x,y,z)]-=scale*ghost*(pr-pl);
            }
        // Never allow a NaN/Inf face to bypass max_divergence() via
        // std::max's unordered floating-point comparison semantics.
        const auto finite=[](const std::vector<double>& a){
            return std::all_of(a.begin(),a.end(),
                               [](double value){return std::isfinite(value);});
        };
        if(!finite(work.pressure_)||!finite(work.u_)||
           !finite(work.v_)||!finite(work.w_))
            throw std::runtime_error("Nonfinite MAC PCG pressure or velocity");
        audit.max_divergence_s_inv=0;
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(work.fluid_[work.cell(x,y,z)]){
                const double div=work.divergence(x,y,z);
                if(!std::isfinite(div))
                    throw std::runtime_error("Nonfinite MAC PCG divergence");
                audit.max_divergence_s_inv=std::max(
                    audit.max_divergence_s_inv,std::abs(div));
            }
        if(!std::isfinite(audit.max_divergence_s_inv)||
           audit.max_divergence_s_inv>max_divergence_s_inv)
            throw std::runtime_error("MAC PCG projected divergence exceeds tolerance");
        *this=std::move(work);
        return audit;
    }

    // Opt-in P2 safeguard: never publish an under-converged pressure solve.
    // The legacy project() remains unchanged for existing prototype callers.
    // This checks the ACTUAL wet-cell divergence, not the Gauss-Seidel
    // pressure-delta stopping condition. Full MAC state is transactional.
    double project_verified(double dt,int iterations=500,
                            double max_divergence_s_inv=1e-8){
        if(!std::isfinite(max_divergence_s_inv)||max_divergence_s_inv<=0)
            throw std::invalid_argument("Invalid MAC divergence tolerance");
        MacProjection3D candidate=*this;
        candidate.project(dt,iterations);
        const auto finite=[](const std::vector<double>& values){
            return std::all_of(values.begin(),values.end(),
                               [](double v){return std::isfinite(v);});
        };
        if(!finite(candidate.pressure_)||!finite(candidate.u_)||
           !finite(candidate.v_)||!finite(candidate.w_))
            throw std::runtime_error("Nonfinite MAC pressure/velocity after projection");
        double maximum=0;
        for(std::size_t z=0;z<nz_;++z)
        for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(candidate.fluid_[candidate.cell(x,y,z)]){
                const double d=candidate.divergence(x,y,z);
                if(!std::isfinite(d))
                    throw std::runtime_error("Nonfinite MAC divergence");
                maximum=std::max(maximum,std::abs(d));
            }
        if(maximum>max_divergence_s_inv)
            throw std::runtime_error("MAC projection divergence tolerance not met");
        *this=std::move(candidate);
        return maximum;
    }
};
} // namespace upc
