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

D2D1_COLOR_F OldSelectFill(bool active)
{
    return active ? D2D1::ColorF(0.23f, 0.10f, 0.15f, 0.98f) : D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.92f);
}

D2D1_COLOR_F OldSelectStroke(bool active)
{
    return active ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(0.42f, 0.25f, 0.34f, 0.90f);
}

D2D1_COLOR_F OldSelectText(bool active)
{
    return active ? D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f) : D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f);
}

float OldSelectStrokeWidth(bool active)
{
    return active ? 3.0f : 1.0f;
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
            ReleaseFrameTargets();
            CreateFrameTargets();
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
        hud << L"   Hidden Boss P" << hiddenBossForm_;
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
        float pct = ClampFloat(boss_.hp / boss_.maxHp, 0.0f, 1.0f);
        if (boss_.bossType == BossType::HiddenBoss)
        {
            const int remainingGauge = std::max(1, std::min(HiddenBossGaugeCount, static_cast<int>(std::ceil(std::max(1.0f, boss_.hp) / hiddenBossGaugeHp_))));
            const float activeGaugeHp = boss_.hp - hiddenBossGaugeHp_ * static_cast<float>(remainingGauge - 1);
            pct = ClampFloat(activeGaugeHp / hiddenBossGaugeHp_, 0.0f, 1.0f);
        }
        textBrush_->SetColor(D2D1::ColorF(0.28f, 0.08f, 0.14f, 0.85f));
        d2dContext_->FillRectangle(D2D1::RectF(left, top, left + bw, top + 14.0f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.24f, 0.35f, 0.95f));
        d2dContext_->FillRectangle(D2D1::RectF(left, top, left + bw * pct, top + 14.0f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
        const wchar_t* bossName = BossName(boss_.bossType);
        d2dContext_->DrawTextW(bossName, static_cast<UINT32>(wcslen(bossName)), smallFormat_.Get(), D2D1::RectF(left, top + 16, left + 220, top + 40), textBrush_.Get());
        if (boss_.bossType == BossType::HiddenBoss)
        {
            std::wostringstream gauge;
            gauge << L"Gauge " << std::max(1, HiddenBossGaugeCount - hiddenBossForm_ + 1) << L"/" << HiddenBossGaugeCount;
            const std::wstring gaugeText = gauge.str();
            d2dContext_->DrawTextW(gaugeText.c_str(), static_cast<UINT32>(gaugeText.size()), smallFormat_.Get(), D2D1::RectF(left + 230.0f, top + 16.0f, left + bw, top + 40.0f), textBrush_.Get());

            std::wstring lockText;
            if (hiddenBossForm_ == 1 && hiddenBossCoreOpenT_ <= 0.0f)
            {
                lockText = L"LOCK: 炎核を壊せ  本体ダメージ軽減中";
            }
            else if (hiddenBossForm_ == 2 && hiddenBossAuraBreakT_ <= 0.0f)
            {
                std::wostringstream ss;
                ss << L"LOCK: 金色弾反射 " << hiddenBossReflectCount_ << L"/" << HiddenBossReflectBreakCount << L"  本体ダメージ軽減中";
                lockText = ss.str();
            }
            else if (hiddenBossForm_ <= 2)
            {
                lockText = L"攻撃チャンス: 本体へ攻撃";
            }
            if (!lockText.empty())
            {
                textBrush_->SetColor(hiddenBossForm_ <= 2 && lockText.rfind(L"LOCK", 0) == 0
                    ? D2D1::ColorF(1.0f, 0.42f, 0.22f, 1.0f)
                    : D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
                d2dContext_->DrawTextW(lockText.c_str(), static_cast<UINT32>(lockText.size()), smallFormat_.Get(),
                    D2D1::RectF(left, top + 38.0f, left + bw, top + 62.0f), textBrush_.Get());
            }
        }
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

    if (screen_ == Screen::HiddenBoss && hiddenBossPhaseIntroT_ > 0.0f)
    {
        const float fade = ClampFloat(hiddenBossPhaseIntroT_ / std::max(0.01f, hiddenBossPhaseIntroLife_), 0.0f, 1.0f);
        const float halfW = GameplayHalfWidth(width_, height_);
        const float halfH = GameplayHalfHeight();
        const float sx = static_cast<float>(width_) * 0.5f + boss_.pos.x / halfW * static_cast<float>(width_) * 0.5f;
        const float sy = static_cast<float>(height_) * 0.5f - boss_.pos.z / halfH * static_cast<float>(height_) * 0.5f;

        textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.36f * fade));
        d2dContext_->FillRectangle(D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.20f, 0.85f * fade));
        const float r = hiddenBossForm_ >= 3 ? 96.0f : 76.0f;
        d2dContext_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(sx, sy), r, r * 0.78f), textBrush_.Get(), hiddenBossForm_ >= 3 ? 6.0f : 4.0f);
        d2dContext_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(sx, sy), r * 1.35f, r * 1.05f), textBrush_.Get(), 2.0f);
        for (int i = 0; i < 18; ++i)
        {
            const float a = TwoPi * i / 18.0f;
            const float edgeX = sx + std::cos(a) * static_cast<float>(width_) * 0.46f;
            const float edgeY = sy + std::sin(a) * static_cast<float>(height_) * 0.44f;
            const float nearX = sx + std::cos(a) * r * 1.55f;
            const float nearY = sy + std::sin(a) * r * 1.20f;
            d2dContext_->DrawLine(D2D1::Point2F(edgeX, edgeY), D2D1::Point2F(nearX, nearY), textBrush_.Get(), 1.5f);
        }
    }

    if (screen_ == Screen::HiddenBoss && hiddenBossPhaseIntroT_ <= 0.0f)
    {
        const float halfW = GameplayHalfWidth(width_, height_);
        const float halfH = GameplayHalfHeight();
        auto toScreen = [&](V2 world)
        {
            return D2D1::Point2F(
                static_cast<float>(width_) * 0.5f + world.x / halfW * static_cast<float>(width_) * 0.5f,
                static_cast<float>(height_) * 0.5f - world.z / halfH * static_cast<float>(height_) * 0.5f);
        };

        if (hiddenBossForm_ == 1 && hiddenBossCoreOpenT_ <= 0.0f)
        {
            textBrush_->SetColor(D2D1::ColorF(1.0f, 0.72f, 0.18f, 0.86f));
            const D2D1_POINT_2F origin = D2D1::Point2F(static_cast<float>(width_) * 0.50f, 235.0f);
            for (const auto& core : hiddenBossCores_)
            {
                if (!core.active) continue;
                const D2D1_POINT_2F p = toScreen(core.pos);
                d2dContext_->DrawLine(origin, p, textBrush_.Get(), 2.0f);
                d2dContext_->DrawEllipse(D2D1::Ellipse(p, 30.0f, 30.0f), textBrush_.Get(), 3.0f);
            }
        }
        else if (hiddenBossForm_ == 2 && hiddenBossAuraBreakT_ <= 0.0f)
        {
            textBrush_->SetColor(D2D1::ColorF(1.0f, 0.78f, 0.18f, 0.78f));
            int shown = 0;
            for (const auto& s : shots_)
            {
                if (!s.enemy || s.dead) continue;
                if (!(s.color.r > 0.85f && s.color.g > 0.55f && s.color.b < 0.35f)) continue;
                const D2D1_POINT_2F p = toScreen(s.pos);
                const D2D1_POINT_2F origin = D2D1::Point2F(static_cast<float>(width_) * 0.50f, 235.0f);
                d2dContext_->DrawLine(origin, p, textBrush_.Get(), 1.2f);
                d2dContext_->DrawEllipse(D2D1::Ellipse(p, 18.0f, 18.0f), textBrush_.Get(), 2.0f);
                d2dContext_->DrawEllipse(D2D1::Ellipse(p, 26.0f, 26.0f), textBrush_.Get(), 1.2f);
                if (++shown >= 8) break;
            }
        }
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
            const bool active = selected || hover;
            if (active)
            {
                textBrush_->SetColor(OldSelectFill(true));
                d2dContext_->FillRectangle(rect, textBrush_.Get());
                textBrush_->SetColor(OldSelectStroke(true));
                d2dContext_->DrawRectangle(rect, textBrush_.Get(), 3.0f);
            }
            textBrush_->SetColor(OldSelectText(active));
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
            ReleaseFrameTargets();
            CreateFrameTargets();
        }
        return;
    }

    if (screen_ == Screen::Title)
    {
        textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 1.0f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
        const wchar_t* title = L"スイーツパニック DX11";
        titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), titleFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.18f, static_cast<float>(width_), static_cast<float>(height_) * 0.29f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
        const wchar_t* start = L"1Pの性能とモードを選択してください。Enterで決定、Cキーでクレジット、Escで音量設定。";
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
    else if (screen_ == Screen::Settings)
    {
        DrawSettingsMenu();
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
            const D2D1_RECT_F rect = D2D1::RectF(x, choiceTop, x + choiceW, choiceTop + 46.0f);
            const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
            const bool active = selected || hover;
            textBrush_->SetColor(OldSelectFill(active));
            d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
            textBrush_->SetColor(OldSelectStroke(active));
            d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), OldSelectStrokeWidth(active));
            textBrush_->SetColor(OldSelectText(active));
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
        ReleaseFrameTargets();
        CreateFrameTargets();
    }
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
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 1.0f));
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

    const float buttonW = 190.0f;
    const float buttonH = 46.0f;
    const float buttonX = (static_cast<float>(width_) - buttonW) * 0.5f;
    const float buttonY = static_cast<float>(height_) * 0.68f;
    const D2D1_RECT_F backRect = D2D1::RectF(buttonX, buttonY, buttonX + buttonW, buttonY + buttonH);
    const bool hover = PointInRect(mouseX_, mouseY_, backRect.left, backRect.top, backRect.right, backRect.bottom);
    textBrush_->SetColor(OldSelectFill(hover));
    d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(backRect, 8.0f, 8.0f), textBrush_.Get());
    textBrush_->SetColor(OldSelectStroke(hover));
    d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(backRect, 8.0f, 8.0f), textBrush_.Get(), hover ? 3.0f : 1.5f);
    const wchar_t* backButton = L"Back";
    textBrush_->SetColor(OldSelectText(hover));
    d2dContext_->DrawTextW(backButton, static_cast<UINT32>(wcslen(backButton)), hudFormat_.Get(),
        D2D1::RectF(backRect.left, backRect.top + 10.0f, backRect.right, backRect.bottom), textBrush_.Get());

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

