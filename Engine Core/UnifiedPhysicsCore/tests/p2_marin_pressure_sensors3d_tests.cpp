// P2 PARTIAL: true native grid pressure readback at eight official MARIN
// sensor coordinates, using the existing MAC projector (not calibrated).
#include "upc/p2_marin_pressure_sensors3d.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
std::size_t assertions=0;
void check(bool condition,const std::string& why) {
    ++assertions;
    if(!condition)throw std::runtime_error(why);
}
void near(double value,double expected,double eps,const std::string& why){
    check(std::isfinite(value)&&std::abs(value-expected)<=eps,why);
}
void mapping(int n,double dx){
    upc::CoupledMacWater3D liquid(n,n/3,n/3,dx,1000.0);
    const auto loc=upc::P2MarinPressureSensors3D::transducers_m();
    const std::array<double,4> offsets{{.021,.061,.101,.141}};
    for(std::size_t k=0;k<4;++k) {
        near(loc[k][0],.8245,1e-14,"P1-P4 front x");
        near(loc[k][1],.471,1e-14,"P1-P4 y");
        near(loc[k][2],offsets[k],1e-14,"P1-P4 front height");
        near(loc[k+4][0],.8245-offsets[k],1e-14,"P5-P8 roof x");
        near(loc[k+4][1],.471,1e-14,"P5-P8 y");
        near(loc[k+4][2],.161,1e-14,"P5-P8 roof z");
    }
    const auto sensor=upc::P2MarinPressureSensors3D::sample(liquid);
    check(sensor.wet_count==0,"initially empty domain no wet pressure gauges");
    for(std::size_t k=0;k<8;++k) {
        const auto& p=sensor.probes[k];
        check(!p.water_wet,"zero pressure sensors initially dry");
        near(p.pressure_pa,0,0,"zero initial pressure");
        check(p.sampled_cell_centre_m[0]>=0 &&
              p.sampled_cell_centre_m[0]<liquid.nx()*dx,
              "sensor sampled actual cell x");
        check(p.sampled_cell_centre_m[1]>=0 &&
              p.sampled_cell_centre_m[1]<liquid.ny()*dx,
              "sensor sampled actual cell y");
        check(p.sampled_cell_centre_m[2]>=0 &&
              p.sampled_cell_centre_m[2]<liquid.nz()*dx,
              "sensor sampled actual cell z");
        if(k<4)check(p.sampled_cell_centre_m[0]>.8245,
                     "front pressure sample lies ahead of wall");
        else check(p.sampled_cell_centre_m[2]>.161,
                   "roof pressure sample lies above box");
    }
    // Establish the aliasing inherent in coarse, not full-resolved probes.
    if(dx>.075) {
        check(sensor.probes[0].grid_z==sensor.probes[1].grid_z,
              "coarse P1-P2 vertical transducer alias");
        check(sensor.probes[5].grid_x==sensor.probes[6].grid_x,
              "coarse P6-P7 roof transducer alias");
    }
    for(std::size_t z=0;z<liquid.nz();++z)
    for(std::size_t y=0;y<liquid.ny();++y)
    for(std::size_t x=0;x<liquid.nx();++x) {
        const double px=(x+0.5)*dx,py=(y+0.5)*dx,pz=(z+0.5)*dx;
        if(px>=.6635&&px<.8245&&py>=.295&&py<.698&&pz<.161)
            liquid.set_solid(x,y,z);
        else liquid.set_water(x,y,z,1);
    }
    const auto filled=upc::P2MarinPressureSensors3D::sample(liquid);
    check(filled.wet_count==8,"all eight sensors wet when fluid fills tank");
    liquid.step(.001,150);
    const auto pressure=upc::P2MarinPressureSensors3D::sample(liquid);
    check(pressure.wet_count==8,"all eight sensors still wet after fluid projection");
    for(const auto& p:pressure.probes) {
        check(std::isfinite(p.pressure_pa),"true native pressure finite");
        check(p.pressure_pa>=-1e-5,"no unphysical huge negative hydrostatic pressure");
    }
    std::cout<<"P2_MARIN_PRESSURE_SENSOR_GRID dx_m="<<dx<<" probes=8 wet="<<pressure.wet_count<<"\n";
}
}
int main(){
    try {
        mapping(32,.1);
        mapping(64,.05);
        std::cout<<"PASS P2_MARIN_PRESSURE_SENSORS assertions="<<assertions
                 <<" phase_complete=false\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL P2_MARIN_PRESSURE_SENSORS "<<e.what()
                 <<" assertions="<<assertions<<"\n";
        return 1;
    }
}
