#include "SweetsApp.h"

#include <filesystem>

namespace
{
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

void SweetsApp::CreateRenderTargets()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)), "Get swapchain buffer");
    backBufferTex_ = backBuffer;
    ThrowIfFailed(device_->CreateRenderTargetView(backBufferTex_.Get(), nullptr, &rtv_), "Create RTV");

    CreateOffscreenTarget(sceneColorTex_, sceneColorRtv_, sceneColorSrv_, DXGI_FORMAT_B8G8R8A8_UNORM);
    CreateOffscreenTarget(additiveTex_, additiveRtv_, additiveSrv_, DXGI_FORMAT_B8G8R8A8_UNORM);
    CreateOffscreenTarget(historyTex_, historyRtv_, historySrv_, DXGI_FORMAT_B8G8R8A8_UNORM);
    CreateOffscreenTarget(resolvedTex_, resolvedRtv_, resolvedSrv_, DXGI_FORMAT_B8G8R8A8_UNORM);

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

void SweetsApp::ReleaseRenderTargets()
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

void SweetsApp::Resize(UINT w, UINT h)
{
    if (w == 0 || h == 0) return;
    width_ = w;
    height_ = h;
    ReleaseRenderTargets();
    ThrowIfFailed(swapChain_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");
    CreateRenderTargets();
}

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
    if (!spriteRenderer_.Initialize(device_.Get(), context_.Get(), spritePath))
    {
        OutputDebugStringW(L"SweetsActionDX11: sprite renderer initialization failed. Gameplay uses 2D shape fallbacks where possible.\n");
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

void SweetsApp::Render()
{
    DrawScene();
    effekseer_.Draw(view_ * proj_);
    DrawAdditiveScene();
    CompositeScene();
    DrawHud();
    swapChain_->Present(1, 0);
}

void SweetsApp::DrawScene()
{
    const float clear[4] = { 0.12f, 0.045f, 0.085f, 1.0f };
    ID3D11RenderTargetView* sceneTarget = sceneColorRtv_ ? sceneColorRtv_.Get() : rtv_.Get();
    context_->ClearRenderTargetView(sceneTarget, clear);
    context_->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    const float halfH = GameplayHalfHeight();
    const float halfW = GameplayHalfWidth(width_, height_);
    if (Use3DRules())
    {
        SyncAll3DState();
        const XMVECTOR eye = XMVectorSet(0.0f, 15.8f, -17.8f, 1.0f);
        const XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.8f, 1.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        view_ = XMMatrixLookAtLH(eye, at, up);
        proj_ = XMMatrixPerspectiveFovLH(XMConvertToRadians(48.0f), static_cast<float>(std::max(1u, width_)) / std::max(1.0f, static_cast<float>(height_)), 0.1f, 80.0f);
        cameraPos_ = { 0.0f, 15.8f, -17.8f };
    }
    else
    {
        view_ = XMMatrixIdentity();
        proj_ = XMMatrixOrthographicOffCenterLH(-halfW, halfW, -halfH, halfH, 0.0f, 10.0f);
        cameraPos_ = { 0.0f, 15.5f, -18.5f };
    }
#if defined(_DEBUG)
    if (debug_.taa)
    {
        const int frame = (debug_.taaFrame % 8) + 1;
        const float jitterX = (Halton(frame, 2) - 0.5f) * 2.0f / std::max(1.0f, static_cast<float>(width_));
        const float jitterY = (Halton(frame, 3) - 0.5f) * 2.0f / std::max(1.0f, static_cast<float>(height_));
        proj_ = proj_ * XMMatrixTranslation(jitterX, jitterY, 0.0f);
    }
#endif

    FrameCB frame{};
    frame.viewProj = view_ * proj_;
    frame.lightDir = XMFLOAT4(-0.35f, -1.0f, 0.55f, 0.0f);
    frame.cameraPos = XMFLOAT4(cameraPos_.x, cameraPos_.y, cameraPos_.z, 1.0f);
    context_->UpdateSubresource(frameCB_.Get(), 0, nullptr, &frame, 0, 0);

    ID3D11RenderTargetView* rtv = sceneTarget;
    context_->OMSetRenderTargets(1, &rtv, dsv_.Get());
    if (Use3DRules())
    {
        DrawGameplay3D();
        return;
    }
    spriteRenderer_.Begin(view_ * proj_, false);

    spriteRenderer_.DrawCircle({ 0.0f, 0.0f }, ArenaRadius + 1.3f, WithAlpha(Rose, 0.24f), 0.95f, 96);
    spriteRenderer_.DrawCircle({ 0.0f, 0.0f }, ArenaRadius, WithAlpha({ 0.18f, 0.07f, 0.12f, 1.0f }, 0.92f), 0.94f, 96);
    spriteRenderer_.DrawRing({ 0.0f, 0.0f }, ArenaRadius, 0.16f, WithAlpha(Gold, 0.76f), 0.20f, 128);
    spriteRenderer_.DrawRing({ 0.0f, 0.0f }, ArenaRadius * 0.68f, 0.035f, WithAlpha(Cream, 0.18f), 0.30f, 96);
    spriteRenderer_.DrawRing({ 0.0f, 0.0f }, ArenaRadius * 0.36f, 0.035f, WithAlpha(Cream, 0.13f), 0.30f, 72);

    for (const auto& o : obstacles_)
    {
        if (o.damageField)
        {
            spriteRenderer_.DrawRing(o.pos, o.radius, 0.18f, WithAlpha(Red, 0.68f), 0.32f);
        }
        else
        {
            const float size = o.radius * (o.cheeseWall ? 2.35f : 2.05f);
            DrawSprite2D(L"2d_obstacle_wall", o.pos, { size, size }, o.ttl * 0.35f, WithAlpha(o.color, 0.92f), 0.38f);
            spriteRenderer_.DrawRing(o.pos, o.radius, 0.08f, WithAlpha(Cream, 0.28f), 0.36f);
        }
    }
    for (const auto& p : pickups_)
    {
        const float bob = 1.0f + 0.10f * std::sin(gameTime_ * 5.0f + p.pos.x);
        DrawSprite2D(PickupSpriteId(p.pickupType), p.pos, { p.radius * 2.8f * bob, p.radius * 2.8f * bob }, gameTime_ * 1.6f, p.color, 0.34f);
        spriteRenderer_.DrawRing(p.pos, p.radius * 1.55f, 0.045f, WithAlpha(p.color, 0.46f), 0.37f, 40);
    }
    for (const auto& s : shots_)
    {
        const wchar_t* id = s.enemy ? L"2d_shot_enemy" : L"2d_shot_player";
        const float size = s.radius * (s.enemy ? 3.35f : 3.05f) * (s.charged ? 1.55f : 1.0f);
        DrawSprite2D(id, s.pos, { size, size }, AngleOf(s.vel), s.color, s.enemy ? 0.25f : 0.24f);
        if (s.enemy)
        {
            spriteRenderer_.DrawRing(s.pos, s.radius * 1.75f, 0.035f, WithAlpha(Cream, 0.35f), 0.27f, 24);
        }
        else if (s.reflected)
        {
            spriteRenderer_.DrawRing(s.pos, s.radius * (2.0f + 0.35f * s.reflectedCount), 0.045f, WithAlpha(Gold, 0.52f), 0.26f, 28);
        }
    }
    for (const auto& e : enemies_)
    {
        Color c = e.flash > 0.0f ? Cream : e.color;
        if (e.barrierT > 0.0f) c = Sky;
        DrawSprite2D(EnemySpriteId(e.type), e.pos, { e.radius * 2.45f, e.radius * 2.45f }, e.face, c, 0.22f);
        if (e.type == EnemyType::Teleport || e.type == EnemyType::Mirror || e.type == EnemyType::Barrier)
        {
            const Player* target = FindNearestPlayer(e.pos);
            const float face = target ? AngleOf(target->pos - e.pos) : e.face;
            DrawSprite2D(L"2d_shot_enemy", e.pos + FromAngle(face) * (e.radius * 0.68f), { e.radius * 0.55f, e.radius * 0.55f }, face, Red, 0.21f);
        }
    }
    if (boss_.active)
    {
        Color c = boss_.flash > 0.0f ? Cream : (boss_.bossType == BossType::HiddenBoss ? Grape : (boss_.bossType == BossType::DonutKing ? Sky : (boss_.bossType == BossType::MirrorMacaron ? Gold : Rose)));
        DrawSprite2D(boss_.bossType == BossType::HiddenBoss ? L"2d_boss_hidden" : L"2d_boss_normal", boss_.pos, { boss_.radius * 2.85f, boss_.radius * 2.85f }, boss_.spin * 0.35f, c, 0.18f);
        spriteRenderer_.DrawRing(boss_.pos, boss_.radius * 1.42f, 0.10f, WithAlpha(Red, 0.45f), 0.19f, 72);
        if (boss_.telegraphT > 0.0f && boss_.telegraphLife > 0.0f)
        {
            const float t = 1.0f - ClampFloat(boss_.telegraphT / boss_.telegraphLife, 0.0f, 1.0f);
            const Color telegraph = boss_.telegraphAttack == 0 ? Grape : (boss_.telegraphAttack == 1 ? Red : (boss_.telegraphAttack == 2 ? Gold : Mint));
            spriteRenderer_.DrawRing(boss_.pos, boss_.radius * (1.75f + t * 1.15f), 0.12f, WithAlpha(telegraph, 0.72f), 0.16f, 96);
            spriteRenderer_.DrawRing(boss_.pos, boss_.radius * (2.45f + t * 0.65f), 0.05f, WithAlpha(Cream, 0.48f), 0.15f, 96);
            if (boss_.telegraphAdd || boss_.telegraphMirror || boss_.telegraphField)
            {
                spriteRenderer_.DrawRing(boss_.pos, boss_.radius * (3.10f + t * 0.55f), 0.045f, WithAlpha(Sky, 0.42f), 0.15f, 96);
            }
        }
    }

    for (const auto& s : slashes_)
    {
        DrawSector(s);
    }

    for (int i = 0; i < MaxPlayers; ++i)
    {
        DrawUltimatePreview(players_[i], i);
    }

    for (const auto& p : players_)
    {
        if (!p.active) continue;
        const Color playerColor = Loadouts[static_cast<int>(p.character)].color;
        Color bodyColor = p.downed ? WithAlpha(Red, 0.65f) : (p.inv > 0.0f ? Cream : playerColor);
        Color faceColor = Cream;
        if (screen_ == Screen::HiddenBoss && !p.downed)
        {
            bodyColor = WithAlpha(bodyColor, 0.42f);
            faceColor = WithAlpha(faceColor, 0.45f);
        }
        DrawSprite2D(CharacterSpriteId(p.character), p.pos, { p.radius * 2.45f, p.radius * 2.45f }, p.face - Pi * 0.5f, bodyColor, 0.14f);
        DrawSprite2D(L"2d_shot_player", p.pos + FromAngle(p.face) * (p.radius * 0.88f), { p.radius * 0.55f, p.radius * 0.55f }, p.face, faceColor, 0.13f);
        if ((p.focus || screen_ == Screen::HiddenBoss) && !p.downed)
        {
            spriteRenderer_.DrawCircle(p.pos, p.hitboxRadius, Red, 0.11f, 20);
            spriteRenderer_.DrawRing(p.pos, p.grazeRadius, 0.045f, WithAlpha(Sky, p.grazeFlash > 0.0f ? 0.70f : 0.30f), 0.12f, 48);
        }
        if (p.shieldT > 0.0f)
        {
            spriteRenderer_.DrawRing(p.pos, p.radius * 2.1f, 0.10f, WithAlpha(Sky, 0.45f), 0.12f, 64);
        }
        if (p.bombT > 0.0f)
        {
            const float t = p.bombT / 1.8f;
            spriteRenderer_.DrawRing(p.pos, 1.5f + (1.0f - t) * 5.8f, 0.16f, WithAlpha(Sky, 0.55f * t), 0.10f, 96);
        }
    }

    for (const auto& p : particles_)
    {
        spriteRenderer_.DrawCircle(p.pos, 0.055f + p.y * 0.04f, WithAlpha(p.color, ClampFloat(p.ttl * 2.0f, 0.0f, 1.0f)), 0.09f, 12);
    }

#if defined(_DEBUG)
    if (debug_.overlays)
    {
        for (const auto& p : players_)
        {
            if (!p.active) continue;
            spriteRenderer_.DrawRing(p.pos, p.hitboxRadius, 0.03f, WithAlpha(Red, 0.85f), 0.06f, 28);
            spriteRenderer_.DrawRing(p.pos, p.grazeRadius, 0.04f, WithAlpha(Sky, 0.55f), 0.07f, 48);
        }
        for (const auto& e : enemies_)
        {
            if (e.dead) continue;
            spriteRenderer_.DrawRing(e.pos, e.radius, 0.04f, WithAlpha(Cream, 0.45f), 0.08f, 40);
        }
    }
#endif
    spriteRenderer_.End();
}

void SweetsApp::DrawGameplay3D()
{
    float blendFactor[4]{ 0, 0, 0, 0 };
    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->PSSetShader(ps_.Get(), nullptr, 0);
    ID3D11Buffer* frameCb = frameCB_.Get();
    ID3D11Buffer* objectCb = objectCB_.Get();
    context_->VSSetConstantBuffers(0, 1, &frameCb);
    context_->PSSetConstantBuffers(0, 1, &frameCb);
    context_->VSSetConstantBuffers(1, 1, &objectCb);
    context_->PSSetConstantBuffers(1, 1, &objectCb);
    context_->RSSetState(rasterState_.Get());
    context_->OMSetDepthStencilState(depthState_.Get(), 0);
    context_->OMSetBlendState(alphaBlend_.Get(), blendFactor, 0xffffffff);

    DrawMesh(floorMesh_,
        XMMatrixScaling(ArenaRadius + 1.3f, 1.0f, ArenaRadius + 1.3f) *
        XMMatrixTranslation(0.0f, -0.035f, 0.0f),
        WithAlpha(Rose, 0.16f));
    DrawMesh(floorMesh_,
        XMMatrixScaling(ArenaRadius, 1.0f, ArenaRadius) *
        XMMatrixTranslation(0.0f, -0.025f, 0.0f),
        WithAlpha({ 0.18f, 0.07f, 0.12f, 1.0f }, 0.94f));
    DrawMesh(ringMesh_,
        XMMatrixScaling(ArenaRadius, 1.0f, ArenaRadius) *
        XMMatrixTranslation(0.0f, 0.015f, 0.0f),
        WithAlpha(Gold, 0.80f));
    DrawMesh(ringMesh_,
        XMMatrixScaling(ArenaRadius * 0.68f, 1.0f, ArenaRadius * 0.68f) *
        XMMatrixTranslation(0.0f, 0.025f, 0.0f),
        WithAlpha(Cream, 0.22f));

    for (const auto& o : obstacles_)
    {
        if (o.damageField)
        {
            DrawCylinder(o.pos, o.radius, 0.08f, WithAlpha(Red, 0.42f));
            DrawMesh(ringMesh_,
                XMMatrixScaling(o.radius, 1.0f, o.radius) *
                XMMatrixTranslation(o.pos.x, 0.16f, o.pos.z),
                WithAlpha(Red, 0.78f));
        }
        else
        {
            DrawCylinder(o.pos, o.radius, o.cheeseWall ? 0.82f : 0.56f, WithAlpha(o.color, 0.86f));
            DrawMesh(ringMesh_,
                XMMatrixScaling(o.radius * 1.08f, 1.0f, o.radius * 1.08f) *
                XMMatrixTranslation(o.pos.x, 0.65f, o.pos.z),
                WithAlpha(Cream, 0.30f));
        }
    }

    for (const auto& p : pickups_) DrawPickupShape3D(p);

    for (const auto& s : shots_)
    {
        const float scale = s.charged ? 1.55f : 1.0f;
        DrawSphere(s.pos, s.height, s.radius * 1.75f * scale, s.color);
        if (s.enemy)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(s.radius * 2.25f, 1.0f, s.radius * 2.25f) *
                XMMatrixTranslation(s.pos.x, s.height, s.pos.z),
                WithAlpha(Cream, 0.26f));
        }
    }

    for (const auto& e : enemies_)
    {
        if (e.dead) continue;
        Color c = e.flash > 0.0f ? Cream : e.color;
        if (e.barrierT > 0.0f) c = Sky;
        DrawSphere(e.pos, e.height, e.radius, c);
        DrawCylinder(e.pos, e.radius * 0.62f, e.height, WithAlpha(c, 0.42f));
    }

    if (boss_.active)
    {
        Color c = boss_.flash > 0.0f ? Cream : (boss_.bossType == BossType::HiddenBoss ? Grape : Rose);
        DrawSphere(boss_.pos, boss_.height, boss_.radius, c);
        DrawMesh(ringMesh_,
            XMMatrixScaling(boss_.radius * 1.42f, 1.0f, boss_.radius * 1.42f) *
            XMMatrixTranslation(boss_.pos.x, 0.10f, boss_.pos.z),
            WithAlpha(Red, 0.45f));
        if (boss_.telegraphT > 0.0f && boss_.telegraphLife > 0.0f)
        {
            const float t = 1.0f - ClampFloat(boss_.telegraphT / boss_.telegraphLife, 0.0f, 1.0f);
            const Color telegraph = boss_.telegraphAttack == 0 ? Grape : (boss_.telegraphAttack == 1 ? Red : (boss_.telegraphAttack == 2 ? Gold : Mint));
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (1.75f + t * 1.15f), 1.0f, boss_.radius * (1.75f + t * 1.15f)) *
                XMMatrixTranslation(boss_.pos.x, 0.14f, boss_.pos.z),
                WithAlpha(telegraph, 0.72f));
        }
    }

    for (const auto& s : slashes_) DrawSector3D(s);

    for (const auto& p : players_)
    {
        if (!p.active) continue;
        const Color base = Loadouts[static_cast<int>(p.character)].color;
        Color body = p.downed ? WithAlpha(Red, 0.65f) : (p.inv > 0.0f ? Cream : base);
        if (screen_ == Screen::HiddenBoss && !p.downed) body = WithAlpha(body, 0.42f);
        DrawSphere(p.pos, PlayerBodyY, p.radius, body);
        const V2 nose = p.pos + FromAngle(p.face) * (p.radius * 0.75f);
        DrawSphere(nose, PlayerBodyY + 0.03f, p.radius * 0.16f, Cream);
        if ((p.focus || screen_ == Screen::HiddenBoss) && !p.downed)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.hitboxRadius, 1.0f, p.hitboxRadius) *
                XMMatrixTranslation(p.pos.x, 0.06f, p.pos.z),
                Red);
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.grazeRadius, 1.0f, p.grazeRadius) *
                XMMatrixTranslation(p.pos.x, 0.055f, p.pos.z),
                WithAlpha(Sky, p.grazeFlash > 0.0f ? 0.70f : 0.30f));
        }
        if (p.shieldT > 0.0f)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.radius * 2.1f, 1.0f, p.radius * 2.1f) *
                XMMatrixTranslation(p.pos.x, 0.08f, p.pos.z),
                WithAlpha(Sky, 0.45f));
        }
    }

    for (const auto& particle : particles_)
    {
        DrawSphere(particle.pos, std::max(0.03f, particle.y), 0.055f + particle.y * 0.04f, WithAlpha(particle.color, ClampFloat(particle.ttl * 2.0f, 0.0f, 1.0f)));
    }

