#include "SweetsApp.h"

#include <algorithm>

namespace
{
bool PointInRect(float sx, float sy, float left, float top, float right, float bottom)
{
    return sx >= left && sx <= right && sy >= top && sy <= bottom;
}

CoopSlotMode CoopModeFromIndex(int index)
{
    switch (index)
    {
    case 1: return CoopSlotMode::AI;
    case 2: return CoopSlotMode::Pad;
    default: return CoopSlotMode::Off;
    }
}

constexpr int DebugActionCount = 12;
constexpr int DebugFxSliderCount = 7;

bool DebugActionRect(int index, float width, float& x, float& y, float& w, float& h)
{
    if (index < 0 || index >= DebugActionCount) return false;
    const float left = width - 342.0f;
    const float buttonW = 148.0f;
    const float buttonH = 30.0f;
    const float gap = 10.0f;
    const float top = 286.0f;
    const int col = index % 2;
    const int row = index / 2;
    x = left + col * (buttonW + gap);
    y = top + row * (buttonH + 8.0f);
    w = buttonW;
    h = buttonH;
    return true;
}

bool DebugFxSliderRect(int index, float width, float& x, float& y, float& w, float& h)
{
    if (index < 0 || index >= DebugFxSliderCount) return false;
    const float left = width - 342.0f;
    x = left + 118.0f;
    y = 540.0f + index * 28.0f;
    w = 190.0f;
    h = 8.0f;
    return true;
}
}

void SweetsApp::OnKeyDown(WPARAM key)
{
    if (HandleDebugKey(key))
    {
        return;
    }

    if (screen_ == Screen::Video)
    {
        if (eventVideoSkippable_ && (key == VK_ESCAPE || key == VK_RETURN || key == VK_SPACE))
        {
            eventVideo_.Stop();
            eventVideoBitmap_.Reset();
            screen_ = eventVideoNextScreen_;
        }
        return;
    }

    if (screen_ == Screen::Paused)
    {
        if (key == 'P' || key == VK_ESCAPE)
        {
            screen_ = Screen::Playing;
            return;
        }
        return;
    }

    if (screen_ == Screen::Title)
    {
        return;
    }

    if (screen_ == Screen::CharacterSelect)
    {
        return;
    }

    if (screen_ == Screen::DifficultySelect)
    {
        return;
    }

    if (screen_ == Screen::Credits)
    {
        return;
    }

    if (screen_ == Screen::GameOver)
    {
        return;
    }

    if (screen_ == Screen::Clear || screen_ == Screen::CompleteClear)
    {
        return;
    }

    if ((key == 'P' || key == VK_ESCAPE) && (screen_ == Screen::Playing || screen_ == Screen::Paused))
    {
        if (screen_ == Screen::Paused)
        {
            screen_ = Screen::Playing;
        }
        else
        {
            pauseMenuIndex_ = 0;
            draggingVolume_ = -1;
            screen_ = Screen::Paused;
        }
    }
    if (screen_ == Screen::Playing || screen_ == Screen::HiddenBoss)
    {
        if (key == 'Q') UseUltimate();
        if (key == 'X' || key == VK_CONTROL) UseBomb();
        if (key == 'R') RestartCurrentRun();
    }
}

bool SweetsApp::HandleDebugKey(WPARAM key)
{
#if defined(_DEBUG)
    if (key == VK_F1)
    {
        debug_.hud = !debug_.hud;
        return true;
    }
    if (key >= VK_F2 && key <= VK_F12)
    {
        return true;
    }
    return false;
#else
    (void)key;
    return false;
#endif
}

bool SweetsApp::DebugPanelContains(float sx, float sy) const
{
#if defined(_DEBUG)
    if (!debug_.hud) return false;
    const float left = static_cast<float>(width_) - 360.0f;
    return sx >= left && sx <= static_cast<float>(width_) && sy >= 0.0f && sy <= static_cast<float>(height_);
#else
    (void)sx;
    (void)sy;
    return false;
#endif
}

