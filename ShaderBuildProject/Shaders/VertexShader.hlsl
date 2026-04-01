struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 Uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.Position = float4(input.Pos, 1.0f);
    output.TexCoord = input.Uv;
    return output;
}
