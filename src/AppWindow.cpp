#include "SweetsApp.h"

#include <algorithm>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

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
    CreateFrameTargets();
    CreateMeshes();
    screen_ = Screen::BootLoading;
    loadPhase_ = LoadPhase::Boot;
    bootLoadElapsed_ = 0.0f;
    loadPhaseElapsed_ = 0.0f;
    lastLoadStep_ = L"Boot";

    ShowWindow(hwnd_, showCmd);
    UpdateWindow(hwnd_);
    lastTick_ = std::chrono::steady_clock::now();
    PresentFrame();
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
        audio_.Update(dt);
        effekseer_.Update(dt);
        PresentFrame();
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
        if ((screen_ == Screen::Paused || screen_ == Screen::Settings) && HandlePauseDrag(mouseX_, mouseY_))
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
        if (screen_ == Screen::Settings && HandleSettingsClick(mouseX_, mouseY_))
        {
            SetCapture(hwnd);
            return 0;
        }
        if (screen_ == Screen::Title && SelectTitleMenuAt(mouseX_, mouseY_))
        {
            return 0;
        }
        if (screen_ == Screen::CharacterSelect && SelectCoopSlotAt(mouseX_, mouseY_))
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

