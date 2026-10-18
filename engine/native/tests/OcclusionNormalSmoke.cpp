#include "../shaders/OcclusionNormal.hlsli"
#include <Aevrynd/Camera.h>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace Aevrynd;
unsigned checks = 0;
void Need(bool ok,const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
bool Finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
void UnitNear(Vec3 actual,Vec3 expected,const char* message) {
    Need(Finite(actual)&&std::abs(Length(actual)-1.f)<2e-5f&&Dot(actual,expected)>.9999f,message);
}

// 獨立幾何參考：從正式相機投影矩陣建立射線，與已知世界平面求交。
// 不以候選 cross 函式生成期望法線；交點另經正式投影核對螢幕位置。
Vec3 PlanePoint(const Camera3D& camera,Vec3 point,Vec3 planeNormal,Vec2 uv) {
    constexpr float aspect = 16.f/9.f;
    const auto projection = CreateProjectionMatrix(camera.projection,aspect);
    const auto view = CreateViewMatrix(camera);
    const Vec3 right{view[0][0],view[0][1],view[0][2]};
    const Vec3 up{view[1][0],view[1][1],view[1][2]};
    const Vec3 forward{view[2][0],view[2][1],view[2][2]};
    const Vec3 ray = forward+right*((uv.x*2.f-1.f)/projection[0][0])+up*((1.f-uv.y*2.f)/projection[1][1]);
    const float denominator = Dot(planeNormal,ray);
    Need(std::abs(denominator)>.01f,"ray parallel to fixture plane");
    const float distance = Dot(planeNormal,point-camera.position)/denominator;
    Need(distance>0.f,"fixture intersection behind camera");
    const Vec3 result = camera.position+ray*distance;
    const Vec4 clip = CreateViewProjectionMatrix(camera,aspect)*Vec4{result.x,result.y,result.z,1.f};
    Need(clip.w>0.f&&clip.z/clip.w>=0.f&&clip.z/clip.w<=1.f,"fixture clip depth invalid");
    Need(std::abs((clip.x/clip.w*.5f+.5f)-uv.x)<2e-5f&&
         std::abs((.5f-clip.y/clip.w*.5f)-uv.y)<2e-5f,"ray plane projection roundtrip");
    return result;
}

void PlaneCase(Camera3D camera,Vec3 point,Vec3 expected) {
    expected = Normalize(expected);
    const Vec2 uv{.5f,.5f};
    const Vec3 center = PlanePoint(camera,point,expected,uv);
    const Vec3 left = PlanePoint(camera,point,expected,{uv.x-1.f/1280.f,uv.y});
    const Vec3 right = PlanePoint(camera,point,expected,{uv.x+1.f/1280.f,uv.y});
    const Vec3 top = PlanePoint(camera,point,expected,{uv.x,uv.y-1.f/720.f});
    const Vec3 bottom = PlanePoint(camera,point,expected,{uv.x,uv.y+1.f/720.f});
    Need(Dot(expected,camera.position-center)>0.f,"fixture must show front face");
    for (const Vec3 h : std::array{center-left,right-center}) {
        for (const Vec3 v : std::array{center-top,bottom-center}) {
            const Vec3 n = ReconstructOcclusionSurfaceNormal(h,v,camera.position-center);
            UnitNear(n,expected,"surface normal orientation mismatch");
            // 舊 HLSL normalize 沒有 Math.h 的 0.0001 截斷；像素差分叉積本來就很小。
            const Vec3 oldCross = Cross(v,h);
            const float oldLength = Length(oldCross);
            Need(std::isfinite(oldLength)&&oldLength>1e-12f,"legacy fixture differential degenerate");
            const Vec3 oldNormal = oldCross*(1.f/oldLength);
            Need(Dot(oldNormal,expected)<-.9999f,"legacy reversed winding negative control");
            Need(std::abs(Dot(n,Normalize(h)))<2e-4f&&std::abs(Dot(n,Normalize(v)))<2e-4f,
                 "coplanar surface must not self occlude");
            // 對應正式 SSAO 的半球測試：凸起有效，背面樣本無效。
            const float front = Clamp(Dot(n,expected)-.035f,0.f,1.f);
            const float back = Clamp(Dot(n,expected*-1.f)-.035f,0.f,1.f);
            Need(front>.96f&&back==0.f,"occlusion hemisphere reversed");
        }
    }
}
}

int main() {
    try {
        PlaneCase({{0,0,-5},{0,0,0},{0,1,0},{}},{0,0,0},{0,0,-1});
        PlaneCase({{0,3,-5},{0,0,0},{0,1,0},{}},{0,0,0},{0,1,0});
        PlaneCase({{0,3,-5},{0,0,0},{0,1,0},{}},{0,0,0},{.2f,1.f,-.3f});
        PlaneCase({{7,4,9},{1,0,2},{.3f,1.f,.2f},{}},{1,0,2},{0,1,0});

        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const Vec3 toward{2,3,-4};
        for (const Vec3 invalid : std::array{Vec3{},Vec3{nan,0,0},Vec3{infinity,0,0},Vec3{1e-20f,0,0}}) {
            UnitNear(ReconstructOcclusionSurfaceNormal(invalid,{0,-1,0},toward),Normalize(toward),"horizontal fallback");
            UnitNear(ReconstructOcclusionSurfaceNormal({1,0,0},invalid,toward),Normalize(toward),"vertical fallback");
            UnitNear(ReconstructOcclusionSurfaceNormal({}, {},invalid),{0,0,-1},"fixed finite fallback");
        }
        UnitNear(ReconstructOcclusionSurfaceNormal({1,0,0},{2,0,0},toward),Normalize(toward),"parallel fallback");
        UnitNear(ReconstructOcclusionSurfaceNormal({1,0,0},{1,1e-8f,0},toward),Normalize(toward),"near parallel fallback");
        UnitNear(ReconstructOcclusionSurfaceNormal({1e30f,0,0},{0,-1e30f,0},{}),{0,0,-1},"large finite differentials");
        UnitNear(ReconstructOcclusionSurfaceNormal({}, {},{0,1e30f,0}),{0,1,0},"large finite fallback");
        UnitNear(ReconstructOcclusionSurfaceNormal({1,0,0},{0,-1,0},{nan,0,0}),{0,0,-1},"valid surface ignores invalid fallback");
        UnitNear(ReconstructOcclusionSurfaceNormal({1,0,0},{0,-1,0},{0,0,1}),{0,0,-1},"must not hide differential winding with camera flip");
        std::cout<<"OCCLUSION_NORMAL_OK checks="<<checks<<" camera_planes=4 neighbor_pairs=16 legacy_negative=16\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"OCCLUSION_NORMAL_FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';
        return 1;
    }
}
