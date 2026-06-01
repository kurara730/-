#include "EffekseerSystem.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <vector>

#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
#include <Effekseer.h>
#include <EffekseerRendererDX11.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif

// EffekseerSystem.cpp は外部Effekseer Runtimeとの接続だけを担当します。
// SWEETS_USE_EFFEKSEER が無いビルドでも、関数は失敗を返すだけでゲームを止めません。

namespace
{
// Effekseer素材は実行ファイルの隣、作業ディレクトリ、親ディレクトリから探します。
std::filesystem::path FindRuntimeAsset(const std::wstring& relativePath)
{
    const std::filesystem::path relative(relativePath);
    std::vector<std::filesystem::path> bases;
    bases.push_back(std::filesystem::current_path());

    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) > 0)
    {
        bases.push_back(std::filesystem::path(modulePath).parent_path());
    }

    for (const auto& base : bases)
    {
        std::error_code ec;
        const auto candidate = base / relative;
        if (std::filesystem::exists(candidate, ec))
        {
            return candidate;
        }
        const auto parentCandidate = base.parent_path() / relative;
        if (std::filesystem::exists(parentCandidate, ec))
        {
            return parentCandidate;
        }
    }

    return relative;
}

// Effekseer API は char16_t ベースのパスを要求するため、wstringから変換します。
std::u16string ToU16(const std::wstring& text)
{
    std::u16string out;
    out.reserve(text.size());
    for (wchar_t ch : text)
    {
        out.push_back(static_cast<char16_t>(ch));
    }
    return out;
}

#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
// DirectXMathの行列をEffekseer側の行列表現へコピーします。
Effekseer::Matrix44 ToEffekseerMatrix(const DirectX::XMMATRIX& matrix)
{
    DirectX::XMFLOAT4X4 m{};
    DirectX::XMStoreFloat4x4(&m, matrix);

    Effekseer::Matrix44 out;
    out.Values[0][0] = m._11;
    out.Values[0][1] = m._12;
    out.Values[0][2] = m._13;
    out.Values[0][3] = m._14;
    out.Values[1][0] = m._21;
    out.Values[1][1] = m._22;
    out.Values[1][2] = m._23;
    out.Values[1][3] = m._24;
    out.Values[2][0] = m._31;
    out.Values[2][1] = m._32;
    out.Values[2][2] = m._33;
    out.Values[2][3] = m._34;
    out.Values[3][0] = m._41;
    out.Values[3][1] = m._42;
    out.Values[3][2] = m._43;
    out.Values[3][3] = m._44;
    return out;
}
#endif
}

struct EffekseerSystem::Impl
{
#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
    Effekseer::ManagerRef manager;
    EffekseerRendererDX11::RendererRef dx11Backend;
    Effekseer::Backend::GraphicsDeviceRef graphicsDevice;
    std::unordered_map<std::wstring, Effekseer::EffectRef> effects;
#endif
};

EffekseerSystem::EffekseerSystem() = default;
EffekseerSystem::~EffekseerSystem() = default;

bool EffekseerSystem::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    Shutdown();
    lastError_.clear();

#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
    if (!device || !context)
    {
        lastError_ = L"Effekseer: DX11 device/context is null.";
        return false;
    }

    impl_ = std::make_unique<Impl>();
    impl_->graphicsDevice = EffekseerRendererDX11::CreateGraphicsDevice(device, context);
    impl_->dx11Backend = EffekseerRendererDX11::Renderer::Create(impl_->graphicsDevice, 4096);
    impl_->manager = Effekseer::Manager::Create(4096);

    if (!impl_->graphicsDevice || !impl_->dx11Backend || !impl_->manager)
    {
        lastError_ = L"Effekseer: runtime initialization failed.";
        Shutdown();
        return false;
    }

    impl_->manager->SetSpriteRenderer(impl_->dx11Backend->CreateSpriteRenderer());
    impl_->manager->SetRibbonRenderer(impl_->dx11Backend->CreateRibbonRenderer());
    impl_->manager->SetRingRenderer(impl_->dx11Backend->CreateRingRenderer());
    impl_->manager->SetTrackRenderer(impl_->dx11Backend->CreateTrackRenderer());
    impl_->manager->SetModelRenderer(impl_->dx11Backend->CreateModelRenderer());
    impl_->manager->SetTextureLoader(impl_->dx11Backend->CreateTextureLoader());
    impl_->manager->SetModelLoader(impl_->dx11Backend->CreateModelLoader());
    impl_->manager->SetMaterialLoader(impl_->dx11Backend->CreateMaterialLoader());
    impl_->manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());
    return true;
#else
    (void)device;
    (void)context;
    impl_ = std::make_unique<Impl>();
    lastError_ = L"Effekseer: SWEETS_USE_EFFEKSEER is disabled.";
    return false;
#endif
}

void EffekseerSystem::Shutdown()
{
    impl_.reset();
}

void EffekseerSystem::LoadEffect(const std::wstring& id, const std::wstring& relativePath, float magnification)
{
#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
    if (!impl_ || !impl_->manager) return;

    const auto path = FindRuntimeAsset(relativePath);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
    {
        lastError_ = L"Effekseer: effect not found: " + relativePath;
        return;
    }

    const std::u16string u16Path = ToU16(path.wstring());
    auto effect = Effekseer::Effect::Create(impl_->manager, u16Path.c_str(), magnification);
    if (!effect)
    {
        lastError_ = L"Effekseer: effect load failed: " + relativePath;
        return;
    }

    impl_->effects[id] = effect;
    lastError_.clear();
#else
    (void)id;
    (void)relativePath;
    (void)magnification;
#endif
}

bool EffekseerSystem::Play(const std::wstring& id, V2 position, float y, float rotationY, float scale)
{
#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
    if (!impl_ || !impl_->manager) return false;
    const auto found = impl_->effects.find(id);
    if (found == impl_->effects.end() || !found->second) return false;

    const Effekseer::Handle handle = impl_->manager->Play(found->second, position.x, position.z, y);
    if (handle < 0) return false;

    impl_->manager->SetRotation(handle, 0.0f, 0.0f, rotationY);
    impl_->manager->SetScale(handle, scale, scale, scale);
    return true;
#else
    (void)id;
    (void)position;
    (void)y;
    (void)rotationY;
    (void)scale;
    return false;
#endif
}

void EffekseerSystem::Update(float dt)
{
#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
    if (!impl_ || !impl_->manager) return;
    impl_->manager->Update(std::max(0.0f, dt) * 60.0f);
#else
    (void)dt;
#endif
}

void EffekseerSystem::Draw(const DirectX::XMMATRIX& viewProjection)
{
#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
    if (!impl_ || !impl_->manager || !impl_->dx11Backend) return;

    Effekseer::Manager::DrawParameter params;
    params.ViewProjectionMatrix = ToEffekseerMatrix(viewProjection);
    params.ZNear = 0.1f;
    params.ZFar = 100.0f;
    impl_->manager->Draw(params);
    impl_->dx11Backend->ResetRenderState();
#else
    (void)viewProjection;
#endif
}

bool EffekseerSystem::Available() const
{
#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
    return impl_ && impl_->manager && impl_->dx11Backend;
#else
    return false;
#endif
}

bool EffekseerSystem::HasEffect(const std::wstring& id) const
{
#if defined(SWEETS_USE_EFFEKSEER) && SWEETS_USE_EFFEKSEER
    if (!impl_) return false;
    const auto found = impl_->effects.find(id);
    return found != impl_->effects.end() && found->second;
#else
    (void)id;
    return false;
#endif
}
