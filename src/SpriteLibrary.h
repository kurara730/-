#pragma once

#include <DirectXMath.h>

#include <string>
#include <unordered_map>

struct SpriteAsset
{
    std::wstring id;
    std::wstring textureId;
    DirectX::XMFLOAT4 uv{ 0.0f, 0.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT2 pivot{ 0.5f, 0.5f };
};

class SpriteLibrary
{
public:
    void Register(std::wstring id, std::wstring textureId, DirectX::XMFLOAT4 uv = { 0.0f, 0.0f, 1.0f, 1.0f });
    const SpriteAsset* Find(const std::wstring& id) const;

private:
    std::unordered_map<std::wstring, SpriteAsset> sprites_;
};
