Texture2D<float4> atmosphereTexture : register(t0);
Texture2D<float> sceneDepthTexture : register(t1);
#define AEVRYND_WORLD_RAY_DETERMINISTIC_NORMALIZE 1
#include "WorldRayReconstruction.hlsli"
#if AEVRYND_ATMOSPHERE_MSAA
Texture2DMS<float4,4> sceneColorTexture : register(t2);
#else
Texture2D<float4> sceneColorTexture : register(t2);
#endif
cbuffer CompositeConstants : register(b0)
{
    row_major float4x4 inverseViewProjection;
    float2 outputExtent;
    float2 viewportOrigin;
    float2 viewportExtent;
    float2 cameraPositionXY;
    float cameraPositionZ;
    float cloudBaseAltitude;
    float cloudThickness;
    uint projectionOrthographic;
    float4 compositePadding;
};

float3 CompositeCameraPosition()
{
    return float3(cameraPositionXY,cameraPositionZ);
}

precise float CompositeDeterministicReciprocal(float value)
{
    return AevryndDeterministicReciprocalSigned(value);
}

precise float2 CompositeViewportUv(uint2 pixel)
{
    precise const float2 inverseExtent=float2(
        CompositeDeterministicReciprocal(max(viewportExtent.x,1.0)),
        CompositeDeterministicReciprocal(max(viewportExtent.y,1.0)));
    precise const float2 pixelCenter=float2(pixel)+float2(.5,.5);
    precise const float2 localPosition=pixelCenter-viewportOrigin;
    precise float2 localUv;
    localUv.x=localPosition.x*inverseExtent.x;
    localUv.y=localPosition.y*inverseExtent.y;
    return saturate(localUv);
}

precise float2 CompositeAtmosphereTexelUv(int2 pixel,uint width,uint height)
{
    precise const float2 inverseExtent=float2(
        CompositeDeterministicReciprocal(float(width)),
        CompositeDeterministicReciprocal(float(height)));
    precise const float2 pixelCenter=float2(pixel)+float2(.5,.5);
    precise float2 result;
    result.x=pixelCenter.x*inverseExtent.x;
    result.y=pixelCenter.y*inverseExtent.y;
    return result;
}

struct FullscreenVertex
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

FullscreenVertex VolumetricAtmosphereCompositeVertexMain(uint id : SV_VertexID)
{
    FullscreenVertex output;
    const float2 position=float2((id<<1)&2,id&2);
    output.uv=float2(position.x,1.0-position.y);
    output.position=float4(position*float2(2.0,-2.0)+float2(-1.0,1.0),0.0,1.0);
    return output;
}

precise float SceneAtmosphereWeight(uint2 pixel,precise float2 localUv)
{
    precise const float depth=sceneDepthTexture.Load(int3(pixel,0));
    if(depth>=0.999999)
        return 1.0;
    precise float3 nearWorld,surfaceWorld;
    if(!AevryndReconstructWorldPosition(localUv,0.0,inverseViewProjection,nearWorld)||
       !AevryndReconstructWorldPosition(localUv,depth,inverseViewProjection,surfaceWorld))
        return 0.0;
    precise const float3 rayOrigin=projectionOrthographic!=0?nearWorld:CompositeCameraPosition();
    precise const float3 rayVector=surfaceWorld-rayOrigin;
    precise const float3 ray=AevryndDeterministicNormalize(rayVector);
    if(abs(ray.y)<=1e-6)
        return 0.0;
    precise const float inverseRayY=CompositeDeterministicReciprocal(ray.y);
    precise const float slab0Height=cloudBaseAltitude-rayOrigin.y;
    precise const float slab1Height=(cloudBaseAltitude+cloudThickness)-rayOrigin.y;
    precise const float slab0=slab0Height*inverseRayY;
    precise const float slab1=slab1Height*inverseRayY;
    precise const float layerEntry=max(0.0,min(slab0,slab1));
    precise const float layerExit=max(slab0,slab1);
    precise const float3 surfaceVector=surfaceWorld-rayOrigin;
    precise const float sceneDistanceX=surfaceVector.x*ray.x;
    precise const float sceneDistanceY=surfaceVector.y*ray.y;
    precise const float sceneDistanceZ=surfaceVector.z*ray.z;
    precise const float sceneDistanceXY=sceneDistanceX+sceneDistanceY;
    precise const float sceneDistance=sceneDistanceXY+sceneDistanceZ;
    return layerExit>layerEntry+1e-3&&sceneDistance>=layerEntry?1.0:0.0;
}

