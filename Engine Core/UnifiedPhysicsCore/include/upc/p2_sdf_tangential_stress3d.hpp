#pragma once
// P2 PARTIAL: local, prescribed two-phase tangential traction matching at
// 3D SDF cut faces. This reconstructs a liquid boundary velocity gradient;
// it does NOT apply a ghost velocity to the MAC/VOF solver or evolve air.
#include "upc/p2_sdf_normal_stress3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace upc {
struct P2SdfTangentialStressFace3D {
    P2SdfNormalStressFace3D normal_stress;
    std::array<double,3> surface_gradient_pa;
    std::array<double,3> tangential_traction_before_pa;
    std::array<double,3> tangential_traction_after_pa;
    P2GradientTensor3D corrected_water_gradient;
    double residual_before_pa;
    double residual_after_pa;
};
struct P2SdfTangentialStressResult3D {
    std::vector<P2SdfTangentialStressFace3D> faces;
};
inline P2SdfTangentialStressResult3D p2_sdf_tangential_stress_faces3d(
    std::size_t nx,std::size_t ny,std::size_t nz,double dx,
    const std::vector<double>& phi,double sigma,double p_air,
    double mu_water,double mu_air,
    const P2GradientTensor3D& grad_water,
    const P2GradientTensor3D& grad_air,
    const std::array<double,3>& surface_tension_gradient_n_m2) {
    for(double a:surface_tension_gradient_n_m2)
        if(!std::isfinite(a))
            throw std::invalid_argument("Nonfinite surface-tension gradient");
    // Validates SDF, viscosities and prescribed tensors before any output.
    const auto normal=p2_sdf_normal_stress_faces3d(nx,ny,nz,dx,phi,
        sigma,p_air,mu_water,mu_air,grad_water,grad_air);
    P2SdfTangentialStressResult3D result;
    result.faces.reserve(normal.faces.size());
    for(const auto& face:normal.faces) {
        const auto& n=face.outward_normal;
        const auto tangent=[&](const std::array<double,3>& v) {
            const double normal_component=v[0]*n[0]+v[1]*n[1]+v[2]*n[2];
            return std::array<double,3>{v[0]-normal_component*n[0],
                v[1]-normal_component*n[1],v[2]-normal_component*n[2]};
        };
        const auto traction=[&](const P2GradientTensor3D& liquid) {
            std::array<double,3> v{};
            for(int i=0;i<3;++i)for(int j=0;j<3;++j) {
                const double dw=liquid[3*i+j]+liquid[3*j+i];
                const double da=grad_air[3*i+j]+grad_air[3*j+i];
                v[i]+=(mu_water*dw-mu_air*da)*n[j];
            }
            return tangent(v);
        };
        const auto target=tangent(surface_tension_gradient_n_m2);
        const auto before=traction(grad_water);
        std::array<double,3> residual{};
        double norm2=0;
        for(int i=0;i<3;++i) {
            residual[i]=target[i]-before[i];
            norm2+=residual[i]*residual[i];
        }
        if(!std::isfinite(norm2))
            throw std::invalid_argument("Tangential traction overflow");
        auto corrected=grad_water;
        // Minimal symmetric local gradient update: Delta G = (r n^T + n r^T)/(2 mu).
        // Since r.n=0, 2 mu Delta D n=r and n.Delta D.n=0 exactly.
        if(mu_water==0) {
            if(std::sqrt(norm2)>1e-12)
                throw std::invalid_argument("Zero liquid viscosity cannot impose shear");
        } else {
            for(int i=0;i<3;++i)for(int j=0;j<3;++j) {
                corrected[3*i+j]+=(residual[i]*n[j]+n[i]*residual[j])/
                                   (2*mu_water);
                if(!std::isfinite(corrected[3*i+j]))
                    throw std::invalid_argument("Tangential gradient correction overflow");
            }
        }
        const auto after=traction(corrected);
        double after_norm2=0;
        for(int i=0;i<3;++i) {
            const double d=target[i]-after[i];
            after_norm2+=d*d;
        }
        if(!std::isfinite(after_norm2))
            throw std::invalid_argument("Nonfinite corrected traction");
        result.faces.push_back({face,target,before,after,corrected,
                                std::sqrt(norm2),std::sqrt(after_norm2)});
    }
    return result;
}
} // namespace upc
