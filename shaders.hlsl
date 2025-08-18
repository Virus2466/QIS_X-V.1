
Texture2D sourceTex : register(t0);
SamplerState samplerState : register(s0);


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

// Bicubic interpolation functions
float W(float x)
{
    x = abs(x);
    if (x < 1.0)
        return (1.5 * x - 2.5) * x * x + 1.0;
    else if (x < 2.0)
        return ((-0.5 * x + 2.5) * x - 4.0) * x + 2.0;
    return 0.0;
}

float4 BicubicSample(Texture2D tex, float2 uv, float2 texSize)
{
    float2 pixelPos = uv * texSize - 0.5;
    float2 fracPart = frac(pixelPos);
    float2 intPart = floor(pixelPos);

    float4 sampled = 0;
    float totalWeight = 0.0;

    [unroll]
    for (int y = -1; y <= 2; y++)
    {
        [unroll]
        for (int x = -1; x <= 2; x++)
        {
            float2 sampleCoord = (intPart + float2(x, y) + 0.5) / texSize;
            if (all(sampleCoord >= 0.0 && sampleCoord <= 1.0))
            {
                float wx = W(x - fracPart.x);
                float wy = W(y - fracPart.y);
                float weight = wx * wy;
                sampled += tex.SampleLevel(samplerState, sampleCoord, 0) * weight;
                totalWeight += weight;
            }
        }
    }
    return sampled / max(totalWeight, 1e-5);
}

// Pixel shader with bicubic
float4 PS_main(PS_IN input) : SV_TARGET
{
    float2 texSize;
 
    sourceTex.GetDimensions(texSize.x, texSize.y);
    
    float4 color = BicubicSample(sourceTex, input.uv, texSize);
    
    // Debug border (red)
    if (input.uv.x < 0.01 || input.uv.x > 0.99 || input.uv.y < 0.01 || input.uv.y > 0.99)
    {
        return float4(1.0, 0.0, 0.0, 1.0);
    }
    
    return color;
}