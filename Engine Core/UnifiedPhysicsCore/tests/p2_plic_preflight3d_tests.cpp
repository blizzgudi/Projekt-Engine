// P2 PARTIAL: deterministic 16^3 periodic PLIC preflight QA.
// No claim that an unresolved mixed interface can be advected.
#include "upc/p2_plic_preflight3d.hpp"
#include "upc/p2_plic_strang_periodic3d.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
constexpr std::size_t n=16;
std::size_t assertions=0;
void check(bool ok,const char* why) {
    ++assertions;
    if(!ok)throw std::runtime_error(why);
}
std::size_t idx(std::size_t x,std::size_t y,std::size_t z) {
    return (z*n+y)*n+x;
}
template<class F> void expect_invalid(F f,const char* why) {
    bool rejected=false;
    try { f(); } catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,why);
}
}
int main() {
    try {
        std::vector<double> f(n*n*n,0);
        auto a=upc::p2_periodic_plic_preflight3d(f,n,n,n);
        check(a.partial_cells==0&&!a.has_unresolved(),"pure air");
        std::fill(f.begin(),f.end(),1);
        a=upc::p2_periodic_plic_preflight3d(f,n,n,n);
        check(a.partial_cells==0&&!a.has_unresolved(),"pure water");
        // Exactly one 0.5 interface layer; periodic seam is resolved.
        for(std::size_t z=0;z<n;++z)
        for(std::size_t y=0;y<n;++y)
        for(std::size_t x=0;x<n;++x)
            f[idx(x,y,z)]=x<4?1.0:(x==4?0.5:0.0);
        a=upc::p2_periodic_plic_preflight3d(f,n,n,n);
        check(a.partial_cells==n*n&&a.resolved_cells==n*n&&
              !a.has_unresolved(),"planar mixed layer resolved");
        check(std::abs(a.min_resolved_gradient_scale-0.5)<1e-15,
              "independent centered gradient");
        const auto normals=upc::p2_periodic_plic_normals3d(f,n,n,n);
        for(std::size_t z=0;z<n;++z)
        for(std::size_t y=0;y<n;++y) {
            const auto normal=normals[idx(4,y,z)];
            check(std::abs(normal[0]-1)<1e-15&&
                  normal[1]==0&&normal[2]==0,
                  "normal orientation matches production PLIC");
        }
        // Isolated mixed cell is genuinely ambiguous, NOT roundoff.
        std::fill(f.begin(),f.end(),0);
        f[idx(7,8,9)]=0.5;
        a=upc::p2_periodic_plic_preflight3d(f,n,n,n);
        check(a.partial_cells==1&&a.unresolved_cells==1&&
              a.unresolved_geometric_cells==1&&
              a.unresolved_endpoint_roundoff_candidates==0,
              "geometric ambiguity classified");
        check(a.first_unresolved_xyz==std::array<std::size_t,3>{7,8,9}&&
              a.first_unresolved_fill==0.5&&
              a.first_unresolved_gradient_scale==0,
              "first geometric ambiguity coordinate");
        expect_invalid([&]{(void)upc::p2_periodic_plic_normals3d(f,n,n,n);},
                       "production PLIC must reject ambiguous geometry");
        // Tiny near-one interior cell: diagnostic candidate, never silently fixed.
        std::fill(f.begin(),f.end(),1);
        f[idx(2,3,4)]=std::nextafter(1.0,0.0);
        const auto original=f;
        a=upc::p2_periodic_plic_preflight3d(f,n,n,n);
        check(a.unresolved_cells==1&&a.unresolved_geometric_cells==0&&
              a.unresolved_endpoint_roundoff_candidates==1,
              "endpoint roundoff candidate classified");
        check(f==original,"preflight must never mutate fractions");
        expect_invalid([&]{(void)upc::p2_periodic_plic_normals3d(f,n,n,n);},
                       "production PLIC must still reject endpoint candidate");
        // Periodic seam: x=0 partial, x=15 wet, x=1 dry.
        std::fill(f.begin(),f.end(),0);
        for(std::size_t z=0;z<n;++z)
        for(std::size_t y=0;y<n;++y) {
            f[idx(0,y,z)]=0.5;
            f[idx(15,y,z)]=1;
        }
        a=upc::p2_periodic_plic_preflight3d(f,n,n,n);
        check(a.partial_cells==n*n&&a.resolved_cells==n*n&&
              a.unresolved_cells==0,"periodic seam gradients resolved");
        check(std::abs(a.min_resolved_gradient_scale-0.5)<1e-15,
              "periodic seam gradient magnitude");
        // Input validation fails closed.
        expect_invalid([&]{(void)upc::p2_periodic_plic_preflight3d(f,1,n,n);},
                       "invalid dimension rejected");
        expect_invalid([&]{(void)upc::p2_periodic_plic_preflight3d(f,n,n,n+1);},
                       "invalid array size rejected");
        f[0]=std::numeric_limits<double>::quiet_NaN();
        expect_invalid([&]{(void)upc::p2_periodic_plic_preflight3d(f,n,n,n);},
                       "NaN rejected");
        f[0]=-0.1;
        expect_invalid([&]{(void)upc::p2_periodic_plic_preflight3d(f,n,n,n);},
                       "negative fill rejected");
        f[0]=1.1;
        expect_invalid([&]{(void)upc::p2_periodic_plic_preflight3d(f,n,n,n);},
                       "overfilled cell rejected");
        std::cout<<"P2_PLIC_PREFLIGHT n=16 resolved_planar=256 "
                    "unresolved_geometric=1 unresolved_endpoint=1 "
                    "periodic_seam_resolved=256\n";
        std::cout<<"PASS P2_PLIC_PREFLIGHT assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"FAIL P2_PLIC_PREFLIGHT "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
