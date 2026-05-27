#include "SweetsApp.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

namespace
{
std::filesystem::path SaveFilePath()
{
    wchar_t localAppData[MAX_PATH]{};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    std::filesystem::path base = (len > 0 && len < MAX_PATH) ? std::filesystem::path(localAppData) : std::filesystem::current_path();
    return base / L"SweetsPanicDX11" / L"save.dat";
}

bool PointInRect(float sx, float sy, float left, float top, float right, float bottom)
{
    return sx >= left && sx <= right && sy >= top && sy <= bottom;
}

float ParseSaveFloat(const std::string& line, const char* key, float fallback)
{
    const std::string prefix = std::string(key) + "=";
    if (line.rfind(prefix, 0) != 0) return fallback;
    try
    {
        return ClampFloat(std::stof(line.substr(prefix.size())), 0.0f, 1.0f);
    }
    catch (...)
    {
        return fallback;
    }
}
}

bool SweetsApp::Initialize(HINSTANCE instance, int showCmd)
{
    const HRESULT coHr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    comInitialized_ = SUCCEEDED(coHr);

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"SweetsActionDX11Window";
    if (!RegisterClassEx(&wc)) return false;

    RECT rc{ 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd_ = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"Sweets Action DX11",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!hwnd_) return false;

    CreateDevice();
    CreateShadersAndStates();
    CreateRenderTargets();
    CreateMeshes();
    LoadAssets();
    titleVideo_.Open(L"assets/video/title.mp4", true);
    LoadProgress();
    ApplyAudioVolume();
    ResetGame();
    screen_ = Screen::Title;

    ShowWindow(hwnd_, showCmd);
    UpdateWindow(hwnd_);
    lastTick_ = std::chrono::steady_clock::now();
    return true;
}

int SweetsApp::Run()
{
    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> diff = now - lastTick_;
        lastTick_ = now;
        const float dt = std::min(diff.count(), 1.0f / 20.0f);
#if defined(_DEBUG)
        UpdateDebugTiming(dt);
        if (debug_.frameStep && screen_ == Screen::Paused)
        {
            if (debug_.stepOnce)
            {
                screen_ = Screen::Playing;
                Update(dt);
                screen_ = Screen::Paused;
                debug_.stepOnce = false;
            }
            else
            {
                UpdateAudioForScreen();
            }
        }
        else
        {
            Update(dt);
        }
#else
        Update(dt);
#endif
        Render();
    }
    return static_cast<int>(msg.wParam);
}