precise float4 SampleDepthAwareAtmosphere(precise float2 localUv,precise float centerDepthWeight)
{
    uint atmosphereWidth,atmosphereHeight;
    atmosphereTexture.GetDimensions(atmosphereWidth,atmosphereHeight);
    const precise float2 atmosphereExtent=float2(atmosphereWidth,atmosphereHeight);
    precise float2 texel;
    texel.x=localUv.x*atmosphereExtent.x-.5;
    texel.y=localUv.y*atmosphereExtent.y-.5;
    const int2 base=int2(floor(texel));
    const precise float2 fraction=frac(texel);
    precise float4 accumulated=0;
    precise float accumulatedWeight=0;
    [unroll] for(int offsetY=0;offsetY<2;++offsetY)
    [unroll] for(int offsetX=0;offsetX<2;++offsetX)
    {
        const int2 candidate=clamp(base+int2(offsetX,offsetY),int2(0,0),int2(atmosphereWidth-1,atmosphereHeight-1));
        const precise float2 candidateUv=CompositeAtmosphereTexelUv(candidate,atmosphereWidth,atmosphereHeight);
        const uint2 candidatePixel=min(uint2(viewportOrigin+candidateUv*viewportExtent),uint2(outputExtent)-1);
        const precise float candidateDepthWeight=SceneAtmosphereWeight(candidatePixel,candidateUv);
        const precise float horizontalWeight=offsetX==0?1.0-fraction.x:fraction.x;
        const precise float verticalWeight=offsetY==0?1.0-fraction.y:fraction.y;
        const precise float spatialWeight=horizontalWeight*verticalWeight;
        // 只在同一深度分類內做雙線性混合，避免半解析雲層跨越近景輪廓。
        const precise float depthDifference=abs(candidateDepthWeight-centerDepthWeight);
        const precise float bilateralMask=depthDifference<.5?1.0:0.0;
        const precise float bilateralWeight=spatialWeight*bilateralMask;
        const precise float4 atmosphereSample=atmosphereTexture.Load(int3(candidate,0));
        precise float4 weightedSample;
        weightedSample.r=atmosphereSample.r*bilateralWeight;
        weightedSample.g=atmosphereSample.g*bilateralWeight;
        weightedSample.b=atmosphereSample.b*bilateralWeight;
        weightedSample.a=atmosphereSample.a*bilateralWeight;
        accumulated.r=accumulated.r+weightedSample.r;
        accumulated.g=accumulated.g+weightedSample.g;
        accumulated.b=accumulated.b+weightedSample.b;
        accumulated.a=accumulated.a+weightedSample.a;
        accumulatedWeight=accumulatedWeight+bilateralWeight;
    }
    if(accumulatedWeight>1e-6)
    {
        const precise float inverseAccumulatedWeight=CompositeDeterministicReciprocal(accumulatedWeight);
        precise float4 resolvedAtmosphere;
        resolvedAtmosphere.r=accumulated.r*inverseAccumulatedWeight;
        resolvedAtmosphere.g=accumulated.g*inverseAccumulatedWeight;
        resolvedAtmosphere.b=accumulated.b*inverseAccumulatedWeight;
        resolvedAtmosphere.a=accumulated.a*inverseAccumulatedWeight;
        return resolvedAtmosphere;
    }
    const uint2 fallback=min(uint2(localUv*atmosphereExtent),uint2(atmosphereWidth-1,atmosphereHeight-1));
    return atmosphereTexture.Load(int3(fallback,0));
}

float4 CompositeAtmosphere(FullscreenVertex input)
{
    const uint2 pixel=min(uint2(input.position.xy),uint2(outputExtent)-1);
    const precise float2 localUv=CompositeViewportUv(pixel);
    const float depthWeight=SceneAtmosphereWeight(pixel,localUv);
    const float4 atmosphere=SampleDepthAwareAtmosphere(localUv,depthWeight);
    const float alpha=saturate(atmosphere.a*depthWeight);
    return float4(max(0.0,atmosphere.rgb)*depthWeight,alpha);
}

