cbuffer SceneConstants : register(b0) { float4 sceneValues[44]; }
#define AEVRYND_WORLD_RAY_DETERMINISTIC_NORMALIZE 1
#include "WorldRayReconstruction.hlsli"

struct SkyVertexOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
SkyVertexOutput BaseSkyVertexMain(uint vertexId : SV_VertexID) { SkyVertexOutput o;o.uv=float2((vertexId<<1)&2,vertexId&2);o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o; }
float4 BaseSkyPixelMain(SkyVertexOutput input) : SV_TARGET {
    const row_major float4x4 inverseViewProjection=float4x4(sceneValues[16],sceneValues[17],sceneValues[18],sceneValues[19]);
    const bool orthographic=dot(abs(sceneValues[29].xyz),1.0.xxx)<1e-5&&abs(sceneValues[29].w-1.0)<1e-5;
    // SV_Position 是固定像素中心；避免全螢幕三角形 TEXCOORD 插值在獨立程序間
    // 偶發落到相鄰 half-float 邊界。sceneValues[15].zw 是 full-frame inverse size，
    // sceneValues[20] 是 viewport origin/extent 的正規化座標。
    precise const float2 screenUv=input.position.xy*sceneValues[15].zw;
    precise const float2 viewportUv=(screenUv-sceneValues[20].xy)/max(sceneValues[20].zw,float2(1e-6,1e-6));
    float3 rayOrigin,ray;
    if(!AevryndReconstructWorldRay(viewportUv,inverseViewProjection,sceneValues[7].xyz,orthographic,rayOrigin,ray))return float4(0,0,0,1);
    float horizonBlend=pow(saturate(1-abs(ray.y)),sceneValues[40].a);
    float3 sky=lerp(ray.y>=0?sceneValues[39].rgb:sceneValues[41].rgb,sceneValues[40].rgb,horizonBlend)*sceneValues[39].a;
    precise const float3 sunDirection=AevryndDeterministicNormalize(-sceneValues[4].xyz);
    float sunMask=smoothstep(cos(sceneValues[41].a*1.8),cos(sceneValues[41].a),dot(ray,sunDirection));
    sky+=sceneValues[43].rgb*sceneValues[43].a*sunMask;
#if defined(AEVRYND_BASE_SKY_LEGACY_BLUE)
    // 測試突變：模擬舊版固定藍色解析天空，必須被 GPU gate 拒絕。
    sky=float3(.035,.22,.78);
#endif
    return float4(max(sky,0),1);
}
