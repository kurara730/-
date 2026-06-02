#include "SweetsApp.h"

#include <filesystem>

// GraphicsDevice.cpp は、DX11/D2D/DWrite の低レベル初期化をまとめたファイルです。
// ゲームルールは扱わず、画面へ何かを出すための土台だけを作ります。

namespace
{
// TAA用のジッター値です。MenuView側にも同じ考え方の表示用処理があります。
float Halton(int index, int base)
{
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0)
    {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }
    return r;
}

// シェーダや画像などのファイルを、実行ファイル周辺と作業ディレクトリから探します。
std::wstring FindAssetFile(const std::wstring& relativePath)
{
    namespace fs = std::filesystem;
    const fs::path rel(relativePath);

    std::array<fs::path, 5> candidates{};
    candidates[0] = fs::current_path() / rel;

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    const fs::path exeDir = fs::path(modulePath).parent_path();
    candidates[1] = exeDir / rel;
    candidates[2] = exeDir.parent_path() / rel;
    candidates[3] = exeDir.parent_path().parent_path() / rel;
    candidates[4] = rel;

    for (const auto& path : candidates)
    {
        std::error_code ec;
        if (fs::exists(path, ec))
        {
            return path.wstring();
        }
    }
    return relativePath;
}

bool PointInRect(float sx, float sy, float left, float top, float right, float bottom)
{
    return sx >= left && sx <= right && sy >= top && sy <= bottom;
}

float GameplayHalfHeight()
{
    return 11.5f;
}

float GameplayHalfWidth(UINT width, UINT height)
{
    const float aspect = static_cast<float>(std::max(1u, width)) / std::max(1.0f, static_cast<float>(height));
    return GameplayHalfHeight() * aspect;
}

const wchar_t* CharacterSpriteId(CharacterType type)
{
    switch (type)
    {
    case CharacterType::Chocolate: return L"2d_player_chocolate";
    case CharacterType::Cheese: return L"2d_player_cheese";
    case CharacterType::Roll: return L"2d_player_roll";
    case CharacterType::Shortcake:
    default: return L"2d_player_shortcake";
    }
}

const wchar_t* EnemySpriteId(EnemyType type)
{
    switch (type)
    {
    case EnemyType::Shield: return L"2d_enemy_shield";
    case EnemyType::Split: return L"2d_enemy_split";
    case EnemyType::Healer: return L"2d_enemy_healer";
    case EnemyType::Barrier: return L"2d_enemy_barrier";
    case EnemyType::Mirror: return L"2d_enemy_mirror";
    case EnemyType::Mine: return L"2d_enemy_mine";
    case EnemyType::Teleport: return L"2d_enemy_teleport";
    case EnemyType::Normal:
    default: return L"2d_enemy_normal";
    }
}

const wchar_t* PickupSpriteId(PickupType type)
{
    switch (type)
    {
    case PickupType::Slow: return L"2d_pickup_slow";
    case PickupType::Invincible: return L"2d_pickup_invincible";
    case PickupType::Magnet: return L"2d_pickup_magnet";
    case PickupType::BombDamage: return L"2d_pickup_bomb";
    case PickupType::Heal: return L"2d_pickup_heal";
    case PickupType::UltFull: return L"2d_pickup_ult";
    case PickupType::Spread: return L"2d_pickup_spread";
    case PickupType::Speed: return L"2d_pickup_speed";
    case PickupType::ScoreDouble: return L"2d_pickup_score";
    case PickupType::Attack:
    default: return L"2d_pickup_attack";
    }
}
}

