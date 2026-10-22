#ifdef __cplusplus
#include <Aevrynd/Math.h>
#include <cmath>
namespace Aevrynd::OcclusionWindowShader {
using float2=Vec2;using float4=Vec4;
inline float saturate(float v){return Clamp(v,0.f,1.f);}
using std::sqrt;
#define AEVRYND_WINDOW_INLINE inline
#else
#define AEVRYND_WINDOW_INLINE
#endif
// 關閉時完整保留舊語意；啟用時只有主體前方的橢圓內使用原物件淡出值。
AEVRYND_WINDOW_INLINE float ScreenWindowOcclusionOpacity(float2 uv,float depth,float opacity,float4 ellipse,float4 settings){
    // 靜態環境probe不繼承玩家遮擋孔洞；僅renderer的明示probe分支設定w。
    if(settings.w>0.5f)return 1.0f;
    if(opacity>=1.0f||settings.x<0.5f)return opacity;
    if(depth>settings.y)return 1.0f;
    const float x=(uv.x-ellipse.x)/ellipse.z,y=(uv.y-ellipse.y)/ellipse.w;
    const float distance=sqrt(x*x+y*y);
    const float t=saturate((distance-settings.z)/(1.0f-settings.z));
    const float edge=t*t*(3.0f-2.0f*t);
    return opacity+(1.0f-opacity)*edge;
}
#undef AEVRYND_WINDOW_INLINE
#ifdef __cplusplus
}
#endif
