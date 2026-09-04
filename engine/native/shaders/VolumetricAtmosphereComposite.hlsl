Texture2D<float4> atmosphereTexture : register(t0);
Texture2D<float> sceneDepthTexture : register(t1);
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

float SceneAtmosphereWeight(uint2 pixel,float2 localUv)
{
    const float depth=sceneDepthTexture.Load(int3(pixel,0));
    if(depth>=0.999999)
        return 1.0;
    const float2 ndc=float2(localUv.x*2.0-1.0,1.0-localUv.y*2.0);
    float4 nearWorld=mul(inverseViewProjection,float4(ndc,0.0,1.0));
    float4 surfaceWorld=mul(inverseViewProjection,float4(ndc,depth,1.0));
    if(abs(nearWorld.w)<=1e-6||abs(surfaceWorld.w)<=1e-6)
        return 0.0;
    nearWorld.xyz/=nearWorld.w;
    surfaceWorld.xyz/=surfaceWorld.w;
    const float3 rayOrigin=projectionOrthographic!=0?nearWorld.xyz:CompositeCameraPosition();
    const float3 ray=normalize(surfaceWorld.xyz-rayOrigin);
    if(abs(ray.y)<=1e-6)
        return 0.0;
    const float inverseRayY=rcp(ray.y);
    const float slab0=(cloudBaseAltitude-rayOrigin.y)*inverseRayY;
    const float slab1=(cloudBaseAltitude+cloudThickness-rayOrigin.y)*inverseRayY;
    const float layerEntry=max(0.0,min(slab0,slab1));
    const float layerExit=max(slab0,slab1);
    const float sceneDistance=dot(surfaceWorld.xyz-rayOrigin,ray);
    return layerExit>layerEntry+1e-3&&sceneDistance>=layerEntry?1.0:0.0;
}

float4 SampleDepthAwareAtmosphere(float2 localUv,float centerDepthWeight)
{
    uint atmosphereWidth,atmosphereHeight;
    atmosphereTexture.GetDimensions(atmosphereWidth,atmosphereHeight);
    const float2 atmosphereExtent=float2(atmosphereWidth,atmosphereHeight);
    const float2 texel=localUv*atmosphereExtent-.5;
    const int2 base=int2(floor(texel));
    const float2 fraction=frac(texel);
    float4 accumulated=0;
    float accumulatedWeight=0;
    [unroll] for(int offsetY=0;offsetY<2;++offsetY)
    [unroll] for(int offsetX=0;offsetX<2;++offsetX)
    {
        const int2 candidate=clamp(base+int2(offsetX,offsetY),int2(0,0),int2(atmosphereWidth-1,atmosphereHeight-1));
        const float2 candidateUv=(float2(candidate)+.5)/atmosphereExtent;
        const uint2 candidatePixel=min(uint2(viewportOrigin+candidateUv*viewportExtent),uint2(outputExtent)-1);
        const float candidateDepthWeight=SceneAtmosphereWeight(candidatePixel,candidateUv);
        const float2 axisWeight=offsetX==0?float2(1.0-fraction.x,0):float2(fraction.x,0);
        const float spatialWeight=axisWeight.x*(offsetY==0?1.0-fraction.y:fraction.y);
        // 只在同一深度分類內做雙線性混合，避免半解析雲層跨越近景輪廓。
        const float bilateralWeight=spatialWeight*(abs(candidateDepthWeight-centerDepthWeight)<.5?1.0:0.0);
        accumulated+=atmosphereTexture.Load(int3(candidate,0))*bilateralWeight;
        accumulatedWeight+=bilateralWeight;
    }
    if(accumulatedWeight>1e-6)
        return accumulated/accumulatedWeight;
    const uint2 fallback=min(uint2(localUv*atmosphereExtent),uint2(atmosphereWidth-1,atmosphereHeight-1));
    return atmosphereTexture.Load(int3(fallback,0));
}

