#include "SpriteRenderer.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
constexpr UINT MaxSpriteVertices = 4096;

struct SpriteFrameCB
{
    XMMATRIX viewProj;
};

XMFLOAT4 ToFloat4(Color c)
{
    return { c.r, c.g, c.b, c.a };
}
}

bool SpriteRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::wstring& shaderPath)
{
    Shutdown();
    if (!device || !context) return false;

    device_ = device;
    context_ = context;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", flags, 0, &vsBlob, &errors);
    if (FAILED(hr)) return false;
    errors.Reset();
    hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", flags, 0, &psBlob, &errors);
    if (FAILED(hr)) return false;

    if (FAILED(device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs_))) return false;
    if (FAILED(device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps_))) return false;

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(device_->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_))) return false;

    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = sizeof(SpriteFrameCB);
    cb.Usage = D3D11_USAGE_DEFAULT;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device_->CreateBuffer(&cb, nullptr, &frameCB_))) return false;

    D3D11_BUFFER_DESC vb{};
    vb.ByteWidth = sizeof(SpriteVertex) * MaxSpriteVertices;
    vb.Usage = D3D11_USAGE_DYNAMIC;
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&vb, nullptr, &vertexBuffer_))) return false;

    const unsigned char white[4] = { 255, 255, 255, 255 };
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA texInit{};
    texInit.pSysMem = white;
    texInit.SysMemPitch = 4;
    ComPtr<ID3D11Texture2D> whiteTex;
    if (FAILED(device_->CreateTexture2D(&texDesc, &texInit, &whiteTex))) return false;
    if (FAILED(device_->CreateShaderResourceView(whiteTex.Get(), nullptr, &whiteTexture_))) return false;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device_->CreateSamplerState(&sd, &sampler_))) return false;

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&rd, &rasterState_))) return false;

    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = FALSE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device_->CreateDepthStencilState(&dd, &depthDisabled_))) return false;

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&bd, &alphaBlend_))) return false;

    D3D11_BLEND_DESC abd{};
    abd.RenderTarget[0].BlendEnable = TRUE;
    abd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    abd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    abd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    abd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    abd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    abd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    abd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&abd, &additiveBlend_))) return false;

    initialized_ = true;
    return true;
}

void SpriteRenderer::Shutdown()
{
    additiveBlend_.Reset();
    alphaBlend_.Reset();
    depthDisabled_.Reset();
    rasterState_.Reset();
    sampler_.Reset();
    whiteTexture_.Reset();
    vertexBuffer_.Reset();
    frameCB_.Reset();
    inputLayout_.Reset();
    ps_.Reset();
    vs_.Reset();
    context_.Reset();
    device_.Reset();
    initialized_ = false;
}

void SpriteRenderer::Begin(const XMMATRIX& viewProjection, bool additive)
{
    if (!initialized_) return;

    SpriteFrameCB cb{};
    cb.viewProj = viewProjection;
    context_->UpdateSubresource(frameCB_.Get(), 0, nullptr, &cb, 0, 0);

    float blendFactor[4]{ 0, 0, 0, 0 };
    context_->OMSetBlendState(additive ? additiveBlend_.Get() : alphaBlend_.Get(), blendFactor, 0xffffffff);
    context_->OMSetDepthStencilState(depthDisabled_.Get(), 0);
    context_->RSSetState(rasterState_.Get());
    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->PSSetShader(ps_.Get(), nullptr, 0);

    ID3D11Buffer* cbPtr = frameCB_.Get();
    context_->VSSetConstantBuffers(0, 1, &cbPtr);
    ID3D11SamplerState* sampler = sampler_.Get();
    context_->PSSetSamplers(0, 1, &sampler);
}

void SpriteRenderer::End()
{
    if (!initialized_) return;
    ID3D11ShaderResourceView* nullSrv = nullptr;
    context_->PSSetShaderResources(0, 1, &nullSrv);
}

void SpriteRenderer::DrawQuad(ID3D11ShaderResourceView* texture, V2 center, V2 size, float rotation, Color tint, float depth)
{
    if (!initialized_) return;

    const float hx = size.x * 0.5f;
    const float hz = size.z * 0.5f;
    const float c = std::cos(rotation);
    const float s = std::sin(rotation);

    auto transform = [&](float x, float z)
    {
        return V2{ center.x + x * c - z * s, center.z + x * s + z * c };
    };

    const V2 p0 = transform(-hx, -hz);
    const V2 p1 = transform(hx, -hz);
    const V2 p2 = transform(hx, hz);
    const V2 p3 = transform(-hx, hz);

    std::vector<SpriteVertex> vertices;
    vertices.reserve(6);
    vertices.push_back(MakeVertex(p0.x, p0.z, depth, 0.0f, 1.0f, tint));
    vertices.push_back(MakeVertex(p1.x, p1.z, depth, 1.0f, 1.0f, tint));
    vertices.push_back(MakeVertex(p2.x, p2.z, depth, 1.0f, 0.0f, tint));
    vertices.push_back(MakeVertex(p0.x, p0.z, depth, 0.0f, 1.0f, tint));
    vertices.push_back(MakeVertex(p2.x, p2.z, depth, 1.0f, 0.0f, tint));
    vertices.push_back(MakeVertex(p3.x, p3.z, depth, 0.0f, 0.0f, tint));
    Submit(vertices, texture);
}

