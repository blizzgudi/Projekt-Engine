// P2 PARTIAL: simultaneous SPD global 3D water/air tangential shear.
#include "upc/p2_two_phase_global_shear3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
int assertions=0;
void check(bool ok,const char* message) {
    ++assertions;if(!ok)throw std::runtime_error(message);
}
void near(double a,double b,double tol,const char* message) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tol,message);
}
template<class F>void rejects(F f) {
    bool threw=false;try{f();}catch(const std::invalid_argument&){threw=true;}
    check(threw,"Invalid global shear input accepted");
}
using upc::P2TwoPhaseGridShear3D;
using upc::p2_two_phase_global_shear_step3d;
P2TwoPhaseGridShear3D planar() {
    P2TwoPhaseGridShear3D g;
    g.nx=4;g.ny=4;g.nz=4;g.dx_m=.1;g.dt_s=.05;
    g.water_density_kg_m3=1000;g.air_density_kg_m3=1.2;
    g.water_viscosity_pa_s=1;g.air_viscosity_pa_s=.1;
    for(int z=0;z<4;++z)for(int y=0;y<4;++y)for(int x=0;x<4;++x) {
        const bool wet=x<2;
        g.phase.push_back(wet?1:0);
        g.velocity_m_s.push_back({0,wet?0.0:1.0,0});
    }
    return g;
}
void planar_reference() {
    auto g=planar();
    const auto l=p2_two_phase_global_shear_step3d(g);
    const double mw=1000*.001,ma=1.2*.001;
    const double b=.05*.01/(.05/1+.05/.1);
    const double slip=1/(1+b*(1/mw+1/ma));
    const double impulse=b*slip;
    check(l.interface_faces==16&&l.axis_faces[0]==16&&
          l.axis_faces[1]==0&&l.axis_faces[2]==0,"Planar interfaces");
    near(l.water_mass_kg,32,1e-12,"Water mass");
    near(l.air_mass_kg,32*ma,1e-12,"Air mass");
    for(int z=0;z<4;++z)for(int y=0;y<4;++y) {
        const std::size_t wi=(std::size_t(z)*4+y)*4+1,ai=wi+1;
        near(g.velocity_m_s[wi][1],impulse/mw,1e-13,"Exact Couette water");
        near(g.velocity_m_s[ai][1],1-impulse/ma,1e-13,"Exact Couette air");
        near(g.velocity_m_s[wi][0],0,1e-14,"Normal water velocity");
        near(g.velocity_m_s[ai][0],0,1e-14,"Normal air velocity");
    }
    for(double r:l.momentum_residual_n_s)near(r,0,1e-12,"Planar momentum");
    near(l.energy_residual_j,0,1e-12,"Planar energy");
    check(l.kinetic_after_j<l.kinetic_before_j,"Planar energy not dissipated");
    check(l.pcg_iterations[1]>0,"PCG did not solve tangential field");
    std::cout<<std::setprecision(17)<<"GLOBAL_PLANAR faces="<<l.interface_faces
             <<" slip="<<g.velocity_m_s[2][1]-g.velocity_m_s[1][1]
             <<" water_y="<<g.velocity_m_s[1][1]
             <<" air_y="<<g.velocity_m_s[2][1]
             <<" energy_residual="<<l.energy_residual_j<<"\n";
}
P2TwoPhaseGridShear3D checkerboard() {
    P2TwoPhaseGridShear3D g;
    g.nx=3;g.ny=3;g.nz=3;g.dx_m=.08;g.dt_s=.03;
    g.water_density_kg_m3=1000;g.air_density_kg_m3=1.2;
    g.water_viscosity_pa_s=1.3;g.air_viscosity_pa_s=.18;
    for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x) {
        g.phase.push_back((x+y+z)%2==0?1:0);
        g.velocity_m_s.push_back({.1*x+.03*y,-.2*y+.04*z,.12*z+.02*x});
    }
    return g;
}
void three_dimensional_reference() {
    auto g=checkerboard();
    const auto original=g;
    const auto l=p2_two_phase_global_shear_step3d(g);
    check(l.interface_faces==54&&l.axis_faces[0]==18&&
          l.axis_faces[1]==18&&l.axis_faces[2]==18,"3D interface topology");
    near(l.water_mass_kg,14*1000*.08*.08*.08,1e-12,"3D water mass");
    near(l.air_mass_kg,13*1.2*.08*.08*.08,1e-12,"3D air mass");
    for(double r:l.momentum_residual_n_s)near(r,0,1e-12,"3D momentum");
    near(l.energy_residual_j,0,1e-12,"3D energy");
    check(l.kinetic_after_j<l.kinetic_before_j,"3D shear did not dissipate");
    for(double r:l.pcg_residual_kg_m_s)check(r<1e-12,"3D PCG residual");
    for(int i:l.pcg_iterations)check(i>0,"3D PCG iterations absent");
    auto shifted=original;
    for(auto& v:shifted.velocity_m_s) {
        v[0]+=.3;v[1]-=.2;v[2]+=.1;
    }
    const auto ls=p2_two_phase_global_shear_step3d(shifted);
    for(std::size_t i=0;i<g.velocity_m_s.size();++i)
        for(int k=0;k<3;++k) {
            const double shift=k==0?.3:(k==1?-.2:.1);
            near(shifted.velocity_m_s[i][k]-shift,g.velocity_m_s[i][k],
                 2e-12,"Galilean invariance");
        }
    for(double r:ls.momentum_residual_n_s)near(r,0,1e-12,"Shifted momentum");
    near(ls.energy_residual_j,0,1e-12,"Shifted energy");
    auto again=original;
    p2_two_phase_global_shear_step3d(again);
    for(std::size_t i=0;i<g.velocity_m_s.size();++i)
        for(int k=0;k<3;++k)
            near(again.velocity_m_s[i][k],g.velocity_m_s[i][k],0,"Deterministic global PCG");
    const auto v0=g.velocity_m_s[0],v13=g.velocity_m_s[13],v26=g.velocity_m_s[26];
    double maxmom=0,maxpcg=0;
    for(double r:l.momentum_residual_n_s)maxmom=std::max(maxmom,std::abs(r));
    for(double r:l.pcg_residual_kg_m_s)maxpcg=std::max(maxpcg,std::abs(r));
    std::cout<<std::setprecision(17)<<"GLOBAL_3D faces="<<l.interface_faces
             <<" axes="<<l.axis_faces[0]<<","<<l.axis_faces[1]<<","<<l.axis_faces[2]
             <<" v0="<<v0[0]<<","<<v0[1]<<","<<v0[2]
             <<" v13="<<v13[0]<<","<<v13[1]<<","<<v13[2]
             <<" v26="<<v26[0]<<","<<v26[1]<<","<<v26[2]
             <<" momentum_residual="<<maxmom
             <<" energy_residual="<<l.energy_residual_j
             <<" pcg_max="<<maxpcg<<"\n";
}
void surface_source() {
    auto g=planar();g.marangoni_gradient_pa={0,2,0};
    const auto l=p2_two_phase_global_shear_step3d(g);
    near(l.surface_impulse_n_s[1],16*.05*.01*2,1e-14,"Surface impulse");
    for(double r:l.momentum_residual_n_s)near(r,0,1e-12,"Source momentum");
    near(l.energy_residual_j,0,1e-12,"Source energy");
    check(l.surface_work_j>0,"Surface source work");
}

