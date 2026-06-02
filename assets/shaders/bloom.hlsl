// ブルーム用シェーダ。
//  - PrefilterPS: 加算エフェクトRTから明るい部分を抽出し半解像度へ
//  - BlurPS:      分離ガウスブラー(横/縦をCBの方向で切替)
// 業界標準の「明るい部分を抽出 → ぼかし → 元画像に加算」フローの前半を担当する。

cbuffer BloomCB : register(b0)
{
    float4 texel;  // xy: ソースのテクセルサイズ, zw: ブラー方向(1,0)/(0,1)
    float4 bparams; // x: しきい値, y: ソフトニー, z: 強度, w: 未使用
};

Texture2D srcTex : register(t0);
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

// 明るい部分を抽出(ソフトしきい値)。加算RTは元々発光成分なので軽めの抽出。
float4 PrefilterPS(VSOut input) : SV_TARGET
{
    float3 c = srcTex.Sample(linearSampler, input.uv).rgb;
    const float threshold = bparams.x;
    const float knee = max(bparams.y, 0.0001f);
    const float br = max(c.r, max(c.g, c.b));
    // ソフトニー: しきい値付近を滑らかに立ち上げる
    float soft = clamp(br - threshold + knee, 0.0f, 2.0f * knee);
    soft = (soft * soft) / (4.0f * knee);
    float contribution = max(soft, br - threshold);
    contribution /= max(br, 0.00001f);
    return float4(c * contribution, 1.0f);
}

// 分離ガウスブラー(9タップ)。texel.zw で横/縦を指定。
float4 BlurPS(VSOut input) : SV_TARGET
{
    const float2 dir = texel.zw * texel.xy;
    const float w0 = 0.227027f;
    const float w1 = 0.1945946f;
    const float w2 = 0.1216216f;
    const float w3 = 0.054054f;
    const float w4 = 0.016216f;
    float3 sum = srcTex.Sample(linearSampler, input.uv).rgb * w0;
    sum += srcTex.Sample(linearSampler, input.uv + dir * 1.0f).rgb * w1;
    sum += srcTex.Sample(linearSampler, input.uv - dir * 1.0f).rgb * w1;
    sum += srcTex.Sample(linearSampler, input.uv + dir * 2.0f).rgb * w2;
    sum += srcTex.Sample(linearSampler, input.uv - dir * 2.0f).rgb * w2;
    sum += srcTex.Sample(linearSampler, input.uv + dir * 3.0f).rgb * w3;
    sum += srcTex.Sample(linearSampler, input.uv - dir * 3.0f).rgb * w3;
    sum += srcTex.Sample(linearSampler, input.uv + dir * 4.0f).rgb * w4;
    sum += srcTex.Sample(linearSampler, input.uv - dir * 4.0f).rgb * w4;
    return float4(sum, 1.0f);
}