void SweetsApp::DrawCharacterSelect()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 1.0f));
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

void SweetsApp::DrawPauseMenu()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.70f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    const float panelW = 480.0f;
    const float panelH = 450.0f;
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
        const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
        const bool active = selected || hover;
        textBrush_->SetColor(OldSelectFill(active));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
        textBrush_->SetColor(OldSelectStroke(active));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), active ? 2.5f : 1.0f);
        textBrush_->SetColor(OldSelectText(active));
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

    const float aimTop = top + 348.0f;
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.92f));
    const wchar_t* aimLabel = L"攻撃方向";
    d2dContext_->DrawTextW(aimLabel, static_cast<UINT32>(wcslen(aimLabel)), smallFormat_.Get(),
        D2D1::RectF(left + 48.0f, aimTop + 6.0f, left + 132.0f, aimTop + 32.0f), textBrush_.Get());
    const float aimButtonW = 104.0f;
    const float aimButtonH = 32.0f;
    const float aimStartX = left + 138.0f;
    for (int i = 0; i < 3; ++i)
    {
        const AimMode mode = static_cast<AimMode>(i);
        const float x = aimStartX + i * (aimButtonW + 10.0f);
        const D2D1_RECT_F rect = D2D1::RectF(x, aimTop, x + aimButtonW, aimTop + aimButtonH);
        const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
        const bool active = aimMode_ == mode || hover;
        textBrush_->SetColor(OldSelectFill(active));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get());
        textBrush_->SetColor(OldSelectStroke(active));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get(), active ? 2.2f : 1.0f);
        const wchar_t* label = AimModeName(mode);
        textBrush_->SetColor(OldSelectText(active));
        d2dContext_->DrawTextW(label, static_cast<UINT32>(wcslen(label)), smallFormat_.Get(),
            D2D1::RectF(rect.left, rect.top + 7.0f, rect.right, rect.bottom), textBrush_.Get());
    }

    const wchar_t* aimHint = L"初期値は移動方向。マウス照準や近い敵オートにも切替できます。";
    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.86f));
    d2dContext_->DrawTextW(aimHint, static_cast<UINT32>(wcslen(aimHint)), smallFormat_.Get(),
        D2D1::RectF(left + 48.0f, aimTop + 42.0f, left + panelW - 48.0f, aimTop + 66.0f), textBrush_.Get());
}

