#ifndef AEVRYND_OCCLUSION_NORMAL_INCLUDED
#define AEVRYND_OCCLUSION_NORMAL_INCLUDED

#ifdef __cplusplus
#include <Aevrynd/Math.h>
namespace Aevrynd {
using OcclusionNormalVector = Vec3;
inline bool OcclusionNormalFinite(OcclusionNormalVector v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
inline float OcclusionNormalAbs(float v) { return std::abs(v); }
inline float OcclusionNormalRoot(float v) { return std::sqrt(v); }
inline float OcclusionNormalDot(OcclusionNormalVector a,OcclusionNormalVector b) { return Dot(a,b); }
inline OcclusionNormalVector OcclusionNormalCross(OcclusionNormalVector a,OcclusionNormalVector b) { return Cross(a,b); }
inline OcclusionNormalVector OcclusionNormalMake(float x,float y,float z) { return {x,y,z}; }
#else
#define OcclusionNormalVector float3
bool OcclusionNormalFinite(float3 v) { return all(isfinite(v)); }
float OcclusionNormalAbs(float v) { return abs(v); }
float OcclusionNormalRoot(float v) { return sqrt(v); }
float OcclusionNormalDot(float3 a,float3 b) { return dot(a,b); }
float3 OcclusionNormalCross(float3 a,float3 b) { return cross(a,b); }
float3 OcclusionNormalMake(float x,float y,float z) { return float3(x,y,z); }
#endif

// 先依最大分量縮放，避免有限的大座標在長度平方時溢位。
// 極小向量與非有限向量視為不可用，不交給 normalize 產生 NaN。
inline OcclusionNormalVector OcclusionNormalUnitOrZero(OcclusionNormalVector v)
{
    OcclusionNormalVector result = OcclusionNormalMake(0.0f,0.0f,0.0f);
    if (OcclusionNormalFinite(v)) {
        const float ax = OcclusionNormalAbs(v.x), ay = OcclusionNormalAbs(v.y), az = OcclusionNormalAbs(v.z);
        const float xy = ax > ay ? ax : ay;
        const float scale = xy > az ? xy : az;
        if (scale > 1e-12f) {
            v = v * (1.0f / scale);
            result = v * (1.0f / OcclusionNormalRoot(OcclusionNormalDot(v,v)));
        }
    }
    return result;
}

// 差分契約：horizontal 朝螢幕右、vertical 朝螢幕下。
// 引擎左手相機的可見面法線為 cross(horizontal,vertical)。
// 不以朝鏡頭翻面掩蓋錯誤差分；towardCamera 僅供退化時回退。
inline OcclusionNormalVector ReconstructOcclusionSurfaceNormal(
    OcclusionNormalVector horizontal, OcclusionNormalVector vertical, OcclusionNormalVector towardCamera)
{
    const OcclusionNormalVector h = OcclusionNormalUnitOrZero(horizontal);
    const OcclusionNormalVector v = OcclusionNormalUnitOrZero(vertical);
    const OcclusionNormalVector crossed = OcclusionNormalCross(h,v);
    // 明確初始化並統一出口，避免 FXC 對內聯多重 return 的未初始化誤判。
    OcclusionNormalVector result = OcclusionNormalMake(0.0f,0.0f,-1.0f);
    const OcclusionNormalVector fallback = OcclusionNormalUnitOrZero(towardCamera);
    if (OcclusionNormalDot(fallback,fallback) > 0.0f) result = fallback;
    if (OcclusionNormalDot(crossed,crossed) > 1e-12f)
        result = OcclusionNormalUnitOrZero(crossed);
    return result;
}

#ifdef __cplusplus
}
#else
#undef OcclusionNormalVector
#endif
#endif