using upc::P2GlobalShearCellProperties3D;
// Independent dense Gaussian elimination: no PCG or solver matrix routines.
std::vector<double> dense_solve(std::vector<std::vector<double>> a,
                                std::vector<double> rhs) {
    const int n=static_cast<int>(rhs.size());
    for(int k=0;k<n;++k) {
        int pivot=k;
        for(int i=k+1;i<n;++i)
            if(std::abs(a[i][k])>std::abs(a[pivot][k]))pivot=i;
        check(std::abs(a[pivot][k])>1e-16,"Dense reference singular");
        std::swap(a[k],a[pivot]);std::swap(rhs[k],rhs[pivot]);
        for(int i=k+1;i<n;++i) {
            const double f=a[i][k]/a[k][k];
            for(int j=k+1;j<n;++j)a[i][j]-=f*a[k][j];
            rhs[i]-=f*rhs[k];
        }
    }
    std::vector<double> x(n);
    for(int i=n-1;i>=0;--i) {
        double r=rhs[i];
        for(int j=i+1;j<n;++j)r-=a[i][j]*x[j];
        x[i]=r/a[i][i];
    }
    return x;
}
void heterogeneous_dense_reference() {
    auto g=checkerboard();
    const auto original=g;
    P2GlobalShearCellProperties3D prop;
    for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x) {
        const bool wet=(x+y+z)%2==0;
        prop.density_kg_m3.push_back(wet?850+10*x+5*y+2*z:
                                             1+.1*x+.02*y+.01*z);
        prop.dynamic_viscosity_pa_s.push_back(wet?.5+.1*x+.05*y+.02*z:
                                                       .06+.01*x+.005*y+.002*z);
    }
    const auto l=p2_two_phase_global_shear_step3d(g,prop);
    check(l.interface_faces==54,"Heterogeneous interface count");
    const int n=27;
    const double volume=std::pow(original.dx_m,3);
    const double area=std::pow(original.dx_m,2);
    double water_mass=0,air_mass=0;
    for(int i=0;i<n;++i) {
        if(original.phase[i])water_mass+=volume*prop.density_kg_m3[i];
        else air_mass+=volume*prop.density_kg_m3[i];
    }
    near(l.water_mass_kg,water_mass,1e-12,"Heterogeneous water mass");
    near(l.air_mass_kg,air_mass,1e-12,"Heterogeneous air mass");
    double max_difference=0;
    for(int k=0;k<3;++k) {
        std::vector<std::vector<double>> a(n,std::vector<double>(n,0));
        std::vector<double> rhs(n);
        for(int i=0;i<n;++i) {
            const double m=volume*prop.density_kg_m3[i];
            a[i][i]=m;
            rhs[i]=m*original.velocity_m_s[i][k];
        }
        for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x) {
            const int i=(z*3+y)*3+x;
            for(int axis=0;axis<3;++axis) {
                if(axis==k)continue;
                const int xx=x+(axis==0),yy=y+(axis==1),zz=z+(axis==2);
                if(xx>=3||yy>=3||zz>=3)continue;
                const int j=(zz*3+yy)*3+xx;
                if(original.phase[i]==original.phase[j])continue;
                const double resistance=.5*original.dx_m/
                     prop.dynamic_viscosity_pa_s[i]+.5*original.dx_m/
                     prop.dynamic_viscosity_pa_s[j];
                const double b=original.dt_s*area/resistance;
                a[i][i]+=b;a[j][j]+=b;
                a[i][j]-=b;a[j][i]-=b;
            }
        }
        const auto expected=dense_solve(a,rhs);
        for(int i=0;i<n;++i) {
            const double error=std::abs(g.velocity_m_s[i][k]-expected[i]);
            max_difference=std::max(max_difference,error);
            near(g.velocity_m_s[i][k],expected[i],2e-12,
                 "Heterogeneous 27-cell dense Gaussian reference");
        }
        near(l.momentum_residual_n_s[k],0,2e-12,
             "Heterogeneous global momentum");
        check(l.pcg_residual_kg_m_s[k]<1e-11,
              "Heterogeneous independent PCG residual");
    }
    near(l.energy_residual_j,0,2e-12,"Heterogeneous global energy");
    check(l.viscous_dissipation_j>0&&l.implicit_dissipation_j>0,
          "Heterogeneous dissipation not positive");
    // Zero-gradient fixed-property overload must preserve legacy behavior.
    auto legacy=original,empty=original;
    const auto ll=p2_two_phase_global_shear_step3d(legacy);
    const auto le=p2_two_phase_global_shear_step3d(empty,{});
    check(legacy.velocity_m_s==empty.velocity_m_s,
          "Default property path changed");
    near(ll.energy_residual_j,le.energy_residual_j,0,
         "Default energy changed");
    // Rejection must not partially modify any cell velocity.
    auto bad=original;
    auto p=prop;
    p.density_kg_m3.pop_back();
    rejects([&]{p2_two_phase_global_shear_step3d(bad,p);});
    check(bad.velocity_m_s==original.velocity_m_s,
          "Size failure changed state");
    p=prop;p.dynamic_viscosity_pa_s[10]=
        std::numeric_limits<double>::quiet_NaN();
    rejects([&]{p2_two_phase_global_shear_step3d(bad,p);});
    check(bad.velocity_m_s==original.velocity_m_s,
          "NaN failure changed state");
    p=prop;p.density_kg_m3[8]=-1;
    rejects([&]{p2_two_phase_global_shear_step3d(bad,p);});
    p=prop;p.dynamic_viscosity_pa_s[0]=1e-320;
    rejects([&]{p2_two_phase_global_shear_step3d(bad,p);});
    check(bad.velocity_m_s==original.velocity_m_s,
          "Bad viscosity failure changed state");
    std::cout<<std::setprecision(17)<<"HETEROGENEOUS_3D faces="<<l.interface_faces
             <<" dense_max_error="<<max_difference
             <<" v0="<<g.velocity_m_s[0][0]<<","<<g.velocity_m_s[0][1]<<","<<g.velocity_m_s[0][2]
             <<" v13="<<g.velocity_m_s[13][0]<<","<<g.velocity_m_s[13][1]<<","<<g.velocity_m_s[13][2]
             <<" v26="<<g.velocity_m_s[26][0]<<","<<g.velocity_m_s[26][1]<<","<<g.velocity_m_s[26][2]
             <<" energy_residual="<<l.energy_residual_j<<"\n";
}