#if defined(_DEBUG)
    if (debug_.overlays)
    {
        for (const auto& p : players_)
        {
            if (!p.active) continue;
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.hitboxRadius, 1.0f, p.hitboxRadius) *
                XMMatrixTranslation(p.pos.x, 0.085f, p.pos.z),
                WithAlpha(Red, 0.85f));
        }
    }
#endif
}

void SweetsApp::DrawAdditiveScene()
{
    if (!additiveRtv_) return;
    const float clear[4] = { 0, 0, 0, 1 };
    context_->ClearRenderTargetView(additiveRtv_.Get(), clear);

    ID3D11RenderTargetView* rtv = additiveRtv_.Get();
    context_->OMSetRenderTargets(1, &rtv, dsv_.Get());
    spriteRenderer_.Begin(view_ * proj_, true);

    for (const auto& s : shots_)
    {
        const float glow = s.enemy ? 2.1f : (s.reflected ? 2.8f : 1.7f);
        spriteRenderer_.DrawCircle(s.pos, s.radius * glow, WithAlpha(s.color, s.enemy ? 0.42f : 0.55f), 0.06f, 24);
    }
    for (const auto& s : slashes_)
    {
        DrawSector(s);
    }
    for (const auto& pulse : effectPulses_)
    {
        const float life = std::max(0.01f, pulse.life);
        const float progress = ClampFloat(1.0f - pulse.ttl / life, 0.0f, 1.0f);
        const float fade = ClampFloat(pulse.ttl / life, 0.0f, 1.0f);
        const float radius = pulse.startRadius + (pulse.endRadius - pulse.startRadius) * progress;
        spriteRenderer_.DrawRing(pulse.pos, radius, 0.15f + radius * 0.025f, WithAlpha(pulse.color, 0.88f * fade), 0.05f, 96);
        spriteRenderer_.DrawRing(pulse.pos, radius * 0.62f, 0.09f, WithAlpha(Cream, 0.34f * fade), 0.04f, 72);
    }
    for (const auto& visual : swordEffectVisuals_)
    {
        const float life = std::max(0.01f, visual.life);
        const float progress = ClampFloat(1.0f - visual.ttl / life, 0.0f, 1.0f);
        const float fade = ClampFloat(visual.ttl / life, 0.0f, 1.0f);
        const float baseScale = std::max(0.1f, visual.scale);
        const float length = visual.range * baseScale * (visual.charged ? 1.05f : 0.96f);
        const float width = std::max(0.18f, std::tan(visual.arc * 0.5f) * visual.range * 0.36f) * baseScale;
        const V2 forward = FromAngle(visual.angle);
        const V2 side{ -forward.z, forward.x };
        const V2 root = visual.pos + forward * (0.12f * baseScale);
        const V2 center = root + forward * (length * 0.48f);
        const float rotation = visual.angle - Pi * 0.5f;
        const float sweep = 1.0f + 0.16f * std::sin(progress * Pi);

        DrawSprite2D(L"effect_sword_thunder", center, { width * sweep, length * (1.0f + progress * 0.10f) }, rotation, WithAlpha(Cream, 0.96f * fade), 0.035f);
        DrawSprite2D(L"effect_sword_line", center - side * (0.16f * baseScale), { width * 0.72f, length * 0.92f }, rotation + 0.05f, WithAlpha(Sky, 0.58f * fade), 0.034f);
        DrawSprite2D(L"effect_sword_line", center + side * (0.18f * baseScale), { width * 0.52f, length * 0.76f }, rotation - 0.07f, WithAlpha(Choco, 0.46f * fade), 0.033f);
        if (visual.charged)
        {
            DrawSprite2D(L"effect_sword_ring", root + forward * (0.42f * baseScale), { 1.55f * baseScale, 1.55f * baseScale }, gameTime_ * 5.0f, WithAlpha(Sky, 0.72f * fade), 0.032f);
            DrawSprite2D(L"effect_sword_thunder", center + forward * (0.32f * baseScale), { width * 1.22f, length * 1.10f }, rotation, WithAlpha(Gold, 0.42f * fade), 0.031f);
        }

        const int sparkCount = visual.charged ? 9 : 5;
        for (int i = 0; i < sparkCount; ++i)
        {
            const float t = (static_cast<float>(i) + 0.35f) / static_cast<float>(sparkCount);
            const float wave = std::sin((t + progress) * TwoPi * 1.7f);
            const V2 sparkPos = root + forward * (length * t) + side * (wave * width * 0.42f);
            const float sparkSize = (visual.charged ? 0.34f : 0.24f) * baseScale * (1.0f - 0.35f * t);
            DrawSprite2D(L"effect_sword_particle", sparkPos, { sparkSize, sparkSize }, visual.angle + t * TwoPi, WithAlpha((i & 1) ? Gold : Cream, 0.82f * fade), 0.030f);
        }
    }
    for (const auto& p : players_)
    {
        if (!p.active) continue;
        if (p.bombT > 0.0f)
        {
            const float t = p.bombT / 1.8f;
            spriteRenderer_.DrawRing(p.pos, 1.5f + (1.0f - t) * 5.8f, 0.24f, WithAlpha(Sky, 0.9f * t), 0.04f, 120);
        }
        if (p.grazeFlash > 0.0f)
        {
            spriteRenderer_.DrawRing(p.pos, p.grazeRadius, 0.08f, WithAlpha(Sky, 0.75f), 0.04f, 64);
        }
    }
    for (const auto& p : particles_)
    {
        spriteRenderer_.DrawCircle(p.pos, 0.10f + p.y * 0.04f, WithAlpha(p.color, ClampFloat(p.ttl * 2.5f, 0.0f, 1.0f)), 0.04f, 16);
    }
    spriteRenderer_.End();
}

