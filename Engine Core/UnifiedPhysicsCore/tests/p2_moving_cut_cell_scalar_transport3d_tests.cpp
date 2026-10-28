#include "upc/p2_moving_cut_cell_scalar_transport3d.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t checks=0;
void check(bool x,const char* msg) {
    ++checks;
    if(!x)throw std::runtime_error(msg);
}
void near(double a,double b,double eps,const char* msg) {
    check(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=eps,msg);
}
template<class F> void reject(F fn,const char* msg) {
    bool threw=false;
    try {fn();}catch(const std::exception&) {threw=true;}
    check(threw,msg);
}
using Box=upc::P2Box3D;
using Solver=upc::P2MovingBoxCutCellProjection3DBuilder;
using Transport=upc::P2MovingCutCellScalarTransport3D;
using Projection=upc::P2MovingBoxCutCellProjection3D;
std::size_t ci(int n,int x,int y,int z) {return (z*n+y)*n+x;}
std::vector<double> scalar(int n,const std::vector<double>& a) {
    std::vector<double> s(a.size(),0.0);
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)for(int x=0;x<n;++x) {
        const auto i=ci(n,x,y,z);
        if(a[i]>0.0)s[i]=0.25+0.01*x+0.02*y-0.015*z;
    }
    return s;
}
// Independent face-oriented finite-volume reference. q is already m^3;
// reference works in physical integrated scalar units, not cell fractions.
std::vector<double> reference(int n,const Projection& p,
                              const std::vector<double>& s) {
    const auto& a=p.wall.geometry.cell_geometry.alpha_old;
    const auto& b=p.wall.geometry.cell_geometry.alpha_new;
    std::vector<double> m(s.size());
    for(std::size_t i=0;i<s.size();++i)m[i]=a[i]*s[i];
    const auto add=[&](int axis,int x,int y,int z,double q) {
        if(q==0.0)return;
        const int lx=x-(axis==0),ly=y-(axis==1),lz=z-(axis==2);
        const auto left=ci(n,lx,ly,lz),right=ci(n,x,y,z);
        const auto donor=q>0.0?left:right;
        const double transported=q*s[donor]; // dx=1 in this test
        m[left]-=transported;
        m[right]+=transported;
    };
    for(int z=0;z<n;++z)for(int y=0;y<n;++y)
    for(int x=1;x<n;++x)
        add(0,x,y,z,p.u_fluid_m3[(z*n+y)*(n+1)+x]);
    for(int z=0;z<n;++z)for(int y=1;y<n;++y)
    for(int x=0;x<n;++x)
        add(1,x,y,z,p.v_fluid_m3[(z*(n+1)+y)*n+x]);
    for(int z=1;z<n;++z)for(int y=0;y<n;++y)
    for(int x=0;x<n;++x)
        add(2,x,y,z,p.w_fluid_m3[(z*n+y)*n+x]);
    for(std::size_t i=0;i<m.size();++i)m[i]=b[i]>1e-12?m[i]/b[i]:0.0;
    return m;
}
void run(int n,int axis,int sign) {
    Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    std::vector<double> state;
    double first_integral=0.0;
    for(int step=0;step<2;++step) {
        Box next=b;
        next.min_m[axis]+=sign*0.25;
        next.max_m[axis]+=sign*0.25;
        const auto p=Solver::sample(n,n,n,1.0,0.25,1000.0,b,next);
        const auto& old_a=p.wall.geometry.cell_geometry.alpha_old;
        const auto& new_a=p.wall.geometry.cell_geometry.alpha_new;
        if(step==0)state=scalar(n,old_a);
        const auto old=state;
        const auto out=Transport::step(n,n,n,1.0,old,p);
        const auto expected=reference(n,p,old);
        for(std::size_t i=0;i<old.size();++i)
            near(out.value_new[i],expected[i],2e-10,"independent upwind flux");
        near(out.integral_residual_m3,0.0,1e-10,"global signed integral");
        check(out.max_gcl_residual<1e-10,"moving geometry continuity");
        check(out.max_donor_courant<=1.0+1e-10,"donor CFL");
        check(out.active_faces>0,"nonzero moving flux");
        if(step==0)first_integral=out.integral_before_m3;
        near(out.integral_after_m3,first_integral,1e-10,"sequential integral");
        std::vector<double> constant(old.size(),0.0);
        for(std::size_t i=0;i<old.size();++i)
            if(old_a[i]>0.0)constant[i]=-1.75;
        const auto uniform=Transport::step(n,n,n,1.0,constant,p);
        for(std::size_t i=0;i<old.size();++i)
            near(uniform.value_new[i],new_a[i]>1e-12?-1.75:0.0,2e-10,
                 "constant state geometric preservation");
        auto opposite=old;
        for(auto& v:opposite)v=-v;
        const auto negative=Transport::step(n,n,n,1.0,opposite,p);
        for(std::size_t i=0;i<old.size();++i)
            near(negative.value_new[i],-out.value_new[i],2e-10,
                 "signed linearity");
        state=out.value_new;
        b=next;
    }
    std::cout<<std::setprecision(17)<<"SCALAR "<<n<<" "<<axis<<" "<<sign
             <<" "<<first_integral<<"\n";
}
void negative() {
    constexpr int n=8;
    const Box b{{3.2,3.1,3.3},{4.7,4.6,4.8}};
    Box next=b;next.min_m[0]+=0.25;next.max_m[0]+=0.25;
    const auto p=Solver::sample(n,n,n,1.0,0.25,1000.0,b,next);
    const auto s=scalar(n,p.wall.geometry.cell_geometry.alpha_old);
    const auto snapshot=s;
    auto bad=p;
    bad.u_fluid_m3[0]=0.01;
    reject([&]{Transport::step(n,n,n,1.0,s,bad);},"exterior flux");
    bad=p;bad.u_fluid_m3[1]=std::numeric_limits<double>::quiet_NaN();
    reject([&]{Transport::step(n,n,n,1.0,s,bad);},"nonfinite flux");
    bad=p;bad.wall.geometry.cell_geometry.alpha_new[0]=0.3;
    reject([&]{Transport::step(n,n,n,1.0,s,bad);},"broken GCL");
    bad=p;bad.u_fluid_m3[(1*n+1)*(n+1)+1]=200.0;
    reject([&]{Transport::step(n,n,n,1.0,s,bad);},"donor CFL");
    auto invalid=s;invalid[ci(n,3,3,3)]=
        std::numeric_limits<double>::infinity();
    reject([&]{Transport::step(n,n,n,1.0,invalid,p);},"nonfinite scalar");
    invalid=s;invalid.pop_back();
    reject([&]{Transport::step(n,n,n,1.0,invalid,p);},"scalar shape");
    reject([&]{Transport::step(n,n,n,0.0,s,p);},"invalid spacing");
    check(s==snapshot,"input immutable after rejection");
}
}
int main() {
    try {
        for(int n:{8,12,16})for(int axis=0;axis<3;++axis)
            for(int sign:{-1,1})run(n,axis,sign);
        negative();
        std::cout<<"PASS: "<<checks
                 <<" native moving cut-cell signed scalar assertions\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";
        return 1;
    }
}
