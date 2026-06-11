#include "SweetsApp.h"

#include <filesystem>
#include <iomanip>

// MenuView.cpp は Direct2D/DirectWrite を使った文字UIとメニュー画面を担当します。
// ゲーム本体の2D/3D表示とは分け、HUD、タイトル、キャラ選択、難易度、ポーズ、
// デバッグパネルなど「画面上の情報」をまとめています。

namespace
{
// TAA用の低差異列です。
// フレームごとに投影を少しずらし、履歴と混ぜてジャギーを減らします。
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

// UIで使う画像などを、作業ディレクトリや実行ファイル周辺から探します。
// Debug/Releaseで配置場所が違っても同じ相対パスを使えるようにしています。
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

// 戦闘中HUDです。
// プレイヤーHP/ULT/ボム、ボスHP、Wave、グレイズ、フィーバーなどをまとめて表示します。
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
        << L"   フィーバー " << static_cast<int>(player_.fever) << L"%";
    if (screen_ == Screen::HiddenBoss)
    {
        hud << L"   Hidden Boss P" << hiddenBossForm_;
    }
    d2dContext_->DrawTextW(hud.str().c_str(), static_cast<UINT32>(hud.str().size()), hudFormat_.Get(),
        D2D1::RectF(18.0f, 14.0f, static_cast<float>(width_) - 18.0f, 48.0f), textBrush_.Get());

