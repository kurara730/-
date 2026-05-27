#include "SweetsApp.h"

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
    LoadProgress();
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
        return 0;
    case WM_LBUTTONDOWN:
        if (screen_ == Screen::Title && SelectTitleMenuAt(static_cast<float>(GET_X_LPARAM(lp)), static_cast<float>(GET_Y_LPARAM(lp))))
        {
            return 0;
        }
        if (screen_ == Screen::Title && SelectLoadoutAt(static_cast<float>(GET_X_LPARAM(lp)), static_cast<float>(GET_Y_LPARAM(lp))))
        {
            return 0;
        }
        if (screen_ == Screen::DifficultySelect && SelectDifficultyAt(static_cast<float>(GET_X_LPARAM(lp)), static_cast<float>(GET_Y_LPARAM(lp))))
        {
            return 0;
        }
        mouseLeft_ = true;
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        mouseLeft_ = false;
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

    if (key >= '1' && key <= '4')
    {
        loadoutIndex_ = static_cast<int>(key - '1');
        player_.weapon = Loadouts[loadoutIndex_].weapon;
        player_.character = Loadouts[loadoutIndex_].character;
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
            StartSelectedTitleItem();
        }
        return;
    }

    if (screen_ == Screen::DifficultySelect)
    {
        const int optionCount = DifficultyOptionCount();
        if (key == VK_ESCAPE || key == VK_BACK)
        {
            screen_ = Screen::Title;
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
        screen_ = screen_ == Screen::Paused ? Screen::Playing : Screen::Paused;
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
    switch (key)
    {
    case VK_F1:
        debug_.hud = !debug_.hud;
        return true;
    case VK_F2:
        debug_.overlays = !debug_.overlays;
        return true;
    case VK_F3:
        debug_.taa = !debug_.taa;
        debug_.taaFrame = 0;
        return true;
    case VK_F4:
        debug_.additiveView = !debug_.additiveView;
        return true;
    case VK_F5:
        debug_.invincible = !debug_.invincible;
        message_ = debug_.invincible ? L"DEBUG: 無敵 ON" : L"DEBUG: 無敵 OFF";
        messageT_ = 1.4f;
        return true;
    case VK_F6:
        for (auto& p : players_)
        {
            if (!p.active) continue;
            p.hp = p.maxHp;
            p.bombs = 9;
            p.ult = 100.0f;
            p.downed = false;
            p.alive = true;
        }
        message_ = L"DEBUG: HP/ボム/必殺 回復";
        messageT_ = 1.4f;
        return true;
    case VK_F7:
        if (screen_ == Screen::Playing)
        {
            boss_.active = false;
            enemies_.clear();
            remainingToSpawn_ = 0;
            ClearWave();
            message_ = L"DEBUG: Wave Skip";
            messageT_ = 1.4f;
        }
        else if (screen_ == Screen::HiddenBoss)
        {
            hiddenBossT_ = HiddenBossDurationSeconds;
        }
        return true;
    case VK_F8:
        if (screen_ == Screen::Playing)
        {
            enemies_.clear();
            shots_.clear();
            bossWave_ = true;
            remainingToSpawn_ = 0;
            boss_ = {};
            SpawnBoss();
            message_ = L"DEBUG: Boss Spawn";
            messageT_ = 1.4f;
        }
        return true;
    case VK_F9:
        SaveProgress();
        message_ = L"DEBUG: 隠しボス解禁";
        messageT_ = 1.4f;
        return true;
    case VK_F10:
        for (auto& s : shots_)
        {
            if (s.enemy) s.dead = true;
        }
        message_ = L"DEBUG: 敵弾消去";
        messageT_ = 1.4f;
        return true;
    case VK_F11:
        CreateShadersAndStates();
        message_ = L"DEBUG: Shader Reload";
        messageT_ = 1.4f;
        return true;
    case VK_F12:
        debug_.frameStep = true;
        debug_.stepOnce = true;
        return true;
    default:
        break;
    }
#else
    (void)key;
#endif
    return false;
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
    screen_ = Screen::DifficultySelect;
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
    const float itemW = 190.0f;
    const float itemH = 42.0f;
    const float gap = 14.0f;
    const float totalW = itemW * 3.0f + gap * 2.0f;
    const float startX = (static_cast<float>(width_) - totalW) * 0.5f;
    const float top = static_cast<float>(height_) * 0.39f;
    for (int i = 0; i < 3; ++i)
    {
        const float x = startX + i * (itemW + gap);
        if (sx >= x && sx <= x + itemW && sy >= top && sy <= top + itemH)
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
    }
}

void SweetsApp::SaveProgress()
{
    hiddenBossUnlocked_ = true;
    const std::filesystem::path path = SaveFilePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    out << "hiddenBossUnlocked=1\n";
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

