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
#include <vector>

namespace upc {
class MacProjection3D {
    std::size_t nx_,ny_,nz_;
    double dx_,rho_;
    std::vector<unsigned char> fluid_,solid_;
    std::vector<double> pressure_,u_,v_,w_;
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
    }
    void set_cell(std::size_t x,std::size_t y,std::size_t z,bool fluid,bool solid=false){
        if(x>=nx_||y>=ny_||z>=nz_|| (fluid&&solid))throw std::invalid_argument("Bad MAC cell");
        const auto i=cell(x,y,z);fluid_[i]=fluid;solid_[i]=solid;
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
                    ++coefficient;
                    if(is_liquid(xx,yy,zz))sum+=p(xx,yy,zz);
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
                u_[fu(x,y,z)]-=scale*(p((int)x,(int)y,(int)z)-p((int)x-1,(int)y,(int)z));
            }
        for(std::size_t z=0;z<nz_;++z)for(std::size_t y=1;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(!is_wall((int)x,(int)y-1,(int)z)&&!is_wall((int)x,(int)y,(int)z)){
                v_[fv(x,y,z)]-=scale*(p((int)x,(int)y,(int)z)-p((int)x,(int)y-1,(int)z));
            }
        for(std::size_t z=1;z<nz_;++z)for(std::size_t y=0;y<ny_;++y)
        for(std::size_t x=0;x<nx_;++x)
            if(!is_wall((int)x,(int)y,(int)z-1)&&!is_wall((int)x,(int)y,(int)z)){
                w_[fw(x,y,z)]-=scale*(p((int)x,(int)y,(int)z)-p((int)x,(int)y,(int)z-1));
            }
    }
};
} // namespace upc
