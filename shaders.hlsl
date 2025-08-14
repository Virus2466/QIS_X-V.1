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
   float4 color = sourceTex.Sample(samplerState, input.uv);
    
   // Add border to verify upscaling
    if (input.uv.x < 0.01 || input.uv.x > 0.99 || input.uv.y < 0.01 || input.uv.y > 0.99)
    {
        return float4(1.0, 0.0, 0.0, 1.0); // Yellow Border
    }
    
    
    return color;
}