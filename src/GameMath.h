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

// ゲーム全体で共有する基本定数です。
// ArenaRadius は当たり判定、敵の出現位置、ステージ境界の基準になるため、
// 変更するとゲーム全体の広さと難易度が大きく変わります。
constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;
constexpr float ArenaRadius = 14.0f;
constexpr int MaxKeys = 256;

// DX11 側へ渡す色もこの構造体で統一します。
// r/g/b/a は 0.0 - 1.0 の範囲で、a が透明度です。
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
constexpr Color Ink{ 0.07f, 0.07f, 0.10f, 1.0f };   // 腕の節（黒）
constexpr Color Navy{ 0.16f, 0.20f, 0.64f, 1.0f };  // ボス本体（青）

// 進行ルールに関わる固定値です。
// 隠しボスは3ゲージ制なので、1ゲージHPとゲージ数を分けて持っています。
constexpr int FinalWave = 12;
constexpr float HiddenBossDurationSeconds = 137.14f;
constexpr int HiddenBossBulletCap = 380;
constexpr int HiddenBossGaugeCount = 3;
constexpr float HiddenBossBaseGaugeHp = 4800.0f;
constexpr float HiddenBossIntroDuration = 10.0f;
constexpr int HiddenBossCoreCount = 4;
constexpr int HiddenBossReflectBreakCount = 5;
constexpr float GameplayYMin = 0.0f;
constexpr float GameplayYMax = 3.2f;
constexpr float PlayerBodyY = 0.24f;
constexpr float EnemyBodyY = 0.32f;
constexpr float BossBodyY = 0.62f;
constexpr float ShotBodyY = 0.34f;
constexpr float PickupBodyY = 0.34f;
constexpr float ObstacleBodyY = 0.28f;

// 難易度ごとの補正値です。
// 表示上は実数として見せますが、内部では敵HP、弾速、出現間隔などへ直接掛けます。
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
    { L"Hard", L"敵の圧が増える", 1.25f, 1.30f, 1.18f, 1.25f, 0.82f, 1.30f, 1.12f, 3, Gold },
    { L"Expert", L"弾幕と出現が速い", 1.55f, 1.65f, 1.42f, 1.55f, 0.66f, 1.65f, 1.08f, 2, Sky },
    { L"Lunatic", L"最高難度。最後まで気を抜くな。", 1.90f, 2.05f, 1.72f, 2.05f, 0.50f, 2.05f, 1.05f, 2, Grape },
} };

// 2Dモードでは x/z だけを使い、3Dモードでは V3 に同期して高さも判定へ含めます。
// z を使っているのは、3D空間の地面(XZ平面)と対応させるためです。
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
    XMFLOAT4 params;  // x: taa blend, y: additive scale, z: view mode, w: brightness(exposure)
    XMFLOAT4 params2; // x: bloom intensity, y: vignette strength, z: tonemap enable, w: unused
};

struct BloomCB
{
    XMFLOAT4 texel;  // xy: texel size of source, zw: blur direction (1,0) or (0,1)
    XMFLOAT4 params; // x: threshold, y: soft knee, z: intensity, w: unused
};
