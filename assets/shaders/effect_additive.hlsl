cbuffer EffectFrameCB : register(b0)
{
    float4x4 viewProj;
};

struct VSIn
{
    float3 pos : POSITION;
    float4 color : COLOR0;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
};

VSOut VSMain(VSIn input)
{
    VSOut output;
    output.pos = mul(float4(input.pos, 1.0), viewProj);
    output.color = input.color;
    return output;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    return saturate(input.color);
}

