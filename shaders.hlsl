// Vertex shader
struct VS_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_IN VS_main(VS_IN input)
{
    PS_IN output;
    output.pos = float4(input.pos, 1.0);
    output.uv = input.uv;
    return output;
}

// Pixel shader - Basic bilinear upscale
Texture2D sourceTex : register(t0);
SamplerState samplerState : register(s0);

float4 PS_main(PS_IN input) : SV_TARGET
{
    return sourceTex.Sample(samplerState, input.uv);
}