void SweetsApp::CompositeScene()
{
    if (!resolvedRtv_ || !postVs_ || !postPs_ || !sceneColorSrv_ || !additiveSrv_ || !historySrv_)
    {
        return;
    }

    const float clear[4] = { 0, 0, 0, 1 };
    context_->ClearRenderTargetView(resolvedRtv_.Get(), clear);
    ID3D11RenderTargetView* rtv = resolvedRtv_.Get();
    context_->OMSetRenderTargets(1, &rtv, nullptr);
    float blendFactor[4]{ 0,0,0,0 };
    context_->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(postVs_.Get(), nullptr, 0);
    context_->PSSetShader(postPs_.Get(), nullptr, 0);
    context_->RSSetState(rasterState_.Get());

    PostCB post{};
#if defined(_DEBUG)
    const bool useTaa = debug_.taa && debug_.taaFrame > 0 && !debug_.additiveView;
    post.params = XMFLOAT4(useTaa ? 0.18f : 0.0f, 1.0f, debug_.additiveView ? 1.0f : 0.0f, 0.0f);
#else
    post.params = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
#endif
    context_->UpdateSubresource(postCB_.Get(), 0, nullptr, &post, 0, 0);
    ID3D11Buffer* pcb = postCB_.Get();
    context_->PSSetConstantBuffers(0, 1, &pcb);
    ID3D11ShaderResourceView* srvs[3] = { sceneColorSrv_.Get(), additiveSrv_.Get(), historySrv_.Get() };
    context_->PSSetShaderResources(0, 3, srvs);
    ID3D11SamplerState* sampler = postSampler_.Get();
    context_->PSSetSamplers(0, 1, &sampler);
    context_->Draw(3, 0);
    ID3D11ShaderResourceView* nullSrvs[3] = { nullptr, nullptr, nullptr };
    context_->PSSetShaderResources(0, 3, nullSrvs);

    if (historyTex_ && resolvedTex_)
    {
        context_->CopyResource(historyTex_.Get(), resolvedTex_.Get());
    }
    if (backBufferTex_ && resolvedTex_)
    {
        context_->CopyResource(backBufferTex_.Get(), resolvedTex_.Get());
    }
#if defined(_DEBUG)
    ++debug_.taaFrame;
#endif
}

void SweetsApp::DrawHud()
{
    d2dContext_->BeginDraw();
    d2dContext_->SetTransform(D2D1::Matrix3x2F::Identity());

    if (screen_ == Screen::BootLoading || screen_ == Screen::GameplayLoading)
    {
        DrawBootLoading();
        DrawDebugHud();
        const HRESULT hr = d2dContext_->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            ReleaseRenderTargets();
            CreateRenderTargets();
        }
        return;
    }

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
    std::wostringstream hud;
    hud << L"スコア " << score_
        << L"   ウェーブ " << wave_
        << L"   ステージ " << StageName(stage_)
        << L"   " << CurrentDifficulty().name
        << L"   反射 " << reflectKills_
        << L"   フィーバー " << static_cast<int>(player_.fever) << L"%";
    if (screen_ == Screen::HiddenBoss)
    {
        hud << L"   隠しボス 残り " << static_cast<int>(std::max(0.0f, HiddenBossDurationSeconds - hiddenBossT_)) << L"秒";
    }
    d2dContext_->DrawTextW(hud.str().c_str(), static_cast<UINT32>(hud.str().size()), hudFormat_.Get(),
        D2D1::RectF(18.0f, 14.0f, static_cast<float>(width_) - 18.0f, 48.0f), textBrush_.Get());

    for (int i = 0; i < MaxPlayers; ++i)
    {
        const Player& p = players_[i];
        if (!p.active) continue;
        const float top = 48.0f + i * 24.0f;
        const int displayHp = p.hp > 0.0f ? std::max(1, static_cast<int>(std::ceil(p.hp))) : 0;
        std::wostringstream line;
        line << L"P" << (i + 1)
            << (i == 0 ? L" 1P " : (p.ai ? L" AI " : L" PAD "))
            << CharacterTexts[static_cast<int>(p.character)].jpName
            << L" HP " << displayHp << L"/" << static_cast<int>(p.maxHp)
            << L" 必殺 " << static_cast<int>(p.ult) << L"%"
            << L" ボム" << p.bombs
            << L" グレイズ" << p.graze
            << (p.downed ? L" ダウン" : L"");
        textBrush_->SetColor(i == 0 ? D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f) : D2D1::ColorF(0.82f, 0.88f, 1.0f, 0.92f));
        d2dContext_->DrawTextW(line.str().c_str(), static_cast<UINT32>(line.str().size()), smallFormat_.Get(),
            D2D1::RectF(18.0f, top, static_cast<float>(width_) - 18.0f, top + 24.0f), textBrush_.Get());
    }

    if (boss_.active)
    {
        const float left = 18.0f;
        const float top = 154.0f;
        const float bw = 360.0f;
        const float pct = boss_.bossType == BossType::HiddenBoss
            ? ClampFloat(1.0f - hiddenBossT_ / HiddenBossDurationSeconds, 0.0f, 1.0f)
            : ClampFloat(boss_.hp / boss_.maxHp, 0.0f, 1.0f);
        textBrush_->SetColor(D2D1::ColorF(0.28f, 0.08f, 0.14f, 0.85f));
        d2dContext_->FillRectangle(D2D1::RectF(left, top, left + bw, top + 14.0f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.24f, 0.35f, 0.95f));
        d2dContext_->FillRectangle(D2D1::RectF(left, top, left + bw * pct, top + 14.0f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
        const wchar_t* bossName = BossName(boss_.bossType);
        d2dContext_->DrawTextW(bossName, static_cast<UINT32>(wcslen(bossName)), smallFormat_.Get(), D2D1::RectF(left, top + 16, left + 220, top + 40), textBrush_.Get());
    }

    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.88f));
    const wchar_t* help = L"WASD/矢印: 移動  |  左クリック/Space: 通常弾  |  右クリック長押し: チャージ  |  Q: 必殺  |  X/Ctrl: ボム  |  P: 一時停止";
    d2dContext_->DrawTextW(help, static_cast<UINT32>(wcslen(help)), smallFormat_.Get(),
        D2D1::RectF(18.0f, static_cast<float>(height_) - 34.0f, static_cast<float>(width_) - 18.0f, static_cast<float>(height_) - 8.0f), textBrush_.Get());

    if (messageT_ > 0.0f && !message_.empty())
    {
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, ClampFloat(messageT_, 0.0f, 1.0f)));
        d2dContext_->DrawTextW(message_.c_str(), static_cast<UINT32>(message_.size()), hudFormat_.Get(),
            D2D1::RectF(18.0f, 188.0f, static_cast<float>(width_) - 18.0f, 226.0f), textBrush_.Get());
    }

    if (screen_ == Screen::Title)
    {
        textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 1.0f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

        const float dividerX = 322.0f;
        textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.80f));
        d2dContext_->DrawLine(D2D1::Point2F(dividerX, 0.0f), D2D1::Point2F(dividerX, static_cast<float>(height_)), textBrush_.Get(), 2.5f);

        titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
        const wchar_t* title = L"Sweets Panic";
        d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
            D2D1::RectF(38.0f, 34.0f, dividerX - 22.0f, 92.0f), textBrush_.Get());

        const std::array<const wchar_t*, 3> items{ L"Story", L"Endless", L"Credits" };
        const float itemW = 248.0f;
        const float itemH = 58.0f;
        const float gap = 12.0f;
        const float menuX = 42.0f;
        const float menuTop = std::max(112.0f, static_cast<float>(height_) * 0.18f);
        for (int i = 0; i < 3; ++i)
        {
            const float y = menuTop + i * (itemH + gap);
            const D2D1_RECT_F rect = D2D1::RectF(menuX, y, menuX + itemW, y + itemH);
            const bool selected = i == titleMenuIndex_;
            const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
            if (hover)
            {
                textBrush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.16f));
                d2dContext_->FillRectangle(rect, textBrush_.Get());
            }
            else if (selected)
            {
                textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.10f));
                d2dContext_->FillRectangle(rect, textBrush_.Get());
            }
            textBrush_->SetColor(selected || hover ? D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f) : D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f));
            d2dContext_->DrawTextW(items[i], static_cast<UINT32>(wcslen(items[i])), titleFormat_.Get(),
                D2D1::RectF(rect.left + 8.0f, rect.top - 2.0f, rect.right, rect.bottom + 8.0f), textBrush_.Get());
        }

        const float mediaLeft = dividerX + 52.0f;
        const float mediaTop = 72.0f;
        const float mediaRight = static_cast<float>(width_) - 48.0f;
        const float mediaBottom = std::min(static_cast<float>(height_) * 0.47f, 390.0f);
        DrawTitleMediaFrame(D2D1::RectF(mediaLeft, mediaTop, mediaRight, mediaBottom));

        textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.90f));
        const wchar_t* hint = L"Story / Endless / Credits を選択";
        d2dContext_->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), smallFormat_.Get(),
            D2D1::RectF(40.0f, menuTop + itemH * 3.0f + gap * 3.0f + 8.0f, dividerX - 18.0f, static_cast<float>(height_) - 24.0f), textBrush_.Get());

        DrawScreenFlashOverlay();
        DrawDebugHud();

        const HRESULT hr = d2dContext_->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            ReleaseRenderTargets();
            CreateRenderTargets();
        }
        return;
    }

    if (screen_ == Screen::Title)
    {
        textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.72f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
        const wchar_t* title = L"スイーツパニック DX11";
        titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.18f, static_cast<float>(width_), static_cast<float>(height_) * 0.29f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
        const wchar_t* start = L"1Pの性能とモードを選択してください。Enterで決定、Cキーでクレジット。";
        d2dContext_->DrawTextW(start, static_cast<UINT32>(wcslen(start)), hudFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.31f, static_cast<float>(width_), static_cast<float>(height_) * 0.38f), textBrush_.Get());

        const std::array<const wchar_t*, 3> items{ L"Story", L"Endless", L"Credits" };
        const float itemW = 190.0f;
        const float itemH = 42.0f;
        const float gap = 14.0f;
        const float totalW = itemW * 3.0f + gap * 2.0f;
        const float startX = (static_cast<float>(width_) - totalW) * 0.5f;
        const float menuTop = static_cast<float>(height_) * 0.39f;
        for (int i = 0; i < 3; ++i)
        {
            const float x = startX + i * (itemW + gap);
            const bool selected = i == titleMenuIndex_;
            textBrush_->SetColor(selected ? D2D1::ColorF(0.20f, 0.08f, 0.13f, 0.98f) : D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.92f));
            d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, menuTop, x + itemW, menuTop + itemH), 8.0f, 8.0f), textBrush_.Get());
            textBrush_->SetColor(selected ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f));
            d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, menuTop, x + itemW, menuTop + itemH), 8.0f, 8.0f), textBrush_.Get(), selected ? 3.0f : 1.0f);
            d2dContext_->DrawTextW(items[i], static_cast<UINT32>(wcslen(items[i])), hudFormat_.Get(),
                D2D1::RectF(x, menuTop + 8.0f, x + itemW, menuTop + itemH), textBrush_.Get());
        }
        DrawLoadoutSelection();
        titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }
    else if (screen_ == Screen::CharacterSelect)
    {
        DrawCharacterSelect();
    }
    else if (screen_ == Screen::Credits)
    {
        DrawCredits();
    }
    else if (screen_ == Screen::DifficultySelect)
    {
        DrawDifficultySelection();
    }
    else if (screen_ == Screen::Clear || screen_ == Screen::CompleteClear)
    {
        DrawClearScreen();
    }
    else if (screen_ == Screen::HiddenBossIntro)
    {
        DrawHiddenBossIntro();
    }
    else if (screen_ == Screen::Paused)
    {
        DrawPauseMenu();
    }
    else if (screen_ == Screen::Video)
    {
        DrawVideoScreen();
    }
    else if (screen_ == Screen::GameOver)
    {
        textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.72f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.30f, 0.38f, 1.0f));
        titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        const wchar_t* over = L"ゲームオーバー";
        d2dContext_->DrawTextW(over, static_cast<UINT32>(wcslen(over)), titleFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.36f, static_cast<float>(width_), static_cast<float>(height_) * 0.46f), textBrush_.Get());
        std::wostringstream ss;
        int totalKills = 0;
        int totalGraze = 0;
        for (const auto& p : players_)
        {
            totalKills += p.kills;
            totalGraze += p.graze;
        }
        ss << L"スコア " << score_ << L"  ウェーブ " << wave_ << L"  撃破 " << totalKills << L"  グレイズ " << totalGraze;
        const std::wstring line = ss.str();
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
        d2dContext_->DrawTextW(line.c_str(), static_cast<UINT32>(line.size()), hudFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.50f, static_cast<float>(width_), static_cast<float>(height_) * 0.58f), textBrush_.Get());
        const std::array<const wchar_t*, 2> choices{ L"Retry", L"Title" };
        const float choiceW = 180.0f;
        const float choiceTop = static_cast<float>(height_) * 0.62f;
        for (int i = 0; i < 2; ++i)
        {
            const bool selected = (i == 0 && gameOverChoice_ == GameOverChoice::Retry) || (i == 1 && gameOverChoice_ == GameOverChoice::Title);
            const float x = static_cast<float>(width_) * 0.5f - choiceW - 10.0f + i * (choiceW + 20.0f);
            textBrush_->SetColor(selected ? D2D1::ColorF(0.20f, 0.08f, 0.13f, 0.98f) : D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.92f));
            d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, choiceTop, x + choiceW, choiceTop + 46.0f), 8.0f, 8.0f), textBrush_.Get());
            textBrush_->SetColor(selected ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f));
            d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, choiceTop, x + choiceW, choiceTop + 46.0f), 8.0f, 8.0f), textBrush_.Get(), selected ? 3.0f : 1.0f);
            d2dContext_->DrawTextW(choices[i], static_cast<UINT32>(wcslen(choices[i])), hudFormat_.Get(),
                D2D1::RectF(x, choiceTop + 10.0f, x + choiceW, choiceTop + 46.0f), textBrush_.Get());
        }
        const wchar_t* guide = L"左右 / 上下で選択、Enterで決定";
        textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.90f));
        d2dContext_->DrawTextW(guide, static_cast<UINT32>(wcslen(guide)), smallFormat_.Get(),
            D2D1::RectF(0, choiceTop + 58.0f, static_cast<float>(width_), choiceTop + 86.0f), textBrush_.Get());
        titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    DrawScreenFlashOverlay();
    DrawDebugHud();

    const HRESULT hr = d2dContext_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        ReleaseRenderTargets();
        CreateRenderTargets();
    }
}