void SweetsApp::DrawSettingsMenu()
{
    // 画面全体を暗くする
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.78f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    const float panelW = 480.0f;
    const float panelH = 300.0f;
    const float left = (static_cast<float>(width_) - panelW) * 0.5f;
    const float top = (static_cast<float>(height_) - panelH) * 0.5f;

    const D2D1_RECT_F panel = D2D1::RectF(left, top, left + panelW, top + panelH);
    textBrush_->SetColor(D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.96f));
    d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(panel, 8.0f, 8.0f), textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.95f));
    d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(panel, 8.0f, 8.0f), textBrush_.Get(), 2.0f);

    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
    const wchar_t* title = L"音量設定";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), hudFormat_.Get(),
        D2D1::RectF(left, top + 22.0f, left + panelW, top + 58.0f), textBrush_.Get());

    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    const std::array<const wchar_t*, 4> labels{ L"全体", L"BGM", L"効果音", L"UI" };
    const float sliderLeft = left + 170.0f;
    const float sliderRight = left + panelW - 48.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float y = top + 110.0f + i * 44.0f;
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

    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f));
    const wchar_t* hint = L"← → / ドラッグで調整   Esc または Enter で戻る";
    d2dContext_->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), smallFormat_.Get(),
        D2D1::RectF(left, top + panelH - 42.0f, left + panelW, top + panelH - 14.0f), textBrush_.Get());
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
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
        const bool active = on || hover;
        const D2D1_RECT_F rect = D2D1::RectF(x, y, x + buttonW, y + buttonH);
        textBrush_->SetColor(active ? OldSelectFill(true) : D2D1::ColorF(0.12f, 0.12f, 0.15f, 0.96f));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get());
        textBrush_->SetColor(active ? OldSelectStroke(true) : D2D1::ColorF(0.86f, 0.86f, 0.90f, 0.94f));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get(), hover ? 2.0f : 1.0f);
        textBrush_->SetColor(active ? OldSelectText(true) : D2D1::ColorF(0.86f, 0.86f, 0.90f, 0.94f));
        d2dContext_->DrawTextW(labels[i], static_cast<UINT32>(wcslen(labels[i])), smallFormat_.Get(),
            D2D1::RectF(x + 8.0f, y + 6.0f, x + buttonW - 8.0f, y + buttonH), textBrush_.Get());
    }

    const std::array<const wchar_t*, 7> fxLabels{
        L"明るさ",
        L"加算FX",
        L"フラッシュ",
        L"敵弾発光",
        L"剣FX",
        L"必殺FX",
        L"隠しオーラ"
    };
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
    d2dContext_->DrawTextW(L"表示調整", 4, smallFormat_.Get(),
        D2D1::RectF(panelLeft + 18.0f, 510.0f, panelRight - 16.0f, 532.0f), textBrush_.Get());

    const float sliderLabelX = panelLeft + 18.0f;
    const float sliderLeft = panelLeft + 118.0f;
    const float sliderRight = panelRight - 52.0f;
    for (int i = 0; i < static_cast<int>(fxLabels.size()); ++i)
    {
        const float y = 540.0f + i * 28.0f;
        const float value = DebugFxSliderValue(i);
        const bool hover = PointInRect(mouseX_, mouseY_, sliderLeft - 8.0f, y - 12.0f, sliderRight + 8.0f, y + 20.0f);
        const bool active = hover || draggingDebugFx_ == i;
        textBrush_->SetColor(active ? OldSelectText(true) : D2D1::ColorF(0.86f, 0.86f, 0.90f, 0.94f));
        d2dContext_->DrawTextW(fxLabels[i], static_cast<UINT32>(wcslen(fxLabels[i])), smallFormat_.Get(),
            D2D1::RectF(sliderLabelX, y - 8.0f, sliderLeft - 8.0f, y + 16.0f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(0.12f, 0.12f, 0.15f, 0.96f));
        d2dContext_->FillRectangle(D2D1::RectF(sliderLeft, y, sliderRight, y + 8.0f), textBrush_.Get());
        textBrush_->SetColor(active ? OldSelectStroke(true) : D2D1::ColorF(0.65f, 0.88f, 1.0f, 0.84f));
        d2dContext_->FillRectangle(D2D1::RectF(sliderLeft, y, sliderLeft + (sliderRight - sliderLeft) * value, y + 8.0f), textBrush_.Get());
        const float knob = sliderLeft + (sliderRight - sliderLeft) * value;
        d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knob, y + 4.0f), active ? 7.0f : 5.5f, active ? 7.0f : 5.5f), textBrush_.Get());
        const float display = i == 0 ? (0.5f + value) : (value * 2.0f);
        std::wostringstream valueText;
        valueText << static_cast<int>(display * 100.0f + 0.5f) << L"%";
        const std::wstring valueString = valueText.str();
        textBrush_->SetColor(D2D1::ColorF(0.86f, 0.86f, 0.90f, 0.86f));
        d2dContext_->DrawTextW(valueString.c_str(), static_cast<UINT32>(valueString.size()), smallFormat_.Get(),
            D2D1::RectF(sliderRight + 8.0f, y - 8.0f, panelRight - 12.0f, y + 16.0f), textBrush_.Get());
    }

    const D2D1_RECT_F resetRect = D2D1::RectF(panelRight - 224.0f, 742.0f, panelRight - 34.0f, 772.0f);
    const bool resetHover = PointInRect(mouseX_, mouseY_, resetRect.left, resetRect.top, resetRect.right, resetRect.bottom);
    textBrush_->SetColor(OldSelectFill(resetHover));
    d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(resetRect, 6.0f, 6.0f), textBrush_.Get());
    textBrush_->SetColor(OldSelectStroke(resetHover));
    d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(resetRect, 6.0f, 6.0f), textBrush_.Get(), resetHover ? 2.0f : 1.0f);
    textBrush_->SetColor(OldSelectText(resetHover));
    d2dContext_->DrawTextW(L"FXリセット", 6, smallFormat_.Get(),
        D2D1::RectF(resetRect.left + 12.0f, resetRect.top + 7.0f, resetRect.right - 12.0f, resetRect.bottom), textBrush_.Get());
    return;
    }
