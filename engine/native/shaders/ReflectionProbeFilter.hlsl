TextureCube<float4> rawProbe : register(t0);
RWTexture2DArray<float4> filteredProbe : register(u0);
SamplerState linearClampSampler : register(s0);

cbuffer FilterConstants : register(b0)
{
    uint outputResolution;
    uint sampleCount;
    uint filterMode;
    float roughness;
};

float3 FaceDirection(uint face,float2 uv)
{
    const float2 p=uv*2.0-1.0;
    if(face==0)return normalize(float3(1.0,-p.y,-p.x));
    if(face==1)return normalize(float3(-1.0,-p.y,p.x));
    if(face==2)return normalize(float3(p.x,1.0,p.y));
    if(face==3)return normalize(float3(p.x,-1.0,-p.y));
    if(face==4)return normalize(float3(p.x,-p.y,1.0));
    return normalize(float3(-p.x,-p.y,-1.0));
}

float RadicalInverse(uint bits)
{
    bits=(bits<<16)|(bits>>16);
    bits=((bits&0x55555555u)<<1)|((bits&0xAAAAAAAAu)>>1);
    bits=((bits&0x33333333u)<<2)|((bits&0xCCCCCCCCu)>>2);
    bits=((bits&0x0F0F0F0Fu)<<4)|((bits&0xF0F0F0F0u)>>4);
    bits=((bits&0x00FF00FFu)<<8)|((bits&0xFF00FF00u)>>8);
    return float(bits)*2.3283064365386963e-10;
}

float3 TangentSample(float3 normal,float3 localDirection)
{
    const float3 up=abs(normal.z)<0.999?float3(0,0,1):float3(1,0,0);
    const float3 tangent=normalize(cross(up,normal));
    return normalize(tangent*localDirection.x+cross(normal,tangent)*localDirection.y+normal*localDirection.z);
}

[numthreads(8,8,1)]
void ReflectionProbeFilterMain(uint3 id:SV_DispatchThreadID)
{
    if(id.x>=outputResolution||id.y>=outputResolution||id.z>=6)return;
    const float2 uv=(float2(id.xy)+0.5)/float(outputResolution);
    const float3 normal=FaceDirection(id.z,uv);
    if(filterMode==0&&roughness<=0.0001)
    {
        filteredProbe[id]=rawProbe.SampleLevel(linearClampSampler,normal,0);
        return;
    }
    float3 accumulated=0;
    float weight=0;
    const uint count=max(1u,sampleCount);
    [loop]for(uint index=0;index<count;++index)
    {
        const float2 xi=float2((float(index)+0.5)/float(count),RadicalInverse(index));
        float3 direction;
        float sampleWeight;
        if(filterMode==0)
        {
            const float alpha=max(0.002,roughness*roughness);
            const float phi=6.28318530718*xi.x;
            const float cosTheta=sqrt((1.0-xi.y)/(1.0+(alpha*alpha-1.0)*xi.y));
            const float sinTheta=sqrt(saturate(1.0-cosTheta*cosTheta));
            const float3 halfVector=TangentSample(normal,float3(cos(phi)*sinTheta,sin(phi)*sinTheta,cosTheta));
            direction=normalize(reflect(-normal,halfVector));
            sampleWeight=saturate(dot(normal,direction));
        }
        else
        {
            const float phi=6.28318530718*xi.x;
            const float radius=sqrt(xi.y);
            direction=TangentSample(normal,float3(radius*cos(phi),radius*sin(phi),sqrt(1.0-xi.y)));
            sampleWeight=1.0;
        }
        accumulated+=rawProbe.SampleLevel(linearClampSampler,direction,0).rgb*sampleWeight;
        weight+=sampleWeight;
    }
    filteredProbe[id]=float4(accumulated/max(weight,1e-5),1.0);
}
