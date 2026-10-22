#include <Aevrynd/PhaseSkyAmbient3D.h>
#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
namespace {
void Need(bool v,const char* text){if(!v)throw std::runtime_error(text);}
using D3=std::array<long double,3>;
D3 D(Vec3 v){return{v.x,v.y,v.z};}
bool Black(Vec3 v){return v.x==0.f&&v.y==0.f&&v.z==0.f;}
bool Exact(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
long double maxQuadratureError{},maxSlabError{};
// 獨立方向積分：球面方位對稱，dOmega=2pi dmu。
// mu=t^32 去除 k=.05 在水平線的尖導數；不使用 helper 的解析權重。
// MSVC long double 等同 double，仍高於被測 float；不宣稱80-bit精度。
D3 SphereQuadrature(const PhaseRadianceProfile3D& s,unsigned intervals){
    Need(intervals>0&&intervals%2==0,"QUADRATURE_INTERVALS");D3 sum{};const auto z=D(s.skyZenith),h=D(s.skyHorizon),g=D(s.groundBounce);
    for(unsigned i=0;i<=intervals;++i){const long double t=static_cast<long double>(i)/intervals;
        const long double mu=std::pow(t,32.L),jacobian=32.L*std::pow(t,31.L),blend=std::pow(mu,static_cast<long double>(s.horizonExponent));
        const long double simpson=i==0||i==intervals?1.L:(i%2?4.L:2.L);
        for(unsigned c=0;c<3;++c){const auto upper=h[c]+(z[c]-h[c])*blend,lower=h[c]+(g[c]-h[c])*blend;
            sum[c]+=simpson*.5L*(upper+lower)*jacobian*static_cast<long double>(s.skyIntensity);}}
    for(auto& v:sum)v/=3.L*intervals;return sum;
}
void Compare(Vec3 result,const D3& expected,const char* reason){const auto a=D(result);for(unsigned c=0;c<3;++c){Need(std::isfinite(a[c])&&a[c]>=0,"AMBIENT_FINITE_NONNEGATIVE");const auto error=std::abs(a[c]-expected[c]);maxQuadratureError=(std::max)(maxQuadratureError,error);Need(error<=2.e-6L*(std::max)(1.L,std::abs(expected[c])),reason);}}
void Slab(Vec3 ambient,double tau,unsigned steps){
    const std::array<double,3> background{.13,.29,.07};const auto input=D(ambient);const double stepT=std::exp(-tau/steps);double transmission=1.;std::array<double,3> sum{};
    for(unsigned i=0;i<steps;++i){for(unsigned c=0;c<3;++c)sum[c]+=transmission*.88*(1.-stepT)*static_cast<double>(input[c]);transmission*=stepT;}
    const double exactT=std::exp(-tau);for(unsigned c=0;c<3;++c){const double actual=sum[c]+transmission*background[c];const double closed=exactT*background[c]+.88*(1.-exactT)*static_cast<double>(input[c]);const auto error=std::abs(actual-closed);maxSlabError=(std::max)(maxSlabError,static_cast<long double>(error));Need(error<=2.e-12*(std::max)(1.,std::abs(closed)),"UNIFORM_SLAB_CLOSED_FORM");}
}
}
int main(){try{
    static_assert(noexcept(EvaluatePhaseSkyAmbientRadiance3D(PhaseRadianceProfile3D{})));
    constexpr std::array<float,5> exponents{.05f,.42f,1.f,2.f,8.f};constexpr std::array<float,4> intensities{0.f,.5f,1.f,2.f};
    unsigned quadrature{},uniform{},black{},independence{},invalid{},slabs{};
    for(float k:exponents)for(float intensity:intensities)for(unsigned palette=0;palette<3;++palette){
        PhaseRadianceProfile3D p;p.horizonExponent=k;p.skyIntensity=intensity;
        if(palette==1){p.skyZenith={.04f,.8f,.16f};p.skyHorizon={.9f,.2f,.07f};p.groundBounce={.15f,.03f,1.1f};}
        if(palette==2){p.skyZenith={1.2f,.1f,.6f};p.skyHorizon={.12f,.7f,.3f};p.groundBounce={.8f,.65f,.05f};}
        PhaseRadianceProfile3D normalized;Need(NormalizePhaseRadianceProfile3D(p,normalized),"VALID_PROFILE");
        const auto q=SphereQuadrature(normalized,16384),q2=SphereQuadrature(normalized,32768);
        for(unsigned c=0;c<3;++c)Need(std::abs(q[c]-q2[c])<=1.e-9L*(std::max)(1.L,std::abs(q2[c])),"QUADRATURE_CONVERGENCE");
        const auto a=EvaluatePhaseSkyAmbientRadiance3D(p);Compare(a,q2,"SPHERE_RADIANCE_MEAN");++quadrature;
        if(intensity==0){Need(Black(a),"ZERO_INTENSITY_ZERO_PHOTONS");++black;}
        auto modified=p;modified.analyticDiffuse={8.f,0.f,4.f};modified.analyticDiffuseIntensity=7.f;Need(Exact(a,EvaluatePhaseSkyAmbientRadiance3D(modified)),"ANALYTIC_DIFFUSE_MUST_NOT_SCALE_SKY");++independence;
        for(double tau:{.01,.1,1.,4.})for(unsigned steps:{4u,8u,16u,32u}){Slab(a,tau,steps);++slabs;}
    }
    for(float k:exponents)for(float intensity:intensities){PhaseRadianceProfile3D p;p.horizonExponent=k;p.skyIntensity=intensity;p.skyZenith=p.skyHorizon=p.groundBounce={.125f,.5f,1.25f};Compare(EvaluatePhaseSkyAmbientRadiance3D(p),D(p.skyZenith*intensity),"UNIFORM_CONSTANT_RADIANCE");++uniform;
        p.skyZenith=p.skyHorizon=p.groundBounce={};Need(Black(EvaluatePhaseSkyAmbientRadiance3D(p)),"BLACK_SKY_ZERO_PHOTONS");++black;}
    PhaseRadianceProfile3D base;const auto a=EvaluatePhaseSkyAmbientRadiance3D(base);auto ground=base;ground.groundBounce.x+=.5f;const auto b=EvaluatePhaseSkyAmbientRadiance3D(ground);Need(b.x>a.x&&b.y==a.y&&b.z==a.z,"GROUND_COLOR_CAUSAL_CHANNEL");
    for(unsigned mode=0;mode<8;++mode){auto p=base;switch(mode){case 0:p.skyZenith.x=std::numeric_limits<float>::quiet_NaN();break;case 1:p.skyHorizon.y=-.01f;break;case 2:p.groundBounce.z=-1.f;break;case 3:p.skyIntensity=-1.f;break;case 4:p.horizonExponent=std::numeric_limits<float>::infinity();break;case 5:p.phaseFingerprint=0;break;case 6:p.analyticDiffuse.x=std::numeric_limits<float>::quiet_NaN();break;default:p.analyticDiffuseIntensity=-1.f;break;}Need(Black(EvaluatePhaseSkyAmbientRadiance3D(p)),"INVALID_PROFILE_FAIL_CLOSED");++invalid;}
    // 正值超界沿正式Normalize截限；這不是重新定義可接受範圍。
    for(float k:{0.f,20.f}){auto p=base;p.horizonExponent=k;PhaseRadianceProfile3D n;Need(NormalizePhaseRadianceProfile3D(p,n),"CLAMP_PROFILE");Need(Exact(EvaluatePhaseSkyAmbientRadiance3D(p),EvaluatePhaseSkyAmbientRadiance3D(n)),"NORMALIZE_K_CLAMP");}
    auto zero=base;zero.skyIntensity=0;auto dark=base;dark.skyZenith=dark.skyHorizon=dark.groundBounce={};
    const auto old=base.analyticDiffuse*(base.analyticDiffuseIntensity*.18f);Need(old.x>0&&old.y>0&&old.z>0&&Black(EvaluatePhaseSkyAmbientRadiance3D(zero))&&Black(EvaluatePhaseSkyAmbientRadiance3D(dark)),"OLD_MODEL_COUNTEREXAMPLE_REQUIRED");
    std::cout<<std::setprecision(17)<<"OLD_MODEL_CONTRACT_FAIL zero_sky_intensity_and_black_sky old_rgb="<<old.x<<','<<old.y<<','<<old.z<<" new_rgb=0,0,0\n";
    Need(quadrature==60&&uniform==20&&black==35&&independence==60&&invalid==8&&slabs==960,"TEST_CASE_CLOSURE");
    std::cout<<"PHASE_SKY_AMBIENT_CPU_COMPLETE quadrature="<<quadrature<<" uniform="<<uniform<<" black="<<black<<" independent="<<independence<<" invalid="<<invalid<<" slabs="<<slabs<<" old_model=FAIL new_model=PASS max_radiance_error="<<maxQuadratureError<<" max_slab_error="<<maxSlabError<<" GPU=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 2;}}
