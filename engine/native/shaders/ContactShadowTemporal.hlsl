// Production-ready temporal ABI: current visibility/depth, previous visibility/depth and motion.
Texture2D<float2> currentContact : register(t0);
Texture2D<float2> previousContact : register(t1);
Texture2D<float2> contactMotion : register(t2);
RWTexture2D<float2> temporalContact : register(u0);
cbuffer ContactShadowTemporalConstants : register(b0)
{
    float historyWeight;
    float depthThreshold;
    float motionThresholdPixels;
    float darkeningHistoryWeight;
    uint historyValid;
    uint cameraCut;
    uint resized;
    uint qualityChanged;
    uint2 outputSize;
    float2 inverseOutputSize;
    float2 currentJitterPixels;
    float2 previousJitterPixels;
};
[numthreads(8,8,1)]
void ContactShadowTemporalMain(uint3 id : SV_DispatchThreadID)
{
    if(any(id.xy>=outputSize))return;
    const float2 current=currentContact.Load(int3(id.xy,0));
    const float currentVisibility=isfinite(current.x)?saturate(current.x):1.0;
    const float currentDepth=isfinite(current.y)?current.y:1.0;
    const bool validFlags=historyValid!=0&&cameraCut==0&&resized==0&&qualityChanged==0;
    // Newly allocated history has no defined texels.  Reject it before any load so
    // an invalid/cut frame cannot propagate undefined NaNs through lerp(..., 0).
    if(!validFlags)
    {
        temporalContact[id.xy]=float2(currentVisibility,currentDepth);
        return;
    }
    uint motionWidth,motionHeight;contactMotion.GetDimensions(motionWidth,motionHeight);
    const bool fullResolutionMotion=motionWidth+1u>=outputSize.x*2u&&motionHeight+1u>=outputSize.y*2u;
    const uint2 motionIndex=min(fullResolutionMotion?id.xy*2u+1u:id.xy,uint2(motionWidth-1u,motionHeight-1u));
    // 速度緩衝是全解析度像素，history 是半解析度，因此位移必須縮半。
    const float2 motion=contactMotion.Load(int3(motionIndex,0))*(fullResolutionMotion?.5:1.0);
    const bool motionFinite=all(isfinite(motion));
    // 速度已包含目前幀 jitter，previous clip 則未 jitter；只補前一幀 jitter。
    const float2 previousPixel=motionFinite?float2(id.xy)-motion+previousJitterPixels*.5:float2(-1.0,-1.0);
    const bool validUv=all(previousPixel>=0.0)&&all(previousPixel<float2(outputSize));
    const float2 clampedPrevious=clamp(previousPixel,0.0,float2(outputSize)-1.0);
    const uint2 previousBase=uint2(floor(clampedPrevious));
    const uint2 previousNext=min(previousBase+1u,outputSize-1u);
    const float2 previousFraction=frac(clampedPrevious);
    const float2 previousTop=lerp(previousContact.Load(int3(previousBase,0)),
                                  previousContact.Load(int3(uint2(previousNext.x,previousBase.y),0)),
                                  previousFraction.x);
    const float2 previousBottom=lerp(previousContact.Load(int3(uint2(previousBase.x,previousNext.y),0)),
                                     previousContact.Load(int3(previousNext,0)),
                                     previousFraction.x);
    const float2 previous=lerp(previousTop,previousBottom,previousFraction.y);
    const uint2 previousDepthIndex=uint2(round(clampedPrevious));
    const float previousDepth=previousContact.Load(int3(previousDepthIndex,0)).y;
    const bool depthValid=abs(current.y-previousDepth)<=max(depthThreshold,0.0);
    const bool motionValid=length(motion)<=max(motionThresholdPixels,0.0);
    const bool accept=validUv&&depthValid&&motionValid&&motionFinite&&all(isfinite(float4(current,previous)));
    const float configuredHistoryWeight=accept?clamp(historyWeight,0.0,0.98):0.0;
    float neighborhoodMinimum=currentVisibility;
    float neighborhoodMaximum=currentVisibility;
    [unroll] for(int y=-1;y<=1;++y)
    {
        [unroll] for(int x=-1;x<=1;++x)
        {
            const uint2 neighbor=uint2(clamp(int2(id.xy)+int2(x,y),int2(0,0),int2(outputSize)-1));
            const float value=saturate(currentContact.Load(int3(neighbor,0)).x);
            neighborhoodMinimum=min(neighborhoodMinimum,value);
            neighborhoodMaximum=max(neighborhoodMaximum,value);
        }
    }
    // 以目前幀鄰域裁切歷史，避免移動遮擋留下舊接觸陰影。
    // 小幅擴張鄰域包絡，保留次像素 jitter 的有效歷史，同時由深度拒絕遮擋變化。
    const float clampEpsilon=0.02;
    const float previousVisibility=clamp(isfinite(previous.x)?saturate(previous.x):1.0,
                                         saturate(neighborhoodMinimum-clampEpsilon),
                                         saturate(neighborhoodMaximum+clampEpsilon));
    // 可見度代表透射率；目前值較低表示新遮擋。讓遮擋快速建立，並沿用
    // 空間限縮與深度／移動拒絕，避免殘留過時的暗色拖影。
    const float darkeningWeight=clamp(darkeningHistoryWeight,0.0,configuredHistoryWeight);
    const float effectiveHistoryWeight=currentVisibility<previousVisibility?darkeningWeight:configuredHistoryWeight;
    temporalContact[id.xy]=float2(lerp(currentVisibility,previousVisibility,effectiveHistoryWeight),currentDepth);
}
