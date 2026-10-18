#include <Aevrynd/SurfaceNormalTransport3D.h>
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace Aevrynd;
using Triangle=std::array<Vec3,3>;
unsigned checks{};
void Need(bool value,const char* label) {
    if (!value) throw std::runtime_error(label);
    ++checks;
}
bool Same(Vec3 a,Vec3 b) {
    return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)
        &&std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)
        &&std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);
}
Vec3 Unit(double x,double y,double z) {
    const double m=(std::max)({std::abs(x),std::abs(y),std::abs(z)});
    x/=m;y/=m;z/=m;const double length=std::sqrt(x*x+y*y+z*z);
    return {float(x/length),float(y/length),float(z/length)};
}
void Near(Vec3 actual,Vec3 expected,const char* label) {
    Need(std::isfinite(actual.x)&&std::isfinite(actual.y)&&std::isfinite(actual.z),label);
    Need(std::abs(double(actual.x)-expected.x)<2e-6&&std::abs(double(actual.y)-expected.y)<2e-6&&std::abs(double(actual.z)-expected.z)<2e-6,label);
    Need(std::abs(std::sqrt(double(actual.x)*actual.x+double(actual.y)*actual.y+double(actual.z)*actual.z)-1)<2e-6,label);
}
Vec3 Positive(const Triangle& a,const Triangle& b,Vec3 n,Vec3 expected,const char* label) {
    Vec3 result{7,8,9};Need(TryTransportSurfaceNormal3D(a,b,n,result),label);Near(result,expected,label);return result;
}
void Reject(const Triangle& a,const Triangle& b,Vec3 n,const char* label) {
    // 負零與NaN payload也必須原樣保留，不能先寫一半輸出再回false。
    Vec3 result{std::bit_cast<float>(0x80000000u),std::bit_cast<float>(0x7fc12345u),-17.5f};const auto before=result;
    Need(!TryTransportSurfaceNormal3D(a,b,n,result),label);Need(Same(result,before),label);
}
}
int main() {
    try {
        const Triangle plane{Vec3{0,0,0},Vec3{1,0,0},Vec3{0,1,0}};
        Positive(plane,plane,{0,0,1},{0,0,1},"identity axis");
        Positive(plane,plane,{2,-3,4},Unit(2,-3,4),"identity authored direction");
        const Triangle translated{Vec3{9,-7,3},Vec3{10,-7,3},Vec3{9,-6,3}};
        Positive(plane,translated,{2,-3,4},Unit(2,-3,4),"translation");
        // +90度繞X軸：n=(x,y,z) -> (x,-z,y)。
        const Triangle rotated{Vec3{},Vec3{1,0,0},Vec3{0,0,1}};
        Positive(plane,rotated,{1,2,3},Unit(1,-3,2),"rotation");
        const Triangle uniform{Vec3{},Vec3{3,0,0},Vec3{0,3,0}};
        Positive(plane,uniform,{1,2,3},Unit(1./3,2./3,3),"uniform tangent scale unit-normal extension");
        Positive(plane,uniform,{0,0,1},{0,0,1},"uniform geometric normal");
        const Triangle nonuniform{Vec3{},Vec3{2,0,0},Vec3{0,4,0}};
        Positive(plane,nonuniform,{1,2,3},Unit(.5,.5,3),"nonuniform tangent scale");
        const Triangle shear{Vec3{},Vec3{1,0,0},Vec3{.75f,1,0}};
        const auto reference=Positive(plane,shear,{1,2,3},Unit(1,1.25,3),"shear inverse transpose");
        Positive({plane[0],plane[2],plane[1]},{shear[0],shear[2],shear[1]},{1,2,3},reference,"reverse winding both triangles");
        const Triangle opposite{plane[0],plane[2],plane[1]};
        Positive(plane,opposite,{0,0,1},{0,0,-1},"single winding changes orientation");

        const float tiny=std::numeric_limits<float>::denorm_min();
        const Triangle smallest{Vec3{},Vec3{tiny,0,0},Vec3{0,tiny,0}};
        Positive(smallest,smallest,{1,2,3},Unit(1,2,3),"subnormal-scale triangle identity");
        Positive(plane,plane,{tiny,0,0},{1,0,0},"subnormal authored normal");
        const Triangle smallArea{Vec3{},Vec3{1e-20f,0,0},Vec3{0,1e-20f,0}};
        Positive(smallArea,smallArea,{0,0,1},{0,0,1},"tiny nonzero area");
        const float huge=std::numeric_limits<float>::max();
        const Triangle largest{Vec3{-huge,-huge,0},Vec3{huge,-huge,0},Vec3{-huge,huge,0}};
        Positive(largest,largest,{1,-2,3},Unit(1,-2,3),"finite coordinates float-subtraction overflow avoided");
        Positive(plane,plane,{huge,huge,-huge},Unit(1,1,-1),"huge authored normal");
        Positive(smallest,largest,{0,0,1},{0,0,1},"extreme expansion");
        Positive(largest,smallest,{1,0,0},{1,0,0},"extreme contraction");
        const float translatedBase=1e30f,next=std::nextafter(translatedBase,huge);
        const Triangle largeTranslated{Vec3{translatedBase,translatedBase,translatedBase},Vec3{next,translatedBase,translatedBase},Vec3{translatedBase,next,translatedBase}};
        Positive(largeTranslated,largeTranslated,{0,0,1},{0,0,1},"large translated finite coordinates");

        Reject(plane,plane,{0,0,0},"zero normal");
        Reject(plane,plane,{-0.f,0,-0.f},"signed zero normal");
        const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
        for(unsigned axis=0;axis<3;++axis)for(const float invalid:{nan,inf,-inf}) {
            Vec3 n{1,2,3};if(axis==0)n.x=invalid;else if(axis==1)n.y=invalid;else n.z=invalid;Reject(plane,plane,n,"nonfinite authored normal");
            for(unsigned vertex=0;vertex<3;++vertex) {
                auto bad=plane;if(axis==0)bad[vertex].x=invalid;else if(axis==1)bad[vertex].y=invalid;else bad[vertex].z=invalid;
                Reject(bad,plane,{0,0,1},"nonfinite source");Reject(plane,bad,{0,0,1},"nonfinite final");
            }
        }
        const Triangle collapsed{Vec3{},Vec3{},Vec3{0,1,0}},collinear{Vec3{},Vec3{1,2,3},Vec3{2,4,6}};
        Reject(collapsed,plane,{0,0,1},"source collapsed");Reject(plane,collapsed,{0,0,1},"final collapsed");
        Reject(collinear,plane,{0,0,1},"source collinear");Reject(plane,collinear,{0,0,1},"final collinear");
        auto alias=plane;Need(TryTransportSurfaceNormal3D(alias,shear,{1,2,3},alias[0]),"aliased output");Near(alias[0],reference,"aliased output remains transactional");
        std::cout<<"SURFACE_NORMAL_TRANSPORT_PASS checks="<<checks<<" gpu=false\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