LRESULT SweetsApp::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_DESTROY:
        audio_.Stop();
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (device_ && wp != SIZE_MINIMIZED)
        {
            Resize(static_cast<UINT>(LOWORD(lp)), static_cast<UINT>(HIWORD(lp)));
        }
        return 0;
    case WM_KEYDOWN:
        if (wp < MaxKeys)
        {
            const bool repeat = (lp & (1 << 30)) != 0;
            keys_[wp] = true;
            if (!repeat) OnKeyDown(wp);
        }
        return 0;
    case WM_KEYUP:
        if (wp < MaxKeys) keys_[wp] = false;
        return 0;
    case WM_MOUSEMOVE:
        mouseX_ = static_cast<float>(GET_X_LPARAM(lp));
        mouseY_ = static_cast<float>(GET_Y_LPARAM(lp));
        if (screen_ == Screen::Paused && HandlePauseDrag(mouseX_, mouseY_))
        {
            return 0;
        }
        return 0;
    case WM_LBUTTONDOWN:
        mouseX_ = static_cast<float>(GET_X_LPARAM(lp));
        mouseY_ = static_cast<float>(GET_Y_LPARAM(lp));
        if (HandleDebugClick(mouseX_, mouseY_))
        {
            return 0;
        }
        if (screen_ == Screen::Paused && HandlePauseClick(mouseX_, mouseY_))
        {
            SetCapture(hwnd);
            return 0;
        }
        if (screen_ == Screen::Title && SelectTitleMenuAt(mouseX_, mouseY_))
        {
            return 0;
        }
        if (screen_ == Screen::CharacterSelect && SelectLoadoutAt(mouseX_, mouseY_))
        {
            return 0;
        }
        if (screen_ == Screen::DifficultySelect && SelectDifficultyAt(mouseX_, mouseY_))
        {
            return 0;
        }
        mouseLeft_ = true;
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        mouseLeft_ = false;
        if (draggingVolume_ >= 0)
        {
            SaveSettings();
        }
        draggingVolume_ = -1;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        if (screen_ == Screen::Playing || screen_ == Screen::HiddenBoss)
        {
            mouseRight_ = true;
        }
        return 0;
    case WM_RBUTTONUP:
    {
        const bool wasRightDown = mouseRight_;
        mouseRight_ = false;
        mouseRightReleased_ = wasRightDown && (screen_ == Screen::Playing || screen_ == Screen::HiddenBoss);
        return 0;
    }
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
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
        if (key == VK_UP || key == 'W')
        {
            pauseMenuIndex_ = (pauseMenuIndex_ + 5) % 6;
            return;
        }
        if (key == VK_DOWN || key == 'S')
        {
            pauseMenuIndex_ = (pauseMenuIndex_ + 1) % 6;
            return;
        }
        if ((key == VK_LEFT || key == 'A') && pauseMenuIndex_ >= 2)
        {
            SetVolumeSlider(pauseMenuIndex_ - 2, VolumeSliderValue(pauseMenuIndex_ - 2) - 0.05f, true);
            return;
        }
        if ((key == VK_RIGHT || key == 'D') && pauseMenuIndex_ >= 2)
        {
            SetVolumeSlider(pauseMenuIndex_ - 2, VolumeSliderValue(pauseMenuIndex_ - 2) + 0.05f, true);
            return;
        }
        if (key == VK_RETURN || key == VK_SPACE)
        {
            ActivatePauseMenuItem();
            return;
        }
        return;
    }

    if (screen_ == Screen::Title)
    {
        if (key == 'C')
        {
            screen_ = Screen::Credits;
            return;
        }
        if (key == VK_UP || key == 'W')
        {
            titleMenuIndex_ = (titleMenuIndex_ + 2) % 3;
            return;
        }
        if (key == VK_DOWN || key == 'S')
        {
            titleMenuIndex_ = (titleMenuIndex_ + 1) % 3;
            return;
        }
        if (key == VK_LEFT || key == 'A')
        {
            return;
        }
        if (key == VK_RIGHT || key == 'D')
        {
            return;
        }
        if (key == VK_RETURN || key == VK_SPACE)
        {
            StartSelectedTitleItem();
        }
        return;
    }

    if (screen_ == Screen::CharacterSelect)
    {
        if (key == VK_ESCAPE || key == VK_BACK)
        {
            screen_ = Screen::Title;
            return;
        }
        if (key >= '1' && key <= '4')
        {
            loadoutIndex_ = static_cast<int>(key - '1');
            player_.weapon = Loadouts[loadoutIndex_].weapon;
            player_.character = Loadouts[loadoutIndex_].character;
            return;
        }
        if (key == VK_LEFT || key == 'A')
        {
            loadoutIndex_ = (loadoutIndex_ + static_cast<int>(Loadouts.size()) - 1) % static_cast<int>(Loadouts.size());
            player_.weapon = Loadouts[loadoutIndex_].weapon;
            player_.character = Loadouts[loadoutIndex_].character;
            return;
        }
        if (key == VK_RIGHT || key == 'D')
        {
            loadoutIndex_ = (loadoutIndex_ + 1) % static_cast<int>(Loadouts.size());
            player_.weapon = Loadouts[loadoutIndex_].weapon;
            player_.character = Loadouts[loadoutIndex_].character;
            return;
        }
        if (key == VK_RETURN || key == VK_SPACE)
        {
            screen_ = Screen::DifficultySelect;
            return;
        }
        return;
    }

    if (screen_ == Screen::DifficultySelect)
    {
        const int optionCount = DifficultyOptionCount();
        if (key == VK_ESCAPE || key == VK_BACK)
        {
            screen_ = Screen::CharacterSelect;
            return;
        }
        if (key == VK_LEFT || key == 'A')
        {
            difficultyIndex_ = (difficultyIndex_ + optionCount - 1) % optionCount;
            return;
        }
        if (key == VK_RIGHT || key == 'D')
        {
            difficultyIndex_ = (difficultyIndex_ + 1) % optionCount;
            return;
        }
        if (key == VK_RETURN || key == VK_SPACE)
        {
            StartGameWithDifficulty(hiddenBossUnlocked_ && difficultyIndex_ == 5);
            return;
        }
        return;
    }

    if (screen_ == Screen::Credits)
    {
        if (key == VK_ESCAPE || key == VK_RETURN || key == VK_BACK || key == 'C')
        {
            screen_ = Screen::Title;
        }
        return;
    }

    if (screen_ == Screen::GameOver)
    {
        if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == 'A' || key == 'D' || key == 'W' || key == 'S')
        {
            gameOverChoice_ = gameOverChoice_ == GameOverChoice::Retry ? GameOverChoice::Title : GameOverChoice::Retry;
            return;
        }
        if (key == VK_RETURN || key == 'R')
        {
            if (gameOverChoice_ == GameOverChoice::Retry || key == 'R')
            {
                RestartCurrentRun();
            }
            else
            {
                screen_ = Screen::Title;
            }
        }
        if (key == VK_ESCAPE || key == VK_BACK)
        {
            screen_ = Screen::Title;
        }
        return;
    }

    if (screen_ == Screen::Clear || screen_ == Screen::CompleteClear)
    {
        if (key == VK_RETURN || key == 'R' || key == VK_ESCAPE || key == VK_BACK)
        {
            screen_ = Screen::Title;
        }
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

    const float left = static_cast<float>(width_) - 342.0f;
    const float buttonW = 148.0f;
    const float buttonH = 30.0f;
    const float gap = 10.0f;
    const float top = 178.0f;
    for (int i = 0; i < 11; ++i)
    {
        const int col = i % 2;
        const int row = i / 2;
        const float x = left + col * (buttonW + gap);
        const float y = top + row * (buttonH + 8.0f);
        if (PointInRect(sx, sy, x, y, x + buttonW, y + buttonH))
        {
            ExecuteDebugAction(i);
            return true;
        }
    }
    return true;
#else
    (void)sx;
    (void)sy;
    return false;
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
            hiddenBossT_ = HiddenBossDurationSeconds;
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

bool SweetsApp::HandlePauseClick(float sx, float sy)
{
    const float panelW = 480.0f;
    const float panelH = 370.0f;
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
            return true;
        }
    }
    return false;
}

