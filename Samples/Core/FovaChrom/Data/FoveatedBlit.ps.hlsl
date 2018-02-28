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
        return YCoCgToRGB(input);
    }
    else if (colorspace == 1) {
        return YCoCg24ToRGB(input);
    }
    else {
        return input.yzy;
    }
}

float4 main(in float2 texC : TEXCOORD, in float4 fragPos : SV_POSITION) : SV_TARGET
{
    float FoveaIntensity = distance(gEyePos.xy, fragPos.xy / float2(1600, 1024));
    FoveaIntensity = 1 - pow(1 - FoveaIntensity, 3);
    FoveaIntensity *= 5;

    float3 col = ColorFn1DfiveC(frac(FoveaIntensity), int(FoveaIntensity));
    float2 CrCb = gTexture.SampleLevel(gSampler, texC, FoveaIntensity).yz;
    float Y = gTexture.Sample(gSampler, texC).x;
    float Cr = CrCb.x;
    float Cb = CrCb.y;

    float3 YCrCb = float3(Y, Cr, Cb);

    float3 rgb = ConvertColor(YCrCb);

    if (gEyePos.z == 1.0) {
        return float4(col, 1.0);
    }
    else {
        return float4(rgb, 1.0);
    }
}