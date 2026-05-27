cbuffer FrameCB : register(b0)
{
    row_major float4x4 viewProj;
    float4 lightDir;
    float4 cameraPos;
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 world;
    float4 tint;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD0;
};

VSOut VSMain(VSIn input)
{
    VSOut output;
    float4 wp = mul(float4(input.pos, 1.0f), world);
    output.pos = mul(wp, viewProj);
    output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    output.color = input.color * tint;
    output.worldPos = wp.xyz;
    return output;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    float3 n = normalize(input.normal);
    float3 l = normalize(-lightDir.xyz);
    float ndl = saturate(dot(n, l));
    float rim = pow(1.0f - saturate(dot(n, normalize(cameraPos.xyz - input.worldPos))), 2.0f) * 0.18f;
    float3 lit = input.color.rgb * (0.35f + ndl * 0.70f + rim);
    return float4(lit, input.color.a);
}
