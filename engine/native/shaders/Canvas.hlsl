cbuffer Canvas : register(b2)
{
    float2 logicalSize;
};

struct CanvasVertexInput
{
    float2 position : POSITION;
    float4 color : COLOR;
};

struct CanvasPixelInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

CanvasPixelInput CanvasVertexMain(CanvasVertexInput input)
{
    CanvasPixelInput output;
    output.position = float4(input.position.x / (logicalSize.x * 0.5) - 1.0,
                             1.0 - input.position.y / (logicalSize.y * 0.5), 0.0, 1.0);
    output.color = input.color;
    return output;
}

float4 CanvasPixelMain(CanvasPixelInput input) : SV_TARGET
{
    return input.color;
}
