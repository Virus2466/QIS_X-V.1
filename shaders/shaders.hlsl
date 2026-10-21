Texture2D sourceTex : register(t0);
Texture2D referenceTex : register(t1);
SamplerState linearClamp : register(s0);
SamplerState pointClamp : register(s1);
cbuffer Settings : register(b0)
{
    float2 sourceSize;
    float sharpness;
    uint algorithm;
    float splitPosition;
    uint comparison;
    float2 padding;
};
struct PixelInput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
PixelInput VS_main(uint id : SV_VertexID)
{
    PixelInput output;
    output.uv = float2((id << 1) & 2, id & 2);
    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}
float4 CopyPS(PixelInput input) : SV_TARGET
{
    float4 color = sourceTex.SampleLevel(linearClamp, input.uv, 0);
    return float4(lerp(float3(0.025, 0.032, 0.044), color.rgb, color.a), 1);
}
// Catmull-Rom samples land on texel centers. Clamp addressing preserves borders.
float CubicWeight(float x)
{
    x = abs(x);
    if (x < 1) return (1.5 * x - 2.5) * x * x + 1;
    if (x < 2) return ((-0.5 * x + 2.5) * x - 4) * x + 2;
    return 0;
}
float3 Bicubic(float2 uv)
{
    float2 pixel = uv * sourceSize - 0.5;
    float2 base = floor(pixel);
    float2 f = pixel - base;
    float3 color = 0;
    [unroll] for (int y = -1; y <= 2; ++y)
        [unroll] for (int x = -1; x <= 2; ++x) {
            float2 coord = (base + float2(x, y) + 0.5) / sourceSize;
            color += sourceTex.SampleLevel(pointClamp, coord, 0).rgb *
                CubicWeight(x - f.x) * CubicWeight(y - f.y);
        }
    return color;
}
float4 UpscalePS(PixelInput input) : SV_TARGET
{
    float3 center = sourceTex.SampleLevel(linearClamp, input.uv, 0).rgb;
    if (algorithm == 0) return float4(center, 1);
    float3 color = Bicubic(input.uv);
    if (algorithm == 2 && sharpness > 0) {
        float2 texel = 1.0 / sourceSize;
        float3 n = sourceTex.SampleLevel(linearClamp, input.uv + float2(0, -texel.y), 0).rgb;
        float3 s = sourceTex.SampleLevel(linearClamp, input.uv + float2(0, texel.y), 0).rgb;
        float3 e = sourceTex.SampleLevel(linearClamp, input.uv + float2(texel.x, 0), 0).rgb;
        float3 w = sourceTex.SampleLevel(linearClamp, input.uv + float2(-texel.x, 0), 0).rgb;
        float3 lo = min(center, min(min(n, s), min(e, w)));
        float3 hi = max(center, max(max(n, s), max(e, w)));
        float contrast = max(hi.r - lo.r, max(hi.g - lo.g, hi.b - lo.b));
        float gain = sharpness * (1 - 0.65 * saturate(contrast));
        color = clamp(color + gain * (center - (n + s + e + w) * 0.25), lo - 0.015, hi + 0.015);
    }
    return float4(saturate(color), 1);
}
float4 ComparePS(PixelInput input) : SV_TARGET
{
    if (comparison == 2 || (comparison == 1 && input.uv.x < splitPosition))
        return referenceTex.SampleLevel(linearClamp, input.uv, 0);
    return sourceTex.SampleLevel(linearClamp, input.uv, 0);
}
