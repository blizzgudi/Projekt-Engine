#pragma once
// P2 PARTIAL: implicit conservative tangential traction exchange for ONE
// prescribed planar water/air interface patch (two finite-volume cells).
// No live MAC/VOF coupling, interface advection, pressure or moving solid.
#include <algorithm>
#include <array>
#include <initializer_list>
#include <cmath>
#include <stdexcept>

namespace upc {
using P2ShearVec3 = std::array<double,3>;
inline double p2_shear_dot(const P2ShearVec3& a,const P2ShearVec3& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
struct P2TwoPhaseShearPatch3D {
    P2ShearVec3 normal{}, water_velocity_m_s{}, air_velocity_m_s{};
    P2ShearVec3 marangoni_gradient_pa{}; // tangent: surface tension gradient N/m^2 = Pa
    double water_mass_kg=0,air_mass_kg=0;
    double water_viscosity_pa_s=0,air_viscosity_pa_s=0;
    double water_center_distance_m=0,air_center_distance_m=0;
    double interface_area_m2=0,dt_s=0;
};
struct P2TwoPhaseShearExchange3D {
    P2ShearVec3 water_velocity_m_s{},air_velocity_m_s{};
    P2ShearVec3 water_impulse_n_s{},air_impulse_n_s{},surface_impulse_n_s{};
    P2ShearVec3 water_traction_pa{},air_traction_pa{};
    double kinetic_before_j=0,kinetic_after_j=0;
    double viscous_dissipation_j=0,implicit_dissipation_j=0;
    double surface_work_j=0,energy_residual_j=0;
};
inline P2TwoPhaseShearExchange3D p2_two_phase_shear_exchange3d(
    const P2TwoPhaseShearPatch3D& p) {
    const double positive[]={
        p.water_mass_kg,p.air_mass_kg,p.water_viscosity_pa_s,
        p.air_viscosity_pa_s,p.water_center_distance_m,
        p.air_center_distance_m,p.interface_area_m2,p.dt_s
    };
    for(double v:positive)if(!(v>0)||!std::isfinite(v))
        throw std::invalid_argument("Invalid two-phase patch mass/viscosity/geometry/dt");
    for(const auto& vec:{p.normal,p.water_velocity_m_s,p.air_velocity_m_s,
                          p.marangoni_gradient_pa})
        for(double v:vec)if(!std::isfinite(v))
            throw std::invalid_argument("Nonfinite two-phase patch vector");
    const double n2=p2_shear_dot(p.normal,p.normal);
    if(std::abs(n2-1)>1e-10)
        throw std::invalid_argument("Interface normal must be unit length");
    const double g2=p2_shear_dot(p.marangoni_gradient_pa,p.marangoni_gradient_pa);
    if(std::abs(p2_shear_dot(p.normal,p.marangoni_gradient_pa))>
       1e-10*std::max(1.0,std::sqrt(g2)))
        throw std::invalid_argument("Marangoni gradient must be tangential");
    const double rw=p.water_center_distance_m/p.water_viscosity_pa_s;
    const double ra=p.air_center_distance_m/p.air_viscosity_pa_s;
    const double r=rw+ra,k=p.interface_area_m2/r,b=p.dt_s*k;
    const double q=1/p.water_mass_kg+1/p.air_mass_kg;
    const double denominator=1+b*q;
    if(!(rw>0&&ra>0&&r>0&&k>0&&std::isfinite(denominator)))
        throw std::invalid_argument("Unresolved two-phase shear resistance");
    P2TwoPhaseShearExchange3D out;
    out.water_velocity_m_s=p.water_velocity_m_s;
    out.air_velocity_m_s=p.air_velocity_m_s;
    out.kinetic_before_j=.5*(p.water_mass_kg*p2_shear_dot(p.water_velocity_m_s,p.water_velocity_m_s)
        +p.air_mass_kg*p2_shear_dot(p.air_velocity_m_s,p.air_velocity_m_s));
    double delta_new_sq=0,water_impulse_sq=0,air_impulse_sq=0;
    for(int i=0;i<3;++i){
        const double normal_water=p2_shear_dot(p.water_velocity_m_s,p.normal);
        const double normal_air=p2_shear_dot(p.air_velocity_m_s,p.normal);
        const double d=(p.air_velocity_m_s[i]-normal_air*p.normal[i])
                      -(p.water_velocity_m_s[i]-normal_water*p.normal[i]);
        // Backward-Euler interfacial stress balance:
        // tau_w=mu_w*(u_interface-u_w)/dw,
        // tau_a=mu_a*(u_a-u_interface)/da, tau_w-tau_a=grad_s(sigma).
        const double dnew=(d+b*(rw/p.air_mass_kg-ra/p.water_mass_kg)*
                          p.marangoni_gradient_pa[i])/denominator;
        const double iw=b*(dnew+ra*p.marangoni_gradient_pa[i]);
        const double ia=-b*(dnew-rw*p.marangoni_gradient_pa[i]);
        out.water_impulse_n_s[i]=iw;
        out.air_impulse_n_s[i]=ia;
        out.surface_impulse_n_s[i]=p.dt_s*p.interface_area_m2*p.marangoni_gradient_pa[i];
        out.water_traction_pa[i]=iw/(p.dt_s*p.interface_area_m2);
        out.air_traction_pa[i]=-ia/(p.dt_s*p.interface_area_m2);
        out.water_velocity_m_s[i]+=iw/p.water_mass_kg;
        out.air_velocity_m_s[i]+=ia/p.air_mass_kg;
        delta_new_sq+=dnew*dnew;
        water_impulse_sq+=iw*iw;
        air_impulse_sq+=ia*ia;
    }
    out.kinetic_after_j=.5*(p.water_mass_kg*p2_shear_dot(out.water_velocity_m_s,out.water_velocity_m_s)
        +p.air_mass_kg*p2_shear_dot(out.air_velocity_m_s,out.air_velocity_m_s));
    out.viscous_dissipation_j=b*delta_new_sq;
    out.implicit_dissipation_j=.5*(water_impulse_sq/p.water_mass_kg+
                                   air_impulse_sq/p.air_mass_kg);
    P2ShearVec3 weighted{};
    for(int i=0;i<3;++i)
        weighted[i]=(ra*out.water_velocity_m_s[i]+rw*out.air_velocity_m_s[i])/r;
    out.surface_work_j=p.dt_s*p.interface_area_m2*
                        p2_shear_dot(p.marangoni_gradient_pa,weighted);
    out.energy_residual_j=out.kinetic_after_j-out.kinetic_before_j-
        out.surface_work_j+out.viscous_dissipation_j+out.implicit_dissipation_j;
    const double scalars[]={out.kinetic_before_j,out.kinetic_after_j,
        out.viscous_dissipation_j,out.implicit_dissipation_j,
        out.surface_work_j,out.energy_residual_j};
    for(double v:scalars)if(!std::isfinite(v))
        throw std::invalid_argument("Nonfinite shear exchange energy");
    for(const auto& vec:{out.water_velocity_m_s,out.air_velocity_m_s,
                         out.water_impulse_n_s,out.air_impulse_n_s})
        for(double v:vec)if(!std::isfinite(v))
            throw std::invalid_argument("Nonfinite shear exchange impulse");
    return out;
}
} // namespace upc
