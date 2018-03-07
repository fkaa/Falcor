void forward_lift(float x, float y, out float average, out float diff) {
    diff = (fmod(y - x, 1));
    average = (fmod((x + (diff / 2.0)), 1));
}

void reverse_lift(out float x, out float y, float average, float diff) {
    x = (fmod(average - (diff / 2.0), 1));
    y = (fmod(x + diff, 1));
}

float3 RGBToYCoCg24(float3 color) {
    float temp, Co, Y, Cg;
    forward_lift(color.r, color.b, temp, Co);
    forward_lift(color.g, temp, Y, Cg);
    return float3(Y, Cg, Co);
}

float3 YCoCg24ToRGB(float3 color) {
    float temp, r, g, b;
    reverse_lift(g, temp, color.x, color.z);
    reverse_lift(r, b, temp, color.y);
    return float3(r, g, b);
}

#define CHROMA_BIAS (0.5 * 256.0 / 255.0)

float3 RGBToYCoCg(float3 rgb)
{
    float3 YCoCg;
    YCoCg.x = dot(rgb, float3(0.25, 0.5, 0.25));
    YCoCg.y = dot(rgb, float3(0.5, 0.0, -0.5)) + CHROMA_BIAS;
    YCoCg.z = dot(rgb, float3(-0.25, 0.5, -0.25)) + CHROMA_BIAS;

    return YCoCg;
}

float3 YCoCgToRGB(float3 YCoCg)
{
    float Y = YCoCg.x;
    float Co = YCoCg.y - CHROMA_BIAS;
    float Cg = YCoCg.z - CHROMA_BIAS;

    float3 rgb;
    rgb.r = Y + Co - Cg;
    rgb.g = Y + Cg;
    rgb.b = Y - Co - Cg;

    return rgb;
}

float3 ColorFn1DfiveC(float x, int c)
{
    x = saturate(x);

    float r, g, b;
    switch (c)
    {
    case 1:
        r = 0.22 + 0.71*x; g = 0.036 + 0.95*x; b = 0.5 + 0.49*x;
        break;

    case 2:
        g = 0.1 + 0.8*x;
        r = 0.48 + x * (1.7 + (-1.8 + 0.56 * x) * x);
        b = x * (-0.21 + x);
        break;

    case 3:
        g = 0.33 + 0.69*x; b = 0.059 + 0.78*x;
        r = x * (-0.21 + (2.6 - 1.5 * x) * x);
        break;

    case 4:
        g = 0.22 + 0.75*x;
        r = 0.033 + x * (-0.35 + (2.7 - 1.5 * x) * x);
        b = 0.45 + (0.97 - 0.46 * x) * x;
        break;

    default:
        r = g = b = 0.025 + 0.96*x;
        break;
    }
    return float3(r, g, b);
}