bool SweetsApp::HandleDebugClick(float sx, float sy)
{
#if defined(_DEBUG)
    if (!DebugPanelContains(sx, sy)) return false;

    for (int i = 0; i < DebugActionCount; ++i)
    {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        DebugActionRect(i, static_cast<float>(width_), x, y, w, h);
        if (PointInRect(sx, sy, x, y, x + w, y + h))
        {
            ExecuteDebugAction(i);
            return true;
        }
    }

    for (int i = 0; i < DebugFxSliderCount; ++i)
    {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        DebugFxSliderRect(i, static_cast<float>(width_), x, y, w, h);
        if (PointInRect(sx, sy, x - 8.0f, y - 12.0f, x + w + 8.0f, y + h + 12.0f))
        {
            draggingDebugFx_ = i;
            SetDebugFxSlider(i, (sx - x) / w);
            return true;
        }
    }

    const float resetX = static_cast<float>(width_) - 224.0f;
    const float resetY = 742.0f;
    if (PointInRect(sx, sy, resetX, resetY, resetX + 190.0f, resetY + 30.0f))
    {
        ResetDebugFxAdjustments();
        return true;
    }
    return true;
#else
    (void)sx;
    (void)sy;
    return false;
#endif
}

bool SweetsApp::HandleDebugDrag(float sx, float sy)
{
#if defined(_DEBUG)
    (void)sy;
    if (draggingDebugFx_ < 0) return false;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    if (!DebugFxSliderRect(draggingDebugFx_, static_cast<float>(width_), x, y, w, h))
    {
        draggingDebugFx_ = -1;
        return false;
    }
    SetDebugFxSlider(draggingDebugFx_, (sx - x) / w);
    return true;
#else
    (void)sx;
    (void)sy;
    return false;
#endif
}

void SweetsApp::ResetDebugFxAdjustments()
{
#if defined(_DEBUG)
    debug_.brightness = 1.0f;
    debug_.additiveFx = 1.0f;
    debug_.screenFlashFx = 1.0f;
    debug_.enemyBulletGlow = 1.0f;
    debug_.swordFx = 1.0f;
    debug_.ultimateFx = 1.0f;
    debug_.hiddenBossAuraFx = 1.0f;
    message_ = L"DEBUG: FX繝ｪ繧ｻ繝・ヨ";
    messageT_ = 1.2f;
#endif
}

void SweetsApp::SetDebugFxSlider(int index, float normalizedValue)
{
#if defined(_DEBUG)
    const float t = ClampFloat(normalizedValue, 0.0f, 1.0f);
    float value = index == 0 ? (0.5f + t) : (t * 2.0f);
    switch (index)
    {
    case 0: debug_.brightness = value; break;
    case 1: debug_.additiveFx = value; break;
    case 2: debug_.screenFlashFx = value; break;
    case 3: debug_.enemyBulletGlow = value; break;
    case 4: debug_.swordFx = value; break;
    case 5: debug_.ultimateFx = value; break;
    case 6: debug_.hiddenBossAuraFx = value; break;
    default: break;
    }
#else
    (void)index;
    (void)normalizedValue;
#endif
}

float SweetsApp::DebugFxSliderValue(int index) const
{
#if defined(_DEBUG)
    switch (index)
    {
    case 0: return ClampFloat(debug_.brightness - 0.5f, 0.0f, 1.0f);
    case 1: return ClampFloat(debug_.additiveFx * 0.5f, 0.0f, 1.0f);
    case 2: return ClampFloat(debug_.screenFlashFx * 0.5f, 0.0f, 1.0f);
    case 3: return ClampFloat(debug_.enemyBulletGlow * 0.5f, 0.0f, 1.0f);
    case 4: return ClampFloat(debug_.swordFx * 0.5f, 0.0f, 1.0f);
    case 5: return ClampFloat(debug_.ultimateFx * 0.5f, 0.0f, 1.0f);
    case 6: return ClampFloat(debug_.hiddenBossAuraFx * 0.5f, 0.0f, 1.0f);
    default: return 0.0f;
    }
#else
    (void)index;
    return 0.5f;
#endif
}