// DX11デバイス、スワップチェーン、D2D/DWriteを作ります。
// ここが失敗すると画面を作れないため、例外で起動を止めます。
void SweetsApp::CreateDevice()
{
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = width_;
    scd.BufferDesc.Height = height_;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd_;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    wchar_t enableDebugLayer[8]{};
    const DWORD debugLayerLen = GetEnvironmentVariableW(
        L"SWEETS_D3D_DEBUG",
        enableDebugLayer,
        static_cast<DWORD>(sizeof(enableDebugLayer) / sizeof(enableDebugLayer[0])));
    if (debugLayerLen > 0 && enableDebugLayer[0] != L'0')
    {
        flags |= D3D11_CREATE_DEVICE_DEBUG;
    }
#endif
    std::array<D3D_FEATURE_LEVEL, 3> levels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1
    };
    D3D_FEATURE_LEVEL created{};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels.data(),
        static_cast<UINT>(levels.size()),
        D3D11_SDK_VERSION,
        &scd,
        &swapChain_,
        &device_,
        &created,
        &context_);
    if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG))
    {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            levels.data(),
            static_cast<UINT>(levels.size()),
            D3D11_SDK_VERSION,
            &scd,
            &swapChain_,
            &device_,
            &created,
            &context_);
    }
    ThrowIfFailed(hr, "D3D11CreateDeviceAndSwapChain");

    ComPtr<IDXGIDevice> dxgiDevice;
    ThrowIfFailed(device_.As(&dxgiDevice), "Query IDXGIDevice");
    ComPtr<IDXGIAdapter> dxgiAdapter;
    ComPtr<IDXGIFactory> dxgiFactory;
    if (SUCCEEDED(dxgiDevice->GetAdapter(&dxgiAdapter)) &&
        SUCCEEDED(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))))
    {
        dxgiFactory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
    }

    ID2D1Factory1* rawFactory = nullptr;
    ThrowIfFailed(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        nullptr,
        reinterpret_cast<void**>(&rawFactory)), "D2D1CreateFactory");
    d2dFactory_.Attach(rawFactory);

    ThrowIfFailed(d2dFactory_->CreateDevice(dxgiDevice.Get(), &d2dDevice_), "Create D2D device");
    ThrowIfFailed(d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_), "Create D2D context");
    const HRESULT wicHr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wicFactory_));
    if (FAILED(wicHr))
    {
        wicFactory_.Reset();
#if defined(_DEBUG)
        OutputDebugStringW(L"SweetsActionDX11: WIC factory creation failed. Title still image will use fallback.\n");
#endif
    }

    ThrowIfFailed(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(writeFactory_.GetAddressOf())), "DWriteCreateFactory");

    ThrowIfFailed(writeFactory_->CreateTextFormat(
        L"Yu Gothic UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        20.0f,
        L"ja-jp",
        &hudFormat_), "Create HUD format");
    ThrowIfFailed(writeFactory_->CreateTextFormat(
        L"Yu Gothic UI",
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        48.0f,
        L"ja-jp",
        &titleFormat_), "Create title format");
    ThrowIfFailed(writeFactory_->CreateTextFormat(
        L"Yu Gothic UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        15.0f,
        L"ja-jp",
        &smallFormat_), "Create small format");
}

// バックバッファと各種オフスクリーンターゲットを作ります。
// 画面サイズが変わるたびに作り直す必要があります。
void SweetsApp::CreateFrameTargets()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)), "Get swapchain buffer");
    backBufferTex_ = backBuffer;
    ThrowIfFailed(device_->CreateRenderTargetView(backBufferTex_.Get(), nullptr, &rtv_), "Create RTV");

    CreateOffscreenTarget(sceneColorTex_, sceneColorRtv_, sceneColorSrv_, DXGI_FORMAT_B8G8R8A8_UNORM);
    // 加算エフェクトは HDR(float)。明るい芯が 1.0 を超えてブルームに反映される。
    CreateOffscreenTarget(additiveTex_, additiveRtv_, additiveSrv_, DXGI_FORMAT_R16G16B16A16_FLOAT);
    CreateOffscreenTarget(historyTex_, historyRtv_, historySrv_, DXGI_FORMAT_B8G8R8A8_UNORM);
    CreateOffscreenTarget(resolvedTex_, resolvedRtv_, resolvedSrv_, DXGI_FORMAT_B8G8R8A8_UNORM);

    // ブルーム用の半解像度 HDR ターゲット(A/B でピンポン)。
    bloomWidth_ = (width_ + 1) / 2;
    bloomHeight_ = (height_ + 1) / 2;
    CreateOffscreenTargetSized(bloomTexA_, bloomRtvA_, bloomSrvA_, DXGI_FORMAT_R16G16B16A16_FLOAT, bloomWidth_, bloomHeight_);
    CreateOffscreenTargetSized(bloomTexB_, bloomRtvB_, bloomSrvB_, DXGI_FORMAT_R16G16B16A16_FLOAT, bloomWidth_, bloomHeight_);

    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = width_;
    depthDesc.Height = height_;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ThrowIfFailed(device_->CreateTexture2D(&depthDesc, nullptr, &depthTex_), "Create depth texture");
    ThrowIfFailed(device_->CreateDepthStencilView(depthTex_.Get(), nullptr, &dsv_), "Create DSV");

    ComPtr<IDXGISurface> surface;
    ThrowIfFailed(swapChain_->GetBuffer(0, IID_PPV_ARGS(&surface)), "Get DXGI surface");

    const UINT windowDpi = GetDpiForWindow(hwnd_);
    const FLOAT dpiX = windowDpi > 0 ? static_cast<FLOAT>(windowDpi) : 96.0f;
    const FLOAT dpiY = dpiX;
    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX,
        dpiY);
    ThrowIfFailed(d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(), &props, &d2dTarget_), "Create D2D target");
    d2dContext_->SetTarget(d2dTarget_.Get());

    ThrowIfFailed(d2dContext_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &textBrush_), "Create text brush");
    if (titleAssetsLoaded_)
    {
        LoadTitleImageBitmap();
    }
}

