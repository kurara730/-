#pragma once

#include <array>
#include <string>

#include "GameMath.h"

// 画面状態です。
// Update と表示処理はこの Screen を見て分岐します。タイトル中に戦闘を進めない、
// ポーズ中はゲーム更新を止める、といった制御の基準になります。
enum class Screen
{
    BootLoading,
    GameplayLoading,
    Title,
    Settings,
    CharacterSelect,
    DifficultySelect,
    Playing,
    Paused,
    Credits,
    Clear,
    HiddenBossIntro,
    HiddenBoss,
    CompleteClear,
    Video,
    GameOver
};

// 起動直後の段階ロード用フェーズです。
// いきなり全素材を読むと初回起動が長くなるため、画面を出してから段階的に進めます。
enum class LoadPhase
{
    Boot = 0,
    Graphics,
    TitleAssets,
    Audio,
    GameplayAssets,
    Effects,
    Ready,
    Done,
    Count
};

inline const wchar_t* LoadPhaseName(LoadPhase phase)
{
    switch (phase)
    {
    case LoadPhase::Boot: return L"Boot";
    case LoadPhase::Graphics: return L"Graphics";
    case LoadPhase::TitleAssets: return L"TitleAssets";
    case LoadPhase::Audio: return L"Audio";
    case LoadPhase::GameplayAssets: return L"GameplayAssets";
    case LoadPhase::Effects: return L"Effects";
    case LoadPhase::Ready: return L"Ready";
    case LoadPhase::Done: return L"Done";
    default: return L"Unknown";
    }
}

// 遊び方のモードです。
// Story はWave12で区切り、Endless はWaveを継続、HiddenBossPractice は隠しボスへ直接入ります。
enum class GameMode
{
    Story,
    Endless,
    HiddenBossPractice,
    BossOnlyDebug      // デバッグ用：ボスのみ即出現し、新技（ビーム/薙ぎ払い/地中突き上げ）だけを使う
};

enum class TitleMenuItem
{
    Story,
    Endless,
    Credits,
    Settings
};

enum class GameOverChoice
{
    Retry,
    Title
};

enum class Difficulty
{
    Easy = 0,
    Normal,
    Hard,
    Expert,
    Lunatic
};

// 通常戦とボス戦でレベルデザインの目的が違うため、調整値も分けています。
// MobRelease は雑魚を倒す爽快感、BossSkillCheck は回避とビルド確認を重視します。
enum class EncounterProfile
{
    MobRelease,
    BossSkillCheck,
    HiddenBossSurvival
};

struct EncounterTuning
{
    const wchar_t* name;
    float mobHpMul;
    float mobShotCooldownMul;
    int mobEliteCap;
    float pickupIntervalMin;
    float pickupIntervalMax;
    int pickupMax;
    int bossAddCap;
    float bossTelegraphTime;
    float bossShotRestMul;
};

inline const std::array<EncounterTuning, 3> EncounterTunings{ {
    { L"雑魚解放", 0.80f, 1.35f, 2, 4.5f, 7.0f, 5, 0, 0.0f, 1.00f },
    { L"ボス試練", 1.00f, 1.00f, 2, 8.0f, 12.0f, 3, 2, 0.50f, 1.15f },
    { L"隠し耐久", 1.00f, 1.00f, 0, 10.0f, 14.0f, 1, 0, 0.0f, 1.00f },
} };

struct DebugState
{
    // F1デバッグパネルで操作できる開発用状態です。
    // Releaseでは無効化されるため、通常プレイの仕様やバランスには影響しません。
    bool hud = false;
    bool overlays = false;
    bool taa = false;
    bool additiveView = false;
    bool invincible = false;
    bool frameStep = false;
    bool stepOnce = false;
    float fps = 0.0f;
    float frameMs = 0.0f;
    float fpsAccum = 0.0f;
    int fpsFrames = 0;
    int taaFrame = 0;
    float brightness = 1.0f;
    float additiveFx = 1.0f;
    float screenFlashFx = 1.0f;
    float enemyBulletGlow = 1.0f;
    float swordFx = 1.0f;
    float ultimateFx = 1.0f;
    float hiddenBossAuraFx = 1.0f;
};

struct UiRect
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct SettingsLayout
{
    UiRect panel{};
    std::array<UiRect, 4> volumeSliders{};
    std::array<UiRect, 3> aimButtons{};
    UiRect fullscreenToggle{};
    float sliderLeft = 0.0f;
    float sliderRight = 0.0f;
};

struct CameraState
{
    V2 center{};
    V2 target{};
    float halfHeight = 9.4f;
    float follow = 8.5f;
};

struct CombatNotice
{
    std::wstring text;
    float ttl = 0.0f;
    float life = 0.0f;
    Color color = Gold;
};

struct WorldTelegraph
{
    V2 pos{};
    V2 dir{ 1.0f, 0.0f };
    float radius = 1.0f;
    float length = 0.0f;
    float ttl = 0.0f;
    float life = 0.0f;
    Color color = Gold;
    BossPatternId pattern = BossPatternId::Radial;
};
