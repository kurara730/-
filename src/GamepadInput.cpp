#include "SweetsApp.h"

#include <Xinput.h>

// コントローラ（XInput）操作をまとめたファイルです。
// 1P はもともとキーボード＋マウスですが、ここで XInput #0 のコントローラ入力を重ねて、
// 移動・反射シールド・ブリンク・必殺・ボム・ポーズ、そしてメニュー操作をパッドだけでも行えるようにします。
// 2P-4P のパッド操作は CoopController.cpp 側（コントローラ #1 以降）で別途処理します。

namespace
{
// XInput のスティック値を -1.0 ～ 1.0 に変換します（CoopController と同じデッドゾーン）。
// 小さな入力は 0 にして、意図しない微妙な移動や照準ブレを防ぎます。
float PadThumb(SHORT value)
{
    constexpr float deadZone = 8200.0f;
    const float f = static_cast<float>(value);
    if (std::fabs(f) < deadZone) return 0.0f;
    return ClampFloat(f / 32767.0f, -1.0f, 1.0f);
}
}

// 画面状態に応じた「決定/クリック」処理です。
// マウス左クリック（AppWindow.cpp）とパッドの A ボタン（UpdateGamepad）から共通で呼びます。
// いずれかのメニューがクリックを消費したら true、ゲーム画面などで未処理なら false を返します。
bool SweetsApp::HandleScreenClick(float sx, float sy)
{
    // デバッグパネル/ポーズ/設定はスライダーのドラッグがあるためマウスをキャプチャする。
    if (HandleDebugClick(sx, sy))
    {
        SetCapture(hwnd_);
        return true;
    }
    if (screen_ == Screen::Paused && HandlePauseClick(sx, sy))
    {
        SetCapture(hwnd_);
        return true;
    }
    if (screen_ == Screen::Settings && HandleSettingsClick(sx, sy))
    {
        SetCapture(hwnd_);
        return true;
    }
    if (screen_ == Screen::Title && SelectTitleMenuAt(sx, sy)) return true;
    if (screen_ == Screen::CharacterSelect && SelectCoopSlotAt(sx, sy)) return true;
    if (screen_ == Screen::CharacterSelect && SelectLoadoutAt(sx, sy)) return true;
    if (screen_ == Screen::DifficultySelect && SelectDifficultyAt(sx, sy)) return true;
    if (screen_ == Screen::Credits && SelectCreditsAt(sx, sy)) return true;
    if (screen_ == Screen::GameOver && SelectGameOverAt(sx, sy)) return true;
    if ((screen_ == Screen::Clear || screen_ == Screen::CompleteClear) && SelectClearAt(sx, sy)) return true;
    return false;
}

