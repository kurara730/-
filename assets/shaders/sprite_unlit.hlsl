cbuffer SpriteFrameCB : register(b0)
{
    row_major float4x4 viewProj;
};

Texture2D spriteTex : register(t0);
SamplerState spriteSampler : register(s0);

struct VSIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VSOut VSMain(VSIn input)
{
    VSOut output;
    output.pos = mul(float4(input.pos, 1.0), viewProj);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    return spriteTex.Sample(spriteSampler, input.uv) * input.color;
}
