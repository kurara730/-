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
constexpr int HiddenBossBulletCap = 320;
constexpr int HiddenBossGaugeCount = 3;
constexpr float HiddenBossBaseGaugeHp = 4800.0f;
constexpr float HiddenBossIntroDuration = 10.0f;
constexpr int HiddenBossCoreCount = 4;
constexpr int HiddenBossReflectBreakCount = 3;
constexpr float GameplayYMin = 0.0f;
constexpr float GameplayYMax = 3.2f;
constexpr float PlayerBodyY = 0.24f;
constexpr float EnemyBodyY = 0.32f;
constexpr float BossBodyY = 0.62f;
constexpr float ShotBodyY = 0.34f;
constexpr float PickupBodyY = 0.34f;
constexpr float ObstacleBodyY = 0.28f;

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

struct V3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline V2 operator+(V2 a, V2 b) { return { a.x + b.x, a.z + b.z }; }
inline V2 operator-(V2 a, V2 b) { return { a.x - b.x, a.z - b.z }; }
inline V2 operator*(V2 a, float s) { return { a.x * s, a.z * s }; }
inline V2 operator/(V2 a, float s) { return { a.x / s, a.z / s }; }
inline V2& operator+=(V2& a, V2 b) { a.x += b.x; a.z += b.z; return a; }
inline V2& operator-=(V2& a, V2 b) { a.x -= b.x; a.z -= b.z; return a; }
inline V2& operator*=(V2& a, float s) { a.x *= s; a.z *= s; return a; }

inline V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline V3 operator/(V3 a, float s) { return { a.x / s, a.y / s, a.z / s }; }
inline V3& operator+=(V3& a, V3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline V3& operator-=(V3& a, V3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline V3& operator*=(V3& a, float s) { a.x *= s; a.y *= s; a.z *= s; return a; }

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

inline float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float LenSq(V3 a) { return Dot(a, a); }
inline float Len(V3 a) { return std::sqrt(LenSq(a)); }
inline V3 Normalize(V3 a)
{
    const float l = Len(a);
    if (l <= 0.0001f) return {};
    return a / l;
}
inline V3 ToV3(V2 xz, float y) { return { xz.x, y, xz.z }; }
inline V2 ToV2(V3 xyz) { return { xyz.x, xyz.z }; }
inline V3 FromYaw(float yaw, float y = 0.0f) { return { std::cos(yaw), y, std::sin(yaw) }; }
inline float DistanceSqXZ(V3 a, V3 b) { return LenSq(ToV2(a) - ToV2(b)); }

enum class GameplayDimension
{
    TwoD,
    ThreeD
};

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

struct PostCB
{
    XMFLOAT4 params;
};
