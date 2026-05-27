#include "TextureLibrary.h"

#include <wincodec.h>

#include <utility>
#include <vector>

namespace
{
bool LoadTextureWic(ID3D11Device* device, IWICImagingFactory* factory, TextureAsset& asset)
{
    if (!device || !factory || asset.path.empty()) return false;

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        asset.path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr)) return false;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return false;

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return false;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return false;

    UINT width = 0;
    UINT height = 0;
    converter->GetSize(&width, &height);
    if (width == 0 || height == 0) return false;

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) return false;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels.data();
    init.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> tex;
    hr = device->CreateTexture2D(&desc, &init, &tex);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(tex.Get(), &srvDesc, &asset.shaderResource);
    if (FAILED(hr)) return false;

    asset.width = width;
    asset.height = height;
    return true;
}
}

void TextureLibrary::Register(std::wstring id, std::wstring path)
{
    TextureAsset asset{};
    asset.id = id;
    asset.path = std::move(path);
    textures_[std::move(id)] = std::move(asset);
}

void TextureLibrary::LoadAll(ID3D11Device* device)
{
    ComPtr<IWICImagingFactory> factory;
    const HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return;

    for (auto& pair : textures_)
    {
        LoadTextureWic(device, factory.Get(), pair.second);
    }
}

const TextureAsset* TextureLibrary::Find(const std::wstring& id) const
{
    const auto it = textures_.find(id);
    if (it == textures_.end()) return nullptr;
    return &it->second;
}
