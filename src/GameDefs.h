#pragma once

#include <array>

constexpr int MaxPlayers = 4;

enum class CharacterType
{
    Shortcake = 0,
    Chocolate = 1,
    Cheese = 2,
    Roll = 3
};

enum class EnemyType
{
    Normal = 0,
    Shield,
    Split,
    Healer,
    Barrier,
    Mirror,
    Mine,
    Teleport
};

enum class BossType
{
    Demon = 0,
    DonutKing,
    MirrorMacaron,
    GravityPudding,
    TerritoryCake,
    DemonParfait
};

enum class StageType
{
    Donut = 0,
    TwinIsland,
    Pinball,
    RingCorridor,
    FourPillars,
    MovingIsland,
    ShrinkRing,
    BossArena
};

enum class PickupType
{
    Attack = 0,
    Slow,
    Invincible,
    Magnet,
    BombDamage,
    Heal,
    UltFull,
    Spread,
    Speed,
    ScoreDouble
};

struct CharacterText
{
    const wchar_t* jpName;
    const wchar_t* roleIcon;
    const wchar_t* normal;
    const wchar_t* charge;
    const wchar_t* ultimate;
};

inline constexpr std::array<CharacterText, 4> CharacterTexts{ {
    { L"ショート", L"ST", L"追尾イチゴ弾", L"分裂チャージ", L"巨大メテオ" },
    { L"チョコ", L"CH", L"近接回転斬り", L"斬撃波", L"時計斬り" },
    { L"チーズ", L"CZ", L"重いチーズ弾", L"トゲ付き壁", L"無敵要塞" },
    { L"ロール", L"RL", L"反射ロール弾", L"転がり突進", L"全画面叩きつけ" },
} };

inline constexpr const wchar_t* StageName(StageType type)
{
    switch (type)
    {
    case StageType::Donut: return L"ドーナツ";
    case StageType::TwinIsland: return L"双子島";
    case StageType::Pinball: return L"ピンボール";
    case StageType::RingCorridor: return L"リング回廊";
    case StageType::FourPillars: return L"四隅の柱";
    case StageType::MovingIsland: return L"動く島";
    case StageType::ShrinkRing: return L"収縮リング";
    case StageType::BossArena: return L"ボス円形";
    default: return L"不明";
    }
}

inline constexpr const wchar_t* BossName(BossType type)
{
    switch (type)
    {
    case BossType::Demon: return L"大ボス";
    case BossType::DonutKing: return L"ドーナツキング";
    case BossType::MirrorMacaron: return L"ミラーマカロン";
    case BossType::GravityPudding: return L"グラビティプリン";
    case BossType::TerritoryCake: return L"テリトリーケーキ";
    case BossType::DemonParfait: return L"魔王パフェ";
    default: return L"ボス";
    }
}