void bulk_tangential_dense_reference() {
    // Three-dimensional stratified water/air field with 9 interfacial
    // and 45 same-phase neighbor faces. Independent dense FV matrix.
    P2TwoPhaseGridShear3D g;
    g.nx=3;g.ny=3;g.nz=3;g.dx_m=.08;g.dt_s=.03;
    g.water_density_kg_m3=1000;g.air_density_kg_m3=1.2;
    g.water_viscosity_pa_s=1.3;g.air_viscosity_pa_s=.18;
    g.marangoni_gradient_pa={0,.4,0};
    P2GlobalShearCellProperties3D prop;
    for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x) {
        const bool wet=x<2;
        g.phase.push_back(wet?1:0);
        g.velocity_m_s.push_back({.07*x+.03*y,-.11*y+.04*z,.09*z-.02*x});
        prop.density_kg_m3.push_back(wet?880+7*x+3*y+z:1+.1*y+.02*z);
        prop.dynamic_viscosity_pa_s.push_back(
            wet?.4+.03*x+.01*y+.005*z:.05+.005*y+.002*z);
    }
    const auto original=g;
    const auto l=p2_two_phase_global_shear_step3d(g,prop,true);
    check(l.interface_faces==9&&l.bulk_faces==45&&
          l.axis_faces[0]==9&&l.axis_faces[1]==0&&l.axis_faces[2]==0,
          "Bulk and interfacial 3D topology");
    near(l.surface_impulse_n_s[1],9*.03*.08*.08*.4,1e-14,
         "Marangoni source counted only at interface");
    near(l.surface_impulse_n_s[0],0,1e-14,"Normal surface impulse");
    check(l.bulk_viscous_dissipation_j>0&&
          l.bulk_viscous_dissipation_j<l.viscous_dissipation_j,
          "Bulk dissipation not included in total ledger");
    for(double v:l.momentum_residual_n_s)near(v,0,2e-12,"Bulk momentum");
    near(l.energy_residual_j,0,2e-12,"Bulk energy");
    const double vol=std::pow(g.dx_m,3),area=std::pow(g.dx_m,2);
    double max_error=0;
    for(int k=0;k<3;++k) {
        std::vector<std::vector<double>> a(27,std::vector<double>(27,0));
        std::vector<double> rhs(27);
        for(int i=0;i<27;++i) {
            const double mass=prop.density_kg_m3[i]*vol;
            a[i][i]=mass;rhs[i]=mass*original.velocity_m_s[i][k];
        }
        for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x) {
            const int i=(z*3+y)*3+x;
            for(int axis=0;axis<3;++axis) {
                if(axis==k)continue;
                const int xx=x+(axis==0),yy=y+(axis==1),zz=z+(axis==2);
                if(xx>=3||yy>=3||zz>=3)continue;
                const int j=(zz*3+yy)*3+xx;
                const double ri=.5*g.dx_m/prop.dynamic_viscosity_pa_s[i];
                const double rj=.5*g.dx_m/prop.dynamic_viscosity_pa_s[j];
                const double b=g.dt_s*area/(ri+rj);
                a[i][i]+=b;a[j][j]+=b;
                a[i][j]-=b;a[j][i]-=b;
                if(original.phase[i]!=original.phase[j]) {
                    const int wi=original.phase[i]?i:j;
                    const int ai=original.phase[i]?j:i;
                    const double rw=.5*g.dx_m/prop.dynamic_viscosity_pa_s[wi];
                    const double ra=.5*g.dx_m/prop.dynamic_viscosity_pa_s[ai];
                    const double source=g.dt_s*area*g.marangoni_gradient_pa[k];
                    rhs[wi]+=source*ra/(rw+ra);
                    rhs[ai]+=source*rw/(rw+ra);
                }
            }
        }
        const auto expected=dense_solve(a,rhs);
        for(int i=0;i<27;++i) {
            max_error=std::max(max_error,
                               std::abs(g.velocity_m_s[i][k]-expected[i]));
            near(g.velocity_m_s[i][k],expected[i],2e-12,
                 "Bulk 27x27 independent dense reference");
        }
        check(l.pcg_residual_kg_m_s[k]<1e-11,
              "Bulk independent final PCG residual");
    }
    // Default path remains interface-only and changes no same-phase face.
    auto old=original;
    const auto lo=p2_two_phase_global_shear_step3d(old,prop);
    check(lo.bulk_faces==0&&lo.interface_faces==9,
          "Opt-in bulk path changed default topology");
    check(std::abs(old.velocity_m_s[0][0]-g.velocity_m_s[0][0])>1e-6,
          "Bulk opt-in did not change tangential intra-phase motion");
    // With no interface, bulk diffusion still conserves physical momentum.
    auto one=original;
    std::fill(one.phase.begin(),one.phase.end(),1);
    one.marangoni_gradient_pa={0,0,0};
    const auto before=one.velocity_m_s;
    const auto lone=p2_two_phase_global_shear_step3d(one,prop,true);
    check(lone.interface_faces==0&&lone.bulk_faces==54&&
          one.velocity_m_s!=before,"Single-phase bulk diffusion missing");
    for(double v:lone.momentum_residual_n_s)
        near(v,0,2e-12,"Single-phase bulk momentum");
    near(lone.energy_residual_j,0,2e-12,"Single-phase bulk energy");
    // Transactional validation.
    auto bad=original;
    auto invalid=prop;
    invalid.density_kg_m3[4]=-1;
    rejects([&]{p2_two_phase_global_shear_step3d(bad,invalid,true);});
    check(bad.velocity_m_s==original.velocity_m_s,
          "Invalid bulk density mutated state");
    double max_momentum=0;
    for(double v:l.momentum_residual_n_s)
        max_momentum=std::max(max_momentum,std::abs(v));
    std::cout<<std::setprecision(17)
             <<"BULK_3D interfaces="<<l.interface_faces
             <<" bulk="<<l.bulk_faces
             <<" dense_max_error="<<max_error
             <<" v0="<<g.velocity_m_s[0][0]<<","<<g.velocity_m_s[0][1]<<","<<g.velocity_m_s[0][2]
             <<" v13="<<g.velocity_m_s[13][0]<<","<<g.velocity_m_s[13][1]<<","<<g.velocity_m_s[13][2]
             <<" v26="<<g.velocity_m_s[26][0]<<","<<g.velocity_m_s[26][1]<<","<<g.velocity_m_s[26][2]
             <<" bulk_dissipation="<<l.bulk_viscous_dissipation_j
             <<" momentum_residual="<<max_momentum
             <<" energy_residual="<<l.energy_residual_j<<"\n";
}

void no_interface_and_invalid() {
    auto g=planar();
    std::fill(g.phase.begin(),g.phase.end(),1);
    const auto before=g.velocity_m_s;
    const auto l=p2_two_phase_global_shear_step3d(g);
    check(l.interface_faces==0&&g.velocity_m_s==before,"No interface changes state");
    g=planar();g.marangoni_gradient_pa={1,0,0};
    const auto prior=g.velocity_m_s;
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    check(g.velocity_m_s==prior,"Normal surface source changed state");
    g=planar();g.phase[0]=2;
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.velocity_m_s[0][0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.dx_m=0;
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.air_viscosity_pa_s=std::numeric_limits<double>::infinity();
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.velocity_m_s.pop_back();
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
    g=planar();g.nx=129;
    rejects([&]{p2_two_phase_global_shear_step3d(g);});
}
}
int main() {
    try {
        planar_reference();three_dimensional_reference();surface_source();
        heterogeneous_dense_reference();bulk_tangential_dense_reference();
        no_interface_and_invalid();
        std::cout<<"PASS: GLOBAL_TWO_PHASE_SHEAR assertions="<<assertions<<"\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
