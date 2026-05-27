#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "GameDefs.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;
constexpr float ArenaRadius = 10.0f;
constexpr int MaxKeys = 256;

struct Color
{
    float r;
    float g;
    float b;
    float a;
};

constexpr Color Berry{ 1.0f, 0.24f, 0.48f, 1.0f };
constexpr Color Rose{ 0.76f, 0.36f, 0.52f, 1.0f };
constexpr Color Cream{ 1.0f, 0.94f, 0.86f, 1.0f };
constexpr Color Gold{ 1.0f, 0.70f, 0.25f, 1.0f };
constexpr Color Mint{ 0.30f, 0.85f, 0.62f, 1.0f };
constexpr Color Sky{ 0.30f, 0.60f, 1.0f, 1.0f };
constexpr Color Grape{ 0.68f, 0.36f, 1.0f, 1.0f };
constexpr Color Choco{ 0.55f, 0.32f, 0.18f, 1.0f };
constexpr Color Red{ 1.0f, 0.24f, 0.22f, 1.0f };
constexpr int FinalWave = 12;
constexpr float HiddenBossDurationSeconds = 137.14f;
constexpr int HiddenBossBulletCap = 420;

struct DifficultyDef
{
    const wchar_t* name;
    const wchar_t* summary;
    float enemyHpMul;
    float enemyAtkMul;
    float bulletSpeedMul;
    float bulletCountMul;
    float spawnIntervalMul;
    float bossHpMul;
    float enemyShotRadiusMul;
    int initialBombs;
    Color color;
};

inline const std::array<DifficultyDef, 5> DifficultyDefs{ {
    { L"Easy", L"弾幕を抑えた練習向け", 0.75f, 0.65f, 0.75f, 0.75f, 1.25f, 0.75f, 1.35f, 5, Mint },
    { L"Normal", L"標準難易度", 1.00f, 1.00f, 0.88f, 1.00f, 1.00f, 1.00f, 1.00f, 3, Cream },
    { L"Hard", L"敵の圧が強くなる", 1.15f, 1.20f, 1.10f, 1.15f, 0.90f, 1.15f, 1.12f, 3, Gold },
    { L"Expert", L"弾速と出現頻度が高い", 1.35f, 1.45f, 1.25f, 1.30f, 0.78f, 1.35f, 1.08f, 2, Sky },
    { L"Lunatic", L"最高難度。最後まで気を抜くな。", 1.55f, 1.70f, 1.40f, 1.55f, 0.68f, 1.55f, 1.05f, 2, Grape },
} };

struct V2
{
    float x = 0.0f;
    float z = 0.0f;
};

inline V2 operator+(V2 a, V2 b) { return { a.x + b.x, a.z + b.z }; }
inline V2 operator-(V2 a, V2 b) { return { a.x - b.x, a.z - b.z }; }
inline V2 operator*(V2 a, float s) { return { a.x * s, a.z * s }; }
inline V2 operator/(V2 a, float s) { return { a.x / s, a.z / s }; }
inline V2& operator+=(V2& a, V2 b) { a.x += b.x; a.z += b.z; return a; }
inline V2& operator-=(V2& a, V2 b) { a.x -= b.x; a.z -= b.z; return a; }
inline V2& operator*=(V2& a, float s) { a.x *= s; a.z *= s; return a; }

inline float Dot(V2 a, V2 b) { return a.x * b.x + a.z * b.z; }
inline float LenSq(V2 a) { return Dot(a, a); }
inline float Len(V2 a) { return std::sqrt(LenSq(a)); }
inline V2 Normalize(V2 a)
{
    const float l = Len(a);
    if (l <= 0.0001f) return {};
    return a / l;
}
inline V2 FromAngle(float a) { return { std::cos(a), std::sin(a) }; }
inline float AngleOf(V2 v) { return std::atan2(v.z, v.x); }
inline float ClampFloat(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

inline Color WithAlpha(Color c, float a)
{
    c.a = a;
    return c;
}

inline void ThrowIfFailed(HRESULT hr, const char* what)
{
    if (FAILED(hr))
    {
        char buffer[256]{};
        std::snprintf(buffer, sizeof(buffer), "%s failed. HRESULT=0x%08X", what, static_cast<unsigned>(hr));
        throw std::runtime_error(buffer);
    }
}

struct Vertex
{
    XMFLOAT3 pos;
    XMFLOAT3 normal;
    XMFLOAT4 color;
};

struct Mesh
{
    ComPtr<ID3D11Buffer> vb;
    ComPtr<ID3D11Buffer> ib;
    UINT indexCount = 0;
};

struct FrameCB
{
    XMMATRIX viewProj;
    XMFLOAT4 lightDir;
    XMFLOAT4 cameraPos;
};

struct ObjectCB
{
    XMMATRIX world;
    XMFLOAT4 tint;
};

enum class Screen
{
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
};

struct PostCB
{
    XMFLOAT4 params;
};

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
    const wchar_t* name;
    const wchar_t* role;
    const wchar_t* summary;
    Weapon weapon;
    CharacterType character;
    float maxHp;
    float speed;
    float damageMul;
    float cooldownMul;
    float ultStart;
    Color color;
};

inline const std::array<LoadoutPreset, 4> Loadouts{ {
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
    float radius = 1.15f;
    float hp = 800.0f;
    float maxHp = 800.0f;
    float speed = 1.4f;
    float atk = 12.0f;
    float attackCd = 1.2f;
    float spin = 0.0f;
    float flash = 0.0f;
    int phase = 1;
    int type = 0;
    BossType bossType = BossType::Demon;
    bool active = false;
};

struct Shot
{
    V2 pos{};
    V2 vel{};
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

struct Slash
{
    V2 pos{};
    float angle = 0.0f;
    float arc = 1.30f;
    float range = 1.9f;
    float ttl = 0.18f;
    float life = 0.18f;
    float damage = 35.0f;
    Color color = Choco;
    bool hitBoss = false;
};

struct Pickup
{
    V2 pos{};
    float radius = 0.28f;
    float ttl = 14.0f;
    int type = 0;
    PickupType pickupType = PickupType::Attack;
    Color color = Gold;
};

struct Obstacle
{
    V2 pos{};
    float radius = 0.65f;
    float hp = 120.0f;
    float ttl = -1.0f;
    float reflectPower = 1.0f;
    int kind = 0;
    int ownerIndex = -1;
    V2 vel{};
    Color color = Choco;
    bool moving = false;
    bool cheeseWall = false;
    bool damageField = false;
};

struct Particle
{
    V2 pos{};
    V2 vel{};
    float y = 0.2f;
    float vy = 1.0f;
    float ttl = 0.4f;
    Color color = Cream;
};