// 画面サイズと同じ一時描画先を作る共通処理です。
// シーン色、加算FX、履歴など、用途ごとに同じ形で確保します。
void SweetsApp::CreateOffscreenTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& rtv, ComPtr<ID3D11ShaderResourceView>& srv, DXGI_FORMAT format)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width_;
    desc.Height = height_;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ThrowIfFailed(device_->CreateTexture2D(&desc, nullptr, &texture), "Create offscreen texture");
    ThrowIfFailed(device_->CreateRenderTargetView(texture.Get(), nullptr, &rtv), "Create offscreen RTV");
    ThrowIfFailed(device_->CreateShaderResourceView(texture.Get(), nullptr, &srv), "Create offscreen SRV");
}

void SweetsApp::CreateOffscreenTargetSized(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& rtv, ComPtr<ID3D11ShaderResourceView>& srv, DXGI_FORMAT format, UINT w, UINT h)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = (w < 1u) ? 1u : w;
    desc.Height = (h < 1u) ? 1u : h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ThrowIfFailed(device_->CreateTexture2D(&desc, nullptr, &texture), "Create sized offscreen texture");
    ThrowIfFailed(device_->CreateRenderTargetView(texture.Get(), nullptr, &rtv), "Create sized offscreen RTV");
    ThrowIfFailed(device_->CreateShaderResourceView(texture.Get(), nullptr, &srv), "Create sized offscreen SRV");
}

// リサイズ前に、画面サイズ依存のリソースを解放します。
// 古い参照が残っていると ResizeBuffers に失敗するため、まとめてResetします。
void SweetsApp::ReleaseFrameTargets()
{
    if (d2dContext_) d2dContext_->SetTarget(nullptr);
    titleVideoBitmap_.Reset();
    titleImageBitmap_.Reset();
    eventVideoBitmap_.Reset();
    textBrush_.Reset();
    d2dTarget_.Reset();
    dsv_.Reset();
    depthTex_.Reset();
    resolvedSrv_.Reset();
    resolvedRtv_.Reset();
    resolvedTex_.Reset();
    bloomSrvA_.Reset();
    bloomRtvA_.Reset();
    bloomTexA_.Reset();
    bloomSrvB_.Reset();
    bloomRtvB_.Reset();
    bloomTexB_.Reset();
    historySrv_.Reset();
    historyRtv_.Reset();
    historyTex_.Reset();
    additiveSrv_.Reset();
    additiveRtv_.Reset();
    additiveTex_.Reset();
    sceneColorSrv_.Reset();
    sceneColorRtv_.Reset();
    sceneColorTex_.Reset();
    rtv_.Reset();
    backBufferTex_.Reset();
}