void SweetsApp::LoadProgress()
{
    hiddenBossUnlocked_ = false;
    masterVolume_ = 1.0f;
    bgmVolume_ = 1.0f;
    seVolume_ = 1.0f;
    uiVolume_ = 1.0f;
    const std::filesystem::path path = SaveFilePath();
    std::ifstream in(path);
    if (!in) return;

    std::string line;
    while (std::getline(in, line))
    {
        if (line == "hiddenBossUnlocked=1")
        {
            hiddenBossUnlocked_ = true;
        }
        masterVolume_ = ParseSaveFloat(line, "masterVolume", masterVolume_);
        bgmVolume_ = ParseSaveFloat(line, "bgmVolume", bgmVolume_);
        seVolume_ = ParseSaveFloat(line, "seVolume", seVolume_);
        uiVolume_ = ParseSaveFloat(line, "uiVolume", uiVolume_);
    }
    ApplyAudioVolume();
}

void SweetsApp::SaveSettings()
{
    const std::filesystem::path path = SaveFilePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    out << "hiddenBossUnlocked=" << (hiddenBossUnlocked_ ? 1 : 0) << "\n";
    out << "masterVolume=" << ClampFloat(masterVolume_, 0.0f, 1.0f) << "\n";
    out << "bgmVolume=" << ClampFloat(bgmVolume_, 0.0f, 1.0f) << "\n";
    out << "seVolume=" << ClampFloat(seVolume_, 0.0f, 1.0f) << "\n";
    out << "uiVolume=" << ClampFloat(uiVolume_, 0.0f, 1.0f) << "\n";
}

void SweetsApp::SaveProgress()
{
    hiddenBossUnlocked_ = true;
    SaveSettings();
}

bool SweetsApp::KeyDown(int key) const
{
    if (key < 0 || key >= MaxKeys) return false;
    return keys_[key];
}

float SweetsApp::Rand(float a, float b)
{
    std::uniform_real_distribution<float> dist(a, b);
    return dist(rng_);
}

int SweetsApp::RandInt(int a, int b)
{
    std::uniform_int_distribution<int> dist(a, b);
    return dist(rng_);
}

V2 SweetsApp::RandInArena(float margin)
{
    const float r = std::sqrt(Rand(0.0f, 1.0f)) * (ArenaRadius - margin);
    const float a = Rand(0.0f, TwoPi);
    return FromAngle(a) * r;
}

void SweetsApp::ClampInside(V2& p, float radius) const
{
    const float maxR = ArenaRadius - radius;
    const float d = Len(p);
    if (d > maxR && d > 0.0001f)
    {
        p = p / d * maxR;
    }
}

