#include "Conversions.hlsli"

cbuffer PerImageCB : register(b0)
{
    Texture2D		gTexture;
    SamplerState	gSampler;
};

cbuffer FoveatedCB : register(b1)
{
    float4 gEyePos;
    float4 gEyeLevels;
};

float3 ConvertColor(float3 input) {
    int colorspace = int(gEyePos.w);

    if (colorspace == 0) {
        return RGBToYCoCg(input);
    }
    else if (colorspace == 1) {
        return RGBToYCoCg24(input);
    }
    else {
        return input.yzy;
    }
}

float3 GrayScale(float3 c)
{
    return dot(c.rgb, float3(0.3, 0.59, 0.11));
}

float4 main(in float2 texC : TEXCOORD) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, texC);
    if (gEyeLevels.x == 1.f) {
        color.xyz = lerp(color.xyz, GrayScale(color.xyz), gEyeLevels.y);
    }

    return float4(ConvertColor(color.xyz), 1.0);
}