    auto fillBar = [&](float l, float t, float w, float h, float pct,
                       D2D1::ColorF bg, D2D1::ColorF fg)
    {
        pct = ClampFloat(pct, 0.0f, 1.0f);
        const D2D1_RECT_F frame = D2D1::RectF(l, t, l + w, t + h);
        textBrush_->SetColor(bg);
        d2dContext_->FillRectangle(frame, textBrush_.Get());
        textBrush_->SetColor(fg);
        d2dContext_->FillRectangle(D2D1::RectF(l, t, l + w * pct, t + h), textBrush_.Get());
        // 視認性のための外枠
        textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.95f));
        d2dContext_->DrawRectangle(frame, textBrush_.Get(), 3.0f);
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.9f));
        d2dContext_->DrawRectangle(frame, textBrush_.Get(), 1.4f);
    };
    auto hudText = [&](const std::wstring& s, float l, float t, float r, D2D1::ColorF c)
    {
        textBrush_->SetColor(c);
        d2dContext_->DrawTextW(s.c_str(), static_cast<UINT32>(s.size()), smallFormat_.Get(),
            D2D1::RectF(l, t, r, t + 22.0f), textBrush_.Get());
    };

    for (int i = 0; i < MaxPlayers; ++i)
    {
        const Player& p = players_[i];
        if (!p.active) continue;
        const float top = 48.0f + i * 26.0f;
        const int displayHp = p.hp > 0.0f ? std::max(1, static_cast<int>(std::ceil(p.hp))) : 0;
        const float hpRatio = ClampFloat(p.hp / std::max(1.0f, p.maxHp), 0.0f, 1.0f);
        const float ultRatio = ClampFloat(p.ult / 100.0f, 0.0f, 1.0f);
        const bool ultReady = p.ult >= 100.0f;
        const D2D1::ColorF barBg(0.18f, 0.06f, 0.10f, 0.85f);

        // 識別タグ（P番号・操作種別・キャラ名）
        const D2D1::ColorF tagColor = p.downed
            ? D2D1::ColorF(1.0f, 0.35f, 0.35f, 0.95f)
            : (i == 0 ? D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f) : D2D1::ColorF(0.82f, 0.88f, 1.0f, 0.92f));
        std::wostringstream tag;
        tag << L"P" << (i + 1) << (i == 0 ? L" " : (p.ai ? L" AI " : L" PAD "))
            << CharacterTexts[static_cast<int>(p.character)].jpName
            << (p.downed ? L" ダウン" : L"");
        hudText(tag.str(), 18.0f, top + 2.0f, 150.0f, tagColor);

        // HPゲージ（残量に応じて赤→緑へ変化、数値はバー上に重ねる）
        const D2D1::ColorF hpFill(0.95f - 0.60f * hpRatio, 0.30f + 0.60f * hpRatio, 0.34f + 0.14f * hpRatio, 0.95f);
        fillBar(156.0f, top + 5.0f, 150.0f, 15.0f, hpRatio, barBg, hpFill);
        std::wostringstream hpStr;
        hpStr << L"HP " << displayHp << L"/" << static_cast<int>(p.maxHp);
        hudText(hpStr.str(), 162.0f, top + 3.0f, 306.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.96f));

        // 必殺ゲージ（満タンで点滅、数値はバー右に表示）
        float ultBright = 1.0f;
        if (ultReady) ultBright = 0.78f + 0.22f * std::sin(gameTime_ * 7.0f);
        const D2D1::ColorF ultFill(1.0f * ultBright, 0.82f * ultBright, 0.30f * ultBright, 0.95f);
        fillBar(320.0f, top + 5.0f, 150.0f, 15.0f, ultRatio, barBg, ultFill);
        std::wostringstream ultStr;
        if (ultReady) ultStr << L"必殺 READY!";
        else ultStr << L"必殺 " << static_cast<int>(p.ult) << L"%";
        const D2D1::ColorF ultTextColor = ultReady
            ? D2D1::ColorF(1.0f, 0.86f, 0.32f, ClampFloat(ultBright, 0.0f, 1.0f))
            : D2D1::ColorF(1.0f, 0.82f, 0.55f, 0.95f);
        hudText(ultStr.str(), 478.0f, top + 3.0f, 600.0f, ultTextColor);

        // ブリンク残量（1Pのみ）。チャージごとにバーで表示し、回復中はゲージが満ちていく。
        if (i == 0)
        {
            hudText(L"ブリンク", 612.0f, top + 3.0f, 690.0f, D2D1::ColorF(0.70f, 0.92f, 1.0f, 0.95f));
            for (int k = 0; k < BlinkMaxCharges; ++k)
            {
                const float bx = 690.0f + k * 26.0f;
                float pct = 0.0f;
                bool ready = false;
                if (k < p.blinkCharges) { pct = 1.0f; ready = true; }
                else if (k == p.blinkCharges && p.blinkCharges < BlinkMaxCharges)
                {
                    pct = ClampFloat(1.0f - p.blinkRechargeT / BlinkChargeCooldown, 0.0f, 1.0f);
                }
                const D2D1::ColorF fg = ready
                    ? D2D1::ColorF(0.40f, 0.90f, 1.0f, 0.97f)   // 使用可：明るいシアン
                    : D2D1::ColorF(0.30f, 0.55f, 0.72f, 0.95f); // 回復中：暗いシアン
                fillBar(bx, top + 5.0f, 22.0f, 14.0f, pct, barBg, fg);
            }
            // シールドスタミナ（構えで減り、下ろすと回復。切れ中は赤）。
            hudText(L"シールド", 762.0f, top + 3.0f, 800.0f, D2D1::ColorF(0.80f, 0.95f, 1.0f, 0.95f));
            const float stamPct = ClampFloat(p.shieldStamina / ShieldStaminaMax, 0.0f, 1.0f);
            const D2D1::ColorF stamFg = p.shieldExhausted
                ? D2D1::ColorF(0.95f, 0.35f, 0.35f, 0.95f)   // 切れ中：赤
                : D2D1::ColorF(0.45f, 0.85f, 1.0f, 0.96f);   // 通常：水色
            fillBar(800.0f, top + 5.0f, 90.0f, 14.0f, stamPct, barBg, stamFg);
        }

        // ショートのヒートゲージ（自機の頭上。撃ち続けると伸び、レッドゾーンで最大火力、振り切るとオーバーヒート）
        if (p.weapon == Weapon::Strawberry && !p.downed && (p.fireHeat > 0.02f || p.overheatT > 0.0f || p.firing))
        {
            const V2 head = WorldToScreen(p.pos);
            const float bw = 72.0f;          // 見やすく拡大
            const float bh = 10.0f;
            const float bx = head.x - bw * 0.5f;
            const float by = head.z - 48.0f;
            // 濃い縁取り付きの背景（視認性アップ）
            textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.85f));
            d2dContext_->FillRectangle(D2D1::RectF(bx - 2.0f, by - 2.0f, bx + bw + 2.0f, by + bh + 2.0f), textBrush_.Get());
            textBrush_->SetColor(D2D1::ColorF(0.16f, 0.07f, 0.10f, 0.95f));
            d2dContext_->FillRectangle(D2D1::RectF(bx, by, bx + bw, by + bh), textBrush_.Get());
            if (p.overheatT > 0.0f)
            {
                // オーバーヒート中：冷却の進み具合を青系で表示＋赤点滅枠＋"OVERHEAT"表示
                const float cool = 1.0f - ClampFloat(p.overheatT / StrawberryOverheatLock, 0.0f, 1.0f);
                textBrush_->SetColor(D2D1::ColorF(0.35f, 0.66f, 1.0f, 0.97f));
                d2dContext_->FillRectangle(D2D1::RectF(bx, by, bx + bw * cool, by + bh), textBrush_.Get());
                const float blink = 0.45f + 0.55f * std::sin(gameTime_ * 14.0f);
                textBrush_->SetColor(D2D1::ColorF(1.0f, 0.28f, 0.24f, ClampFloat(blink, 0.0f, 1.0f)));
                d2dContext_->DrawRectangle(D2D1::RectF(bx - 2.0f, by - 2.0f, bx + bw + 2.0f, by + bh + 2.0f), textBrush_.Get(), 2.2f);
                textBrush_->SetColor(D2D1::ColorF(1.0f, 0.45f, 0.40f, ClampFloat(0.55f + blink * 0.45f, 0.0f, 1.0f)));
                d2dContext_->DrawTextW(L"OVERHEAT", 8, smallFormat_.Get(),
                    D2D1::RectF(bx - 20.0f, by - 22.0f, bx + bw + 20.0f, by - 2.0f), textBrush_.Get());
            }
            else
            {
                const float hr = ClampFloat(p.fireHeat / StrawberryHeatMax, 0.0f, 1.0f);
                const bool redline = hr >= StrawberryRedline;
                const D2D1::ColorF heatCol = redline ? D2D1::ColorF(1.0f, 0.72f, 0.20f, 0.98f)
                    : hr >= 0.38f ? D2D1::ColorF(1.0f, 0.30f, 0.26f, 0.96f)
                    : hr >= 0.18f ? D2D1::ColorF(1.0f, 0.40f, 0.60f, 0.94f)
                    : D2D1::ColorF(0.95f, 0.92f, 0.92f, 0.90f);
                textBrush_->SetColor(heatCol);
                d2dContext_->FillRectangle(D2D1::RectF(bx, by, bx + bw * hr, by + bh), textBrush_.Get());
                // レッドゾーンの境界マーカー（縦線）
                const float rx = bx + bw * StrawberryRedline;
                textBrush_->SetColor(D2D1::ColorF(1.0f, 0.95f, 0.55f, 0.95f));
                d2dContext_->FillRectangle(D2D1::RectF(rx - 1.0f, by - 2.0f, rx + 1.0f, by + bh + 2.0f), textBrush_.Get());
                // 外枠（常時）
                textBrush_->SetColor(D2D1::ColorF(1.0f, 0.96f, 0.90f, 0.85f));
                d2dContext_->DrawRectangle(D2D1::RectF(bx, by, bx + bw, by + bh), textBrush_.Get(), 1.0f);
                if (redline)
                {
                    // レッドゾーンは点滅枠で「もうすぐ過熱」を警告
                    const float blink = 0.55f + 0.45f * std::sin(gameTime_ * 16.0f);
                    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.85f, 0.30f, ClampFloat(blink, 0.0f, 1.0f)));
                    d2dContext_->DrawRectangle(D2D1::RectF(bx - 2.0f, by - 2.0f, bx + bw + 2.0f, by + bh + 2.0f), textBrush_.Get(), 2.0f);
                }
            }
        }
    }

    if (boss_.active)
    {
        const float bw = std::min(520.0f, static_cast<float>(width_) * 0.58f);
        const float left = (static_cast<float>(width_) - bw) * 0.5f;
        const float top = 154.0f;
        float pct = ClampFloat(boss_.hp / boss_.maxHp, 0.0f, 1.0f);
        if (boss_.bossType == BossType::HiddenBoss)
        {
            textBrush_->SetColor(D2D1::ColorF(0.12f, 0.04f, 0.09f, 0.92f));
            d2dContext_->FillRectangle(D2D1::RectF(left, top, left + bw, top + 14.0f), textBrush_.Get());
            const D2D1::ColorF gaugeColors[HiddenBossGaugeCount] =
            {
                D2D1::ColorF(1.0f, 0.24f, 0.35f, 0.96f),
                D2D1::ColorF(1.0f, 0.70f, 0.25f, 0.94f),
                D2D1::ColorF(0.68f, 0.36f, 1.0f, 0.92f)
            };
            const float gap = 1.0f;
            const float stripH = (14.0f - gap * static_cast<float>(HiddenBossGaugeCount - 1)) / static_cast<float>(HiddenBossGaugeCount);
            for (int gauge = 0; gauge < HiddenBossGaugeCount; ++gauge)
            {
                const float y = top + static_cast<float>(HiddenBossGaugeCount - 1 - gauge) * (stripH + gap);
                const float amount = ClampFloat((boss_.hp - hiddenBossGaugeHp_ * static_cast<float>(gauge)) / hiddenBossGaugeHp_, 0.0f, 1.0f);
                const D2D1_RECT_F strip = D2D1::RectF(left + 1.0f, y, left + bw - 1.0f, y + stripH);
                textBrush_->SetColor(D2D1::ColorF(0.22f, 0.08f, 0.12f, 0.70f));
                d2dContext_->FillRectangle(strip, textBrush_.Get());
                if (amount > 0.0f)
                {
                    textBrush_->SetColor(gaugeColors[gauge]);
                    d2dContext_->FillRectangle(D2D1::RectF(strip.left, strip.top, strip.left + (strip.right - strip.left) * amount, strip.bottom), textBrush_.Get());
                }
            }
        }
        else
        {
            // SAO風の分割HPバー：HPを BossGaugeCount 本のゲージに分割表示。
            // 区切り線で「分割」を示し、現在のフェーズに応じて色が緑→黄→橙→赤と変化する。
            const float barH = 14.0f;
            textBrush_->SetColor(D2D1::ColorF(0.10f, 0.05f, 0.07f, 0.88f));
            d2dContext_->FillRectangle(D2D1::RectF(left, top, left + bw, top + barH), textBrush_.Get());
            const D2D1::ColorF phaseColors[4] =
            {
                D2D1::ColorF(0.32f, 0.86f, 0.42f, 0.96f), // PHASE1 緑
                D2D1::ColorF(0.96f, 0.86f, 0.26f, 0.96f), // PHASE2 黄
                D2D1::ColorF(1.0f,  0.56f, 0.20f, 0.96f), // PHASE3 橙
                D2D1::ColorF(1.0f,  0.26f, 0.32f, 0.96f)  // PHASE4 赤
            };
            const int ci = std::max(0, std::min(3, boss_.phase - 1));
            textBrush_->SetColor(phaseColors[ci]);
            d2dContext_->FillRectangle(D2D1::RectF(left, top, left + bw * pct, top + barH), textBrush_.Get());
            // 各ゲージの区切り線。
            textBrush_->SetColor(D2D1::ColorF(0.05f, 0.03f, 0.04f, 0.95f));
            for (int g = 1; g < BossGaugeCount; ++g)
            {
                const float x = left + bw * (static_cast<float>(g) / static_cast<float>(BossGaugeCount));
                d2dContext_->FillRectangle(D2D1::RectF(x - 1.0f, top, x + 1.0f, top + barH), textBrush_.Get());
            }
            // 残りゲージ本数を右端（名前行）に表示。
            const std::wstring gaugeText = std::to_wstring(BossGaugeCount - boss_.phase + 1) + L"/" + std::to_wstring(BossGaugeCount);
            textBrush_->SetColor(D2D1::ColorF(1.0f, 0.95f, 0.85f, 0.95f));
            d2dContext_->DrawTextW(gaugeText.c_str(), static_cast<UINT32>(gaugeText.size()), smallFormat_.Get(), D2D1::RectF(left + bw - 70.0f, top + 22.0f, left + bw, top + 46.0f), textBrush_.Get());
        }
        // 崩し（体幹）ゲージ。反射の蓄積でHPバー直下に水色表示。満タン/崩し中は強調。
        if (boss_.bossType != BossType::HiddenBoss && boss_.breakGaugeMax > 0.0f)
        {
            const float by = top + 15.0f;
            const float bhh = 5.0f;
            const float bpct = boss_.breakT > 0.0f ? 1.0f
                : ClampFloat(boss_.breakGauge / boss_.breakGaugeMax, 0.0f, 1.0f);
            textBrush_->SetColor(D2D1::ColorF(0.06f, 0.12f, 0.18f, 0.80f));
            d2dContext_->FillRectangle(D2D1::RectF(left, by, left + bw, by + bhh), textBrush_.Get());
            if (boss_.breakT > 0.0f)
            {
                // ブレイク中は明滅で「動けない＝攻撃チャンス」を強調。
                const float blink = 0.6f + 0.4f * std::sin(gameTime_ * 12.0f);
                textBrush_->SetColor(D2D1::ColorF(1.0f, 0.85f, 0.30f, ClampFloat(blink, 0.0f, 1.0f)));
                d2dContext_->FillRectangle(D2D1::RectF(left, by, left + bw, by + bhh), textBrush_.Get());
                const wchar_t* bt = L"崩し!";
                textBrush_->SetColor(D2D1::ColorF(1.0f, 0.95f, 0.55f, 1.0f));
                d2dContext_->DrawTextW(bt, static_cast<UINT32>(wcslen(bt)), smallFormat_.Get(), D2D1::RectF(left + bw * 0.5f - 40.0f, top + 22.0f, left + bw * 0.5f + 40.0f, top + 46.0f), textBrush_.Get());
            }
            else
            {
                textBrush_->SetColor(D2D1::ColorF(0.35f, 0.80f, 1.0f, 0.95f));
                d2dContext_->FillRectangle(D2D1::RectF(left, by, left + bw * bpct, by + bhh), textBrush_.Get());
            }
        }
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
        const wchar_t* bossName = BossName(boss_.bossType);
        d2dContext_->DrawTextW(bossName, static_cast<UINT32>(wcslen(bossName)), smallFormat_.Get(), D2D1::RectF(left, top + 22, left + 220, top + 46), textBrush_.Get());
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
                ss << L"LOCK: 金色キー弾反射 " << hiddenBossReflectCount_ << L"/" << HiddenBossReflectBreakCount << L"  本体ダメージ軽減中";
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
        // デバッグ（ボスラッシュ）：このボスの技セットと、技を即発動できるキー一覧を表示。
        if (gameMode_ == GameMode::BossOnlyDebug)
        {
            std::wostringstream dbg;
            dbg << L"[技] ";
            if (boss_.kitTurret) dbg << L"タレット ";
            if (boss_.kitBeam) dbg << L"ビーム ";
            if (boss_.kitSplit) dbg << L"分裂 ";
            if (boss_.kitFanSlash) dbg << L"扇 ";
            if (boss_.kitShockwave) dbg << L"衝撃波 ";
            const wchar_t* big = boss_.bigMove == static_cast<int>(BossBigMove::MegaBeam) ? L"薙払"
                : (boss_.bigMove == static_cast<int>(BossBigMove::Meteor) ? L"隕石" : L"突進");
            dbg << L"/ 大技:" << big;
            const std::wstring kitText = dbg.str();
            textBrush_->SetColor(D2D1::ColorF(0.65f, 0.95f, 0.78f, 0.95f));
            d2dContext_->DrawTextW(kitText.c_str(), static_cast<UINT32>(kitText.size()), smallFormat_.Get(),
                D2D1::RectF(left, top + 44.0f, left + bw, top + 66.0f), textBrush_.Get());
            const wchar_t* keyHint = L"DBG発動: 1ビーム 2タレット 3分裂 4扇 5衝撃波 6薙払 7隕石 8突進 9つかみ";
            textBrush_->SetColor(D2D1::ColorF(0.85f, 0.85f, 0.85f, 0.85f));
            d2dContext_->DrawTextW(keyHint, static_cast<UINT32>(wcslen(keyHint)), smallFormat_.Get(),
                D2D1::RectF(left, top + 64.0f, left + bw + 200.0f, top + 86.0f), textBrush_.Get());
        }
        // ブレイクコンボ表示（ブレイク中のみ）。ヒットを重ねるほど倍率が上がる。
        if (boss_.breakT > 0.0f && breakCombo_ > 0)
        {
            const float mul = std::min(BreakComboMaxMul, 1.0f + static_cast<float>(breakCombo_) * BreakComboDamagePerHit);
            wchar_t buf[64];
            swprintf_s(buf, L"BREAK COMBO  %d   x%.2f", breakCombo_, mul);
            const float pop = 1.0f + 0.06f * std::sin(gameTime_ * 18.0f);
            D2D1_MATRIX_3X2_F prev; d2dContext_->GetTransform(&prev);
            const D2D1_POINT_2F c = D2D1::Point2F(static_cast<float>(width_) * 0.5f, top + 64.0f);
            d2dContext_->SetTransform(D2D1::Matrix3x2F::Scale(pop, pop, c) * prev);
            smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.55f));
            d2dContext_->DrawTextW(buf, static_cast<UINT32>(wcslen(buf)), smallFormat_.Get(), D2D1::RectF(c.x - 159.0f, c.y + 1.0f, c.x + 161.0f, c.y + 27.0f), textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
            textBrush_->SetColor(D2D1::ColorF(1.0f, 0.85f, 0.30f, 1.0f));
            d2dContext_->DrawTextW(buf, static_cast<UINT32>(wcslen(buf)), smallFormat_.Get(), D2D1::RectF(c.x - 160.0f, c.y, c.x + 160.0f, c.y + 26.0f), textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
            smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            d2dContext_->SetTransform(prev);
        }
    }
    // ダメージ数値（モンハンライズ風）。ワールド座標→スクリーンへ投影し、黒縁付きで描く。
    if (!damageNumbers_.empty()) smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    for (const auto& dn : damageNumbers_)
    {
        const float fade = ClampFloat(dn.ttl / std::max(0.01f, dn.life), 0.0f, 1.0f);
        const V2 s = WorldToScreen(dn.pos);
        wchar_t buf[24];
        swprintf_s(buf, L"%d", static_cast<int>(dn.value + 0.5f));
        const float scale = dn.crit ? 1.5f : 1.0f;
        D2D1_MATRIX_3X2_F prev; d2dContext_->GetTransform(&prev);
        const D2D1_POINT_2F c = D2D1::Point2F(s.x, s.z);
        d2dContext_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, c) * prev);
        const D2D1_RECT_F box = D2D1::RectF(c.x - 60.0f, c.y - 13.0f, c.x + 60.0f, c.y + 13.0f);
        // 黒縁
        textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.7f * fade));
        for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox)
        {
            if (ox == 0 && oy == 0) continue;
            d2dContext_->DrawTextW(buf, static_cast<UINT32>(wcslen(buf)), smallFormat_.Get(),
                D2D1::RectF(box.left + ox, box.top + oy, box.right + ox, box.bottom + oy), textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
        }
        textBrush_->SetColor(D2D1::ColorF(dn.color.r, dn.color.g, dn.color.b, fade));
        d2dContext_->DrawTextW(buf, static_cast<UINT32>(wcslen(buf)), smallFormat_.Get(), box, textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
        d2dContext_->SetTransform(prev);
    }
    if (!damageNumbers_.empty()) smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    // 自機付近：ジャスト回避カウント（常時）とネガポジ状態を小さく表示。
    if ((screen_ == Screen::Playing || screen_ == Screen::HiddenBoss) && !player_.downed)
    {
        const V2 ps = WorldToScreen(player_.pos);
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        if (negaposiT_ > 0.0f)
        {
            wchar_t nb[64];
            swprintf_s(nb, L"NEGA/POSI %.1f  蓄積 %d", negaposiT_, static_cast<int>(negaposiAccum_));
            const float gl = 0.6f + 0.4f * std::sin(gameTime_ * 10.0f);
            textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.6f));
            d2dContext_->DrawTextW(nb, static_cast<UINT32>(wcslen(nb)), smallFormat_.Get(), D2D1::RectF(ps.x - 109.0f, ps.z - 61.0f, ps.x + 111.0f, ps.z - 41.0f), textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
            textBrush_->SetColor(D2D1::ColorF(0.88f, 0.6f, 1.0f, ClampFloat(gl, 0.0f, 1.0f)));
            d2dContext_->DrawTextW(nb, static_cast<UINT32>(wcslen(nb)), smallFormat_.Get(), D2D1::RectF(ps.x - 110.0f, ps.z - 62.0f, ps.x + 110.0f, ps.z - 42.0f), textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
        }
        else
        {
            wchar_t jb[32];
            swprintf_s(jb, L"反射 %d/%d", reflectCount_, NegaPosiReflectReq);
            const bool nearReady = reflectCount_ >= NegaPosiReflectReq - 1;
            textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.55f));
            d2dContext_->DrawTextW(jb, static_cast<UINT32>(wcslen(jb)), smallFormat_.Get(), D2D1::RectF(ps.x - 59.0f, ps.z - 57.0f, ps.x + 61.0f, ps.z - 39.0f), textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
            textBrush_->SetColor(nearReady ? D2D1::ColorF(1.0f, 0.7f, 0.9f, 0.95f) : D2D1::ColorF(1.0f, 0.9f, 0.45f, 0.85f));
            d2dContext_->DrawTextW(jb, static_cast<UINT32>(wcslen(jb)), smallFormat_.Get(), D2D1::RectF(ps.x - 60.0f, ps.z - 58.0f, ps.x + 60.0f, ps.z - 40.0f), textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
        }
        smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.88f));
    const wchar_t* help = L"WASD/矢印: 移動  |  左クリック: 反射シールド  |  右クリック: リフレクションコア設置(チャージ満タンで)  |  Space: ブリンク回避  |  E: 必殺  |  P: 一時停止";
    d2dContext_->DrawTextW(help, static_cast<UINT32>(wcslen(help)), smallFormat_.Get(),
        D2D1::RectF(18.0f, static_cast<float>(height_) - 34.0f, static_cast<float>(width_) - 18.0f, static_cast<float>(height_) - 8.0f), textBrush_.Get());

    // 下部に出ていた短文通知は表示しません。
    // message_ と combatNotices_ 自体は内部状態として残し、ゲーム進行やデバッグ処理には影響させないようにします。

    if (screen_ == Screen::HiddenBoss && hiddenBossPhaseIntroT_ > 0.0f)
    {
        const float fade = ClampFloat(hiddenBossPhaseIntroT_ / std::max(0.01f, hiddenBossPhaseIntroLife_), 0.0f, 1.0f);
        const V2 bossScreen = WorldToScreen(boss_.pos);
        const float sx = bossScreen.x;
        const float sy = bossScreen.z;

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
        auto toScreen = [&](V2 world)
        {
            const V2 screen = WorldToScreen(world);
            return D2D1::Point2F(screen.x, screen.z);
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

        const std::array<const wchar_t*, 3> items{ L"ボス戦", L"Credits", L"設定 (音量)" };
        const float itemW = 248.0f;
        const float itemH = 58.0f;
        const float gap = 12.0f;
        const float menuX = 42.0f;
        const float menuTop = std::max(112.0f, static_cast<float>(height_) * 0.18f);
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            const float y = menuTop + i * (itemH + gap);
            const D2D1_RECT_F rect = D2D1::RectF(menuX, y, menuX + itemW, y + itemH);
            const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
            // マウスのホバーに加え、コントローラの上下選択（titleMenuIndex_）もハイライトする。
            const bool active = hover || i == titleMenuIndex_;
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
        const wchar_t* hint = L"クリックで選択。ゲーム中はWASD/矢印で移動、左クリック/Spaceで通常弾。";
        d2dContext_->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), smallFormat_.Get(),
            D2D1::RectF(40.0f, menuTop + itemH * 4.0f + gap * 4.0f + 8.0f, dividerX - 18.0f, static_cast<float>(height_) - 24.0f), textBrush_.Get());

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

    if (screen_ == Screen::CharacterSelect)
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
        for (const auto& p : players_)
        {
            totalKills += p.kills;
        }
        ss << L"スコア " << score_ << L"  ウェーブ " << wave_ << L"  撃破 " << totalKills;
        const std::wstring line = ss.str();
        textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 1.0f));
        d2dContext_->DrawTextW(line.c_str(), static_cast<UINT32>(line.size()), hudFormat_.Get(),
            D2D1::RectF(0, static_cast<float>(height_) * 0.50f, static_cast<float>(width_), static_cast<float>(height_) * 0.58f), textBrush_.Get());
        const std::array<const wchar_t*, 2> choices{ L"Retry", L"Title" };
        const float choiceW = 180.0f;
        const float choiceTop = static_cast<float>(height_) * 0.62f;
        for (int i = 0; i < 2; ++i)
        {
            const float x = static_cast<float>(width_) * 0.5f - choiceW - 10.0f + i * (choiceW + 20.0f);
            const D2D1_RECT_F rect = D2D1::RectF(x, choiceTop, x + choiceW, choiceTop + 46.0f);
            const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
            const bool active = hover;
            textBrush_->SetColor(OldSelectFill(active));
            d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
            textBrush_->SetColor(OldSelectStroke(active));
            d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), OldSelectStrokeWidth(active));
            textBrush_->SetColor(OldSelectText(active));
            d2dContext_->DrawTextW(choices[i], static_cast<UINT32>(wcslen(choices[i])), hudFormat_.Get(),
                D2D1::RectF(x, choiceTop + 10.0f, x + choiceW, choiceTop + 46.0f), textBrush_.Get());
        }
        const wchar_t* guide = L"クリックで選択。Esc / Backspaceでタイトルへ戻れます。";
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