void SweetsApp::DrawScreenFlashOverlay()
{
    if (screenFlashT_ <= 0.0f || screenFlashLife_ <= 0.0f)
    {
        return;
    }

    const float t = ClampFloat(screenFlashT_ / std::max(0.01f, screenFlashLife_), 0.0f, 1.0f);
    const float alpha = 0.34f * t * t;
    const D2D1_RECT_F full = D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_));

    textBrush_->SetColor(D2D1::ColorF(screenFlashColor_.r, screenFlashColor_.g, screenFlashColor_.b, alpha));
    d2dContext_->FillRectangle(full, textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.28f));
    d2dContext_->FillRectangle(full, textBrush_.Get());
}

void SweetsApp::DrawBootLoading()
{
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float t = bootLoadElapsed_;

    textBrush_->SetColor(D2D1::ColorF(0.045f, 0.018f, 0.035f, 1.0f));
    d2dContext_->FillRectangle(D2D1::RectF(0.0f, 0.0f, w, h), textBrush_.Get());

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
    const wchar_t* title = screen_ == Screen::GameplayLoading ? L"Preparing Game" : L"Sweets Panic";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
        D2D1::RectF(0.0f, h * 0.30f, w, h * 0.40f), textBrush_.Get());

    std::wostringstream phase;
    phase << L"Loading: " << LoadPhaseName(loadPhase_);
    const std::wstring phaseText = phase.str();
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.95f));
    d2dContext_->DrawTextW(phaseText.c_str(), static_cast<UINT32>(phaseText.size()), hudFormat_.Get(),
        D2D1::RectF(0.0f, h * 0.43f, w, h * 0.49f), textBrush_.Get());

    const float barW = std::min(520.0f, w * 0.56f);
    const float barH = 12.0f;
    const float barL = (w - barW) * 0.5f;
    const float barT = h * 0.52f;
    const float readyIndex = static_cast<float>(static_cast<int>(LoadPhase::Ready));
    const float phaseIndex = static_cast<float>(std::min(static_cast<int>(loadPhase_), static_cast<int>(LoadPhase::Ready)));
    const float pct = ClampFloat((phaseIndex + ClampFloat(loadPhaseElapsed_ * 2.0f, 0.0f, 0.85f)) / std::max(1.0f, readyIndex), 0.0f, 1.0f);
    textBrush_->SetColor(D2D1::ColorF(0.18f, 0.08f, 0.12f, 1.0f));
    d2dContext_->FillRectangle(D2D1::RectF(barL, barT, barL + barW, barT + barH), textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.55f, 0.72f, 0.95f));
    d2dContext_->FillRectangle(D2D1::RectF(barL, barT, barL + barW * pct, barT + barH), textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.88f));
    d2dContext_->DrawRectangle(D2D1::RectF(barL, barT, barL + barW, barT + barH), textBrush_.Get(), 1.0f);

    const float pulse = 0.5f + 0.5f * std::sin(t * 5.4f);
    textBrush_->SetColor(D2D1::ColorF(0.30f, 0.60f, 1.0f, 0.25f + pulse * 0.35f));
    d2dContext_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(w * 0.5f, h * 0.61f), 18.0f + pulse * 8.0f, 18.0f + pulse * 8.0f), textBrush_.Get(), 3.0f);

    if (!lastLoadStep_.empty())
    {
        textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.90f));
        d2dContext_->DrawTextW(lastLoadStep_.c_str(), static_cast<UINT32>(lastLoadStep_.size()), smallFormat_.Get(),
            D2D1::RectF(w * 0.15f, h * 0.68f, w * 0.85f, h * 0.73f), textBrush_.Get());
    }
    if (!lastLoadWarning_.empty())
    {
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.70f, 0.25f, 0.92f));
        d2dContext_->DrawTextW(lastLoadWarning_.c_str(), static_cast<UINT32>(lastLoadWarning_.size()), smallFormat_.Get(),
            D2D1::RectF(w * 0.12f, h * 0.76f, w * 0.88f, h * 0.82f), textBrush_.Get());
    }

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawCredits()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.78f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
    const wchar_t* title = L"クレジット";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.20f, static_cast<float>(width_), static_cast<float>(height_) * 0.31f), textBrush_.Get());

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
    const wchar_t* music1 = L"Gameplay BGM: 空想キャンパス - BGMer 様";
    d2dContext_->DrawTextW(music1, static_cast<UINT32>(wcslen(music1)), hudFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.40f, static_cast<float>(width_), static_cast<float>(height_) * 0.47f), textBrush_.Get());

    const wchar_t* music2 = L"Game Over BGM: ruins - DOVA-SYNDROME 様";
    d2dContext_->DrawTextW(music2, static_cast<UINT32>(wcslen(music2)), hudFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.48f, static_cast<float>(width_), static_cast<float>(height_) * 0.55f), textBrush_.Get());

    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.9f));
    const wchar_t* back = L"Esc / Enter / Backspace / C でタイトルへ戻る";
    d2dContext_->DrawTextW(back, static_cast<UINT32>(wcslen(back)), smallFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.68f, static_cast<float>(width_), static_cast<float>(height_) * 0.74f), textBrush_.Get());

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawCharacterSelect()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.78f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
    const wchar_t* title = L"キャラクター選択";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.16f, static_cast<float>(width_), static_cast<float>(height_) * 0.25f), textBrush_.Get());

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.94f));
    const wchar_t* guide = L"カードクリック / 左右キー / 1-4 で選択。Enter で難易度選択、Esc でタイトルへ戻る";
    d2dContext_->DrawTextW(guide, static_cast<UINT32>(wcslen(guide)), hudFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.28f, static_cast<float>(width_), static_cast<float>(height_) * 0.34f), textBrush_.Get());

    DrawLoadoutSelection();
    DrawCoopSlotSelection();

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::LoadTitleImageBitmap()
{
    titleImageBitmap_.Reset();
    if (!wicFactory_ || !d2dContext_)
    {
        return;
    }

    const std::wstring imagePath = FindAssetFile(L"assets/textures/title.jpg");
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wicFactory_->CreateDecoderFromFilename(
        imagePath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr))
    {
#if defined(_DEBUG)
        OutputDebugStringW(L"SweetsActionDX11: assets/textures/title.jpg could not be opened. Title media fallback is used.\n");
#endif
        return;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return;

    ComPtr<IWICFormatConverter> converter;
    hr = wicFactory_->CreateFormatConverter(&converter);
    if (FAILED(hr)) return;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) return;

    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    d2dContext_->CreateBitmapFromWicBitmap(converter.Get(), &props, &titleImageBitmap_);
}

