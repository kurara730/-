#pragma once

#include <array>
#include <string>

#include "GameDefs.h"
#include "GameMath.h"

enum class Weapon
{
    Strawberry = 0,
    Chocolate = 1,
    Cheese = 2,
    Roll = 3
};

struct WeaponDef
{
    const wchar_t* name;
    Color color;
    float cooldown;
    float speed;
    float damage;
    float radius;
    int pierce;
    int bounce;
};

inline const std::array<WeaponDef, 4> Weapons{ {
    { L"イチゴ", Berry, 0.15f, 15.0f, 18.0f, 0.13f, 0, 0 },
    { L"チョコ", Choco, 0.38f, 0.0f, 38.0f, 0.0f, 0, 0 },
    { L"チーズ", Gold, 0.32f, 10.0f, 31.0f, 0.22f, 1, 0 },
    { L"ロール", Cream, 0.54f, 11.5f, 34.0f, 0.19f, 0, 5 },
} };

struct LoadoutPreset
{
    std::wstring name;
    std::wstring role;
    std::wstring summary;
    Weapon weapon;
    CharacterType character;
    float maxHp;
    float speed;
    float damageMul;
    float cooldownMul;
    float ultStart;
    Color color;
};

// CSV(assets/data/characters.csv)で実行時に上書き可能。詳細は DataTables.h を参照。
inline std::array<LoadoutPreset, 4> Loadouts{ {
    { L"ショート", L"速度 / 連射", L"追尾弾と分裂弾で攻める", Weapon::Strawberry, CharacterType::Shortcake, 92.0f, 6.15f, 0.92f, 0.82f, 18.0f, Berry },
    { L"チョコ", L"体力 / 近接", L"近距離で広く薙ぎ払う", Weapon::Chocolate, CharacterType::Chocolate, 145.0f, 4.55f, 1.10f, 1.06f, 8.0f, Choco },
    { L"チーズ", L"火力 / 反射壁", L"壁で敵弾を跳ね返す", Weapon::Cheese, CharacterType::Cheese, 112.0f, 4.95f, 1.28f, 1.14f, 12.0f, Gold },
    { L"ロール", L"制御 / 反射", L"跳ねる弾と突進で崩す", Weapon::Roll, CharacterType::Roll, 108.0f, 5.35f, 1.00f, 0.98f, 24.0f, Cream },
} };

struct Player
{
    V2 pos{};
    V2 vel{};
    V2 dashVel{};
    V3 pos3{};
    V3 vel3{};
    V3 dashVel3{};
    float radius = 0.42f;
    float hitboxRadius = 0.13f;
    float grazeRadius = 0.62f;
    float hp = 100.0f;
    float maxHp = 100.0f;
    float speed = 5.2f;
    float damageMul = 1.0f;
    float cooldownMul = 1.0f;
    float fireCd = 0.0f;
    float ult = 0.0f;
    float inv = 0.0f;
    float shieldT = 0.0f;
    float bombT = 0.0f;
    float grazeFlash = 0.0f;
    float dmgBuffT = 0.0f;
    float speedBuffT = 0.0f;
    float scoreDoubleT = 0.0f;
    float magnetT = 0.0f;
    float spreadT = 0.0f;
    float chargeT = 0.0f;
    float chargeCd = 0.0f;
    float dashT = 0.0f;
    float reviveT = 0.0f;
    float fever = 0.0f;
    float feverT = 0.0f;
    float corePower = 0.0f;
    float coreBounce = 0.0f;
    float corePierce = 0.0f;
    int level = 1;
    int xp = 0;
    int kills = 0;
    int graze = 0;
    int grazeChain = 0;
    int bombs = 3;
    int skillCount = 0;
    int index = 0;
    Weapon weapon = Weapon::Strawberry;
    CharacterType character = CharacterType::Shortcake;
    float face = 0.0f;
    bool active = true;
    bool ai = false;
    bool focus = false;
    bool charging = false;
    bool chargeReady = false;
    bool downed = false;
    bool alive = true;
};

