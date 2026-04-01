// Draw image with ...
// * border color
// * alpha checker board
// * draw selection rect
// * color space conversion
// * tonemap
// * exposure collect
// * gamut mapping
// * output PQ/BT709

Texture2D srcTexture : register(t0);
SamplerState samplerState : register(s0);
RWBuffer<int> maxRGB : register(u1);

// TODO: HableCurveなどフレーム内で同じ値をC#側で事前計算

cbuffer DrawParams : register(b0)
{
    float4 srcRect;
    float4 dstRect;
    float4 selRect;
    float4 canvasColor; //xyz:color,  w:-
    float4 param; //xy:checker scale,  z:gamutScale,  w:gamma

    float4 toBT2020_0;  //float3 + padding
    float4 toBT2020_1;  //float3 + padding
    float4 toBT2020_2;  //float3 + padding

    float2 thickness;
    float maxCLL;
    float displayMaxNits;
    float exposure;
    float whiteLevel;
    uint  flags;
    //        +-------+-------+-------+------- 32bit
    // flags: | FLAG  | T | A | TF    | CS    |
    //                                  CS:color space (CS_*)
    //                          TF:transfer function (TF_*)
    //                      A:alpha mode (AM_*)
    //                  T:tonemap type (TM_*)
    //          FLAG:
    //          0x01:hideChecker
    //          0x02:preMultiplied
    //          0x04:inputSDR
    //          0x08:outputPQ
    //          0x10:checkGamutScale
    float dummy; // (16bytes boundary)
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

static const float3 linearWhite = float3(1.0, 1.0, 1.0);
static const float3 linearGray = float3(0.6, 0.6, 0.6);  // ==sRGB(0.8, 0.8, 0.8)
static const float3 linearRed = float3(1.0, 0.0, 0.0);
static const float3 LumaCoeff2020 = float3(0.2627, 0.6780, 0.0593);
static const float pq_c1 = 0.8359375;       // 107.0 / 128.0
static const float pq_c2 = 18.8515625;      // 2413.0 / 128.0
static const float pq_c3 = 18.6875;         // 2392.0 / 128.0
static const float pq_m = 0.1593017578125;  // 1305.0 / 8192.0
static const float pq_n = 78.84375;         // 2523.0 / 32.0
static const float hlg_a = 0.17883277;
static const float hlg_b = 0.28466892;
static const float hlg_c = 0.55991073;
static const float hlg_gammaSys = 1.2; //1.2;

#define CS_UNKNOWN    0 // color space
#define CS_LINEAR     1
#define CS_SRGB       2
#define CS_BT601      3
#define CS_BT709      4
#define CS_BT2020     5
#define CS_ADOBE98    6
#define CS_DCI_P3     7
#define CS_DISPLAY_P3 8

#define TF_DEFAULT    0 // transfer function
#define TF_LINEAR     1
#define TF_SRGB       2
#define TF_BT601      3
#define TF_BT709      4
#define TF_BT2020     5
#define TF_ADOBE98    6
#define TF_DCI_P3     7
#define TF_DISPLAY_P3 8
#define TF_PQ         100
#define TF_HLG        101
#define TF_GAMMA_N    102

#define AM_IGNORE        0 // alpha blend mode
#define AM_LINEAR        1
#define AM_GAMMA         2

#define TM_NONE          0 // tone map type
#define TM_REINHARD      1
#define TM_REINHARDJODIE 2
#define TM_HABLE         3
#define TM_ACES          4
#define TM_LOTTES        5

//------------------------------------------------------------------------------
static const float3x3 BT709_to_BT2020 = {
    0.6274039, 0.3292830, 0.0433131,
    0.0690973, 0.9195404, 0.0113623,
    0.0163914, 0.0880133, 0.8955953
};
static const float3x3 BT2020_to_BT709 = {
     1.6604910, -0.5876411, -0.0728499,
    -0.1245505,  1.1328999, -0.0083494,
    -0.0181508, -0.1005789,  1.1187297,
};
static const float3x3 Identity = { 1,0,0, 0,1,0, 0,0,1 };

//------------------------------------------------------------------------------
float3 LinearToPQ(float3 linear_nits, float max_nits)
{
    float3 Y = linear_nits / max_nits;
    float3 Y_m = pow(max(Y, 0.0), pq_m);
    return pow((pq_c1 + pq_c2 * Y_m) / (1.0 + pq_c3 * Y_m), pq_n);
}
float3 LinearToSRGB(float3 c)
{
    c = saturate(c);
    float3 lo = 12.92 * c;
    float3 hi = 1.055 * pow(c, 1.0/2.4) - 0.055;
    float3 t  = step(0.0031308, c);
    return lerp(lo, hi, t);
}
float3 LinearToG22(float3 c)
{
    return pow(max(c, 0.0), 1.0 / 2.2);
}

float3 toLinear(float3 c, int tf, float gamma) {
    [branch]
    if (tf == TF_SRGB || tf == TF_DISPLAY_P3){
        c = saturate(c);
        float3 lo = c / 12.92;
        float3 hi = pow((c + 0.055) / 1.055, 2.4);
        float3 t  = step(0.04045, c);
        c = lerp(lo, hi, t);
    }else if (tf == TF_LINEAR) {
        c = c;
    }else if (tf == TF_ADOBE98) {
        c = pow(max(c, 0.0), 2.2);
    }else if (tf == TF_DCI_P3) {
        c = pow(max(c, 0.0), 2.6);
    }else if (tf == TF_GAMMA_N) {
        c = pow(max(c, 0.0), 1/gamma);
    }else if (tf == TF_PQ) {
        float3 a = pow(max(c, 0.0), 1.0/pq_n);
        float3 num = max(a - pq_c1, 0.0);
        float3 den = pq_c2 - pq_c3 * a;
        float3 L = pow(num / max(den, 1e-6), 1.0/pq_m);
        c = L * 10000.0 / whiteLevel;
    }else if (tf == TF_HLG) {
        c = (c < 0.5)? (c * c) / 3.0 : (exp((c - hlg_c) / hlg_a) + hlg_b) / 12.0;
    }else {
        //TF_BT601, TF_BT709, TF_BT2020
        float3 lo = c / 4.5;
        float3 hi = pow(max((c + 0.099) / 1.099, 0.0), 1.0/0.45);
        float3 t  = step(0.081, c);
        c = lerp(lo, hi, t);
    }
    return c;
}

float3 fromLinear(float3 c, int tf, float gamma) {
    [branch]
    if (tf == TF_SRGB || tf == TF_DISPLAY_P3){
        c = LinearToSRGB(c);
    }else if (tf == TF_LINEAR) {
        c = c;
    }else if (tf == TF_ADOBE98) {
        c = pow(max(c, 0.0), 1/2.2);
    }else if (tf == TF_DCI_P3) {
        c = pow(max(c, 0.0), 1/2.6);
    }else if (tf == TF_GAMMA_N) {
        c = pow(max(c, 0.0), gamma);
    }else if (tf == TF_PQ) {
        c = LinearToPQ(c, 10000.0/whiteLevel);
    }else if (tf == TF_HLG) {
        c = (c < 1.0 / 12.0)? sqrt(3.0 * c) : hlg_a * log(12.0 * c - hlg_b) + hlg_c;
    }else{
        //TF_BT601, TF_BT709, TF_BT2020
        float3 lo = 4.5 * c;
        float3 hi = 1.099 * pow(max(c, 0.0), 0.45) - 0.099;
        float3 t  = step(0.018, c);
        c = lerp(lo, hi, t);
    }
    return c;
}


//------------------------------------------------------------------------------
float3 RescaleByLuminance(float3 rgb, float L_old, float L_new)
{
    float s = (L_old > 1e-8) ? (L_new / L_old) : 0.0;
    return rgb * s;
}

float Tonemap_None(float L, float w)
{
    return saturate(L / max(w, 1.0));
}

float Tonemap_Reinhard(float L, float w)
{
    float x = L / max(w, 1e-6);
    return saturate(x / (1.0 + x));
}

float Tonemap_ReinhardJodie(float L, float w)
{
    float x = L;
    float y = (x * (1.0 + x / (w*w))) / (1.0 + x);
    return saturate(y / (1.0 + 1.0 / w));
}

float HableCurve(float x)
{
    const float A=0.15, B=0.50, C=0.10, D=0.20, E=0.02, F=0.30;
    return ((x*(A*x + C*B) + D*E) / (x*(A*x + B) + D*F)) - (E/F);
}

float Tonemap_Hable(float L, float w) // a.k.a. Uncharted2 / Filmic
{
    float W = max(w,1e-6);
    return saturate(HableCurve(L) / HableCurve(W));
}

float ACESCurve(float x)
{
    const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return saturate((x*(a*x+b)) / (x*(c*x+d)+e));
}

float Tonemap_ACES(float L, float w) // Academy Color Encoding System
{
    float W = max(w,1e-6);
    return saturate(ACESCurve(L) / ACESCurve(W));
}

float LottesCurve(float x)
{
    const float a=1.6, d=0.977, b=0.1, c=0.0, e=0.0;
    float num = pow(max(x, 0.0), a) + c*pow(max(x, 0.0), b) + e;
    float den = pow(max(x, 0.0), a) + d;
    return num/max(den,1e-6);
}

float Tonemap_Lottes(float L, float w)
{
    float W = max(w,1e-6);
    return saturate(LottesCurve(L) / LottesCurve(W));
}


//------------------------------------------------------------------------------


float isOnRect(float2 pos, float4 rect/*, float2 thickness*/) {
    float ret = 0;
    float x = pos.x;
    float y = pos.y;
    float2 outer_min = rect.xy;// - thickness/2;
    float2 outer_max = rect.xy + rect.zw;// + thickness/2;
    float2 inner_min = outer_min + thickness;
    float2 inner_max = outer_max - thickness;
    float2 lineCoord = (pos - selRect.xy)/dstRect.zw / (thickness*2);
    if (outer_min.x < x && x < outer_max.x){
        float inside_top = step(outer_min.y, y) * step(y, inner_min.y);
        float inside_btm = step(inner_max.y, y) * step(y, outer_max.y);
        ret += max(inside_top, inside_btm) * fmod(ceil(lineCoord.x), 2);
    }
    if (outer_min.y < y && y < outer_max.y){
        float inside_left = step(outer_min.x, x) * step(x, inner_min.x);
        float inside_right = step(inner_max.x, x) * step(x, outer_max.x);
        ret += max(inside_left, inside_right) * fmod(ceil(lineCoord.y), 2);
    }
    return ret;
}

//------------------------------------------------------------------------------

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.TexCoord;
    float2 grid          = param.xy;
    float gamutScale     = param.z;
    float gamma          = param.w;
    int cs               = (flags & 0x000000ff);
    int tf               = (flags & 0x0000ff00)>>8;
    int alphaMode        = (flags & 0x000f0000)>>16;
    int tonemapType      = (flags & 0x00f00000)>>20;
    bool hideChecker     = flags & 0x01000000;
    bool preMultiplied   = flags & 0x02000000;
    bool inputSDR        = flags & 0x04000000;
    bool outputPQ        = flags & 0x08000000;
    bool checkGamutScale = flags & 0x10000000;
    float3x3 toBT2020 = float3x3(toBT2020_0.xyz, toBT2020_1.xyz, toBT2020_2.xyz);

    if (tf == 0) tf = cs;

    float3 outCol;
    if (isOnRect(uv, selRect) != 0){
        outCol = linearRed * whiteLevel;
    }else{
        if (uv.x >= dstRect.x && uv.x <= dstRect.x + dstRect.z &&
            uv.y >= dstRect.y && uv.y <= dstRect.y + dstRect.w) {

            float2 normalizedCoord = (uv - dstRect.xy) / dstRect.zw;
            float2 srcCoord = srcRect.xy + normalizedCoord * srcRect.zw;
            float2 scaledCoord = srcCoord * grid;
            float4 col = srcTexture.Sample(samplerState, srcCoord);
            if (preMultiplied){
                if (col.a > 0){
                    col.rgb *= rcp(col.a);
                }else{
                    col.rgb = float3(0,0,0);
                }
            }

            float mod2 = fmod(floor(scaledCoord.x) + floor(scaledCoord.y), 2.0);
            float3 linearCol = mul(toBT2020, toLinear(col.rgb, tf, gamma));

            // tone map
            if (inputSDR || !outputPQ){
                outCol = linearCol * whiteLevel;
            }else{
                float3 srcNits = linearCol * whiteLevel;
                float3 srcRel = srcNits / displayMaxNits;
                float w = maxCLL / displayMaxNits;
                float L = dot(srcRel, LumaCoeff2020);
                float Lm;
                [branch]
                switch (tonemapType) {
                    default:
                    case TM_NONE:          Lm = Tonemap_None(L, w);          break;
                    case TM_REINHARD:      Lm = Tonemap_Reinhard(L, w);      break;
                    case TM_REINHARDJODIE: Lm = Tonemap_ReinhardJodie(L, w); break;
                    case TM_HABLE:         Lm = Tonemap_Hable(L, w);         break;
                    case TM_ACES:          Lm = Tonemap_ACES(L, w);          break;
                    case TM_LOTTES:        Lm = Tonemap_Lottes(L, w);        break;
                }
                float3 outRel = RescaleByLuminance(srcRel, L, Lm);
                outCol = outRel * displayMaxNits;
            }

            // alpha blend
            float3 bgColor;
            if (hideChecker){
                bgColor = mul(BT709_to_BT2020, toLinear(canvasColor.rgb, TF_SRGB, 0));
            }else{
                bgColor = (mod2 == 0) ? linearGray : linearWhite;
            }

            if (alphaMode == AM_LINEAR){
                outCol = lerp(bgColor * whiteLevel / gamutScale, outCol, col.a);
            }else if (alphaMode == AM_GAMMA){
                if (inputSDR){
                    float3 g_bg = fromLinear(bgColor / gamutScale, tf, gamma);
                    float3 g_col = fromLinear(outCol / whiteLevel, tf, gamma);
                    outCol = toLinear(lerp(g_bg, g_col, col.a), tf, gamma) * whiteLevel;
                }else{
                    float3 g_bg = fromLinear(bgColor * whiteLevel/displayMaxNits / gamutScale, tf, gamma);
                    float3 g_col = fromLinear(outCol/displayMaxNits, tf, gamma);
                    outCol = toLinear(lerp(g_bg, g_col, col.a), tf, gamma) * displayMaxNits;
                }
            }
        }else{
            outCol = mul(BT709_to_BT2020, toLinear(canvasColor.rgb, TF_SRGB, 0)) * whiteLevel;
        }
    }

    outCol *= exposure; // トーンマップ後の方が良いらしい？

    // EOTF
    if (outputPQ){
        float3 finalColor = LinearToPQ(outCol, 10000.0);
        return float4(finalColor, 1.0);
    }else{
        float3 sRGBColor = mul(BT2020_to_BT709, outCol / whiteLevel);
        if (checkGamutScale){
            float maxVal = max(sRGBColor.r, max(sRGBColor.g, sRGBColor.b));
            InterlockedMax(maxRGB[0], (int)(maxVal*65535));
        }
        float3 finalColor = LinearToSRGB(sRGBColor * gamutScale);

        return float4(finalColor, 1.0);
    }
}
