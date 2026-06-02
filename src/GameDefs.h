#pragma once

#include <array>

constexpr int MaxPlayers = 4;

// キャラクター、敵、ボス、ステージ、アイテムなどの分類を enum で固定します。
// int のまま扱うと「0 が何を意味するか」が分かりにくいため、コード上では
// CharacterType::Chocolate のように名前で読める形へ寄せています。
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
    DemonParfait,
    ThunderCaptain,
    HiddenBoss
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

enum class FieldShape
{
    Circle = 0,
    Rectangle,
    Octagon,
    Corridor,
    Ring,
    ShrinkCircle
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

enum class CoopSlotMode
{
    Off = 0,
    AI,
    Pad
};

// 攻撃方向の決め方です。
// Cursor はマウス照準、MoveDirection は移動方向、AutoTarget は近い敵を自動で向きます。
enum class AimMode
{
    Cursor = 0,
    MoveDirection,
    AutoTarget
};

enum class BossDamageKind
{
    NormalShot = 0,
    ChargeShot,
    ChocolateCharge,
    Melee,
    Bomb,
    Ultimate,
    ReflectedShot
};

enum class BossPatternId
{
    Radial = 0,
    Aimed,
    Spiral,
    Curve,
    Seal,
    GuardRing,
    MirrorSplit,
    GravityWell,
    TerritoryZone,
    Beam,
    SkyLaser
};

// UI表示用の短い名前を返すヘルパーです。
// ここで表示名をまとめておくと、ポーズ画面など複数箇所で同じ表記を使えます。
inline constexpr const wchar_t* AimModeName(AimMode mode)
{
    switch (mode)
    {
    case AimMode::Cursor: return L"マウス";
    case AimMode::AutoTarget: return L"近い敵";
    case AimMode::MoveDirection:
    default: return L"移動方向";
    }
}

struct CharacterText
{
    const wchar_t* jpName;
    const wchar_t* roleIcon;
    const wchar_t* normal;
    const wchar_t* charge;
    const wchar_t* ultimate;
};

// キャラごとの説明文です。
// キャラ選択画面ではここを参照し、性能値は GameStateTypes.h の Loadouts を参照します。
inline constexpr std::array<CharacterText, 4> CharacterTexts{ {
    { L"ショート", L"ST", L"誘導いちご弾", L"苺リコシェ場", L"巨大メテオ" },
    { L"チョコ", L"CH", L"ヨーヨー弾", L"斬撃ヨーヨー", L"時計斬り" },
    { L"チーズ", L"CZ", L"敵弾キャッチ", L"前方チーズ壁", L"無敵要塞" },
    { L"ロール", L"RL", L"バウンドロール弾", L"最大溜め突進", L"全画面叩きつけ" },
} };

// ステージ名やボス名は、ゲーム内メッセージやHUD表示で使います。
// 実装側では StageType / BossType だけを渡し、文字列はここで一元管理します。
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
    case BossType::ThunderCaptain: return L"キャプテンサンダー";
    case BossType::HiddenBoss: return L"隠しボス";
    default: return L"ボス";
    }
}
