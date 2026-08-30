Texture2D<float4> atmosphereTexture : register(t0);
Texture2D<float> sceneDepthTexture : register(t1);
SamplerState linearClampSampler : register(s0);

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

float4 VolumetricAtmosphereCompositePixelMain(FullscreenVertex input) : SV_Target
{
    const float4 atmosphere=atmosphereTexture.SampleLevel(linearClampSampler,input.uv,0);
    const float depth=sceneDepthTexture.SampleLevel(linearClampSampler,input.uv,0);
    // 天空完整接收體積層，幾何只保留少量遠距空氣透視，避免雲層蓋過角色。
    const float depthWeight=depth>=0.9999?1.0:lerp(0.06,0.22,saturate(depth));
    const float alpha=saturate(atmosphere.a*depthWeight);
    return float4(max(0.0,atmosphere.rgb)*depthWeight,alpha);
}
