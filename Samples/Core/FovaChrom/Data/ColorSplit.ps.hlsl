#include "Conversions.hlsli"

cbuffer PerImageCB : register(b0)
{
    Texture2D		gTexture;
    SamplerState	gSampler;
};

cbuffer FoveatedCB : register(b1)
{
    float4 gEyePos;
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

float4 main(in float2 texC : TEXCOORD) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, texC);

    return float4(ConvertColor(color.xyz), 1.0);
}