precise float4 CompositeSceneColor(uint2 pixel,uint sampleIndex,bool preserveSemanticAlpha)
{
#if AEVRYND_ATMOSPHERE_MSAA
    precise const float4 scene=sceneColorTexture.Load(pixel,sampleIndex);
#else
    precise const float4 scene=sceneColorTexture.Load(int3(pixel,0));
#endif
    const precise float2 localUv=CompositeViewportUv(pixel);
    const precise float depthWeight=SceneAtmosphereWeight(pixel,localUv);
    const precise float4 atmosphere=SampleDepthAwareAtmosphere(localUv,depthWeight);
    const precise float alpha=saturate(atmosphere.a*depthWeight);
    const precise float inverseAlpha=1.0-alpha;
    precise float3 atmosphereRgb;
    atmosphereRgb.r=max(0.0,atmosphere.r)*depthWeight;
    atmosphereRgb.g=max(0.0,atmosphere.g)*depthWeight;
    atmosphereRgb.b=max(0.0,atmosphere.b)*depthWeight;
    precise float3 scaledScene;
    scaledScene.r=scene.r*inverseAlpha;
    scaledScene.g=scene.g*inverseAlpha;
    scaledScene.b=scene.b*inverseAlpha;
    precise float3 rgb;
    rgb.r=atmosphereRgb.r+scaledScene.r;
    rgb.g=atmosphereRgb.g+scaledScene.g;
    rgb.b=atmosphereRgb.b+scaledScene.b;
    precise const float composedAlpha=alpha+scene.a*inverseAlpha;
    const precise float outputAlpha=preserveSemanticAlpha?scene.a:composedAlpha;
    return float4(rgb,outputAlpha);
}

precise float4 VolumetricAtmosphereCompositePixelMain(FullscreenVertex input,uint sampleIndex : SV_SampleIndex) : SV_Target
{
    const uint2 pixel=min(uint2(input.position.xy),uint2(outputExtent)-1);
    precise const float4 output=CompositeSceneColor(pixel,sampleIndex,false);
    return output;
}

precise float4 VolumetricAtmosphereCompositeBloomSemanticPixelMain(FullscreenVertex input,uint sampleIndex : SV_SampleIndex) : SV_Target
{
    const uint2 pixel=min(uint2(input.position.xy),uint2(outputExtent)-1);
    precise const float4 output=CompositeSceneColor(pixel,sampleIndex,true);
    return output;
}

// 僅供負向回歸測試執行舊 nearest upscale；深度排除與 production 完全相同。
float4 VolumetricAtmosphereCompositeNearestPixelMain(FullscreenVertex input) : SV_Target
{
    const uint2 pixel=min(uint2(input.position.xy),uint2(outputExtent)-1);
    uint atmosphereWidth,atmosphereHeight;
    atmosphereTexture.GetDimensions(atmosphereWidth,atmosphereHeight);
    const precise float2 localUv=CompositeViewportUv(pixel);
    const uint2 atmospherePixel=min(uint2(localUv*float2(atmosphereWidth,atmosphereHeight)),uint2(atmosphereWidth-1,atmosphereHeight-1));
    const float4 atmosphere=atmosphereTexture.Load(int3(atmospherePixel,0));
    const float depthWeight=SceneAtmosphereWeight(pixel,localUv);
    return float4(max(0.0,atmosphere.rgb)*depthWeight,saturate(atmosphere.a*depthWeight));
}

// 僅供負向回歸測試執行舊硬體深度權重，證明近景會被錯誤洗白。
float4 VolumetricAtmosphereCompositeLegacyDepthPixelMain(FullscreenVertex input) : SV_Target
{
    const uint2 pixel=min(uint2(input.position.xy),uint2(outputExtent)-1);
    uint atmosphereWidth,atmosphereHeight;
    atmosphereTexture.GetDimensions(atmosphereWidth,atmosphereHeight);
    const precise float2 localUv=CompositeViewportUv(pixel);
    const uint2 atmospherePixel=min(uint2(localUv*float2(atmosphereWidth,atmosphereHeight)),uint2(atmosphereWidth-1,atmosphereHeight-1));
    const float4 atmosphere=atmosphereTexture.Load(int3(atmospherePixel,0));
    const float hardwareDepth=sceneDepthTexture.Load(int3(pixel,0));
    const float legacyWeight=lerp(.06,.22,saturate(hardwareDepth));
    return float4(max(0.0,atmosphere.rgb)*legacyWeight,saturate(atmosphere.a*legacyWeight));
}
