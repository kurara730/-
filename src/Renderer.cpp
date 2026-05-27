#include "SweetsApp.h"

#include <filesystem>

namespace
{
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
    flags |= D3D11_CREATE_DEVICE_DEBUG;
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
    ThrowIfFailed(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_), "Create RTV");

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
}

void SweetsApp::ReleaseRenderTargets()
{
    if (d2dContext_) d2dContext_->SetTarget(nullptr);
    textBrush_.Reset();
    d2dTarget_.Reset();
    dsv_.Reset();
    depthTex_.Reset();
    rtv_.Reset();
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
    DrawHud();
    swapChain_->Present(1, 0);
}

void SweetsApp::DrawScene()
{
    const float clear[4] = { 0.12f, 0.045f, 0.085f, 1.0f };
    context_->ClearRenderTargetView(rtv_.Get(), clear);
    context_->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    XMVECTOR eye = XMVectorSet(cameraPos_.x, cameraPos_.y, cameraPos_.z, 1.0f);
    XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    view_ = XMMatrixLookAtLH(eye, at, up);
    proj_ = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), std::max(0.1f, static_cast<float>(width_) / height_), 0.1f, 100.0f);

    FrameCB frame{};
    frame.viewProj = view_ * proj_;
    frame.lightDir = XMFLOAT4(-0.35f, -1.0f, 0.55f, 0.0f);
    frame.cameraPos = XMFLOAT4(cameraPos_.x, cameraPos_.y, cameraPos_.z, 1.0f);
    context_->UpdateSubresource(frameCB_.Get(), 0, nullptr, &frame, 0, 0);

    ID3D11RenderTargetView* rtv = rtv_.Get();
    context_->OMSetRenderTargets(1, &rtv, dsv_.Get());
    context_->OMSetDepthStencilState(depthState_.Get(), 0);
    float blendFactor[4]{ 0,0,0,0 };
    context_->OMSetBlendState(alphaBlend_.Get(), blendFactor, 0xffffffff);
    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->PSSetShader(ps_.Get(), nullptr, 0);
    ID3D11Buffer* frameCb = frameCB_.Get();
    ID3D11Buffer* objectCb = objectCB_.Get();
    context_->VSSetConstantBuffers(0, 1, &frameCb);
    context_->VSSetConstantBuffers(1, 1, &objectCb);
    context_->PSSetConstantBuffers(0, 1, &frameCb);
    context_->PSSetConstantBuffers(1, 1, &objectCb);
    context_->RSSetState(rasterState_.Get());

    DrawMesh(floorMesh_, XMMatrixScaling(ArenaRadius, 1.0f, ArenaRadius), WithAlpha(Rose, 0.88f));
    DrawMesh(ringMesh_, XMMatrixScaling(ArenaRadius, 1.0f, ArenaRadius) * XMMatrixTranslation(0, 0.04f, 0), Gold);

    for (const auto& o : obstacles_)
    {
        if (o.damageField)
        {
            DrawMesh(ringMesh_, XMMatrixScaling(o.radius, 1.0f, o.radius) *
                XMMatrixTranslation(o.pos.x, 0.09f, o.pos.z), WithAlpha(Red, 0.65f));
        }
        else
        {
            DrawCylinder(o.pos, o.radius, o.cheeseWall ? 0.82f : 0.55f, o.color);
        }
    }
    for (const auto& p : pickups_)
    {
        const float bob = 0.22f + 0.12f * std::sin(gameTime_ * 5.0f + p.pos.x);
        DrawSphere(p.pos, bob, p.radius, p.color);
    }
    for (const auto& s : shots_)
    {
        const float y = s.enemy ? 0.24f : 0.28f;
        DrawSphere(s.pos, y, s.radius, s.color);
        if (s.enemy)
        {
            DrawMesh(ringMesh_, XMMatrixScaling(s.radius * 1.65f, 1.0f, s.radius * 1.65f) *
                XMMatrixTranslation(s.pos.x, 0.08f, s.pos.z), WithAlpha(Cream, 0.35f));
        }
        else if (s.reflected)
        {
            DrawMesh(ringMesh_, XMMatrixScaling(s.radius * (2.0f + 0.3f * s.reflectedCount), 1.0f, s.radius * (2.0f + 0.3f * s.reflectedCount)) *
                XMMatrixTranslation(s.pos.x, 0.08f, s.pos.z), WithAlpha(Gold, 0.45f));
        }
    }
    for (const auto& e : enemies_)
    {
        const VisualRole role = (e.type == EnemyType::Teleport || e.type == EnemyType::Mirror || e.type == EnemyType::Barrier) ? VisualRole::EnemyShooter : (e.type == EnemyType::Mine ? VisualRole::EnemyHeavy : VisualRole::EnemyRunner);
        const Color baseColor = assetCatalog_.Get(role).fallbackColor;
        Color c = e.flash > 0.0f ? Cream : Color{
            (e.color.r + baseColor.r) * 0.5f,
            (e.color.g + baseColor.g) * 0.5f,
            (e.color.b + baseColor.b) * 0.5f,
            e.color.a
        };
        if (e.barrierT > 0.0f) c = Sky;
        DrawSphere(e.pos, e.radius, e.radius, c);
        if (e.type == EnemyType::Teleport || e.type == EnemyType::Mirror || e.type == EnemyType::Barrier)
        {
            const Player* target = FindNearestPlayer(e.pos);
            const float face = target ? AngleOf(target->pos - e.pos) : e.face;
            DrawSphere(e.pos + FromAngle(face) * 0.30f, e.radius + 0.05f, 0.13f, Red);
        }
    }
    if (boss_.active)
    {
        const Color bossBase = assetCatalog_.Get(VisualRole::Boss).fallbackColor;
        Color c = boss_.flash > 0.0f ? Cream : (boss_.bossType == BossType::HiddenBoss ? Grape : (boss_.bossType == BossType::DonutKing ? Sky : (boss_.bossType == BossType::MirrorMacaron ? Gold : bossBase)));
        DrawSphere(boss_.pos, boss_.radius, boss_.radius, c);
        DrawMesh(ringMesh_, XMMatrixScaling(boss_.radius * 1.35f, 1.0f, boss_.radius * 1.35f) *
            XMMatrixTranslation(boss_.pos.x, 0.08f, boss_.pos.z), WithAlpha(Red, 0.45f));
    }

    for (const auto& s : slashes_)
    {
        DrawSector(s);
    }

    for (const auto& p : players_)
    {
        if (!p.active) continue;
        const Color playerColor = Loadouts[static_cast<int>(p.character)].color;
        DrawSphere(p.pos, p.radius, p.radius, p.downed ? WithAlpha(Red, 0.65f) : (p.inv > 0.0f ? Cream : playerColor));
        DrawCylinder(p.pos + FromAngle(p.face) * 0.43f, 0.08f, 0.28f, Cream);
        if (p.focus && !p.downed)
        {
            DrawSphere(p.pos, 0.50f, p.hitboxRadius, Red);
            DrawMesh(ringMesh_, XMMatrixScaling(p.grazeRadius, 1.0f, p.grazeRadius) *
                XMMatrixTranslation(p.pos.x, 0.10f, p.pos.z), WithAlpha(Sky, p.grazeFlash > 0.0f ? 0.70f : 0.30f));
        }
        if (p.shieldT > 0.0f)
        {
            DrawMesh(ringMesh_, XMMatrixScaling(p.radius * 2.1f, 1.0f, p.radius * 2.1f) *
                XMMatrixTranslation(p.pos.x, 0.13f, p.pos.z), WithAlpha(Sky, 0.45f));
        }
        if (p.bombT > 0.0f)
        {
            const float t = p.bombT / 1.8f;
            DrawMesh(ringMesh_, XMMatrixScaling(1.5f + (1.0f - t) * 5.8f, 1.0f, 1.5f + (1.0f - t) * 5.8f) *
                XMMatrixTranslation(p.pos.x, 0.15f, p.pos.z), WithAlpha(Sky, 0.55f * t));
        }
    }

    for (const auto& p : particles_)
    {
        DrawSphere(p.pos, std::max(0.05f, p.y), 0.055f, WithAlpha(p.color, ClampFloat(p.ttl * 2.0f, 0.0f, 1.0f)));
    }
}

