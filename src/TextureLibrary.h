#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <string>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

struct TextureAsset
{
    std::wstring id;
    std::wstring path;
    UINT width = 0;
    UINT height = 0;
    ComPtr<ID3D11ShaderResourceView> shaderResource;
};

class TextureLibrary
{
public:
    void Register(std::wstring id, std::wstring path);
    void LoadAll(ID3D11Device* device);
    const TextureAsset* Find(const std::wstring& id) const;

private:
    std::unordered_map<std::wstring, TextureAsset> textures_;
};
