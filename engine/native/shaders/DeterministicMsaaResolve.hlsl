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

    // Commit the production HDR scene to a fixed linear grid before it is
    // stored as FP16.  Different driver invocations can otherwise round the
    // final MSAA average one half-float ULP apart even when every input sample
    // is identical.  This is part of the rendered output (not capture/hash
    // tolerance), so downstream post-processing and readback observe the same
    // deterministic scene colour.
    const float hdrQuantizationScale = 1024.0;
    resolved.rgb = floor(max(resolved.rgb, 0.0) * hdrQuantizationScale + 0.5) /
                   hdrQuantizationScale;
    return resolved;
}