#endif
}

void SweetsApp::DrawDifficultySelection()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 1.0f));
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
        const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
        const bool active = selected || hover;

        textBrush_->SetColor(OldSelectFill(active));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
        textBrush_->SetColor(active ? OldSelectStroke(true) : D2D1::ColorF(def.color.r, def.color.g, def.color.b, 0.75f));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), OldSelectStrokeWidth(active));

        std::wstring name = practice ? L"Hidden Boss Practice" : def.name;
        textBrush_->SetColor(active ? OldSelectText(true) : D2D1::ColorF(def.color.r, def.color.g, def.color.b, 1.0f));
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

    const float buttonW = 220.0f;
    const float buttonH = 46.0f;
    const float buttonX = (static_cast<float>(width_) - buttonW) * 0.5f;
    const float buttonY = static_cast<float>(height_) * 0.62f;
    const D2D1_RECT_F rect = D2D1::RectF(buttonX, buttonY, buttonX + buttonW, buttonY + buttonH);
    const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
    textBrush_->SetColor(OldSelectFill(hover));
    d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
    textBrush_->SetColor(OldSelectStroke(hover));
    d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), hover ? 3.0f : 1.5f);
    const wchar_t* back = L"Title";
    textBrush_->SetColor(OldSelectText(hover));
    d2dContext_->DrawTextW(back, static_cast<UINT32>(wcslen(back)), hudFormat_.Get(),
        D2D1::RectF(rect.left, rect.top + 10.0f, rect.right, rect.bottom), textBrush_.Get());

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
        const bool hover = PointInRect(mouseX_, mouseY_, card.left, card.top, card.right, card.bottom);
        const bool active = selected || hover;

        textBrush_->SetColor(OldSelectFill(active));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(card, 8.0f, 8.0f), textBrush_.Get());

        textBrush_->SetColor(OldSelectStroke(active));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(card, 8.0f, 8.0f), textBrush_.Get(), OldSelectStrokeWidth(active));

        D2D1_COLOR_F accent = D2D1::ColorF(loadout.color.r, loadout.color.g, loadout.color.b, 1.0f);
        fill(D2D1::RectF(x, top, x + cardW, top + 5.0f), accent);

        std::wostringstream index;
        index << CharacterTexts[i].roleIcon;
        drawText(index.str(), hudFormat_.Get(), D2D1::RectF(x + 12.0f, top + 12.0f, x + 42.0f, top + 42.0f), accent);
        drawText(loadout.name, hudFormat_.Get(), D2D1::RectF(x + 40.0f, top + 12.0f, x + cardW - 12.0f, top + 40.0f), OldSelectText(active));
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
            const bool active = selected || hover;
            fill(rect, active ? OldSelectFill(true) : D2D1::ColorF(0.08f, 0.04f, 0.07f, 0.82f));
            stroke(rect, active ? OldSelectStroke(true) : D2D1::ColorF(0.42f, 0.25f, 0.34f, 0.9f), active ? 2.0f : 1.0f);
            draw(modes[mode], D2D1::RectF(x, y + 6.0f, x + modeW, y + rowH), OldSelectText(active));
        }

        const int loadoutIndex = std::max(0, std::min(coopLoadoutIndices_[playerIndex], static_cast<int>(Loadouts.size()) - 1));
        const D2D1_RECT_F charRect = D2D1::RectF(charX, y, charX + charW, y + rowH);
        const bool charHover = PointInRect(mouseX_, mouseY_, charRect.left, charRect.top, charRect.right, charRect.bottom);
        fill(charRect, charHover ? OldSelectFill(true) : (coopSlotModes_[playerIndex] == CoopSlotMode::Off ? D2D1::ColorF(0.06f, 0.035f, 0.055f, 0.72f) : D2D1::ColorF(0.12f, 0.055f, 0.09f, 0.92f)));
        stroke(charRect, charHover ? OldSelectStroke(true) : D2D1::ColorF(0.42f, 0.25f, 0.34f, 0.9f), charHover ? 2.0f : 1.0f);
        std::wstring charText = L"Character: ";
        charText += Loadouts[loadoutIndex].name;
        draw(charText, D2D1::RectF(charRect.left + 6.0f, charRect.top + 6.0f, charRect.right - 6.0f, charRect.bottom), OldSelectText(charHover));
    }
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