void SweetsApp::DrawBitmapCover(ID2D1Bitmap1* bitmap, const D2D1_RECT_F& rect, float opacity)
{
    if (!bitmap)
    {
        return;
    }

    const D2D1_SIZE_F size = bitmap->GetSize();
    const float dstW = std::max(1.0f, rect.right - rect.left);
    const float dstH = std::max(1.0f, rect.bottom - rect.top);
    const float srcW = std::max(1.0f, size.width);
    const float srcH = std::max(1.0f, size.height);
    const float srcAspect = srcW / srcH;
    const float dstAspect = dstW / dstH;

    D2D1_RECT_F src = D2D1::RectF(0.0f, 0.0f, srcW, srcH);
    if (srcAspect > dstAspect)
    {
        const float cropW = srcH * dstAspect;
        const float x = (srcW - cropW) * 0.5f;
        src = D2D1::RectF(x, 0.0f, x + cropW, srcH);
    }
    else
    {
        const float cropH = srcW / dstAspect;
        const float y = (srcH - cropH) * 0.5f;
        src = D2D1::RectF(0.0f, y, srcW, y + cropH);
    }

    d2dContext_->DrawBitmap(bitmap, rect, ClampFloat(opacity, 0.0f, 1.0f), D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
}

void SweetsApp::DrawTitleMediaFrame(const D2D1_RECT_F& rect)
{
    textBrush_->SetColor(D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.88f));
    d2dContext_->FillRectangle(rect, textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.88f));
    d2dContext_->DrawRectangle(rect, textBrush_.Get(), 2.0f);

    if (titleVideo_.HasFrame())
    {
        const uint64_t serial = titleVideo_.FrameSerial();
        if (!titleVideoBitmap_ || titleVideoSerial_ != serial)
        {
            titleVideoBitmap_.Reset();
            const auto props = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
            d2dContext_->CreateBitmap(
                D2D1::SizeU(titleVideo_.Width(), titleVideo_.Height()),
                titleVideo_.Pixels().data(),
                titleVideo_.Width() * 4u,
                props,
                &titleVideoBitmap_);
            titleVideoSerial_ = serial;
        }
        if (titleVideoBitmap_)
        {
            DrawBitmapCover(titleVideoBitmap_.Get(), rect, 1.0f);
            return;
        }
    }

    if (titleImageBitmap_)
    {
        DrawBitmapCover(titleImageBitmap_.Get(), rect, 1.0f);
        textBrush_->SetColor(D2D1::ColorF(0.04f, 0.02f, 0.04f, 0.28f));
        d2dContext_->FillRectangle(rect, textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.88f));
        d2dContext_->DrawRectangle(rect, textBrush_.Get(), 2.0f);
        return;
    }

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.88f));
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    const wchar_t* placeholder = L"Title media";
    d2dContext_->DrawTextW(placeholder, static_cast<UINT32>(wcslen(placeholder)), hudFormat_.Get(),
        D2D1::RectF(rect.left, rect.top + (rect.bottom - rect.top) * 0.42f, rect.right, rect.bottom), textBrush_.Get());
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    const wchar_t* hint = L"assets/video/title.mp4";
    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.76f));
    d2dContext_->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), smallFormat_.Get(),
        D2D1::RectF(rect.left, rect.top + (rect.bottom - rect.top) * 0.54f, rect.right, rect.bottom), textBrush_.Get());
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawPauseMenu()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.70f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    const float panelW = 480.0f;
    const float panelH = 370.0f;
    const float left = (static_cast<float>(width_) - panelW) * 0.5f;
    const float top = (static_cast<float>(height_) - panelH) * 0.5f;
    const D2D1_RECT_F panel = D2D1::RectF(left, top, left + panelW, top + panelH);
    textBrush_->SetColor(D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.96f));
    d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(panel, 8.0f, 8.0f), textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.95f));
    d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(panel, 8.0f, 8.0f), textBrush_.Get(), 2.0f);

    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
    const wchar_t* title = L"Pause";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), hudFormat_.Get(),
        D2D1::RectF(left, top + 22.0f, left + panelW, top + 58.0f), textBrush_.Get());

    const std::array<const wchar_t*, 2> buttons{ L"Resume", L"Title" };
    const float buttonW = 190.0f;
    const float buttonH = 42.0f;
    const float buttonX = left + 44.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float y = top + 76.0f + i * 56.0f;
        const bool selected = pauseMenuIndex_ == i;
        const D2D1_RECT_F rect = D2D1::RectF(buttonX, y, buttonX + buttonW, y + buttonH);
        textBrush_->SetColor(selected ? D2D1::ColorF(0.23f, 0.10f, 0.15f, 1.0f) : D2D1::ColorF(0.16f, 0.07f, 0.11f, 0.94f));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
        textBrush_->SetColor(selected ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), selected ? 2.5f : 1.0f);
        d2dContext_->DrawTextW(buttons[i], static_cast<UINT32>(wcslen(buttons[i])), hudFormat_.Get(),
            D2D1::RectF(rect.left, rect.top + 9.0f, rect.right, rect.bottom), textBrush_.Get());
    }

    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    const std::array<const wchar_t*, 4> labels{ L"Master", L"BGM", L"SE", L"UI" };
    const float sliderLeft = left + 170.0f;
    const float sliderRight = left + panelW - 48.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float y = top + 196.0f + i * 38.0f;
        const bool selected = pauseMenuIndex_ == i + 2;
        textBrush_->SetColor(selected ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.92f));
        d2dContext_->DrawTextW(labels[i], static_cast<UINT32>(wcslen(labels[i])), smallFormat_.Get(),
            D2D1::RectF(left + 48.0f, y - 10.0f, left + 152.0f, y + 14.0f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(0.24f, 0.11f, 0.17f, 0.94f));
        d2dContext_->FillRectangle(D2D1::RectF(sliderLeft, y, sliderRight, y + 8.0f), textBrush_.Get());
        const float value = VolumeSliderValue(i);
        textBrush_->SetColor(D2D1::ColorF(0.65f, 0.88f, 1.0f, 0.95f));
        d2dContext_->FillRectangle(D2D1::RectF(sliderLeft, y, sliderLeft + (sliderRight - sliderLeft) * value, y + 8.0f), textBrush_.Get());
        textBrush_->SetColor(selected ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f));
        const float knob = sliderLeft + (sliderRight - sliderLeft) * value;
        d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knob, y + 4.0f), 7.0f, 7.0f), textBrush_.Get());
        std::wostringstream pct;
        pct << static_cast<int>(value * 100.0f + 0.5f) << L"%";
        const std::wstring pctText = pct.str();
        d2dContext_->DrawTextW(pctText.c_str(), static_cast<UINT32>(pctText.size()), smallFormat_.Get(),
            D2D1::RectF(sliderRight + 10.0f, y - 10.0f, left + panelW - 8.0f, y + 14.0f), textBrush_.Get());
    }
}

void SweetsApp::DrawVideoScreen()
{
    textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());
    if (eventVideo_.HasFrame())
    {
        const uint64_t serial = eventVideo_.FrameSerial();
        if (!eventVideoBitmap_ || eventVideoSerial_ != serial)
        {
            eventVideoBitmap_.Reset();
            const auto props = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
            d2dContext_->CreateBitmap(
                D2D1::SizeU(eventVideo_.Width(), eventVideo_.Height()),
                eventVideo_.Pixels().data(),
                eventVideo_.Width() * 4u,
                props,
                &eventVideoBitmap_);
            eventVideoSerial_ = serial;
        }
        if (eventVideoBitmap_)
        {
            d2dContext_->DrawBitmap(eventVideoBitmap_.Get(),
                D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)),
                1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
    }
    if (eventVideoSkippable_)
    {
        textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.82f));
        const wchar_t* guide = L"Esc / Enter / Space: Skip";
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        d2dContext_->DrawTextW(guide, static_cast<UINT32>(wcslen(guide)), smallFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) - 34.0f, static_cast<float>(width_) - 18.0f, static_cast<float>(height_) - 8.0f), textBrush_.Get());
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }
}

void SweetsApp::DrawDebugHud()
{
#if defined(_DEBUG)
    if (!debug_.hud) return;

    {
    const float panelLeft = static_cast<float>(width_) - 360.0f;
    const float panelRight = static_cast<float>(width_);
    textBrush_->SetColor(D2D1::ColorF(0.015f, 0.015f, 0.020f, 0.88f));
    d2dContext_->FillRectangle(D2D1::RectF(panelLeft, 0.0f, panelRight, static_cast<float>(height_)), textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(0.65f, 0.88f, 1.0f, 0.90f));
    d2dContext_->DrawLine(D2D1::Point2F(panelLeft, 0.0f), D2D1::Point2F(panelLeft, static_cast<float>(height_)), textBrush_.Get(), 2.0f);

    const int enemyBullets = static_cast<int>(std::count_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.enemy && !s.dead; }));
    const int playerShots = static_cast<int>(std::count_if(shots_.begin(), shots_.end(), [](const Shot& s) { return !s.enemy && !s.dead; }));
    const EncounterTuning& encounter = CurrentEncounterTuning();
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    std::wostringstream ss;
    ss << L"デバッグパネル\n"
        << L"FPS " << static_cast<int>(debug_.fps) << L"  " << static_cast<int>(debug_.frameMs * 10.0f) / 10.0f << L" ms\n"
        << L"画面 " << static_cast<int>(screen_) << L"  モード " << static_cast<int>(gameMode_) << L"\n"
        << L"難易度 " << CurrentDifficulty().name << L"\n"
        << L"戦闘目的 " << encounter.name << L"  特殊 " << EliteEnemyCount() << L"  ボス雑魚 " << BossAddCount() << L"\n"
        << L"ウェーブ " << wave_ << L"  敵 " << enemies_.size() << L"\n"
        << L"弾 自機/敵 " << playerShots << L"/" << enemyBullets << L"\n"
        << L"描画/ルール " << (Use3DRules() ? L"3D" : L"2D") << L"  スプライト " << spriteLibrary_.Count() << L"\n"
        << L"BGM " << static_cast<int>(audio_.CurrentTrack()) << L"  音量 " << static_cast<int>(audio_.Volume() * 100.0f) << L"%\n"
        << L"TAA " << (debug_.taa ? L"ON" : L"OFF") << L"  加算RT " << (debug_.additiveView ? L"表示" : L"合成") << L"\n"
        << L"F1で閉じる";
    ss << L"\nロード " << static_cast<int>(bootLoadElapsed_ * 100.0f) / 100.0f << L"s  " << LoadPhaseName(loadPhase_) << L"\n"
        << L"最終 " << lastLoadStep_ << L"\n"
        << L"音声 " << audio_.StreamStatus() << L"\n";
    ss << L"Effekseer sword " << (effekseer_.HasEffect(L"sword_slash") ? L"OK" : L"NG") << L"\n";
    if (!effekseer_.LastError().empty())
    {
        ss << L"Effekseer " << effekseer_.LastError() << L"\n";
    }
    const std::wstring text = ss.str();
    textBrush_->SetColor(D2D1::ColorF(0.82f, 1.0f, 0.90f, 0.96f));
    d2dContext_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), smallFormat_.Get(),
        D2D1::RectF(panelLeft + 18.0f, 18.0f, panelRight - 16.0f, 236.0f), textBrush_.Get());

    const std::array<const wchar_t*, 12> labels{
        L"TAA",
        L"加算RT",
        L"当たり判定",
        L"無敵",
        L"全回復",
        L"ウェーブ進行",
        L"ボス召喚",
        L"EX解禁",
        L"敵弾消去",
        L"シェーダー再読込",
        L"1F進行",
        L"2D/3D切替"
    };
    const float left = static_cast<float>(width_) - 342.0f;
    const float buttonW = 148.0f;
    const float buttonH = 30.0f;
    const float gap = 10.0f;
    const float top = 286.0f;
    for (int i = 0; i < static_cast<int>(labels.size()); ++i)
    {
        const int col = i % 2;
        const int row = i / 2;
        const float x = left + col * (buttonW + gap);
        const float y = top + row * (buttonH + 8.0f);
        bool on = false;
        if (i == 0) on = debug_.taa;
        if (i == 1) on = debug_.additiveView;
        if (i == 2) on = debug_.overlays;
        if (i == 3) on = debug_.invincible;
        if (i == 11) on = Use3DRules();
        const bool hover = PointInRect(mouseX_, mouseY_, x, y, x + buttonW, y + buttonH);
        const D2D1_RECT_F rect = D2D1::RectF(x, y, x + buttonW, y + buttonH);
        textBrush_->SetColor(on ? D2D1::ColorF(0.18f, 0.32f, 0.38f, 0.98f) : (hover ? D2D1::ColorF(0.28f, 0.28f, 0.32f, 0.98f) : D2D1::ColorF(0.12f, 0.12f, 0.15f, 0.96f)));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get());
        textBrush_->SetColor(on ? D2D1::ColorF(0.65f, 0.88f, 1.0f, 1.0f) : D2D1::ColorF(0.86f, 0.86f, 0.90f, 0.94f));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get(), 1.0f);
        d2dContext_->DrawTextW(labels[i], static_cast<UINT32>(wcslen(labels[i])), smallFormat_.Get(),
            D2D1::RectF(x + 8.0f, y + 6.0f, x + buttonW - 8.0f, y + buttonH), textBrush_.Get());
    }
    return;
    }
