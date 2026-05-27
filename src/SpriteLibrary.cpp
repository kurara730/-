#include "SpriteLibrary.h"

#include <utility>

void SpriteLibrary::Register(std::wstring id, std::wstring textureId, DirectX::XMFLOAT4 uv)
{
    SpriteAsset asset{};
    asset.id = id;
    asset.textureId = std::move(textureId);
    asset.uv = uv;
    sprites_[std::move(id)] = std::move(asset);
}

const SpriteAsset* SpriteLibrary::Find(const std::wstring& id) const
{
    const auto it = sprites_.find(id);
    if (it == sprites_.end()) return nullptr;
    return &it->second;
}
