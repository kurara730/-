cbuffer PostCB : register(b0)
{
    float4 params;  // x: taa blend, y: additive scale, z: view mode, w: brightness(exposure)
    float4 params2; // x: bloom intensity, y: vignette strength, z: tonemap enable, w: unused
};

Texture2D sceneTex : register(t0);
Texture2D additiveTex : register(t1);
Texture2D historyTex : register(t2);
Texture2D bloomTex : register(t3);
SamplerState linearSampler : register(s0);

// ACES フィルミックトーンマッピング(近似)。HDR -> 表示用に階調圧縮。
float3 ACESFilm(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

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
    float3 scene = sceneTex.Sample(linearSampler, input.uv).rgb;
    float3 additive = additiveTex.Sample(linearSampler, input.uv).rgb;
    float3 bloom = bloomTex.Sample(linearSampler, input.uv).rgb;
    if (params.z > 0.5f)
    {
        // デバッグ: 加算RT(+ブルーム)のみ表示
        return float4(additive + bloom * params2.x, 1.0f);
    }

    const float exposure = max(params.w, 0.0f);
    // シーン + 生エフェクト + にじみ(ブルーム) を HDR で合成
    float3 hdr = (scene + additive * params.y + bloom * params2.x) * exposure;

    // トーンマッピング(ACES)。z<=0.5 なら従来どおり素通し。
    float3 mapped = (params2.z > 0.5f) ? ACESFilm(hdr) : saturate(hdr);

    // ビネット(画面端を僅かに落とす)
    float2 q = input.uv - 0.5f;
    float vignette = 1.0f - dot(q, q) * params2.y;
    mapped *= saturate(vignette);

    float4 current = float4(mapped, 1.0f);
    float4 history = historyTex.Sample(linearSampler, input.uv);
    return lerp(current, history, saturate(params.x));
}