#endif
}

void SweetsApp::DrawDifficultySelection()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.78f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
    const wchar_t* title = L"難易度選択";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.16f, static_cast<float>(width_), static_cast<float>(height_) * 0.25f), textBrush_.Get());

    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.94f));
    const wchar_t* guide = L"左右キー / A,D / クリックで選択、Enterで開始";
    d2dContext_->DrawTextW(guide, static_cast<UINT32>(wcslen(guide)), hudFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.27f, static_cast<float>(width_), static_cast<float>(height_) * 0.33f), textBrush_.Get());

    const int optionCount = DifficultyOptionCount();
    const float cardW = std::min(210.0f, (static_cast<float>(width_) - 100.0f) / 3.0f);
    const float cardH = 92.0f;
    const float gap = 16.0f;
    const float totalW = cardW * 3.0f + gap * 2.0f;
    const float startX = (static_cast<float>(width_) - totalW) * 0.5f;
    const float top = static_cast<float>(height_) * 0.36f;

    for (int i = 0; i < optionCount; ++i)
    {
        const bool practice = i == 5;
        const DifficultyDef& def = practice ? DifficultyDefs[static_cast<int>(Difficulty::Lunatic)] : DifficultyDefs[i];
        const int col = i % 3;
        const int row = i / 3;
        const float x = startX + col * (cardW + gap);
        const float y = top + row * (cardH + gap);
        const bool selected = i == difficultyIndex_;
        const D2D1_RECT_F rect = D2D1::RectF(x, y, x + cardW, y + cardH);

        textBrush_->SetColor(selected ? D2D1::ColorF(0.20f, 0.08f, 0.14f, 0.98f) : D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.93f));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
        textBrush_->SetColor(selected ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(def.color.r, def.color.g, def.color.b, 0.75f));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), selected ? 3.0f : 1.0f);

        std::wstring name = practice ? L"Hidden Boss Practice" : def.name;
        textBrush_->SetColor(D2D1::ColorF(def.color.r, def.color.g, def.color.b, 1.0f));
        d2dContext_->DrawTextW(name.c_str(), static_cast<UINT32>(name.size()), hudFormat_.Get(),
            D2D1::RectF(x + 10.0f, y + 12.0f, x + cardW - 10.0f, y + 40.0f), textBrush_.Get());

        const std::wstring summary = practice ? L"解禁済み: 隠しボス戦から開始" : def.summary;
        textBrush_->SetColor(D2D1::ColorF(0.92f, 0.84f, 0.88f, 0.96f));
        d2dContext_->DrawTextW(summary.c_str(), static_cast<UINT32>(summary.size()), smallFormat_.Get(),
            D2D1::RectF(x + 12.0f, y + 44.0f, x + cardW - 12.0f, y + 66.0f), textBrush_.Get());

        std::wostringstream stats;
        stats << L"弾 " << static_cast<int>(def.bulletCountMul * 100.0f) << L"% / ボム " << def.initialBombs;
        const std::wstring statLine = stats.str();
        textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.90f));
        d2dContext_->DrawTextW(statLine.c_str(), static_cast<UINT32>(statLine.size()), smallFormat_.Get(),
            D2D1::RectF(x + 12.0f, y + 66.0f, x + cardW - 12.0f, y + 86.0f), textBrush_.Get());
    }

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawClearScreen()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.72f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    const bool complete = screen_ == Screen::CompleteClear;
    const wchar_t* title = complete ? L"完全クリア" : (pendingHiddenBoss_ ? L"Lunatic Clear" : L"Clear");
    textBrush_->SetColor(complete ? D2D1::ColorF(0.60f, 0.90f, 1.0f, 1.0f) : D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
        D2D1::RectF(0, static_cast<float>(height_) * 0.30f, static_cast<float>(width_), static_cast<float>(height_) * 0.42f), textBrush_.Get());

    std::wostringstream ss;
    int totalKills = 0;
    int totalGraze = 0;
    for (const auto& p : players_)
    {
        totalKills += p.kills;
        totalGraze += p.graze;
    }
    ss << L"スコア " << score_ << L"  撃破 " << totalKills << L"  グレイズ " << totalGraze;
    if (pendingHiddenBoss_ && screen_ == Screen::Clear) ss << L"  - 何かが近づいてくる";
    else ss << L"  - Enterでタイトルへ";
    const std::wstring line = ss.str();
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
    d2dContext_->DrawTextW(line.c_str(), static_cast<UINT32>(line.size()), hudFormat_.Get(),
        D2D1::RectF(0, static_cast<float>(height_) * 0.47f, static_cast<float>(width_), static_cast<float>(height_) * 0.56f), textBrush_.Get());

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawHiddenBossIntro()
{
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float openT = ClampFloat((hiddenIntroT_ - 0.18f) / 1.55f, 0.0f, 1.0f);
    const float ease = openT * openT * (3.0f - 2.0f * openT);
    const float split = ease * w * 0.34f;
    const float flash = ClampFloat(1.0f - std::fabs(hiddenIntroT_ - 0.36f) / 0.18f, 0.0f, 1.0f);
    const bool mono = flash > 0.03f;

    textBrush_->SetColor(mono ? D2D1::ColorF(0.02f, 0.02f, 0.02f, 1.0f) : D2D1::ColorF(0.05f, 0.02f, 0.04f, 1.0f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, w, h), textBrush_.Get());

    const D2D1_COLOR_F panelColor = mono
        ? D2D1::ColorF(0.86f, 0.86f, 0.86f, 0.98f)
        : D2D1::ColorF(0.94f, 0.88f, 0.92f, 0.98f);
    const D2D1_COLOR_F lineColor = mono
        ? D2D1::ColorF(0.05f, 0.05f, 0.05f, 1.0f)
        : D2D1::ColorF(0.34f, 0.05f, 0.25f, 1.0f);

    const D2D1_RECT_F leftPanel = D2D1::RectF(-split, 0.0f, cx + 14.0f - split, h);
    const D2D1_RECT_F rightPanel = D2D1::RectF(cx - 14.0f + split, 0.0f, w + split, h);
    textBrush_->SetColor(panelColor);
    d2dContext_->FillRectangle(leftPanel, textBrush_.Get());
    d2dContext_->FillRectangle(rightPanel, textBrush_.Get());

    if (titleImageBitmap_)
    {
        const D2D1_RECT_F imageRect = D2D1::RectF(cx + 74.0f + split, 72.0f, w - 74.0f + split, h * 0.54f);
        DrawBitmapCover(titleImageBitmap_.Get(), imageRect, mono ? 0.32f : 0.72f);
        textBrush_->SetColor(mono ? D2D1::ColorF(0.82f, 0.82f, 0.82f, 0.42f) : D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.24f));
        d2dContext_->FillRectangle(imageRect, textBrush_.Get());
        textBrush_->SetColor(lineColor);
        d2dContext_->DrawRectangle(imageRect, textBrush_.Get(), 2.0f);
    }

    std::array<D2D1_POINT_2F, 8> crack{ {
        D2D1::Point2F(cx - 18.0f, -12.0f),
        D2D1::Point2F(cx + 18.0f, h * 0.12f),
        D2D1::Point2F(cx - 28.0f, h * 0.26f),
        D2D1::Point2F(cx + 22.0f, h * 0.42f),
        D2D1::Point2F(cx - 18.0f, h * 0.57f),
        D2D1::Point2F(cx + 32.0f, h * 0.73f),
        D2D1::Point2F(cx - 16.0f, h * 0.88f),
        D2D1::Point2F(cx + 22.0f, h + 12.0f),
    } };
    textBrush_->SetColor(lineColor);
    for (size_t i = 1; i < crack.size(); ++i)
    {
        D2D1_POINT_2F a = crack[i - 1];
        D2D1_POINT_2F b = crack[i];
        a.x += (i & 1) ? split * 0.08f : -split * 0.08f;
        b.x += (i & 1) ? -split * 0.08f : split * 0.08f;
        d2dContext_->DrawLine(a, b, textBrush_.Get(), 4.0f + ease * 6.0f);
    }
    for (size_t i = 1; i < crack.size(); ++i)
    {
        D2D1_POINT_2F a = crack[i - 1];
        D2D1_POINT_2F b = crack[i];
        a.x -= split;
        b.x -= split;
        d2dContext_->DrawLine(a, b, textBrush_.Get(), 2.0f);
        a = crack[i - 1];
        b = crack[i];
        a.x += split;
        b.x += split;
        d2dContext_->DrawLine(a, b, textBrush_.Get(), 2.0f);
    }

    const float bossT = ClampFloat((hiddenIntroT_ - 0.62f) / 1.05f, 0.0f, 1.0f);
    if (bossT > 0.0f)
    {
        textBrush_->SetColor(mono ? D2D1::ColorF(0.10f, 0.10f, 0.10f, 0.82f) : D2D1::ColorF(0.22f, 0.02f, 0.32f, 0.82f));
        const float r = 64.0f + bossT * 48.0f;
        d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 18.0f), r * 0.72f, r), textBrush_.Get());
        textBrush_->SetColor(mono ? D2D1::ColorF(0.92f, 0.92f, 0.92f, bossT) : D2D1::ColorF(0.68f, 0.36f, 1.0f, bossT));
        d2dContext_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 18.0f), r * 0.76f, r * 1.02f), textBrush_.Get(), 3.0f);
    }

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(mono ? D2D1::ColorF(0.04f, 0.04f, 0.04f, 1.0f) : D2D1::ColorF(0.28f, 0.06f, 0.18f, 1.0f));
    const wchar_t* clear = L"クリア";
    d2dContext_->DrawTextW(clear, static_cast<UINT32>(wcslen(clear)), titleFormat_.Get(),
        D2D1::RectF(30.0f - split, h * 0.22f, cx - 28.0f - split, h * 0.34f), textBrush_.Get());
    const wchar_t* media = L"???";
    d2dContext_->DrawTextW(media, static_cast<UINT32>(wcslen(media)), titleFormat_.Get(),
        D2D1::RectF(cx + 50.0f + split, h * 0.58f, w - 50.0f + split, h * 0.70f), textBrush_.Get());

    textBrush_->SetColor(mono ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f) : D2D1::ColorF(0.68f, 0.36f, 1.0f, 1.0f));
    const wchar_t* title = L"隠しボス出現";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
        D2D1::RectF(0, h * 0.72f, w, h * 0.84f), textBrush_.Get());

    if (flash > 0.0f)
    {
        textBrush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, flash * 0.55f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, w, h), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, flash * 0.22f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, w, h), textBrush_.Get());
    }

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawLoadoutSelection()
{
    const float gap = 14.0f;
    const float cardW = std::min(250.0f, (static_cast<float>(width_) - 96.0f - gap * 3.0f) / 4.0f);
    const float cardH = 214.0f;
    const float startX = (static_cast<float>(width_) - (cardW * 4.0f + gap * 3.0f)) * 0.5f;
    const float top = static_cast<float>(height_) * 0.49f;

    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    auto fill = [&](const D2D1_RECT_F& rect, const D2D1_COLOR_F& color)
    {
        textBrush_->SetColor(color);
        d2dContext_->FillRectangle(rect, textBrush_.Get());
    };

    auto drawText = [&](const std::wstring& text, IDWriteTextFormat* format, const D2D1_RECT_F& rect, const D2D1_COLOR_F& color)
    {
        textBrush_->SetColor(color);
        d2dContext_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format, rect, textBrush_.Get());
    };

    auto statBar = [&](float x, float y, float w, const wchar_t* label, float value, const D2D1_COLOR_F& color)
    {
        drawText(label, smallFormat_.Get(), D2D1::RectF(x, y - 2.0f, x + 46.0f, y + 18.0f), D2D1::ColorF(0.86f, 0.74f, 0.80f, 1.0f));
        fill(D2D1::RectF(x + 50.0f, y + 5.0f, x + w, y + 13.0f), D2D1::ColorF(0.22f, 0.10f, 0.16f, 0.95f));
        textBrush_->SetColor(color);
        d2dContext_->FillRectangle(D2D1::RectF(x + 50.0f, y + 5.0f, x + 50.0f + (w - 50.0f) * ClampFloat(value, 0.0f, 1.0f), y + 13.0f), textBrush_.Get());
    };

    for (int i = 0; i < static_cast<int>(Loadouts.size()); ++i)
    {
        const LoadoutPreset& loadout = Loadouts[i];
        const bool selected = i == loadoutIndex_;
        const float x = startX + i * (cardW + gap);
        const D2D1_RECT_F card = D2D1::RectF(x, top, x + cardW, top + cardH);

        textBrush_->SetColor(selected ? D2D1::ColorF(0.20f, 0.08f, 0.13f, 0.98f) : D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.92f));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(card, 8.0f, 8.0f), textBrush_.Get());

        textBrush_->SetColor(selected ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(0.42f, 0.25f, 0.34f, 1.0f));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(card, 8.0f, 8.0f), textBrush_.Get(), selected ? 3.0f : 1.0f);

        D2D1_COLOR_F accent = D2D1::ColorF(loadout.color.r, loadout.color.g, loadout.color.b, 1.0f);
        fill(D2D1::RectF(x, top, x + cardW, top + 5.0f), accent);

        std::wostringstream index;
        index << CharacterTexts[i].roleIcon;
        drawText(index.str(), hudFormat_.Get(), D2D1::RectF(x + 12.0f, top + 12.0f, x + 42.0f, top + 42.0f), accent);
        drawText(loadout.name, hudFormat_.Get(), D2D1::RectF(x + 40.0f, top + 12.0f, x + cardW - 12.0f, top + 40.0f), D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
        drawText(loadout.role, smallFormat_.Get(), D2D1::RectF(x + 14.0f, top + 43.0f, x + cardW - 14.0f, top + 63.0f), D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.95f));
        drawText(loadout.summary, smallFormat_.Get(), D2D1::RectF(x + 14.0f, top + 63.0f, x + cardW - 14.0f, top + 84.0f), D2D1::ColorF(0.84f, 0.75f, 0.78f, 0.95f));
        drawText(CharacterTexts[i].normal, smallFormat_.Get(), D2D1::RectF(x + 14.0f, top + 86.0f, x + cardW - 14.0f, top + 105.0f), D2D1::ColorF(0.95f, 0.85f, 0.88f, 0.95f));
        drawText(CharacterTexts[i].charge, smallFormat_.Get(), D2D1::RectF(x + 14.0f, top + 104.0f, x + cardW - 14.0f, top + 123.0f), D2D1::ColorF(0.78f, 0.88f, 1.0f, 0.95f));
        drawText(CharacterTexts[i].ultimate, smallFormat_.Get(), D2D1::RectF(x + 14.0f, top + 122.0f, x + cardW - 14.0f, top + 141.0f), D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.95f));

        const float statX = x + 14.0f;
        const float statW = cardW - 28.0f;
        statBar(statX, top + 145.0f, statW, L"体力", loadout.maxHp / 150.0f, accent);
        statBar(statX, top + 161.0f, statW, L"速度", loadout.speed / 6.4f, accent);
        statBar(statX, top + 177.0f, statW, L"火力", loadout.damageMul / 1.35f, accent);
        statBar(statX, top + 193.0f, statW, L"反射", Weapons[static_cast<int>(loadout.weapon)].bounce / 6.0f, accent);

        if (selected)
        {
            drawText(L"P1", smallFormat_.Get(), D2D1::RectF(x + cardW - 42.0f, top + 43.0f, x + cardW - 12.0f, top + 62.0f), D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
        }
    }

    const wchar_t* hint = L"カードクリック / 左右キー / 1-4で選択。Enterで開始、Cでクレジット。通常弾は左クリック/Space、チャージは右クリック長押し。";
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    drawText(hint, smallFormat_.Get(), D2D1::RectF(0.0f, top + cardH + 12.0f, static_cast<float>(width_), top + cardH + 36.0f), D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.9f));
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawCoopSlotSelection()
{
    const float cardH = 214.0f;
    const float loadoutTop = static_cast<float>(height_) * 0.49f;
    const float rowTop = loadoutTop + cardH + 46.0f;
    const float rowH = 30.0f;
    const float rowGap = 8.0f;
    const float startX = std::max(42.0f, (static_cast<float>(width_) - 760.0f) * 0.5f);
    const float labelW = 58.0f;
    const float modeW = 72.0f;
    const float modeGap = 8.0f;
    const float charX = startX + labelW + (modeW + modeGap) * 3.0f + 24.0f;
    const float charW = 230.0f;
    const std::array<const wchar_t*, 3> modes{ L"Off", L"AI", L"Pad" };

    auto fill = [&](const D2D1_RECT_F& rect, const D2D1_COLOR_F& color)
    {
        textBrush_->SetColor(color);
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get());
    };
    auto stroke = [&](const D2D1_RECT_F& rect, const D2D1_COLOR_F& color, float width)
    {
        textBrush_->SetColor(color);
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get(), width);
    };
    auto draw = [&](const std::wstring& text, const D2D1_RECT_F& rect, const D2D1_COLOR_F& color)
    {
        textBrush_->SetColor(color);
        d2dContext_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), smallFormat_.Get(), rect, textBrush_.Get());
    };

    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    for (int playerIndex = 1; playerIndex < MaxPlayers; ++playerIndex)
    {
        const float y = rowTop + (playerIndex - 1) * (rowH + rowGap);
        std::wostringstream label;
        label << (playerIndex + 1) << L"P";
        draw(label.str(), D2D1::RectF(startX, y + 5.0f, startX + labelW - 8.0f, y + rowH), D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.95f));

        for (int mode = 0; mode < 3; ++mode)
        {
            const float x = startX + labelW + mode * (modeW + modeGap);
            const D2D1_RECT_F rect = D2D1::RectF(x, y, x + modeW, y + rowH);
            const bool selected = coopSlotModes_[playerIndex] == static_cast<CoopSlotMode>(mode);
            const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
            fill(rect, selected ? D2D1::ColorF(0.22f, 0.10f, 0.15f, 0.95f) : D2D1::ColorF(0.08f, 0.04f, 0.07f, 0.82f));
            if (hover)
            {
                fill(rect, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f));
            }
            stroke(rect, selected ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.95f) : D2D1::ColorF(0.42f, 0.25f, 0.34f, 0.9f), selected ? 2.0f : 1.0f);
            draw(modes[mode], D2D1::RectF(x, y + 6.0f, x + modeW, y + rowH), selected ? D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f) : D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.9f));
        }

        const int loadoutIndex = std::max(0, std::min(coopLoadoutIndices_[playerIndex], static_cast<int>(Loadouts.size()) - 1));
        const D2D1_RECT_F charRect = D2D1::RectF(charX, y, charX + charW, y + rowH);
        const bool charHover = PointInRect(mouseX_, mouseY_, charRect.left, charRect.top, charRect.right, charRect.bottom);
        fill(charRect, coopSlotModes_[playerIndex] == CoopSlotMode::Off ? D2D1::ColorF(0.06f, 0.035f, 0.055f, 0.72f) : D2D1::ColorF(0.12f, 0.055f, 0.09f, 0.92f));
        if (charHover)
        {
            fill(charRect, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f));
        }
        stroke(charRect, D2D1::ColorF(0.42f, 0.25f, 0.34f, 0.9f), 1.0f);
        std::wstring charText = L"Character: ";
        charText += Loadouts[loadoutIndex].name;
        draw(charText, D2D1::RectF(charRect.left + 6.0f, charRect.top + 6.0f, charRect.right - 6.0f, charRect.bottom), D2D1::ColorF(0.92f, 0.82f, 0.86f, 0.95f));
    }
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawSprite2D(const std::wstring& spriteId, V2 pos, V2 size, float rotation, Color tint, float depth)
{
    const SpriteAsset* sprite = spriteLibrary_.Find(spriteId);
    const TextureAsset* texture = sprite ? textureLibrary_.Find(sprite->textureId) : nullptr;
    ID3D11ShaderResourceView* srv = texture ? texture->shaderResource.Get() : nullptr;
    if (srv)
    {
        spriteRenderer_.DrawQuad(srv, pos, size, rotation, tint, depth);
        return;
    }

    const float radius = std::max(size.x, size.z) * 0.5f;
    spriteRenderer_.DrawCircle(pos, radius, tint, depth, 28);
    spriteRenderer_.DrawRing(pos, radius, std::max(0.025f, radius * 0.12f), WithAlpha(Cream, tint.a * 0.35f), depth - 0.01f, 28);
}

