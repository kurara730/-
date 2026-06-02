#include "SweetsApp.h"

SweetsApp* g_app = nullptr;

// Win32のウィンドウプロシージャからSweetsAppへ処理を渡します。
// C APIのコールバックはクラスメンバー関数を直接指定できないため、g_appを中継にしています。
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (g_app)
    {
        return g_app->HandleMessage(hwnd, msg, wp, lp);
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Windowsアプリの開始点です。
// ここではSweetsAppを作って起動するだけにし、実際の処理はSweetsApp側へ任せます。
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCmd)
{
    try
    {
        SweetsApp app;
        g_app = &app;
        if (!app.Initialize(instance, showCmd))
        {
            MessageBox(nullptr, L"Failed to initialize Sweets Action DX11.", L"Error", MB_ICONERROR);
            return 1;
        }
        return app.Run();
    }
    catch (const std::exception& ex)
    {
        MessageBoxA(nullptr, ex.what(), "Sweets Action DX11", MB_ICONERROR);
        return 1;
    }
}
