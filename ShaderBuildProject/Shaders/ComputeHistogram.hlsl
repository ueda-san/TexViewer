#define HISTOGRAM_BINS 1024

#define CS_UNKNOWN     0
#define CS_LINEAR      1
#define CS_SRGB        2
#define CS_BT601       3
#define CS_BT709       4
#define CS_BT2020      5
#define CS_ADOBE98     6
#define CS_DCI_P3      7
#define CS_DISPLAY_P3  8

#define TF_DEFAULT     0<<8
#define TF_LINEAR      1<<8
#define TF_SRGB        2<<8
#define TF_BT601       3<<8
#define TF_BT709       4<<8
#define TF_BT2020      5<<8
#define TF_ADOBE98     6<<8
#define TF_DCI_P3      7<<8
#define TF_DISPLAY_P3  8<<8
#define TF_PQ          100<<8
#define TF_HLG         101<<8
#define TF_GAMMA_N     102<<8


Texture2D<float4> g_Input : register(t0);
RWStructuredBuffer<uint> g_Histogram : register(u0);

cbuffer Params : register(b0)
{
    uint g_Width;
    uint g_Height;
    uint g_ColorSpace;
    uint g_TransferFunc;
};

float Linearize(float v, uint tf)
{
    float ret = v;
    switch (tf)
    {
      case TF_LINEAR:
        break;
      case TF_SRGB:
        ret = (v <= 0.04045) ? v / 12.92 : pow(max((v + 0.055) / 1.055, 0.0), 2.4);
        break;
      case TF_DCI_P3:
      case TF_DISPLAY_P3:
      case TF_BT709:
      case TF_BT601:
        ret = (v < 0.081) ? v / 4.5 : pow(max((v + 0.099) / 1.099, 0.0), 1.0 / 0.45);
        break;
      case TF_BT2020:
        ret = (v < 0.08145) ? v / 4.5 : pow(max((v + 0.0993) / 1.0993, 0.0), 1.0 / 0.45);
        break;
      case TF_ADOBE98:
        ret = pow(max(v, 0.0), 2.19921875); // ≈2.2
        break;
      case TF_PQ:
        {
            // ST.2084 PQ
            const float m1 = 2610.0 / 16384.0;
            const float m2 = 2523.0 / 32.0;
            const float c1 = 3424.0 / 4096.0;
            const float c2 = 2413.0 / 128.0;
            const float c3 = 2392.0 / 128.0;

            float vp = pow(max(v, 0.0), 1.0 / m2);
            ret = pow(max(max(vp - c1, 0.0) / (c2 - c3 * vp), 0.0), 1.0 / m1);
        }
        break;
      case TF_HLG:
        {
            // ARIB STD-B67
            const float a = 0.17883277;
            const float b = 0.28466892;
            const float c = 0.55991073;
            ret = (v <= 0.5) ? (v * v / 3.0) : ((exp((v - c) / a) + b) / 12.0);
        }
        break;
    }
    return ret;
}

float3 GetLumaWeights(uint cs)
{
    float3 ret = float3(0.2126, 0.7152, 0.0722);
    switch (cs)
    {
      case CS_BT601:
        ret = float3(0.299, 0.587, 0.114);
        break;
      case CS_SRGB:
      case CS_BT709:
        ret = float3(0.2126, 0.7152, 0.0722);
        break;
      case CS_LINEAR:
      case CS_BT2020:
        ret = float3(0.2627, 0.6780, 0.0593);
        break;
      case CS_ADOBE98:
        ret = float3(0.2974, 0.6274, 0.0753);
        break;
      case CS_DCI_P3:
        ret = float3(0.2095, 0.7216, 0.0689);
        break;
      case CS_DISPLAY_P3:
        ret = float3(0.22897, 0.69174, 0.07929);
        break;
    }
    return ret;
}


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_Width || DTid.y >= g_Height) return;

    float4 color = g_Input.Load(int3(DTid.xy, 0));

    float3 rgb;
    rgb.r = Linearize(color.r, g_TransferFunc);
    rgb.g = Linearize(color.g, g_TransferFunc);
    rgb.b = Linearize(color.b, g_TransferFunc);

    float3 w = GetLumaWeights(g_ColorSpace);
    float Y = dot(rgb, w);

    float luminanceNits;
    if (g_TransferFunc == TF_PQ || g_TransferFunc == TF_HLG)
        luminanceNits = Y * 10000.0;
    else
        luminanceNits = Y * 100.0;

    uint bin = (uint)min(luminanceNits / 10000.0 * (HISTOGRAM_BINS - 1), HISTOGRAM_BINS - 1);
    InterlockedAdd(g_Histogram[bin], 1);
}
