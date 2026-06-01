#pragma once

#include "GameTypes.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <string>
#include <vector>

// 2Dゲーム画面用のスプライト描画ヘルパーです。
// テクスチャ付き四角形だけでなく、円、リング、扇形も同じ入口で描けます。
class SpriteCanvas
{
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::wstring& shaderPath);
    void Shutdown();

    void Begin(const DirectX::XMMATRIX& viewProjection, bool additive);
    void End();

    // center/size はゲーム内ワールド座標です。depth は重なり順の調整に使います。
    void DrawQuad(ID3D11ShaderResourceView* texture, V2 center, V2 size, float rotation, Color tint, float depth = 0.5f);
    void DrawCircle(V2 center, float radius, Color tint, float depth = 0.5f, int segments = 36);
    void DrawRing(V2 center, float radius, float thickness, Color tint, float depth = 0.5f, int segments = 48);
    void DrawArc(V2 center, float radius, float thickness, float angle, float arc, Color tint, float depth = 0.5f, int segments = 28);

    bool Available() const { return initialized_; }

private:
    struct SpriteVertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT4 color;
    };

    void Submit(const std::vector<SpriteVertex>& vertices, ID3D11ShaderResourceView* texture);
    SpriteVertex MakeVertex(float x, float y, float depth, float u, float v, Color tint) const;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> frameCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteTexture_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthDisabled_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> alphaBlend_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> additiveBlend_;
    bool initialized_ = false;
};