void SweetsApp::DrawHud()
{
    d2dContext_->BeginDraw();
    d2dContext_->SetTransform(D2D1::Matrix3x2F::Identity());

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
        std::wostringstream line;
        line << L"P" << (i + 1)
            << (i == 0 ? L" 1P " : (p.ai ? L" AI " : L" PAD "))
            << CharacterTexts[static_cast<int>(p.character)].jpName
            << L" HP " << static_cast<int>(std::max(0.0f, p.hp)) << L"/" << static_cast<int>(p.maxHp)
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
        const wchar_t* start = L"1Pの性能を選択してください。Enterで難易度選択、Cキーでクレジット。";
        d2dContext_->DrawTextW(start, static_cast<UINT32>(wcslen(start)), hudFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.31f, static_cast<float>(width_), static_cast<float>(height_) * 0.38f), textBrush_.Get());
        DrawLoadoutSelection();
        titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
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
        textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.65f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        const wchar_t* paused = L"一時停止";
        d2dContext_->DrawTextW(paused, static_cast<UINT32>(wcslen(paused)), hudFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.46f, static_cast<float>(width_), static_cast<float>(height_) * 0.54f), textBrush_.Get());
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
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
        ss << L"スコア " << score_ << L"  ウェーブ " << wave_ << L"  撃破 " << totalKills << L"  グレイズ " << totalGraze << L"  - Enterで再開";
        const std::wstring line = ss.str();
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
        d2dContext_->DrawTextW(line.c_str(), static_cast<UINT32>(line.size()), hudFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.50f, static_cast<float>(width_), static_cast<float>(height_) * 0.58f), textBrush_.Get());
        titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    const HRESULT hr = d2dContext_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        ReleaseRenderTargets();
        CreateRenderTargets();
    }
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
    const float t = ClampFloat(hiddenIntroT_ / 2.2f, 0.0f, 1.0f);
    const float split = t * static_cast<float>(width_) * 0.36f;

    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.80f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(0.92f, 0.84f, 0.88f, 0.92f));
    d2dContext_->FillRectangle(D2D1::RectF(-split, 0, static_cast<float>(width_) * 0.5f - split, static_cast<float>(height_)), textBrush_.Get());
    d2dContext_->FillRectangle(D2D1::RectF(static_cast<float>(width_) * 0.5f + split, 0, static_cast<float>(width_) + split, static_cast<float>(height_)), textBrush_.Get());

    textBrush_->SetColor(D2D1::ColorF(0.40f, 0.08f, 0.28f, 1.0f));
    const float cx = static_cast<float>(width_) * 0.5f;
    const float cy = static_cast<float>(height_) * 0.5f;
    std::array<D2D1_POINT_2F, 7> crack{ {
        D2D1::Point2F(cx - 18.0f, cy - 220.0f),
        D2D1::Point2F(cx + 12.0f, cy - 155.0f),
        D2D1::Point2F(cx - 25.0f, cy - 80.0f),
        D2D1::Point2F(cx + 18.0f, cy - 10.0f),
        D2D1::Point2F(cx - 12.0f, cy + 70.0f),
        D2D1::Point2F(cx + 25.0f, cy + 145.0f),
        D2D1::Point2F(cx - 16.0f, cy + 220.0f),
    } };
    for (size_t i = 1; i < crack.size(); ++i)
    {
        d2dContext_->DrawLine(crack[i - 1], crack[i], textBrush_.Get(), 4.0f + t * 5.0f);
    }

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(D2D1::ColorF(0.68f, 0.36f, 1.0f, 1.0f));
    const wchar_t* title = L"隠しボス出現";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
        D2D1::RectF(0, static_cast<float>(height_) * 0.68f, static_cast<float>(width_), static_cast<float>(height_) * 0.80f), textBrush_.Get());
    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
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

