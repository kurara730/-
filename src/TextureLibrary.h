#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstddef>
#include <string>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

// 読み込んだ画像1枚分の情報です。
// shaderResource が空の場合は、表示側が図形フォールバックを使います。
struct TextureAsset
{
    std::wstring id;
    std::wstring path;
    UINT width = 0;
    UINT height = 0;
    ComPtr<ID3D11ShaderResourceView> shaderResource;
};

// 画像IDとファイルパスを対応させ、WICでDX11用テクスチャへ読み込みます。
class TextureLibrary
{
public:
    void Register(std::wstring id, std::wstring path);
    void LoadAll(ID3D11Device* device);
    const TextureAsset* Find(const std::wstring& id) const;
    size_t Count() const { return textures_.size(); }

private:
    std::unordered_map<std::wstring, TextureAsset> textures_;
};
