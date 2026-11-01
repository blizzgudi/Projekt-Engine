// P2 PARTIAL: independent MARIN/SPHERIC Test-02 geometry baseline with
// existing CoupledMacWater3D. Outputs signals for measured-data comparison.
// A successful numerical run DOES NOT mean experimental validation.
#include "upc/coupled_water3d.hpp"
#include "upc/p2_marin_pressure_sensors3d.hpp"
#include "upc/p2_marin_wall_pressure_interp3d.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
double overlap(double a,double b,double c,double d){
    return std::max(0.0,std::min(b,d)-std::max(a,c));
}
double gauge_height(const upc::CoupledMacWater3D& w,double x_m,double y_m) {
    const auto dx=w.cell_size_m();
    const auto x=static_cast<std::size_t>(std::floor(x_m/dx));
    const auto y=static_cast<std::size_t>(std::floor(y_m/dx));
    if(x>=w.nx()||y>=w.ny())throw std::runtime_error("Probe outside grid");
    double height=0;
    for(std::size_t k=0;k<w.nz();++k)
        height+=w.water_fraction(x,y,k)*dx;
    return height;
}
}
int main(int argc,char** argv){
    try {
        const bool pcg_pressure=argc>=4 && std::string(argv[argc-1])=="pcg";
        const int mode_argc=argc-(pcg_pressure?1:0);
        const bool finer_grid=mode_argc>=4 && std::string(argv[3])=="fine";
        const bool dense=(mode_argc>=4 && std::string(argv[3])=="dense") ||
                         (mode_argc>=5 && std::string(argv[4])=="dense");
        const bool interp_probes=
            (mode_argc==5 && std::string(argv[3])=="dense" &&
             std::string(argv[4])=="interp") ||
            (mode_argc==6 && std::string(argv[3])=="fine" &&
             std::string(argv[4])=="dense" &&
             std::string(argv[5])=="interp");
        const bool pressure_probes=interp_probes||
            (mode_argc==5 && std::string(argv[3])=="dense" &&
             std::string(argv[4])=="pressure") ||
            (mode_argc==6 && std::string(argv[3])=="fine" &&
             std::string(argv[4])=="dense" &&
             std::string(argv[5])=="pressure");
        const double dx=finer_grid?0.05:0.1;
        const std::size_t nx=finer_grid?64:32,ny=finer_grid?20:10,nz=finer_grid?20:10;
        const int steps=argc>=2?std::stoi(argv[1]):40;
        const bool throughflow=argc>=3 && std::string(argv[2])=="throughflow";
        if(mode_argc>6||(mode_argc>=3&&!throughflow)||
           (mode_argc>=4&&!finer_grid&&!dense)||
           (mode_argc==5 && !pressure_probes && (!finer_grid||!dense))||
           (mode_argc==6 && !pressure_probes))
            throw std::invalid_argument("Unknown benchmark transport mode");
        if(steps<1||steps>300)
            throw std::invalid_argument("Invalid benchmark step budget");
        upc::CoupledMacWater3D w(nx,ny,nz,dx,1000.0);
        w.set_conservative_throughflow_transport(throughflow);
        w.set_pcg_pressure_projection(pcg_pressure);
        // SPHERIC tank 3.22 x 1 x 1 m. Equal physical 3.20 x 1 x 1 m
        // represented domains for coarse and doubled-resolution grid.
        // Initial right-hand water reservoir starts x=1.992 m, z<0.55 m.
        // Product of exact x and z intersections preserves partial cells.
        for(std::size_t k=0;k<nz;++k)
        for(std::size_t j=0;j<ny;++j)
        for(std::size_t i=0;i<nx;++i){
            const double x=(i+0.5)*dx,y=(j+0.5)*dx,z=(k+0.5)*dx;
            // Instrument obstacle x=[.6635,.8245], y=[.295,.698],
            // z=[0,.161] m; binary cell-centre approximation.
            const bool obstacle=(x>=.6635&&x<.8245&&
                                 y>=.295&&y<.698&&z<.161);
            if(obstacle){w.set_solid(i,j,k);continue;}
            const double f_x=overlap(i*dx,(i+1)*dx,1.992,3.22)/dx;
            const double f_z=overlap(k*dx,(k+1)*dx,0,.55)/dx;
            const double f=std::clamp(f_x*f_z,0.0,1.0);
            if(f>0)w.set_water(i,j,k,f);
        }
        const auto initial_volume=w.volume_m3();
        const double expected_volume=(nx*dx-1.992)*.55;
        if(std::abs(initial_volume-expected_volume)>1e-12)
            throw std::runtime_error("MARIN initial reservoir mismatch");
        const std::array<double,4> x_probe{{.496,.992,1.488,2.632}};
        std::cout<<std::setprecision(17);
        std::cout<<"# MARIN_COARSE_3D dx_m="<<dx<<" nx="<<nx<<" ny="<<ny
                 <<" nz="<<nz<<" transport="<<(throughflow?"throughflow":"legacy")
                 <<" grid="<<(finer_grid?"fine":"coarse")
                 <<" sampling="<<(dense?"dense":"sparse")
                 <<" pressure_solver="<<(pcg_pressure?"pcg":"gauss_seidel")
                 <<" initial_volume_m3="<<initial_volume
                 <<" target_full_extent_x_m=3.22 represented_extent_x_m="
                 <<nx*dx<<" obstacle_x_m=[0.6635,0.8245] obstacle_y_m=[0.295,0.698]"
                 <<" obstacle_z_m=[0,0.161] probe_x_m=[0.496,0.992,1.488,2.632]"
                 <<" pressure_sensors_NOT_simulated="<<(pressure_probes?"false":"true")
                 <<" pressure_proxy_only="<<(interp_probes?"interpolated_tangent":
                       pressure_probes?"true":"none")<<"\n";
        std::cout<<"time_s,H1_m,H2_m,H3_m,H4_m,water_volume_m3,"
                   "max_projection_divergence_s_inv";
        if(pressure_probes)
            std::cout<<",P1_Pa,P2_Pa,P3_Pa,P4_Pa,P5_Pa,P6_Pa,P7_Pa,P8_Pa,"
                         "pressure_probe_wet_count";
        std::cout<<"\n";
        const auto emit=[&](double t) {
            std::cout<<t;
            for(double x:x_probe)std::cout<<","<<gauge_height(w,x,.5);
            std::cout<<","<<w.volume_m3()<<","
                     <<w.last_projection_divergence();
            if(interp_probes) {
                const auto sensor=upc::P2MarinWallPressureInterpolation3D::sample(w);
                for(const auto& p:sensor.probes)
                    std::cout<<","<<p.pressure_pa;
                std::cout<<","<<sensor.wet_count;
            }else if(pressure_probes) {
                const auto sensor=upc::P2MarinPressureSensors3D::sample(w);
                for(const auto& p:sensor.probes)
                    std::cout<<","<<p.pressure_pa;
                std::cout<<","<<sensor.wet_count;
            }
            std::cout<<"\n";
        };
        emit(0);
        for(int i=1;i<=steps;++i) {
            if(pcg_pressure) w.step_verified(.01,finer_grid?1600:1000,1e-6);
            else w.step(.01,finer_grid?350:175);
            if(std::abs(w.volume_m3()-initial_volume)>1e-8)
                throw std::runtime_error("MARIN water mass drift");
            if(dense||i==5||i==10||i==20||i==40||i==100||
               i==200||i==steps)emit(i*.01);
        }
        std::cout<<"PASS MARIN_COARSE_BASELINE partial_only=true steps="
                 <<steps<<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL MARIN_COARSE_BASELINE "<<e.what()<<"\n";
        return 1;
    }
}