void SweetsApp::DrawSector(const Slash& s)
{
    const float alpha = ClampFloat(s.ttl / s.life, 0.0f, 1.0f) * 0.60f;
    DrawMesh(wedgeMesh_,
        XMMatrixScaling(s.range, 1.0f, s.range) *
        XMMatrixRotationY(-s.angle) *
        XMMatrixTranslation(s.pos.x, 0.11f, s.pos.z),
        WithAlpha(s.color, alpha));
}

V2 SweetsApp::ScreenToWorld(float sx, float sy) const
{
    const float px = (2.0f * sx / std::max(1u, width_)) - 1.0f;
    const float py = 1.0f - (2.0f * sy / std::max(1u, height_));
    XMMATRIX inv = XMMatrixInverse(nullptr, view_ * proj_);
    XMVECTOR nearP = XMVector3TransformCoord(XMVectorSet(px, py, 0.0f, 1.0f), inv);
    XMVECTOR farP = XMVector3TransformCoord(XMVectorSet(px, py, 1.0f, 1.0f), inv);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(farP, nearP));

    XMFLOAT3 n{};
    XMFLOAT3 d{};
    XMStoreFloat3(&n, nearP);
    XMStoreFloat3(&d, dir);
    const float t = std::fabs(d.y) > 0.0001f ? -n.y / d.y : 0.0f;
    V2 out{ n.x + d.x * t, n.z + d.z * t };
    ClampInside(out, 0.0f);
    return out;
}
