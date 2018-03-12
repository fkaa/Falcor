#include "Conversions.hlsli"

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

cbuffer PerImageCB : register(b0)
{
    Texture2D		gTextureLeft;
    Texture2D		gTextureRight;
    SamplerState	gSampler;
};

cbuffer FoveatedCB : register(b1)
{
    float4 gEyePos;
};

struct StereoOut {
    float4 eyeL : SV_TARGET0;
    float4 eyeR : SV_TARGET1;
};

StereoOut main(in float2 texC : TEXCOORD, in float4 fragPos : SV_POSITION)
{
    StereoOut output;
    float FoveaIntensity = distance(gEyePos.xy, fragPos.xy / float2(2560, 1440));
    FoveaIntensity = 1 - pow(1 - FoveaIntensity, 3);
    FoveaIntensity *= 5;

    float3 col = ColorFn1DfiveC(frac(FoveaIntensity), int(FoveaIntensity));
    float2 CrCb = gTextureLeft.SampleLevel(gSampler, texC, FoveaIntensity).yz;
    float Y = gTextureLeft.Sample(gSampler, texC).x;
    float Cr = CrCb.x;
    float Cb = CrCb.y;

    float3 YCrCb = float3(Y, Cr, Cb);

    float3 rgb = ConvertColor(YCrCb);

    if (gEyePos.z == 1.0) {
        output.eyeL = float4(col, 1.0);
        output.eyeR = float4(col, 1.0);
    }
    else {
        output.eyeL = float4(rgb, 1.0);
        output.eyeR = float4(rgb, 1.0);
    }

    return output;
}