void SweetsApp::ExecuteDebugAction(int action)
{
#if defined(_DEBUG)
    switch (action)
    {
    case 0:
        debug_.taa = !debug_.taa;
        debug_.taaFrame = 0;
        break;
    case 1:
        debug_.additiveView = !debug_.additiveView;
        break;
    case 2:
        debug_.overlays = !debug_.overlays;
        break;
    case 3:
        debug_.invincible = !debug_.invincible;
        message_ = debug_.invincible ? L"DEBUG: 無敵 ON" : L"DEBUG: 無敵 OFF";
        messageT_ = 1.4f;
        break;
    case 4:
        for (auto& p : players_)
        {
            if (!p.active) continue;
            p.hp = p.maxHp;
            p.bombs = 9;
            p.ult = 100.0f;
            p.downed = false;
            p.alive = true;
        }
        message_ = L"DEBUG: 全回復";
        messageT_ = 1.4f;
        break;
    case 5:
        if (screen_ == Screen::Playing)
        {
            boss_.active = false;
            enemies_.clear();
            remainingToSpawn_ = 0;
            ClearWave();
            message_ = L"DEBUG: ウェーブ進行";
            messageT_ = 1.4f;
        }
        else if (screen_ == Screen::HiddenBoss)
        {
            hiddenBossPhaseIntroT_ = 0.0f;
            DamageBoss(boss_.hp + 1.0f, true, 0);
        }
        break;
    case 6:
        if (screen_ == Screen::Playing)
        {
            enemies_.clear();
            shots_.clear();
            bossWave_ = true;
            remainingToSpawn_ = 0;
            boss_ = {};
            SpawnBoss();
            message_ = L"DEBUG: ボス召喚";
            messageT_ = 1.4f;
        }
        break;
    case 7:
        SaveProgress();
        message_ = L"DEBUG: EX解禁";
        messageT_ = 1.4f;
        break;
    case 8:
        for (auto& s : shots_)
        {
            if (s.enemy) s.dead = true;
        }
        message_ = L"DEBUG: 敵弾消去";
        messageT_ = 1.4f;
        break;
    case 9:
        CreateShadersAndStates();
        message_ = L"DEBUG: シェーダー再読込";
        messageT_ = 1.4f;
        break;
    case 10:
        debug_.frameStep = true;
        debug_.stepOnce = true;
        if (screen_ == Screen::Playing)
        {
            screen_ = Screen::Paused;
        }
        break;
    case 11:
        SetGameplayDimension(gameplayDimension_ == GameplayDimension::TwoD ? GameplayDimension::ThreeD : GameplayDimension::TwoD);
        break;
    default:
        break;
    }
#else
    (void)action;
#endif
}

void SweetsApp::ActivatePauseMenuItem()
{
    if (pauseMenuIndex_ == 0)
    {
        screen_ = Screen::Playing;
    }
    else if (pauseMenuIndex_ == 1)
    {
        SaveSettings();
        draggingVolume_ = -1;
        screen_ = Screen::Title;
    }
}

void SweetsApp::SetAimMode(AimMode mode, bool save)
{
    aimMode_ = mode;
    if (save)
    {
        SaveSettings();
    }
    message_ = L"攻撃方向: ";
    message_ += AimModeName(aimMode_);
    messageT_ = 1.2f;
}

bool SweetsApp::HandlePauseClick(float sx, float sy)
{
    const float panelW = 480.0f;
    const float panelH = 450.0f;
    const float left = (static_cast<float>(width_) - panelW) * 0.5f;
    const float top = (static_cast<float>(height_) - panelH) * 0.5f;
    if (!PointInRect(sx, sy, left, top, left + panelW, top + panelH))
    {
        return false;
    }

    const float buttonW = 190.0f;
    const float buttonH = 42.0f;
    const float buttonX = left + 44.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float y = top + 76.0f + i * 56.0f;
        if (PointInRect(sx, sy, buttonX, y, buttonX + buttonW, y + buttonH))
        {
            pauseMenuIndex_ = i;
            ActivatePauseMenuItem();
            return true;
        }
    }

    const float sliderLeft = left + 170.0f;
    const float sliderRight = left + panelW - 48.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float y = top + 196.0f + i * 38.0f;
        if (PointInRect(sx, sy, sliderLeft - 8.0f, y - 12.0f, sliderRight + 8.0f, y + 16.0f))
        {
            pauseMenuIndex_ = i + 2;
            draggingVolume_ = i;
            SetVolumeSlider(i, (sx - sliderLeft) / (sliderRight - sliderLeft), true);
            return true;
        }
    }

    const float aimTop = top + 348.0f;
    const float aimButtonW = 104.0f;
    const float aimButtonH = 32.0f;
    const float aimStartX = left + 138.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float x = aimStartX + i * (aimButtonW + 10.0f);
        if (PointInRect(sx, sy, x, aimTop, x + aimButtonW, aimTop + aimButtonH))
        {
            pauseMenuIndex_ = 6 + i;
            SetAimMode(static_cast<AimMode>(i), true);
            return true;
        }
    }
    return true;
}

