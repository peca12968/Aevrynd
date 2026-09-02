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
    float padding;
    uint historyValid;
    uint cameraCut;
    uint resized;
    uint qualityChanged;
    uint2 outputSize;
    float2 inverseOutputSize;
};
[numthreads(8,8,1)]
void ContactShadowTemporalMain(uint3 id : SV_DispatchThreadID)
{
    if(any(id.xy>=outputSize))return;
    const float2 current=currentContact.Load(int3(id.xy,0));
    const float2 motion=contactMotion.Load(int3(id.xy,0));
    const bool motionFinite=all(isfinite(motion));
    const float2 previousPixel=motionFinite?float2(id.xy)-motion:float2(-1.0,-1.0);
    const bool validFlags=historyValid!=0&&cameraCut==0&&resized==0&&qualityChanged==0;
    const bool validUv=all(previousPixel>=0.0)&&all(previousPixel<float2(outputSize));
    const uint2 previousIndex=uint2(clamp(previousPixel,0.0,float2(outputSize)-1.0));
    const float2 previous=previousContact.Load(int3(previousIndex,0));
    const bool depthValid=abs(current.y-previous.y)<=max(depthThreshold,0.0);
    const bool motionValid=length(motion)<=max(motionThresholdPixels,0.0);
    const bool accept=validFlags&&validUv&&depthValid&&motionValid&&motionFinite&&all(isfinite(float4(current,previous)));
    const float weight=accept?clamp(historyWeight,0.0,0.98):0.0;
    const float currentVisibility=isfinite(current.x)?saturate(current.x):1.0;
    const float previousVisibility=isfinite(previous.x)?saturate(previous.x):1.0;
    const float currentDepth=isfinite(current.y)?current.y:1.0;
    temporalContact[id.xy]=float2(lerp(currentVisibility,previousVisibility,weight),currentDepth);
}
