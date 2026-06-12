#include "SweetsApp.h"

#include <algorithm>

// AppWindow.cpp はWin32アプリとしての外枠です。
// ウィンドウ作成、メッセージ処理、メインループを担当し、ゲーム内容は他ファイルへ任せます。

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

void SweetsApp::SetFullscreen(bool enabled, bool save)
{
    if (!hwnd_ || fullscreen_ == enabled)
    {
        if (save && settingsLoaded_)
        {
            SaveSettings();
        }
        return;
    }

    if (enabled)
    {
        windowStyle_ = static_cast<DWORD>(GetWindowLongPtr(hwnd_, GWL_STYLE));
        windowExStyle_ = static_cast<DWORD>(GetWindowLongPtr(hwnd_, GWL_EXSTYLE));
        windowPlacement_.length = sizeof(windowPlacement_);
        GetWindowPlacement(hwnd_, &windowPlacement_);

        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        const HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        if (!GetMonitorInfoW(monitor, &monitorInfo))
        {
            return;
        }

        const RECT& rc = monitorInfo.rcMonitor;
        const DWORD borderlessStyle = windowStyle_ & ~(WS_CAPTION | WS_THICKFRAME);
        const DWORD borderlessExStyle = windowExStyle_ & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
        SetWindowLongPtr(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(borderlessStyle));
        SetWindowLongPtr(hwnd_, GWL_EXSTYLE, static_cast<LONG_PTR>(borderlessExStyle));
        SetWindowPos(
            hwnd_,
            HWND_TOP,
            rc.left,
            rc.top,
            rc.right - rc.left,
            rc.bottom - rc.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    else
    {
        SetWindowLongPtr(hwnd_, GWL_STYLE, static_cast<LONG_PTR>(windowStyle_));
        SetWindowLongPtr(hwnd_, GWL_EXSTYLE, static_cast<LONG_PTR>(windowExStyle_));
        windowPlacement_.length = sizeof(windowPlacement_);
        SetWindowPlacement(hwnd_, &windowPlacement_);
        SetWindowPos(
            hwnd_,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    fullscreen_ = enabled;
    if (save && settingsLoaded_)
    {
        SaveSettings();
    }
}

void SweetsApp::ToggleFullscreen()
{
    SetFullscreen(!fullscreen_, true);
}

// 最小限の初期化を行い、重い素材読み込みはロード画面へ回します。
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

// Windowsメッセージを処理しつつ、毎フレームの経過時間を計算してUpdate/PresentFrameを呼びます。
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

// Win32から届く入力やリサイズ通知をゲーム側の状態へ変換します。
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
    case WM_SYSKEYDOWN:
        if (wp == VK_RETURN && (HIWORD(lp) & KF_ALTDOWN))
        {
            const bool repeat = (lp & (1 << 30)) != 0;
            if (!repeat)
            {
                ToggleFullscreen();
            }
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
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
    case WM_KILLFOCUS:
        // フォーカスが外れるとキー/マウスのKEYUP・BUTTONUPを取りこぼし、
        // 押しっぱなし状態が固着する（例：Dが固着するとAを押しても打ち消され移動不能）。
        // フォーカス喪失時に全入力状態をリセットして固着を防ぐ。
        for (auto& k : keys_) k = false;
        mouseLeft_ = false;
        prevMouseLeft_ = false;
        prevSpace_ = false;
        mouseRight_ = false;
        mouseRightReleased_ = false;
        return 0;
    case WM_MOUSEMOVE:
        mouseX_ = static_cast<float>(GET_X_LPARAM(lp));
        mouseY_ = static_cast<float>(GET_Y_LPARAM(lp));
        if (HandleDebugDrag(mouseX_, mouseY_))
        {
            return 0;
        }
        if ((screen_ == Screen::Paused || screen_ == Screen::Settings) && HandlePauseDrag(mouseX_, mouseY_))
        {
            return 0;
        }
        return 0;
    case WM_LBUTTONDOWN:
        mouseX_ = static_cast<float>(GET_X_LPARAM(lp));
        mouseY_ = static_cast<float>(GET_Y_LPARAM(lp));
        // メニュー系のクリック判定はマウス/パッド共通の HandleScreenClick に集約。
        // どのメニューも消費しなければ、ゲーム中の反射シールド入力として扱う。
        if (HandleScreenClick(mouseX_, mouseY_))
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
        draggingDebugFx_ = -1;
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

