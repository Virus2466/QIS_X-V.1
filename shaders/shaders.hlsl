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

// Pixel shader with bicubic and sharpening
float4 PS_main(PS_IN input) : SV_TARGET
{
    float2 texSize;
    sourceTex.GetDimensions(texSize.x, texSize.y);
    
    // Perform bicubic sampling
    float4 color = BicubicSample(sourceTex, input.uv, texSize);
    
    // Sharpening parameters (tunable)
    static const float sharpenStrength = 1.5; // Adjust for intensity (e.g., 0.5 to 2.0)
    static const float2 texelSize = float2(1.0 / 854.0, 1.0 / 480.0); // Based on low-res render target
    
    // Sample the 3x3 neighborhood for sharpening
    float2 offsets[9] =
    {
        float2(-texelSize.x, -texelSize.y), // Top-left
        float2(0.0, -texelSize.y), // Top-center
        float2(texelSize.x, -texelSize.y), // Top-right
        float2(-texelSize.x, 0.0), // Middle-left
        float2(0.0, 0.0), // Center
        float2(texelSize.x, 0.0), // Middle-right
        float2(-texelSize.x, texelSize.y), // Bottom-left
        float2(0.0, texelSize.y), // Bottom-center
        float2(texelSize.x, texelSize.y) // Bottom-right
    };

    float4 blurred = float4(0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < 9; i++)
    {
        blurred += sourceTex.SampleLevel(samplerState, input.uv + offsets[i], 0);
    }
    blurred /= 9.0; // Average for blur

    // Apply unsharp mask
    float4 sharpened = color + sharpenStrength * (color - blurred);
    
    // Clamp to valid color range
    sharpened = saturate(sharpened);
    
    // Debug border (red)
    if (input.uv.x < 0.01 || input.uv.x > 0.99 || input.uv.y < 0.01 || input.uv.y > 0.99)
    {
        return float4(1.0, 0.0, 0.0, 1.0);
    }
    
    return sharpened;
}