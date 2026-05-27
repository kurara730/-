#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include <memory>
#include <string>

#include "GameTypes.h"

class EffekseerSystem
{
public:
    EffekseerSystem();
    ~EffekseerSystem();

    EffekseerSystem(const EffekseerSystem&) = delete;
    EffekseerSystem& operator=(const EffekseerSystem&) = delete;

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    void LoadEffect(const std::wstring& id, const std::wstring& relativePath, float magnification = 1.0f);
    bool Play(const std::wstring& id, V2 position, float y = 0.45f, float rotationY = 0.0f, float scale = 1.0f);
    void Update(float dt);
    void Draw(const DirectX::XMMATRIX& viewProjection);

    bool Available() const;
    const std::wstring& LastError() const { return lastError_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::wstring lastError_;
};