void SpriteRenderer::DrawCircle(V2 center, float radius, Color tint, float depth, int segments)
{
    if (!initialized_ || radius <= 0.0f) return;
    segments = std::max(8, std::min(segments, 96));

    std::vector<SpriteVertex> vertices;
    vertices.reserve(static_cast<size_t>(segments) * 3);
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = TwoPi * static_cast<float>(i) / segments;
        const float a1 = TwoPi * static_cast<float>(i + 1) / segments;
        vertices.push_back(MakeVertex(center.x, center.z, depth, 0.5f, 0.5f, tint));
        vertices.push_back(MakeVertex(center.x + std::cos(a0) * radius, center.z + std::sin(a0) * radius, depth, 0.0f, 0.0f, tint));
        vertices.push_back(MakeVertex(center.x + std::cos(a1) * radius, center.z + std::sin(a1) * radius, depth, 1.0f, 0.0f, tint));
    }
    Submit(vertices, nullptr);
}

void SpriteRenderer::DrawRing(V2 center, float radius, float thickness, Color tint, float depth, int segments)
{
    if (!initialized_ || radius <= 0.0f || thickness <= 0.0f) return;
    segments = std::max(8, std::min(segments, 128));
    const float inner = std::max(0.01f, radius - thickness * 0.5f);
    const float outer = radius + thickness * 0.5f;

    std::vector<SpriteVertex> vertices;
    vertices.reserve(static_cast<size_t>(segments) * 6);
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = TwoPi * static_cast<float>(i) / segments;
        const float a1 = TwoPi * static_cast<float>(i + 1) / segments;
        const V2 o0{ center.x + std::cos(a0) * outer, center.z + std::sin(a0) * outer };
        const V2 o1{ center.x + std::cos(a1) * outer, center.z + std::sin(a1) * outer };
        const V2 i0{ center.x + std::cos(a0) * inner, center.z + std::sin(a0) * inner };
        const V2 i1{ center.x + std::cos(a1) * inner, center.z + std::sin(a1) * inner };
        vertices.push_back(MakeVertex(o0.x, o0.z, depth, 0.0f, 0.0f, tint));
        vertices.push_back(MakeVertex(o1.x, o1.z, depth, 1.0f, 0.0f, tint));
        vertices.push_back(MakeVertex(i1.x, i1.z, depth, 1.0f, 1.0f, tint));
        vertices.push_back(MakeVertex(o0.x, o0.z, depth, 0.0f, 0.0f, tint));
        vertices.push_back(MakeVertex(i1.x, i1.z, depth, 1.0f, 1.0f, tint));
        vertices.push_back(MakeVertex(i0.x, i0.z, depth, 0.0f, 1.0f, tint));
    }
    Submit(vertices, nullptr);
}

void SpriteRenderer::DrawArc(V2 center, float radius, float thickness, float angle, float arc, Color tint, float depth, int segments)
{
    if (!initialized_ || radius <= 0.0f || thickness <= 0.0f || arc <= 0.0f) return;
    segments = std::max(4, std::min(segments, 96));
    const float inner = std::max(0.01f, radius - thickness * 0.5f);
    const float outer = radius + thickness * 0.5f;
    const float start = angle - arc * 0.5f;

    std::vector<SpriteVertex> vertices;
    vertices.reserve(static_cast<size_t>(segments) * 6);
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = start + arc * static_cast<float>(i) / segments;
        const float a1 = start + arc * static_cast<float>(i + 1) / segments;
        const V2 o0{ center.x + std::cos(a0) * outer, center.z + std::sin(a0) * outer };
        const V2 o1{ center.x + std::cos(a1) * outer, center.z + std::sin(a1) * outer };
        const V2 i0{ center.x + std::cos(a0) * inner, center.z + std::sin(a0) * inner };
        const V2 i1{ center.x + std::cos(a1) * inner, center.z + std::sin(a1) * inner };
        vertices.push_back(MakeVertex(o0.x, o0.z, depth, 0.0f, 0.0f, tint));
        vertices.push_back(MakeVertex(o1.x, o1.z, depth, 1.0f, 0.0f, tint));
        vertices.push_back(MakeVertex(i1.x, i1.z, depth, 1.0f, 1.0f, tint));
        vertices.push_back(MakeVertex(o0.x, o0.z, depth, 0.0f, 0.0f, tint));
        vertices.push_back(MakeVertex(i1.x, i1.z, depth, 1.0f, 1.0f, tint));
        vertices.push_back(MakeVertex(i0.x, i0.z, depth, 0.0f, 1.0f, tint));
    }
    Submit(vertices, nullptr);
}

void SpriteRenderer::Submit(const std::vector<SpriteVertex>& vertices, ID3D11ShaderResourceView* texture)
{
    if (!initialized_ || vertices.empty()) return;
    if (vertices.size() > MaxSpriteVertices) return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(vertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    std::memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(SpriteVertex));
    context_->Unmap(vertexBuffer_.Get(), 0);

    UINT stride = sizeof(SpriteVertex);
    UINT offset = 0;
    ID3D11Buffer* vb = vertexBuffer_.Get();
    context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

    ID3D11ShaderResourceView* srv = texture ? texture : whiteTexture_.Get();
    context_->PSSetShaderResources(0, 1, &srv);
    context_->Draw(static_cast<UINT>(vertices.size()), 0);
}

SpriteRenderer::SpriteVertex SpriteRenderer::MakeVertex(float x, float y, float depth, float u, float v, Color tint) const
{
    return { { x, y, depth }, { u, v }, ToFloat4(tint) };
}