bool SweetsApp::HandlePauseDrag(float sx, float sy)
{
    (void)sy;
    if (draggingVolume_ < 0) return false;
    const float panelW = 480.0f;
    const float left = (static_cast<float>(width_) - panelW) * 0.5f;
    const float sliderLeft = left + 170.0f;
    const float sliderRight = left + panelW - 48.0f;
    SetVolumeSlider(draggingVolume_, (sx - sliderLeft) / (sliderRight - sliderLeft), false);
    return true;
}

float* SweetsApp::MutableVolumeSliderValue(int index)
{
    switch (index)
    {
    case 0: return &masterVolume_;
    case 1: return &bgmVolume_;
    case 2: return &seVolume_;
    case 3: return &uiVolume_;
    default: return nullptr;
    }
}

float SweetsApp::VolumeSliderValue(int index) const
{
    switch (index)
    {
    case 0: return masterVolume_;
    case 1: return bgmVolume_;
    case 2: return seVolume_;
    case 3: return uiVolume_;
    default: return 0.0f;
    }
}

void SweetsApp::SetVolumeSlider(int index, float value, bool save)
{
    if (float* target = MutableVolumeSliderValue(index))
    {
        *target = ClampFloat(value, 0.0f, 1.0f);
        ApplyAudioVolume();
        if (save)
        {
            SaveSettings();
        }
    }
}

void SweetsApp::PlayVideo(const std::wstring& relativePath, Screen nextScreen, bool skippable)
{
    eventVideoNextScreen_ = nextScreen;
    eventVideoSkippable_ = skippable;
    eventVideoBitmap_.Reset();
    eventVideoSerial_ = 0;
    if (eventVideo_.Open(relativePath, false))
    {
        screen_ = Screen::Video;
    }
    else
    {
        screen_ = nextScreen;
    }
}

void SweetsApp::RestartCurrentRun()
{
    pendingGameMode_ = gameMode_ == GameMode::HiddenBossPractice ? GameMode::Story : gameMode_;
    StartGameWithDifficulty(gameMode_ == GameMode::HiddenBossPractice);
}

void SweetsApp::StartSelectedTitleItem()
{
    const TitleMenuItem item = static_cast<TitleMenuItem>(titleMenuIndex_);
    if (item == TitleMenuItem::Credits)
    {
        screen_ = Screen::Credits;
        return;
    }

    pendingGameMode_ = item == TitleMenuItem::Endless ? GameMode::Endless : GameMode::Story;
    screen_ = Screen::CharacterSelect;
}

bool SweetsApp::SelectLoadoutAt(float sx, float sy)
{
    const float gap = 14.0f;
    const float cardW = std::min(250.0f, (static_cast<float>(width_) - 96.0f - gap * 3.0f) / 4.0f);
    const float cardH = 214.0f;
    const float startX = (static_cast<float>(width_) - (cardW * 4.0f + gap * 3.0f)) * 0.5f;
    const float top = static_cast<float>(height_) * 0.49f;

    for (int i = 0; i < static_cast<int>(Loadouts.size()); ++i)
    {
        const float x = startX + i * (cardW + gap);
        if (sx >= x && sx <= x + cardW && sy >= top && sy <= top + cardH)
        {
            loadoutIndex_ = i;
            player_.weapon = Loadouts[loadoutIndex_].weapon;
            player_.character = Loadouts[loadoutIndex_].character;
            screen_ = Screen::DifficultySelect;
            return true;
        }
    }
    return false;
}