struct Enemy
{
    V2 pos{};
    V2 vel{};
    V3 pos3{};
    V3 vel3{};
    float height = EnemyBodyY;
    float radius = 0.38f;
    float hp = 20.0f;
    float maxHp = 20.0f;
    float speed = 2.0f;
    float atk = 8.0f;
    float touchCd = 0.0f;
    float shootCd = 0.0f;
    float dashCd = 0.0f;
    float dashT = 0.0f;
    float barrierT = 0.0f;
    float teleportCd = 0.0f;
    float face = 0.0f;
    float flash = 0.0f;
    int kind = 0;
    EnemyType type = EnemyType::Normal;
    int score = 100;
    Color color = Rose;
    bool dead = false;
};

struct Boss
{
    V2 pos{};
    V2 vel{};
    V3 pos3{};
    V3 vel3{};
    float height = BossBodyY;
    float radius = 1.15f;
    float hp = 800.0f;
    float maxHp = 800.0f;
    float speed = 1.4f;
    float atk = 12.0f;
    float attackCd = 1.2f;
    float telegraphT = 0.0f;
    float telegraphLife = 0.0f;
    float spin = 0.0f;
    float flash = 0.0f;
    int phase = 1;
    int type = 0;
    int telegraphAttack = -1;
    bool telegraphAdd = false;
    bool telegraphMirror = false;
    bool telegraphField = false;
    BossType bossType = BossType::Demon;
    bool active = false;
};

struct Shot
{
    V2 pos{};
    V2 vel{};
    V3 pos3{};
    V3 vel3{};
    float height = ShotBodyY;
    float radius = 0.12f;
    float damage = 10.0f;
    float ttl = 2.0f;
    int pierce = 0;
    int bounce = 0;
    int ownerIndex = 0;
    int reflectedCount = 0;
    int splitCount = 0;
    float angularVel = 0.0f;
    float accel = 0.0f;
    float homingStrength = 0.0f;
    CharacterType sourceCharacter = CharacterType::Shortcake;
    bool enemy = false;
    bool dead = false;
    bool grazed = false;
    bool reflected = false;
    bool charged = false;
    Color color = Cream;
};

enum class SlashVisualMode
{
    Sector,
    Hidden,
    Line
};

struct Slash
{
    V2 pos{};
    V3 pos3{};
    float height = 0.74f;
    float angle = 0.0f;
    float arc = 1.30f;
    float range = 1.9f;
    float ttl = 0.18f;
    float life = 0.18f;
    float damage = 35.0f;
    Color color = Choco;
    SlashVisualMode visualMode = SlashVisualMode::Sector;
    bool hitBoss = false;
};

struct Pickup
{
    V2 pos{};
    V3 pos3{};
    float height = PickupBodyY;
    float radius = 0.28f;
    float ttl = 14.0f;
    int type = 0;
    PickupType pickupType = PickupType::Attack;
    Color color = Gold;
};

struct Obstacle
{
    V2 pos{};
    V3 pos3{};
    float radius = 0.65f;
    float height = ObstacleBodyY;
    float hp = 120.0f;
    float ttl = -1.0f;
    float reflectPower = 1.0f;
    int kind = 0;
    int ownerIndex = -1;
    V2 vel{};
    V3 vel3{};
    Color color = Choco;
    bool moving = false;
    bool cheeseWall = false;
    bool damageField = false;
};

struct Particle
{
    V2 pos{};
    V2 vel{};
    V3 pos3{};
    V3 vel3{};
    float y = 0.2f;
    float vy = 1.0f;
    float ttl = 0.4f;
    Color color = Cream;
};

struct EffectPulse
{
    V2 pos{};
    V3 pos3{};
    float startRadius = 0.6f;
    float endRadius = 2.0f;
    float ttl = 0.32f;
    float life = 0.32f;
    float y = 0.22f;
    Color color = Cream;
};

struct SwordEffectVisual
{
    V2 pos{};
    V3 pos3{};
    float angle = 0.0f;
    float scale = 1.0f;
    float range = 2.0f;
    float arc = 1.35f;
    float height = 0.56f;
    float ttl = 0.20f;
    float life = 0.20f;
    bool charged = false;
};
