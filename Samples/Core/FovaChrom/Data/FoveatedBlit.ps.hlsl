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
        return YCoCgToRGB(input);
    }
    else if (colorspace == 1) {
        return YCoCg24ToRGB(input);
    }
    else {
        return input.yzy;
    }
}

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

float4 main(in float2 texC : TEXCOORD, in float4 fragPos : SV_POSITION) : SV_TARGET
{
    float FoveaIntensity = GetMipLevel(fragPos);

    float col = FoveaIntensity/10.f;
    float2 CrCb = gTexture.SampleLevel(gSampler, texC, FoveaIntensity).yz;
    float Y = gTexture.Sample(gSampler, texC).x;
    float Cr = CrCb.x;
    float Cb = CrCb.y;

    float3 YCrCb = float3(Y, Cr, Cb);

    float3 rgb = ConvertColor(YCrCb);

    if (gEyePos.z == 1.0) {
        //if (FoveaIntensity < 0.05) {
            rgb = col.xxx;
        //}
        return float4(rgb, 1.0);
    }
    else {
        return float4(rgb, 1.0);
    }
}