// コントローラ（XInput #0）の毎フレーム処理です。
// ゲーム中は移動/照準/反射シールド/ブリンク/必殺/ボム/ポーズを 1P の入力として重ね、
// メニュー中は左スティック/十字キーで仮想カーソルを動かし、A 決定・B 戻るで操作します。
//
// 操作対応（ゲーム中）:
//   左スティック/十字キー … 移動（WASD相当）
//   右スティック         … 照準（倒している間その方向を向く）
//   LB                  … 集中移動（Shift相当）
//   RT / RB             … 反射シールド（左クリック長押し相当）
//   A                   … ブリンク（スペース相当）
//   Y                   … 必殺技（Eキー相当）
//   Back                … 攻撃方向モード切替（Tキー相当）
//   Start               … ポーズ
void SweetsApp::UpdateGamepad(float dt)
{
    // ゲーム中入力フラグは毎フレーム初期化する。未接続やスティックを離した時に
    // 押しっぱなし状態が残らないようにし、キーボード操作とも自然に共存させる。
    padMove_ = {};
    padAim_ = {};
    padFocus_ = false;
    padPrimaryHeld_ = false;
    padBlinkHeld_ = false;

    XINPUT_STATE state{};
    if (XInputGetState(0, &state) != ERROR_SUCCESS)
    {
        padConnected_ = false;
        padPrevButtons_ = 0;
        return;
    }
    padConnected_ = true;

    const WORD buttons = state.Gamepad.wButtons;
    const WORD prev = padPrevButtons_;
    // 「このフレームで押した瞬間」だけ true を返すヘルパ（長押しでの連続発動を防ぐ）。
    auto pressed = [&](WORD mask) { return (buttons & mask) != 0 && (prev & mask) == 0; };

    // 十字キーと左スティックをまとめて移動入力にする（W/上=+z, A/左=-x の規約に合わせる）。
    V2 move{ PadThumb(state.Gamepad.sThumbLX), PadThumb(state.Gamepad.sThumbLY) };
    if (buttons & XINPUT_GAMEPAD_DPAD_LEFT) move.x -= 1.0f;
    if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) move.x += 1.0f;
    if (buttons & XINPUT_GAMEPAD_DPAD_UP) move.z += 1.0f;
    if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) move.z -= 1.0f;
    if (LenSq(move) > 1.0f) move = Normalize(move); // 斜め入力が速くなりすぎないよう上限1に

    // Start はどの画面でも「ポーズ切替/戻る」として各画面の Esc 処理を流用する。
    if (pressed(XINPUT_GAMEPAD_START)) OnKeyDown(VK_ESCAPE);

    if (screen_ == Screen::Playing || screen_ == Screen::HiddenBoss)
    {
        padMove_ = move;
        padAim_ = { PadThumb(state.Gamepad.sThumbRX), PadThumb(state.Gamepad.sThumbRY) };
        padFocus_ = (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        padPrimaryHeld_ = state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD
            || (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        padBlinkHeld_ = (buttons & XINPUT_GAMEPAD_A) != 0;

        if (pressed(XINPUT_GAMEPAD_Y)) UseUltimate(); // 必殺技
        if (pressed(XINPUT_GAMEPAD_BACK))              // 攻撃方向モード切替（T相当）
        {
            SetAimMode(static_cast<AimMode>((static_cast<int>(aimMode_) + 1) % 3), true);
        }
        padNavPrevY_ = 0; // ゲーム中はメニューの上下ナビ状態をリセット
    }
    else if (screen_ == Screen::Title)
    {
        // タイトルは仮想カーソルを使わず、十字キー/スティックの上下で項目選択（titleMenuIndex_）する。
        int navY = 0;
        if ((buttons & XINPUT_GAMEPAD_DPAD_UP) || PadThumb(state.Gamepad.sThumbLY) > 0.5f) navY = 1;
        else if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) || PadThumb(state.Gamepad.sThumbLY) < -0.5f) navY = -1;
        const int itemCount = 3; // ボス戦 / Credits / 設定
        if (navY == 1 && padNavPrevY_ != 1) titleMenuIndex_ = (titleMenuIndex_ + itemCount - 1) % itemCount; // 上=前の項目
        else if (navY == -1 && padNavPrevY_ != -1) titleMenuIndex_ = (titleMenuIndex_ + 1) % itemCount;      // 下=次の項目
        padNavPrevY_ = navY;

        if (pressed(XINPUT_GAMEPAD_A)) StartSelectedTitleItem(); // 決定
    }
    else
    {
        padNavPrevY_ = 0;
        // メニュー中は左スティック/十字キーで仮想カーソル（マウス座標）を動かし、
        // 既存のホバー表示とクリック処理をそのまま流用する。
        if (LenSq(move) > 0.0001f)
        {
            constexpr float cursorSpeed = 900.0f; // px/秒
            mouseX_ = ClampFloat(mouseX_ + move.x * cursorSpeed * dt, 0.0f, static_cast<float>(width_));
            mouseY_ = ClampFloat(mouseY_ - move.z * cursorSpeed * dt, 0.0f, static_cast<float>(height_)); // 上スティック=画面上
            // 実カーソルも追従させて、矢印とハイライトの位置がずれないようにする。
            if (hwnd_)
            {
                POINT pt{ static_cast<LONG>(mouseX_), static_cast<LONG>(mouseY_) };
                ClientToScreen(hwnd_, &pt);
                SetCursorPos(pt.x, pt.y);
            }
        }

        if (pressed(XINPUT_GAMEPAD_A))
        {
            HandleScreenClick(mouseX_, mouseY_);
            // スライダー等のドラッグ状態を掴んだままにしない（1回押し＝1クリック扱い）。
            mouseLeft_ = false;
            draggingVolume_ = -1;
            draggingDebugFx_ = -1;
            ReleaseCapture();
        }
        if (pressed(XINPUT_GAMEPAD_B)) OnKeyDown(VK_ESCAPE); // 戻る/再開（各画面の Esc 処理を流用）
    }

    padPrevButtons_ = buttons;
}