bool SweetsApp::SelectCoopSlotAt(float sx, float sy)
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

    for (int playerIndex = 1; playerIndex < MaxPlayers; ++playerIndex)
    {
        const float y = rowTop + (playerIndex - 1) * (rowH + rowGap);
        for (int mode = 0; mode < 3; ++mode)
        {
            const float x = startX + labelW + mode * (modeW + modeGap);
            if (PointInRect(sx, sy, x, y, x + modeW, y + rowH))
            {
                coopSlotModes_[playerIndex] = CoopModeFromIndex(mode);
                return true;
            }
        }

        if (PointInRect(sx, sy, charX, y, charX + charW, y + rowH))
        {
            coopLoadoutIndices_[playerIndex] = (coopLoadoutIndices_[playerIndex] + 1) % static_cast<int>(Loadouts.size());
            return true;
        }
    }

    return false;
}

bool SweetsApp::SelectTitleMenuAt(float sx, float sy)
{
    const float itemW = 248.0f;
    const float itemH = 58.0f;
    const float gap = 12.0f;
    const float startX = 42.0f;
    const float top = std::max(112.0f, static_cast<float>(height_) * 0.18f);
    for (int i = 0; i < 3; ++i)
    {
        const float y = top + i * (itemH + gap);
        if (sx >= startX && sx <= startX + itemW && sy >= y && sy <= y + itemH)
        {
            titleMenuIndex_ = i;
            StartSelectedTitleItem();
            return true;
        }
    }
    return false;
}

bool SweetsApp::SelectDifficultyAt(float sx, float sy)
{
    const int optionCount = DifficultyOptionCount();
    const float cardW = std::min(210.0f, (static_cast<float>(width_) - 100.0f) / 3.0f);
    const float cardH = 92.0f;
    const float gap = 16.0f;
    const float totalW = cardW * 3.0f + gap * 2.0f;
    const float startX = (static_cast<float>(width_) - totalW) * 0.5f;
    const float top = static_cast<float>(height_) * 0.36f;

    for (int i = 0; i < optionCount; ++i)
    {
        const int col = i % 3;
        const int row = i / 3;
        const float x = startX + col * (cardW + gap);
        const float y = top + row * (cardH + gap);
        if (sx >= x && sx <= x + cardW && sy >= y && sy <= y + cardH)
        {
            difficultyIndex_ = i;
            StartGameWithDifficulty(hiddenBossUnlocked_ && difficultyIndex_ == 5);
            return true;
        }
    }
    return false;
}

bool SweetsApp::SelectCreditsAt(float sx, float sy)
{
    const float buttonW = 190.0f;
    const float buttonH = 46.0f;
    const float x = (static_cast<float>(width_) - buttonW) * 0.5f;
    const float y = static_cast<float>(height_) * 0.68f;
    if (PointInRect(sx, sy, x, y, x + buttonW, y + buttonH))
    {
        screen_ = Screen::Title;
        return true;
    }
    return false;
}

bool SweetsApp::SelectGameOverAt(float sx, float sy)
{
    const float choiceW = 180.0f;
    const float choiceTop = static_cast<float>(height_) * 0.62f;
    for (int i = 0; i < 2; ++i)
    {
        const float x = static_cast<float>(width_) * 0.5f - choiceW - 10.0f + i * (choiceW + 20.0f);
        if (PointInRect(sx, sy, x, choiceTop, x + choiceW, choiceTop + 46.0f))
        {
            if (i == 0)
            {
                gameOverChoice_ = GameOverChoice::Retry;
                RestartCurrentRun();
            }
            else
            {
                gameOverChoice_ = GameOverChoice::Title;
                screen_ = Screen::Title;
            }
            return true;
        }
    }
    return false;
}

bool SweetsApp::SelectClearAt(float sx, float sy)
{
    const float buttonW = 220.0f;
    const float buttonH = 46.0f;
    const float x = (static_cast<float>(width_) - buttonW) * 0.5f;
    const float y = static_cast<float>(height_) * 0.62f;
    if (PointInRect(sx, sy, x, y, x + buttonW, y + buttonH))
    {
        screen_ = Screen::Title;
        return true;
    }
    return false;
}

