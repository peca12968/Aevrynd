Texture2DMS<float4, 4> multisampleHdr : register(t0);

struct ResolveVertexOutput
{
    float4 position : SV_POSITION;
};

float4 DeterministicMsaaResolvePixelMain(ResolveVertexOutput input) : SV_TARGET
{
    const int2 pixel = int2(input.position.xy);
    precise float4 sum = multisampleHdr.Load(pixel, 0);
    sum = sum + multisampleHdr.Load(pixel, 1);
    sum = sum + multisampleHdr.Load(pixel, 2);
    sum = sum + multisampleHdr.Load(pixel, 3);
    precise float4 resolved = sum * 0.25;

    return resolved;
}
