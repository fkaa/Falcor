#include "Conversions.hlsli"

cbuffer FoveatedCB : register(b1)
{
    float4 gEyePos;
    float4 gEyeLevels;
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

float GetMipLevel(float4 fragPos)
{
    float dist = distance(gEyePos.xy, fragPos.xy / float2(1600, 1024));
    float level = 0;

    if (dist < 0.125) level = lerp(0, gEyeLevels.x, dist * 8);
    else if (dist < 0.375) level = lerp(gEyeLevels.x, gEyeLevels.y, (dist - 0.125) * 4);
    else if (dist < 0.625) level = lerp(gEyeLevels.y, gEyeLevels.z, (dist - 0.375) * 4);
    else level = lerp(gEyeLevels.z, gEyeLevels.w, (dist - 0.625) * 4);

    return level;
}

StereoOut main(in float2 texC : TEXCOORD, in float4 fragPos : SV_POSITION)
{
    StereoOut output;

    float FoveaIntensity_L = GetMipLevel(fragPos);
    float FoveaIntensity_R = GetMipLevel(fragPos);

    float2 CrCb_L = gTextureLeft.SampleLevel(gSampler, texC, FoveaIntensity_L).yz;
    float Y_L = gTextureLeft.Sample(gSampler, texC).x;
    float2 CrCb_R = gTextureRight.SampleLevel(gSampler, texC, FoveaIntensity_R).yz;
    float Y_R = gTextureRight.Sample(gSampler, texC).x;

    float3 Rgb_L = ConvertColor(float3(Y_L, CrCb_L));
    float3 Rgb_R = ConvertColor(float3(Y_R, CrCb_R));

    if (gEyePos.z == 1.0) {
        float3 col_L;// = ColorFn1DfiveC(frac(FoveaIntensity_L), int(FoveaIntensity_L));
        float3 col_R;// = ColorFn1DfiveC(frac(FoveaIntensity_R), int(FoveaIntensity_R));

        float col = FoveaIntensity_L/10.f;
        if (FoveaIntensity_L < 0.05) {
            Rgb_L = col.xxx;
        }
        if (FoveaIntensity_R < 0.05) {
            Rgb_R = col.xxx;
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