void SweetsApp::DrawMesh(const Mesh& mesh, const XMMATRIX& world, Color tint)
{
    ObjectCB object{};
    object.world = world;
    object.tint = XMFLOAT4(tint.r, tint.g, tint.b, tint.a);
    context_->UpdateSubresource(objectCB_.Get(), 0, nullptr, &object, 0, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ID3D11Buffer* vb = mesh.vb.Get();
    context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context_->IASetIndexBuffer(mesh.ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    context_->DrawIndexed(mesh.indexCount, 0, 0);
}

void SweetsApp::DrawSphere(V2 p, float y, float r, Color c)
{
    DrawMesh(sphereMesh_, XMMatrixScaling(r, r, r) * XMMatrixTranslation(p.x, y, p.z), c);
}

void SweetsApp::DrawCylinder(V2 p, float radius, float height, Color c)
{
    DrawMesh(cylinderMesh_, XMMatrixScaling(radius, height, radius) * XMMatrixTranslation(p.x, 0.0f, p.z), c);
}

void SweetsApp::DrawPickupShape(const Pickup& p)
{
    const float bob = 0.34f + 0.10f * std::sin(gameTime_ * 5.0f + p.pos.x * 1.7f);
    const float spin = gameTime_ * 1.65f + p.pos.x * 0.41f + p.pos.z * 0.19f;
    const float r = p.radius;
    const Color body = p.color;
    const Color glow = WithAlpha(body, 0.50f);
    const Color pale = WithAlpha(Cream, 0.82f);

    DrawMesh(ringMesh_,
        XMMatrixScaling(r * 1.95f, 1.0f, r * 1.95f) *
        XMMatrixTranslation(p.pos.x, 0.075f, p.pos.z),
        glow);
    DrawMesh(ringMesh_,
        XMMatrixScaling(r * 1.22f, 1.0f, r * 1.22f) *
        XMMatrixRotationY(-spin) *
        XMMatrixTranslation(p.pos.x, 0.105f, p.pos.z),
        WithAlpha(Cream, 0.32f));

    auto cube = [&](float sx, float sy, float sz, V2 offset, float yOffset, float yaw, Color c)
    {
        DrawMesh(cubeMesh_,
            XMMatrixScaling(sx, sy, sz) *
            XMMatrixRotationY(yaw) *
            XMMatrixTranslation(p.pos.x + offset.x, bob + yOffset, p.pos.z + offset.z),
            c);
    };

    auto sphere = [&](V2 offset, float yOffset, float radius, Color c)
    {
        DrawSphere(p.pos + offset, bob + yOffset, radius, c);
    };

    auto wedge = [&](float scale, float yaw, Color c, float alpha = 0.68f)
    {
        DrawMesh(wedgeMesh_,
            XMMatrixScaling(scale, 1.0f, scale) *
            XMMatrixRotationY(yaw) *
            XMMatrixTranslation(p.pos.x, bob + 0.01f, p.pos.z),
            WithAlpha(c, alpha));
    };

    switch (p.pickupType)
    {
    case PickupType::Attack:
        for (int i = 0; i < 5; ++i)
        {
            wedge(r * 1.16f, spin + TwoPi * i / 5.0f, Gold, 0.72f);
        }
        sphere({ 0.0f, 0.0f }, 0.02f, r * 0.46f, Berry);
        break;
    case PickupType::Slow:
        for (int i = 0; i < 3; ++i)
        {
            cube(r * 1.85f, r * 0.18f, r * 0.16f, { 0.0f, 0.0f }, 0.0f, spin + TwoPi * i / 3.0f, Sky);
        }
        sphere({ 0.0f, 0.0f }, 0.03f, r * 0.26f, pale);
        break;
    case PickupType::Invincible:
        cube(r * 0.82f, r * 0.22f, r * 1.16f, { 0.0f, 0.0f }, 0.0f, spin + Pi * 0.25f, Gold);
        cube(r * 0.56f, r * 0.20f, r * 0.76f, FromAngle(spin + Pi) * (r * 0.16f), -0.01f, spin + Pi * 0.25f, pale);
        DrawMesh(ringMesh_,
            XMMatrixScaling(r * 1.50f, 1.0f, r * 1.50f) *
            XMMatrixRotationY(spin) *
            XMMatrixTranslation(p.pos.x, bob + 0.03f, p.pos.z),
            WithAlpha(Gold, 0.62f));
        break;
    case PickupType::Magnet:
    {
        const V2 side = FromAngle(spin + Pi * 0.5f);
        const V2 forward = FromAngle(spin);
        cube(r * 0.22f, r * 0.22f, r * 1.02f, side * (r * 0.50f), 0.0f, spin, Red);
        cube(r * 0.22f, r * 0.22f, r * 1.02f, side * (-r * 0.50f), 0.0f, spin, Sky);
        cube(r * 0.98f, r * 0.20f, r * 0.22f, FromAngle(spin + Pi) * (r * 0.46f), -0.02f, spin, pale);
        sphere(side * (r * 0.50f) + forward * (r * 0.58f), 0.04f, r * 0.18f, Red);
        sphere(side * (-r * 0.50f) + forward * (r * 0.58f), 0.04f, r * 0.18f, Sky);
        break;
    }
    case PickupType::BombDamage:
        cube(r * 0.92f, r * 0.22f, r * 0.92f, { 0.0f, 0.0f }, 0.0f, spin + Pi * 0.25f, Red);
        for (int i = 0; i < 6; ++i)
        {
            wedge(r * 0.64f, spin + TwoPi * i / 6.0f, Gold, 0.58f);
        }
        sphere({ 0.0f, 0.0f }, 0.05f, r * 0.24f, Cream);
        break;
    case PickupType::Heal:
        cube(r * 1.25f, r * 0.28f, r * 0.22f, { 0.0f, 0.0f }, 0.0f, spin, Mint);
        cube(r * 1.25f, r * 0.28f, r * 0.22f, { 0.0f, 0.0f }, 0.0f, spin + Pi * 0.5f, Mint);
        sphere({ 0.0f, 0.0f }, 0.04f, r * 0.20f, pale);
        break;
    case PickupType::UltFull:
        cube(r * 0.92f, r * 0.24f, r * 0.92f, { 0.0f, 0.0f }, -0.02f, spin + Pi * 0.25f, Grape);
        for (int i = -1; i <= 1; ++i)
        {
            const float a = spin + Pi * 0.5f + i * 0.42f;
            sphere(FromAngle(a) * (r * 0.50f), 0.22f + 0.04f * (1 - std::abs(i)), r * 0.20f, Gold);
        }
        break;
    case PickupType::Spread:
        for (int i = -1; i <= 1; ++i)
        {
            wedge(r * 0.88f, spin + i * 0.45f, Sky, 0.56f);
            sphere(FromAngle(spin + i * 0.45f) * (r * 0.88f), 0.06f, r * 0.16f, Gold);
        }
        sphere({ 0.0f, 0.0f }, 0.02f, r * 0.18f, pale);
        break;
    case PickupType::Speed:
        wedge(r * 1.15f, spin, Mint, 0.76f);
        cube(r * 0.92f, r * 0.22f, r * 0.18f, FromAngle(spin + Pi) * (r * 0.42f), -0.02f, spin, Mint);
        cube(r * 0.52f, r * 0.14f, r * 0.14f, FromAngle(spin + Pi) * (r * 0.92f), -0.03f, spin, pale);
        break;
    case PickupType::ScoreDouble:
        DrawMesh(ringMesh_,
            XMMatrixScaling(r * 0.96f, 1.0f, r * 0.96f) *
            XMMatrixRotationY(spin) *
            XMMatrixTranslation(p.pos.x - r * 0.32f, bob + 0.02f, p.pos.z),
            Gold);
        DrawMesh(ringMesh_,
            XMMatrixScaling(r * 0.96f, 1.0f, r * 0.96f) *
            XMMatrixRotationY(spin) *
            XMMatrixTranslation(p.pos.x + r * 0.32f, bob + 0.08f, p.pos.z),
            pale);
        sphere({ -r * 0.32f, 0.0f }, 0.02f, r * 0.18f, Gold);
        sphere({ r * 0.32f, 0.0f }, 0.08f, r * 0.18f, pale);
        break;
    default:
        sphere({ 0.0f, 0.0f }, 0.0f, r * 0.50f, body);
        break;
    }
}

void SweetsApp::DrawPickupShape3D(const Pickup& p)
{
    DrawPickupShape(p);
}

void SweetsApp::DrawSector(const Slash& s)
{
    const float alpha = ClampFloat(s.ttl / s.life, 0.0f, 1.0f) * 0.60f;
    if (s.visualMode == SlashVisualMode::Hidden)
    {
        return;
    }

    const V2 center = s.pos + FromAngle(s.angle) * (s.range * 0.45f);
    if (s.visualMode == SlashVisualMode::Line)
    {
        DrawSprite2D(L"2d_shot_player", center, { s.range * 0.20f, s.range * 1.75f }, s.angle - Pi * 0.5f, WithAlpha(s.color, alpha), 0.08f);
        return;
    }

    DrawSprite2D(L"2d_slash", center, { s.range * 1.65f, s.range * 0.92f }, s.angle, WithAlpha(s.color, alpha), 0.08f);
    spriteRenderer_.DrawArc(s.pos, s.range * 0.60f, s.range * 0.34f, s.angle, s.arc, WithAlpha(Cream, alpha * 0.58f), 0.07f, 32);
}

void SweetsApp::DrawSector3D(const Slash& s)
{
    if (s.visualMode == SlashVisualMode::Hidden) return;
    const float alpha = ClampFloat(s.ttl / s.life, 0.0f, 1.0f) * 0.55f;
    const V2 center = s.pos + FromAngle(s.angle) * (s.range * 0.42f);
    DrawMesh(wedgeMesh_,
        XMMatrixScaling(s.range * 0.82f, 1.0f, s.range * 0.82f) *
        XMMatrixRotationY(-s.angle) *
        XMMatrixTranslation(center.x, s.height, center.z),
        WithAlpha(s.color, alpha));
    DrawMesh(ringMesh_,
        XMMatrixScaling(s.range * 0.60f, 1.0f, s.range * 0.60f) *
        XMMatrixTranslation(s.pos.x, s.height + 0.015f, s.pos.z),
        WithAlpha(Cream, alpha * 0.28f));
}

void SweetsApp::DrawUltimatePreview(const Player& p, int ownerIndex)
{
    if (!p.active || p.downed || p.ult < 100.0f)
    {
        return;
    }

    auto drawRing = [&](V2 center, float radius, Color color, float alpha)
    {
        spriteRenderer_.DrawRing(center, radius, 0.10f + radius * 0.015f, WithAlpha(color, alpha), 0.09f, 96);
    };

    const Color accent = Loadouts[static_cast<int>(p.character)].color;
    bool comboReady = false;
    for (const auto& other : players_)
    {
        if (!other.active || other.index == ownerIndex || other.downed || other.ult < 100.0f) continue;
        comboReady = true;
        break;
    }
    if (comboReady)
    {
        drawRing(p.pos, 2.4f, Gold, 0.35f);
    }

    switch (p.weapon)
    {
    case Weapon::Strawberry:
    {
        const V2 target = ownerIndex == 0 ? ScreenToWorld(mouseX_, mouseY_) : FindNearestEnemyOrBoss(p.pos);
        drawRing(target, 3.4f, Berry, 0.62f);
        drawRing(target, 1.7f, Cream, 0.32f);
        spriteRenderer_.DrawCircle(target, 0.11f, WithAlpha(Berry, 0.80f), 0.08f, 18);
        break;
    }
    case Weapon::Chocolate:
        drawRing({ 0.0f, 0.0f }, ArenaRadius * 0.96f, Choco, 0.52f);
        drawRing(p.pos, 3.2f, accent, 0.35f);
        break;
    case Weapon::Cheese:
        drawRing(p.pos, 2.1f, Gold, 0.58f);
        drawRing(p.pos, 3.0f, Cream, 0.28f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = TwoPi * i / 8.0f;
            spriteRenderer_.DrawCircle(p.pos + FromAngle(a) * 2.1f, 0.08f, WithAlpha(Gold, 0.70f), 0.08f, 12);
        }
        break;
    case Weapon::Roll:
        drawRing({ 0.0f, 0.0f }, ArenaRadius * 0.96f, Cream, 0.48f);
        drawRing(p.pos, 4.4f, accent, 0.34f);
        break;
    default:
        drawRing(p.pos, 3.2f, accent, 0.45f);
        break;
    }
}

V2 SweetsApp::ScreenToWorld(float sx, float sy) const
{
    const float nx = (2.0f * sx / std::max(1u, width_)) - 1.0f;
    const float ny = 1.0f - (2.0f * sy / std::max(1u, height_));
    if (Use3DRules())
    {
        const XMMATRIX view = XMMatrixLookAtLH(
            XMVectorSet(0.0f, 15.8f, -17.8f, 1.0f),
            XMVectorSet(0.0f, 0.0f, 0.8f, 1.0f),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        const XMMATRIX proj = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(48.0f),
            static_cast<float>(std::max(1u, width_)) / std::max(1.0f, static_cast<float>(height_)),
            0.1f,
            80.0f);
        const XMMATRIX inv = XMMatrixInverse(nullptr, view * proj);
        XMVECTOR nearP = XMVector3TransformCoord(XMVectorSet(nx, ny, 0.0f, 1.0f), inv);
        XMVECTOR farP = XMVector3TransformCoord(XMVectorSet(nx, ny, 1.0f, 1.0f), inv);
        XMVECTOR dir = XMVector3Normalize(farP - nearP);
        const float nearY = XMVectorGetY(nearP);
        const float dirY = XMVectorGetY(dir);
        float t = 0.0f;
        if (std::fabs(dirY) > 0.0001f)
        {
            t = -nearY / dirY;
        }
        XMVECTOR hit = nearP + dir * std::max(0.0f, t);
        V2 out{ XMVectorGetX(hit), XMVectorGetZ(hit) };
        ClampInside(out, 0.0f);
        return out;
    }
    V2 out{ nx * GameplayHalfWidth(width_, height_), ny * GameplayHalfHeight() };
    ClampInside(out, 0.0f);
    return out;
}