// 起動ロード画面です。
// ウィンドウをすぐ出した後、段階ロードの進行状況をここで見せます。
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

// クレジット画面です。
// BGM素材など、明示したい表記をタイトルから確認できるようにしています。
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
    const wchar_t* back = L"Backボタン / Esc / Backspace でタイトルへ戻る";
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

// キャラクター選択画面です。
// タイトルではゲーム背景を出さず、ここで1Pキャラと2P-4P参加設定を選びます。
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
    const wchar_t* guide = L"カードをクリックしてキャラを選択。2P-4Pは下の行で参加設定。Escでタイトルへ戻る";
    d2dContext_->DrawTextW(guide, static_cast<UINT32>(wcslen(guide)), hudFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.28f, static_cast<float>(width_), static_cast<float>(height_) * 0.34f), textBrush_.Get());

    DrawLoadoutSelection();
    DrawCoopSlotSelection();

    titleFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

// ポーズ画面です。
// ゲームを止めたまま、再開、リスタート、音量、照準モード、タイトル戻りを操作します。
void SweetsApp::DrawPauseMenu()
{
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 0.70f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    const float panelW = 420.0f;
    const float panelH = 392.0f;
    const float left = (static_cast<float>(width_) - panelW) * 0.5f;
    const float top = (static_cast<float>(height_) - panelH) * 0.5f;
    const D2D1_RECT_F panel = D2D1::RectF(left, top, left + panelW, top + panelH);
    textBrush_->SetColor(D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.96f));
    d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(panel, 8.0f, 8.0f), textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.95f));
    d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(panel, 8.0f, 8.0f), textBrush_.Get(), 2.0f);

    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
    const wchar_t* title = L"ポーズ";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), hudFormat_.Get(),
        D2D1::RectF(left, top + 22.0f, left + panelW, top + 58.0f), textBrush_.Get());

    const std::array<const wchar_t*, 4> buttons{ L"続ける", L"リスタート", L"音量設定", L"タイトルに戻る" };
    const float buttonW = panelW - 88.0f;
    const float buttonH = 50.0f;
    const float buttonX = left + 44.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float y = top + 80.0f + i * 62.0f;
        const D2D1_RECT_F rect = D2D1::RectF(buttonX, y, buttonX + buttonW, y + buttonH);
        const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
        const bool active = hover;
        textBrush_->SetColor(OldSelectFill(active));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
        textBrush_->SetColor(OldSelectStroke(active));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), active ? 2.5f : 1.0f);
        textBrush_->SetColor(OldSelectText(active));
        d2dContext_->DrawTextW(buttons[i], static_cast<UINT32>(wcslen(buttons[i])), hudFormat_.Get(),
            D2D1::RectF(rect.left, rect.top + 12.0f, rect.right, rect.bottom), textBrush_.Get());
    }

    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.88f));
    const wchar_t* hint = L"Esc / Pで再開。タイトルへ戻る場合はボタンをクリック。";
    d2dContext_->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), smallFormat_.Get(),
        D2D1::RectF(left, top + panelH - 36.0f, left + panelW, top + panelH - 12.0f), textBrush_.Get());
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

