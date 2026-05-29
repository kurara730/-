cbuffer PostCB : register(b0)
{
    float4 params; // x: taa blend, y: additive scale, z: view mode, w: brightness
};

Texture2D sceneTex : register(t0);
Texture2D additiveTex : register(t1);
Texture2D historyTex : register(t2);
SamplerState linearSampler : register(s0);

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vertexId : SV_VertexID)
{
    VSOut output;
    float2 pos[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };
    output.pos = float4(pos[vertexId], 0.0f, 1.0f);
    output.uv = float2(output.pos.x * 0.5f + 0.5f, 0.5f - output.pos.y * 0.5f);
    return output;
}

float4 PSMain(VSOut input) : SV_TARGET
{
    float4 sceneColor = sceneTex.Sample(linearSampler, input.uv);
    float4 additive = additiveTex.Sample(linearSampler, input.uv);
    if (params.z > 0.5f)
    {
        return float4(additive.rgb, 1.0f);
    }

    float brightness = max(params.w, 0.0f);
    float4 current = float4((sceneColor.rgb + additive.rgb * params.y) * brightness, 1.0f);
    float4 history = historyTex.Sample(linearSampler, input.uv);
    return lerp(current, history, saturate(params.x));
}
