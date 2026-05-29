#pragma once

#include <array>

#include "GameMath.h"

enum class Screen
{
    BootLoading,
    GameplayLoading,
    Title,
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

enum class GameMode
{
    Story,
    Endless,
    HiddenBossPractice
};

enum class TitleMenuItem
{
    Story,
    Endless,
    Credits
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
