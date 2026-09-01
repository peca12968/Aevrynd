uint RotateLeftOcclusion(uint value,uint shift)
{
    return (value<<shift)|(value>>(32u-shift));
}

float OcclusionDitherThreshold(float3 objectCoordinate,uint2 seeds)
{
    const uint3 quantized=asuint(int3(floor(objectCoordinate*64.0)));
    uint noise=quantized.x*0x9e3779b9u^RotateLeftOcclusion(quantized.y,11u)^RotateLeftOcclusion(quantized.z,22u)^seeds.x;
    noise^=RotateLeftOcclusion(seeds.y,7u);
    noise^=noise>>16u;noise*=0x7feb352du;noise^=noise>>15u;noise*=0x846ca68bu;noise^=noise>>16u;
    return (float(noise)+0.5)/4294967296.0;
}

void ClipOcclusionOpacity(float3 objectCoordinate,float opacity,uint2 seeds)
{
    if(opacity>=1.0)return;
    if(opacity<=0.0)clip(-1.0);
    clip(opacity-OcclusionDitherThreshold(objectCoordinate,seeds));
}