// ウィンドウサイズ変更時の処理です。
// DX11ターゲット、D2Dターゲット、スプライト表示範囲を新しいサイズへ合わせます。
void SweetsApp::Resize(UINT w, UINT h)
{
    if (w == 0 || h == 0) return;
    width_ = w;
    height_ = h;
    ReleaseFrameTargets();
    ThrowIfFailed(swapChain_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");
    CreateFrameTargets();
}

// HLSLシェーダ、入力レイアウト、ブレンド/深度/サンプラ状態を作ります。
// 2Dスプライト、3Dメッシュ、合成、ブルームなどの表示で使い分けます。
void SweetsApp::CreateShadersAndStates()
{
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> postVsBlob;
    ComPtr<ID3DBlob> postPsBlob;
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif
    const std::wstring shaderPath = FindAssetFile(L"assets/shaders/basic_lit.hlsl");
    HRESULT hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", flags, 0, &vsBlob, &errors);
    if (FAILED(hr))
    {
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "VS compile failed");
    }
    errors.Reset();
    hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", flags, 0, &psBlob, &errors);
    if (FAILED(hr))
    {
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "PS compile failed");
    }

    ThrowIfFailed(device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs_), "Create VS");
    ThrowIfFailed(device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps_), "Create PS");

    const std::wstring postPath = FindAssetFile(L"assets/shaders/postprocess.hlsl");
    errors.Reset();
    hr = D3DCompileFromFile(postPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", flags, 0, &postVsBlob, &errors);
    if (FAILED(hr))
    {
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Post VS compile failed");
    }
    errors.Reset();
    hr = D3DCompileFromFile(postPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", flags, 0, &postPsBlob, &errors);
    if (FAILED(hr))
    {
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Post PS compile failed");
    }
    ThrowIfFailed(device_->CreateVertexShader(postVsBlob->GetBufferPointer(), postVsBlob->GetBufferSize(), nullptr, &postVs_), "Create post VS");
    ThrowIfFailed(device_->CreatePixelShader(postPsBlob->GetBufferPointer(), postPsBlob->GetBufferSize(), nullptr, &postPs_), "Create post PS");

    // ブルーム用シェーダ(VSMain / PrefilterPS / BlurPS)
    const std::wstring bloomPath = FindAssetFile(L"assets/shaders/bloom.hlsl");
    ComPtr<ID3DBlob> bloomVsBlob;
    ComPtr<ID3DBlob> bloomPreBlob;
    ComPtr<ID3DBlob> bloomBlurBlob;
    errors.Reset();
    hr = D3DCompileFromFile(bloomPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", flags, 0, &bloomVsBlob, &errors);
    if (FAILED(hr))
    {
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Bloom VS compile failed");
    }
    errors.Reset();
    hr = D3DCompileFromFile(bloomPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PrefilterPS", "ps_5_0", flags, 0, &bloomPreBlob, &errors);
    if (FAILED(hr))
    {
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Bloom prefilter PS compile failed");
    }
    errors.Reset();
    hr = D3DCompileFromFile(bloomPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "BlurPS", "ps_5_0", flags, 0, &bloomBlurBlob, &errors);
    if (FAILED(hr))
    {
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Bloom blur PS compile failed");
    }
    ThrowIfFailed(device_->CreateVertexShader(bloomVsBlob->GetBufferPointer(), bloomVsBlob->GetBufferSize(), nullptr, &bloomVs_), "Create bloom VS");
    ThrowIfFailed(device_->CreatePixelShader(bloomPreBlob->GetBufferPointer(), bloomPreBlob->GetBufferSize(), nullptr, &bloomPrefilterPs_), "Create bloom prefilter PS");
    ThrowIfFailed(device_->CreatePixelShader(bloomBlurBlob->GetBufferPointer(), bloomBlurBlob->GetBufferSize(), nullptr, &bloomBlurPs_), "Create bloom blur PS");

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ThrowIfFailed(device_->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_), "Create input layout");

    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = sizeof(FrameCB);
    cb.Usage = D3D11_USAGE_DEFAULT;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ThrowIfFailed(device_->CreateBuffer(&cb, nullptr, &frameCB_), "Create frame CB");
    cb.ByteWidth = sizeof(ObjectCB);
    ThrowIfFailed(device_->CreateBuffer(&cb, nullptr, &objectCB_), "Create object CB");
    cb.ByteWidth = sizeof(PostCB);
    ThrowIfFailed(device_->CreateBuffer(&cb, nullptr, &postCB_), "Create post CB");
    cb.ByteWidth = sizeof(BloomCB);
    ThrowIfFailed(device_->CreateBuffer(&cb, nullptr, &bloomCB_), "Create bloom CB");

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    ThrowIfFailed(device_->CreateRasterizerState(&rd, &rasterState_), "Create rasterizer");

    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ThrowIfFailed(device_->CreateDepthStencilState(&dd, &depthState_), "Create depth state");

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ThrowIfFailed(device_->CreateBlendState(&bd, &alphaBlend_), "Create blend state");

    D3D11_BLEND_DESC abd{};
    abd.RenderTarget[0].BlendEnable = TRUE;
    abd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    abd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    abd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    abd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    abd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    abd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    abd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ThrowIfFailed(device_->CreateBlendState(&abd, &additiveBlend_), "Create additive blend state");

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ThrowIfFailed(device_->CreateSamplerState(&sd, &postSampler_), "Create post sampler");

    const std::wstring spritePath = FindAssetFile(L"assets/shaders/sprite_unlit.hlsl");
    if (!spriteCanvas_.Initialize(device_.Get(), context_.Get(), spritePath))
    {
        OutputDebugStringW(L"SweetsActionDX11: sprite canvas initialization failed. Gameplay uses 2D shape fallbacks where possible.\n");
    }
}

Mesh SweetsApp::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    Mesh mesh{};
    D3D11_BUFFER_DESC vb{};
    vb.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    vb.Usage = D3D11_USAGE_DEFAULT;
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vdata{};
    vdata.pSysMem = vertices.data();
    ThrowIfFailed(device_->CreateBuffer(&vb, &vdata, &mesh.vb), "Create VB");

    D3D11_BUFFER_DESC ib{};
    ib.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ib.Usage = D3D11_USAGE_DEFAULT;
    ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA idata{};
    idata.pSysMem = indices.data();
    ThrowIfFailed(device_->CreateBuffer(&ib, &idata, &mesh.ib), "Create IB");
    mesh.indexCount = static_cast<UINT>(indices.size());
    return mesh;
}

// 3D表示やフォールバック表示に使う基本メッシュを作ります。
// 2Dメインでも、3D切替やデバッグ用途で残しています。
void SweetsApp::CreateMeshes()
{
    {
        std::vector<Vertex> v;
        std::vector<uint32_t> i;
        const int slices = 32;
        const int stacks = 16;
        for (int y = 0; y <= stacks; ++y)
        {
            const float vf = static_cast<float>(y) / stacks;
            const float phi = vf * Pi;
            const float sy = std::cos(phi);
            const float rr = std::sin(phi);
            for (int x = 0; x <= slices; ++x)
            {
                const float uf = static_cast<float>(x) / slices;
                const float theta = uf * TwoPi;
                XMFLOAT3 n{ rr * std::cos(theta), sy, rr * std::sin(theta) };
                v.push_back({ n, n, {1, 1, 1, 1} });
            }
        }
        for (int y = 0; y < stacks; ++y)
        {
            for (int x = 0; x < slices; ++x)
            {
                const uint32_t a = y * (slices + 1) + x;
                const uint32_t b = a + 1;
                const uint32_t c = a + slices + 1;
                const uint32_t d = c + 1;
                i.insert(i.end(), { a, c, b, b, c, d });
            }
        }
        sphereMesh_ = CreateMesh(v, i);
    }

    {
        std::vector<Vertex> v;
        std::vector<uint32_t> i;
        const int n = 64;
        for (int s = 0; s <= n; ++s)
        {
            const float a = TwoPi * s / n;
            const float x = std::cos(a);
            const float z = std::sin(a);
            v.push_back({ { x, 0, z }, { x, 0, z }, {1, 1, 1, 1} });
            v.push_back({ { x, 1, z }, { x, 0, z }, {1, 1, 1, 1} });
        }
        for (int s = 0; s < n; ++s)
        {
            const uint32_t a = s * 2;
            i.insert(i.end(), { a, a + 1, a + 2, a + 2, a + 1, a + 3 });
        }
        const uint32_t topCenter = static_cast<uint32_t>(v.size());
        v.push_back({ {0, 1, 0}, {0, 1, 0}, {1, 1, 1, 1} });
        const uint32_t bottomCenter = static_cast<uint32_t>(v.size());
        v.push_back({ {0, 0, 0}, {0, -1, 0}, {1, 1, 1, 1} });
        for (int s = 0; s < n; ++s)
        {
            const uint32_t a = s * 2;
            const uint32_t b = ((s + 1) % n) * 2;
            i.insert(i.end(), { topCenter, a + 1, b + 1, bottomCenter, b, a });
        }
        cylinderMesh_ = CreateMesh(v, i);
    }

    {
        std::vector<Vertex> v;
        std::vector<uint32_t> i;
        const int n = 96;
        v.push_back({ {0, 0, 0}, {0, 1, 0}, {1, 1, 1, 1} });
        for (int s = 0; s <= n; ++s)
        {
            const float a = TwoPi * s / n;
            v.push_back({ {std::cos(a), 0, std::sin(a)}, {0, 1, 0}, {1, 1, 1, 1} });
        }
        for (int s = 1; s <= n; ++s)
        {
            i.insert(i.end(), { 0, static_cast<uint32_t>(s), static_cast<uint32_t>(s + 1) });
        }
        floorMesh_ = CreateMesh(v, i);
    }

    {
        std::vector<Vertex> v;
        std::vector<uint32_t> i;
        const int n = 96;
        constexpr float inner = 0.94f;
        for (int s = 0; s <= n; ++s)
        {
            const float a = TwoPi * s / n;
            const float ca = std::cos(a);
            const float sa = std::sin(a);
            v.push_back({ {ca * inner, 0.0f, sa * inner}, {0, 1, 0}, {1, 1, 1, 1} });
            v.push_back({ {ca, 0.0f, sa}, {0, 1, 0}, {1, 1, 1, 1} });
        }
        for (int s = 0; s < n; ++s)
        {
            const uint32_t a = s * 2;
            i.insert(i.end(), { a, a + 1, a + 2, a + 2, a + 1, a + 3 });
        }
        ringMesh_ = CreateMesh(v, i);
    }

    {
        const std::array<XMFLOAT3, 8> p{ {
            {-0.5f,-0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{ 0.5f,-0.5f,-0.5f},
            {-0.5f,-0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f},
        } };
        std::vector<Vertex> v;
        std::vector<uint32_t> i;
        auto face = [&](int a, int b, int c, int d, XMFLOAT3 n)
        {
            const uint32_t base = static_cast<uint32_t>(v.size());
            v.push_back({ p[a], n, {1,1,1,1} });
            v.push_back({ p[b], n, {1,1,1,1} });
            v.push_back({ p[c], n, {1,1,1,1} });
            v.push_back({ p[d], n, {1,1,1,1} });
            i.insert(i.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
        };
        face(0, 1, 2, 3, {0, 0, -1});
        face(7, 6, 5, 4, {0, 0, 1});
        face(4, 5, 1, 0, {-1, 0, 0});
        face(3, 2, 6, 7, {1, 0, 0});
        face(1, 5, 6, 2, {0, 1, 0});
        face(4, 0, 3, 7, {0, -1, 0});
        cubeMesh_ = CreateMesh(v, i);
    }

    {
        std::vector<Vertex> v;
        std::vector<uint32_t> i;
        const int n = 28;
        const float half = 0.65f;
        v.push_back({ {0, 0, 0}, {0, 1, 0}, {1, 1, 1, 0.5f} });
        for (int s = 0; s <= n; ++s)
        {
            const float a = -half + (half * 2.0f) * s / n;
            v.push_back({ {std::cos(a), 0, std::sin(a)}, {0, 1, 0}, {1, 1, 1, 0.5f} });
        }
        for (int s = 1; s <= n; ++s)
        {
            i.insert(i.end(), { 0, static_cast<uint32_t>(s), static_cast<uint32_t>(s + 1) });
        }
        wedgeMesh_ = CreateMesh(v, i);
    }
}