// タイトル/ポーズから開く設定画面です。
// 現状は音量と攻撃方向を扱い、保存して次回起動にも反映します。
void SweetsApp::DrawSettingsMenu()
{
    // 画面全体を暗くする
    textBrush_->SetColor(D2D1::ColorF(0.05f, 0.02f, 0.04f, 1.0f));
    d2dContext_->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(width_), static_cast<float>(height_)), textBrush_.Get());

    const SettingsLayout layout = BuildSettingsLayout();
    const float left = layout.panel.left;
    const float top = layout.panel.top;
    const float panelW = layout.panel.right - layout.panel.left;
    const float panelH = layout.panel.bottom - layout.panel.top;

    const D2D1_RECT_F panel = D2D1::RectF(layout.panel.left, layout.panel.top, layout.panel.right, layout.panel.bottom);
    textBrush_->SetColor(D2D1::ColorF(0.10f, 0.045f, 0.075f, 0.96f));
    d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(panel, 8.0f, 8.0f), textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 0.95f));
    d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(panel, 8.0f, 8.0f), textBrush_.Get(), 2.0f);

    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.36f, 1.0f));
    const wchar_t* title = L"設定";
    d2dContext_->DrawTextW(title, static_cast<UINT32>(wcslen(title)), hudFormat_.Get(),
        D2D1::RectF(left, top + 22.0f, left + panelW, top + 58.0f), textBrush_.Get());

    hudFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    const std::array<const wchar_t*, 5> labels{ L"全体", L"BGM", L"効果音", L"UI", L"画面振動" };
    for (int i = 0; i < 5; ++i)
    {
        const float y = top + 110.0f + i * 44.0f;
        const UiRect& hit = layout.volumeSliders[i];
        const bool hover = PointInRect(mouseX_, mouseY_, hit.left, hit.top, hit.right, hit.bottom);
        const bool active = hover || draggingVolume_ == i;
        textBrush_->SetColor(active ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.92f));
        d2dContext_->DrawTextW(labels[i], static_cast<UINT32>(wcslen(labels[i])), smallFormat_.Get(),
            D2D1::RectF(left + 48.0f, y - 10.0f, left + 152.0f, y + 14.0f), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(0.24f, 0.11f, 0.17f, 0.94f));
        d2dContext_->FillRectangle(D2D1::RectF(layout.sliderLeft, y, layout.sliderRight, y + 8.0f), textBrush_.Get());
        const float value = VolumeSliderValue(i);
        textBrush_->SetColor(D2D1::ColorF(0.65f, 0.88f, 1.0f, 0.95f));
        d2dContext_->FillRectangle(D2D1::RectF(layout.sliderLeft, y, layout.sliderLeft + (layout.sliderRight - layout.sliderLeft) * value, y + 8.0f), textBrush_.Get());
        textBrush_->SetColor(active ? D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f) : D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f));
        const float knob = layout.sliderLeft + (layout.sliderRight - layout.sliderLeft) * value;
        d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knob, y + 4.0f), 7.0f, 7.0f), textBrush_.Get());
        std::wostringstream pct;
        pct << static_cast<int>(value * 100.0f + 0.5f) << L"%";
        const std::wstring pctText = pct.str();
        d2dContext_->DrawTextW(pctText.c_str(), static_cast<UINT32>(pctText.size()), smallFormat_.Get(),
            D2D1::RectF(layout.sliderRight + 10.0f, y - 10.0f, left + panelW - 8.0f, y + 14.0f), textBrush_.Get());
    }

    // 攻撃方向(照準モード)。マウスクリックで選択(ゲーム中は T キーでも切替可)。
    const float aimTop = top + 110.0f + 5 * 44.0f + 8.0f;
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.92f));
    const wchar_t* aimLabel = L"自機の攻撃方向";
    d2dContext_->DrawTextW(aimLabel, static_cast<UINT32>(wcslen(aimLabel)), smallFormat_.Get(),
        D2D1::RectF(left + 38.0f, aimTop + 6.0f, left + 140.0f, aimTop + 32.0f), textBrush_.Get());
    int hoveredAim = -1;
    for (int i = 0; i < 3; ++i)
    {
        const UiRect& hit = layout.aimButtons[i];
        if (PointInRect(mouseX_, mouseY_, hit.left, hit.top, hit.right, hit.bottom))
        {
            hoveredAim = i;
            break;
        }
    }
    for (int i = 0; i < 3; ++i)
    {
        const AimMode mode = static_cast<AimMode>(i);
        const UiRect& hit = layout.aimButtons[i];
        const D2D1_RECT_F rect = D2D1::RectF(hit.left, hit.top, hit.right, hit.bottom);
        const bool hover = hoveredAim == i;
        const bool active = hover || (aimMode_ == mode && hoveredAim < 0);
        textBrush_->SetColor(OldSelectFill(active));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get());
        textBrush_->SetColor(OldSelectStroke(active));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 6.0f, 6.0f), textBrush_.Get(), active ? 2.2f : 1.0f);
        const wchar_t* label = AimModeName(mode);
        textBrush_->SetColor(OldSelectText(active));
        d2dContext_->DrawTextW(label, static_cast<UINT32>(wcslen(label)), smallFormat_.Get(),
            D2D1::RectF(rect.left, rect.top + 7.0f, rect.right, rect.bottom), textBrush_.Get());
    }

    const UiRect& fullscreenHit = layout.fullscreenToggle;
    const D2D1_RECT_F fullscreenRect = D2D1::RectF(fullscreenHit.left, fullscreenHit.top, fullscreenHit.right, fullscreenHit.bottom);
    const bool fullscreenHover = PointInRect(mouseX_, mouseY_, fullscreenHit.left, fullscreenHit.top, fullscreenHit.right, fullscreenHit.bottom);
    const bool fullscreenActive = fullscreenHover || fullscreen_;
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.94f, 0.86f, 0.92f));
    const wchar_t* fullscreenLabel = L"Fullscreen";
    d2dContext_->DrawTextW(fullscreenLabel, static_cast<UINT32>(wcslen(fullscreenLabel)), smallFormat_.Get(),
        D2D1::RectF(left + 38.0f, fullscreenHit.top + 6.0f, left + 180.0f, fullscreenHit.bottom), textBrush_.Get());
    textBrush_->SetColor(OldSelectFill(fullscreenActive));
    d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(fullscreenRect, 6.0f, 6.0f), textBrush_.Get());
    textBrush_->SetColor(OldSelectStroke(fullscreenActive));
    d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(fullscreenRect, 6.0f, 6.0f), textBrush_.Get(), fullscreenActive ? 2.2f : 1.0f);
    const wchar_t* fullscreenText = fullscreen_ ? L"ON" : L"OFF";
    textBrush_->SetColor(OldSelectText(fullscreenActive));
    d2dContext_->DrawTextW(fullscreenText, static_cast<UINT32>(wcslen(fullscreenText)), smallFormat_.Get(),
        D2D1::RectF(fullscreenRect.left, fullscreenRect.top + 7.0f, fullscreenRect.right, fullscreenRect.bottom), textBrush_.Get());

    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    textBrush_->SetColor(D2D1::ColorF(0.86f, 0.74f, 0.80f, 0.92f));
    const wchar_t* hint = L"スライダーと攻撃方向をクリックで操作。Esc / Backspaceで戻る";
    d2dContext_->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), smallFormat_.Get(),
        D2D1::RectF(left, top + panelH - 40.0f, left + panelW, top + panelH - 12.0f), textBrush_.Get());
    smallFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}

