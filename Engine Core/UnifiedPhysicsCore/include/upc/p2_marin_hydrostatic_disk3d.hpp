#pragma once
// P2 PARTIAL: independent, exact hydrostatic disk wetting and gauge-pressure
// reference for MARIN Test-02 P1-P8. Prescribed planar waterline only.
// NOT a calibrated transducer, live VOF stress, or CFD acceptance.
#include "upc/p2_marin_pressure_sensors3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace upc {
struct P2MarinHydrostaticDiskProbe3D {
    double wet_area_fraction=0.;
    double disk_mean_gauge_pressure_pa=0.;
    double integrated_gauge_load_n=0.; // positive scalar p*dA, not signed wall force
};
class P2MarinHydrostaticDisk3D {
public:
    static P2MarinHydrostaticDiskProbe3D front(
        double z_center_m,double waterline_m,double radius_m,
        double rho_kg_m3=1000.,double gravity_m_s2=9.81) {
        validate(waterline_m,radius_m,rho_kg_m3,gravity_m_s2);
        if(!std::isfinite(z_center_m)||z_center_m<radius_m)
            throw std::invalid_argument("Front disk intersects tank floor");
        constexpr double pi=3.141592653589793238462643383279502884;
        const double t=(waterline_m-z_center_m)/radius_m;
        P2MarinHydrostaticDiskProbe3D out{};
        if(t<=-1.)return out;
        if(t>=1.) {
            out.wet_area_fraction=1.;
            out.disk_mean_gauge_pressure_pa=
                rho_kg_m3*gravity_m_s2*(waterline_m-z_center_m);
        } else {
            const double root=std::sqrt(std::max(0.,1.-t*t));
            out.wet_area_fraction=0.5+
                (std::asin(t)+t*root)/pi;
            // Integrate rho*g*(H-z) over the wetted part of a VERTICAL disk,
            // divided by the ENTIRE disk area, including dry patches.
            // No VOF cell-centre wetness or empirical calibration.
            out.disk_mean_gauge_pressure_pa=
                rho_kg_m3*gravity_m_s2*radius_m/pi*
                (t*(t*root+std::asin(t)+pi/2.)+
                 (2./3.)*root*root*root);
        }
        out.integrated_gauge_load_n=
            out.disk_mean_gauge_pressure_pa*pi*radius_m*radius_m;
        return out;
    }
    static P2MarinHydrostaticDiskProbe3D roof(
        double roof_z_m,double waterline_m,double radius_m,
        double rho_kg_m3=1000.,double gravity_m_s2=9.81) {
        validate(waterline_m,radius_m,rho_kg_m3,gravity_m_s2);
        if(!std::isfinite(roof_z_m)||roof_z_m<0)
            throw std::invalid_argument("Invalid roof elevation");
        constexpr double pi=3.141592653589793238462643383279502884;
        P2MarinHydrostaticDiskProbe3D out{};
        if(waterline_m<=roof_z_m)return out;
        out.wet_area_fraction=1.;
        out.disk_mean_gauge_pressure_pa=
            rho_kg_m3*gravity_m_s2*(waterline_m-roof_z_m);
        out.integrated_gauge_load_n=
            out.disk_mean_gauge_pressure_pa*pi*radius_m*radius_m;
        return out;
    }
    static std::array<P2MarinHydrostaticDiskProbe3D,8> test02(
        double waterline_m,double radius_m,
        double rho_kg_m3=1000.,double gravity_m_s2=9.81) {
        // The radius is a HYPOTHESIS, not a verified laboratory dimension.
        if(radius_m>0.020)
            throw std::invalid_argument("MARIN disk crosses box face edge");
        const auto sensors=P2MarinPressureSensors3D::transducers_m();
        std::array<P2MarinHydrostaticDiskProbe3D,8> out{};
        for(std::size_t i=0;i<8;++i) {
            out[i]=i<4?
                front(sensors[i][2],waterline_m,radius_m,rho_kg_m3,gravity_m_s2):
                roof(sensors[i][2],waterline_m,radius_m,rho_kg_m3,gravity_m_s2);
        }
        return out;
    }
private:
    static void validate(double H,double R,double rho,double g) {
        if(!std::isfinite(H)||H<0||!std::isfinite(R)||R<=0||
           !std::isfinite(rho)||rho<=0||!std::isfinite(g)||g<=0)
            throw std::invalid_argument("Invalid hydrostatic disk parameters");
    }
};
} // namespace upc