float4 VolumetricAtmosphereCompositePixelMain(FullscreenVertex input) : SV_Target
{
    const uint2 pixel=min(uint2(input.position.xy),uint2(outputExtent)-1);
    const float2 localUv=saturate((float2(pixel)+.5-viewportOrigin)/max(viewportExtent,1.0));
    float depthWeight=SceneAtmosphereWeight(pixel,localUv);
    const float4 atmosphere=SampleDepthAwareAtmosphere(localUv,depthWeight);
    // MSAA HDR 邊界使用單樣本深度時，保守檢查鄰近點可避免 resolve 後在近景輪廓形成亮邊。
    [unroll] for(int offsetY=-1;offsetY<=1;++offsetY)
    [unroll] for(int offsetX=-1;offsetX<=1;++offsetX)
    {
        const int2 neighborInt=clamp(int2(pixel)+int2(offsetX,offsetY),int2(viewportOrigin),int2(viewportOrigin+viewportExtent-1.0));
        const uint2 neighbor=uint2(neighborInt);
        const float2 neighborUv=saturate((float2(neighbor)+.5-viewportOrigin)/max(viewportExtent,1.0));
        depthWeight=min(depthWeight,SceneAtmosphereWeight(neighbor,neighborUv));
    }
    const float alpha=saturate(atmosphere.a*depthWeight);
    return float4(max(0.0,atmosphere.rgb)*depthWeight,alpha);
}

float4 VolumetricAtmosphereCompositeBloomSemanticPixelMain(FullscreenVertex input) : SV_Target
{
    // A-only PSO 仍需此 alpha 供 RGB 的 INV_SRC_ALPHA；alpha channel 本身由 ZERO/ONE 保留目的語意。
    return VolumetricAtmosphereCompositePixelMain(input);
}

// 僅供負向回歸測試執行舊 nearest upscale；深度排除與 production 完全相同。
float4 VolumetricAtmosphereCompositeNearestPixelMain(FullscreenVertex input) : SV_Target
{
    const uint2 pixel=min(uint2(input.position.xy),uint2(outputExtent)-1);
    uint atmosphereWidth,atmosphereHeight;
    atmosphereTexture.GetDimensions(atmosphereWidth,atmosphereHeight);
    const float2 localUv=saturate((float2(pixel)+.5-viewportOrigin)/max(viewportExtent,1.0));
    const uint2 atmospherePixel=min(uint2(localUv*float2(atmosphereWidth,atmosphereHeight)),uint2(atmosphereWidth-1,atmosphereHeight-1));
    const float4 atmosphere=atmosphereTexture.Load(int3(atmospherePixel,0));
    float depthWeight=SceneAtmosphereWeight(pixel,localUv);
    [unroll] for(int offsetY=-1;offsetY<=1;++offsetY)
    [unroll] for(int offsetX=-1;offsetX<=1;++offsetX)
    {
        const int2 neighborInt=clamp(int2(pixel)+int2(offsetX,offsetY),int2(viewportOrigin),int2(viewportOrigin+viewportExtent-1.0));
        const uint2 neighbor=uint2(neighborInt);
        const float2 neighborUv=saturate((float2(neighbor)+.5-viewportOrigin)/max(viewportExtent,1.0));
        depthWeight=min(depthWeight,SceneAtmosphereWeight(neighbor,neighborUv));
    }
    return float4(max(0.0,atmosphere.rgb)*depthWeight,saturate(atmosphere.a*depthWeight));
}

// 僅供負向回歸測試執行舊硬體深度權重，證明近景會被錯誤洗白。
float4 VolumetricAtmosphereCompositeLegacyDepthPixelMain(FullscreenVertex input) : SV_Target
{
    const uint2 pixel=min(uint2(input.position.xy),uint2(outputExtent)-1);
    uint atmosphereWidth,atmosphereHeight;
    atmosphereTexture.GetDimensions(atmosphereWidth,atmosphereHeight);
    const float2 localUv=saturate((float2(pixel)+.5-viewportOrigin)/max(viewportExtent,1.0));
    const uint2 atmospherePixel=min(uint2(localUv*float2(atmosphereWidth,atmosphereHeight)),uint2(atmosphereWidth-1,atmosphereHeight-1));
    const float4 atmosphere=atmosphereTexture.Load(int3(atmospherePixel,0));
    const float hardwareDepth=sceneDepthTexture.Load(int3(pixel,0));
    const float legacyWeight=lerp(.06,.22,saturate(hardwareDepth));
    return float4(max(0.0,atmosphere.rgb)*legacyWeight,saturate(atmosphere.a*legacyWeight));
}
