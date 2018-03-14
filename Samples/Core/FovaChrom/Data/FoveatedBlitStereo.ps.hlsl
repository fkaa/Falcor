#include "Conversions.hlsli"

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
        return YCoCg24ToRGB(input).brg;
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

struct StereoOut {
    float4 eyeL : SV_TARGET0;
    float4 eyeR : SV_TARGET1;
};

float GetFovea(float4 fragPos, int side)
{
    float FoveaIntensity = distance(gEyePos.xy, (side * float2(1480, 0) + fragPos.xy) / float2(1280, 1440));
    FoveaIntensity = 1 - pow(1 - FoveaIntensity, 3);
    FoveaIntensity *= 5;

    return FoveaIntensity;
}

StereoOut main(in float2 texC : TEXCOORD, in float4 fragPos : SV_POSITION)
{
    StereoOut output;

    float FoveaIntensity_L = GetFovea(fragPos, 0);
    float FoveaIntensity_R = GetFovea(fragPos, 0);

    float2 CrCb_L = gTextureLeft.SampleLevel(gSampler, texC, FoveaIntensity_L).yz;
    float Y_L = gTextureLeft.Sample(gSampler, texC).x;
    float2 CrCb_R = gTextureRight.SampleLevel(gSampler, texC, FoveaIntensity_R).yz;
    float Y_R = gTextureRight.Sample(gSampler, texC).x;

    float3 Rgb_L = ConvertColor(float3(Y_L, CrCb_L));
    float3 Rgb_R = ConvertColor(float3(Y_R, CrCb_R));

    if (gEyePos.z == 1.0) {
        float3 col_L;// = ColorFn1DfiveC(frac(FoveaIntensity_L), int(FoveaIntensity_L));
        float3 col_R;// = ColorFn1DfiveC(frac(FoveaIntensity_R), int(FoveaIntensity_R));
        if (FoveaIntensity_L < 0.05) {
            Rgb_L = float3(1, 0.4, 0.3);
        }
        if (FoveaIntensity_R < 0.05) {
            Rgb_R = float3(1, 0.4, 0.3);
        }
        
        output.eyeL = float4(Rgb_L, 1.0);
        output.eyeR = float4(Rgb_R, 1.0);
    }
    else {
        output.eyeL = float4(Rgb_L, 1.0);
        output.eyeR = float4(Rgb_R, 1.0);
    }

    return output;
}