// F1デバッグパネルです。
// FPS、画面状態、敵数、FX調整、チート操作を開発用にまとめています。
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
        L"明るさ値",
        L"加算FX値",
        L"フラッシュ値",
        L"敵弾発光値",
        L"剣FX値",
        L"必殺FX値",
        L"隠しオーラ値"
    };
    textBrush_->SetColor(D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
    d2dContext_->DrawTextW(L"表示調整（実数）", 8, smallFormat_.Get(),
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
        const float display = DebugFxDisplayValue(i);
        std::wostringstream valueText;
        valueText << std::fixed << std::setprecision(2) << display;
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

// 難易度選択画面です。
// 数値比較ではなく、難易度名・説明・初期ボムだけを見せる画面です。
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
    const wchar_t* guide = L"クリックで難易度を選択して開始";
    d2dContext_->DrawTextW(guide, static_cast<UINT32>(wcslen(guide)), hudFormat_.Get(),
        D2D1::RectF(0.0f, static_cast<float>(height_) * 0.27f, static_cast<float>(width_), static_cast<float>(height_) * 0.33f), textBrush_.Get());

    const int optionCount = DifficultyOptionCount();
    const float cardW = std::min(210.0f, (static_cast<float>(width_) - 100.0f) / 3.0f);
    const float cardH = 92.0f;
    const float gap = 16.0f;
    const float totalW = cardW * 3.0f + gap * 2.0f;
    const float startX = (static_cast<float>(width_) - totalW) * 0.5f;
    const float top = static_cast<float>(height_) * 0.36f;

    const int debugIndex = optionCount - 1; // 末尾はデバッグカード
    for (int i = 0; i < optionCount; ++i)
    {
        const bool isDebug = i == debugIndex;
        const bool practice = !isDebug && hiddenBossUnlocked_ && i == 5;
        const DifficultyDef& def = (practice || isDebug) ? DifficultyDefs[static_cast<int>(Difficulty::Lunatic)] : DifficultyDefs[i];
        const int col = i % 3;
        const int row = i / 3;
        const float x = startX + col * (cardW + gap);
        const float y = top + row * (cardH + gap);
        const D2D1_RECT_F rect = D2D1::RectF(x, y, x + cardW, y + cardH);
        const bool hover = PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom);
        const bool active = hover;

        textBrush_->SetColor(OldSelectFill(active));
        d2dContext_->FillRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get());
        textBrush_->SetColor(active ? OldSelectStroke(true) : D2D1::ColorF(def.color.r, def.color.g, def.color.b, 0.75f));
        d2dContext_->DrawRoundedRectangle(D2D1::RoundedRect(rect, 8.0f, 8.0f), textBrush_.Get(), OldSelectStrokeWidth(active));

        std::wstring name = isDebug ? L"Boss Only (Debug)" : (practice ? L"Hidden Boss Practice" : def.name);
        textBrush_->SetColor(active ? OldSelectText(true) : D2D1::ColorF(def.color.r, def.color.g, def.color.b, 1.0f));
        d2dContext_->DrawTextW(name.c_str(), static_cast<UINT32>(name.size()), hudFormat_.Get(),
            D2D1::RectF(x + 10.0f, y + 12.0f, x + cardW - 10.0f, y + 40.0f), textBrush_.Get());

        const std::wstring summary = isDebug ? L"新技のみ / ボス即出現" : (practice ? L"解禁済み: 隠しボス戦から開始" : def.summary);
        textBrush_->SetColor(D2D1::ColorF(0.92f, 0.84f, 0.88f, 0.96f));
        d2dContext_->DrawTextW(summary.c_str(), static_cast<UINT32>(summary.size()), smallFormat_.Get(),
            D2D1::RectF(x + 12.0f, y + 44.0f, x + cardW - 12.0f, y + 66.0f), textBrush_.Get());

        std::wostringstream stats;
        stats << L"初期ボム " << def.initialBombs;
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
    for (const auto& p : players_)
    {
        totalKills += p.kills;
    }
    ss << L"スコア " << score_ << L"  撃破 " << totalKills;
    if (pendingHiddenBoss_ && screen_ == Screen::Clear) ss << L"  - 何かが近づいてくる";
    else ss << L"  - Titleボタンでタイトルへ";
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

// 隠しボス登場演出です。
// クリア画面を割る演出と、10秒間の飛行/降下演出を表示します。
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
    const float sigilT = ClampFloat((hiddenIntroT_ - 1.05f) / 4.8f, 0.0f, 1.0f);
    const float dropT = ClampFloat((hiddenIntroT_ / HiddenBossIntroDuration - 0.70f) / 0.30f, 0.0f, 1.0f);
    const float landingFlash = ClampFloat(1.0f - std::fabs(hiddenIntroT_ - HiddenBossIntroDuration * 0.90f) / 0.34f, 0.0f, 1.0f);
    const float bossRevealT = ClampFloat((hiddenIntroT_ - 0.62f) / 1.05f, 0.0f, 1.0f);
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

    if (sigilT > 0.0f)
    {
        const float alpha = mono ? 0.70f : (0.22f + sigilT * 0.58f);
        const D2D1_POINT_2F center = D2D1::Point2F(cx, cy - 18.0f + dropT * 26.0f);
        for (int ring = 0; ring < 4; ++ring)
        {
            const float rr = (70.0f + ring * 30.0f) * (0.78f + sigilT * 0.36f + dropT * 0.22f);
            const float wobble = std::sin(hiddenIntroT_ * (1.6f + ring * 0.35f)) * 8.0f;
            textBrush_->SetColor(mono
                ? D2D1::ColorF(0.04f, 0.04f, 0.04f, alpha * (0.95f - ring * 0.14f))
                : D2D1::ColorF(0.70f, 0.34f + ring * 0.05f, 1.0f, alpha * (0.82f - ring * 0.12f)));
            d2dContext_->DrawEllipse(D2D1::Ellipse(center, rr + wobble, rr * 0.34f + ring * 4.0f), textBrush_.Get(), 2.0f + sigilT * 2.0f);
        }
        textBrush_->SetColor(mono ? D2D1::ColorF(0.02f, 0.02f, 0.02f, alpha) : D2D1::ColorF(1.0f, 0.78f, 0.28f, alpha));
        for (int i = 0; i < 18; ++i)
        {
            const float a = hiddenIntroT_ * (0.42f + (i % 3) * 0.11f) + TwoPi * i / 18.0f;
            const float inner = 58.0f + 10.0f * std::sin(hiddenIntroT_ * 2.0f + i);
            const float outer = 142.0f + dropT * 78.0f + 18.0f * std::cos(hiddenIntroT_ * 1.3f + i);
            const D2D1_POINT_2F p0 = D2D1::Point2F(center.x + std::cos(a) * inner, center.y + std::sin(a) * inner * 0.42f);
            const D2D1_POINT_2F p1 = D2D1::Point2F(center.x + std::cos(a) * outer, center.y + std::sin(a) * outer * 0.42f);
            d2dContext_->DrawLine(p0, p1, textBrush_.Get(), 1.2f + sigilT * 1.8f);
        }
        if (dropT > 0.0f)
        {
            textBrush_->SetColor(mono ? D2D1::ColorF(0.08f, 0.08f, 0.08f, 0.36f + dropT * 0.32f) : D2D1::ColorF(0.82f, 0.52f, 1.0f, 0.18f + dropT * 0.30f));
            for (int i = -3; i <= 3; ++i)
            {
                const float x = cx + i * 46.0f + std::sin(hiddenIntroT_ * 3.0f + i) * 10.0f;
                d2dContext_->DrawLine(D2D1::Point2F(x, 0.0f), D2D1::Point2F(cx + i * 14.0f, cy + 132.0f + dropT * 70.0f), textBrush_.Get(), 2.0f + dropT * 4.0f);
            }
            const float shock = ClampFloat((dropT - 0.78f) / 0.22f, 0.0f, 1.0f);
            if (shock > 0.0f)
            {
                textBrush_->SetColor(mono ? D2D1::ColorF(0.02f, 0.02f, 0.02f, 0.82f * (1.0f - shock * 0.35f)) : D2D1::ColorF(1.0f, 0.72f, 0.30f, 0.72f * (1.0f - shock * 0.25f)));
                d2dContext_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + 118.0f), 84.0f + shock * 260.0f, 18.0f + shock * 58.0f), textBrush_.Get(), 6.0f - shock * 2.0f);
                textBrush_->SetColor(mono ? D2D1::ColorF(0.92f, 0.92f, 0.92f, 0.26f * (1.0f - shock)) : D2D1::ColorF(0.70f, 0.34f, 1.0f, 0.34f * (1.0f - shock)));
                d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + 118.0f), 78.0f + shock * 210.0f, 13.0f + shock * 42.0f), textBrush_.Get());
            }
        }
    }

    auto drawIntroBoss = [&](float alphaMul, bool outlineOnly)
    {
        if (bossRevealT <= 0.0f) return;
        const float alpha = ClampFloat((0.42f + bossRevealT * 0.58f + dropT * 0.25f) * alphaMul, 0.0f, 1.0f);
        const float r = 64.0f + bossRevealT * 48.0f + dropT * 18.0f;
        const D2D1_POINT_2F center = D2D1::Point2F(cx, cy - 18.0f + dropT * 52.0f);
        const D2D1_COLOR_F body = mono
            ? D2D1::ColorF(0.08f, 0.08f, 0.08f, alpha * 0.90f)
            : D2D1::ColorF(0.16f, 0.02f, 0.26f, alpha * 0.92f);
        const D2D1_COLOR_F rim = mono
            ? D2D1::ColorF(0.92f, 0.92f, 0.92f, alpha)
            : D2D1::ColorF(0.78f, 0.42f, 1.0f, alpha);
        const D2D1_COLOR_F gold = mono
            ? D2D1::ColorF(0.02f, 0.02f, 0.02f, alpha)
            : D2D1::ColorF(1.0f, 0.76f, 0.28f, alpha);

        if (!outlineOnly)
        {
            textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha * 0.34f));
            d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + 118.0f), r * 1.05f, r * 0.20f), textBrush_.Get());
            textBrush_->SetColor(body);
            d2dContext_->FillEllipse(D2D1::Ellipse(center, r * (0.72f + dropT * 0.12f), r * (1.00f + dropT * 0.10f)), textBrush_.Get());
            d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y - r * 0.56f), r * 0.42f, r * 0.34f), textBrush_.Get());
        }

        textBrush_->SetColor(rim);
        d2dContext_->DrawEllipse(D2D1::Ellipse(center, r * 0.78f, r * 1.04f), textBrush_.Get(), 3.0f + dropT * 2.0f);
        d2dContext_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y - r * 0.56f), r * 0.44f, r * 0.36f), textBrush_.Get(), 2.4f);
        textBrush_->SetColor(gold);
        for (int side = -1; side <= 1; side += 2)
        {
            const D2D1_POINT_2F horn0 = D2D1::Point2F(center.x + side * r * 0.20f, center.y - r * 0.80f);
            const D2D1_POINT_2F horn1 = D2D1::Point2F(center.x + side * r * 0.62f, center.y - r * (1.13f + 0.08f * dropT));
            const D2D1_POINT_2F horn2 = D2D1::Point2F(center.x + side * r * 0.34f, center.y - r * 0.50f);
            d2dContext_->DrawLine(horn0, horn1, textBrush_.Get(), 3.0f);
            d2dContext_->DrawLine(horn1, horn2, textBrush_.Get(), 2.0f);
            d2dContext_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + side * r * 0.20f, center.y - r * 0.58f), r * 0.055f, r * 0.038f), textBrush_.Get());
        }
        d2dContext_->DrawEllipse(D2D1::Ellipse(center, r * (1.05f + dropT * 0.35f), r * (0.42f + dropT * 0.16f)), textBrush_.Get(), 2.0f);
    };
    drawIntroBoss(1.0f, false);

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

    if (landingFlash > 0.0f)
    {
        textBrush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, landingFlash * 0.42f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, w, h), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, landingFlash * 0.20f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, w, h), textBrush_.Get());
    }

    if (flash > 0.0f)
    {
        textBrush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, flash * 0.42f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, w, h), textBrush_.Get());
        textBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, flash * 0.16f));
        d2dContext_->FillRectangle(D2D1::RectF(0, 0, w, h), textBrush_.Get());
    }
    drawIntroBoss(0.62f, true);

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

    for (int i = 0; i < static_cast<int>(Loadouts.size()); ++i)
    {
        const LoadoutPreset& loadout = Loadouts[i];
        const bool selected = i == loadoutIndex_;
        const float x = startX + i * (cardW + gap);
        const D2D1_RECT_F card = D2D1::RectF(x, top, x + cardW, top + cardH);
        const bool hover = PointInRect(mouseX_, mouseY_, card.left, card.top, card.right, card.bottom);
        const bool active = hover;

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

        if (selected)
        {
            drawText(L"P1", smallFormat_.Get(), D2D1::RectF(x + cardW - 42.0f, top + 43.0f, x + cardW - 12.0f, top + 62.0f), D2D1::ColorF(1.0f, 0.82f, 0.28f, 1.0f));
        }
    }

    const wchar_t* hint = L"カードをクリックしてキャラ選択。通常弾は左クリック/Space、チャージは右クリック長押し。";
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

        int hoveredMode = -1;
        for (int mode = 0; mode < 3; ++mode)
        {
            const float x = startX + labelW + mode * (modeW + modeGap);
            const D2D1_RECT_F rect = D2D1::RectF(x, y, x + modeW, y + rowH);
            if (PointInRect(mouseX_, mouseY_, rect.left, rect.top, rect.right, rect.bottom))
            {
                hoveredMode = mode;
                break;
            }
        }

        for (int mode = 0; mode < 3; ++mode)
        {
            const float x = startX + labelW + mode * (modeW + modeGap);
            const D2D1_RECT_F rect = D2D1::RectF(x, y, x + modeW, y + rowH);
            const bool selected = coopSlotModes_[playerIndex] == static_cast<CoopSlotMode>(mode);
            const bool hover = hoveredMode == mode;
            const bool active = hover || (selected && hoveredMode < 0);
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

