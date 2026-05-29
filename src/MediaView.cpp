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

