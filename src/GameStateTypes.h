#pragma once

#include <array>
#include <string>

#include "GameDefs.h"
#include "GameMath.h"

// ショートのヒート（撃ち続けて到達する最大秒数）。0〜この値で連射速度・威力・反射・サイズを補間します。
// この値に達するとオーバーヒートし、StrawberryOverheatLock 秒だけ発射ロックされます。
// レッドゾーン到達までは普通に上がるが、ゾーン内は過熱が StrawberryRedlineHeatMul 倍に減速し、
// 最大火力を長く維持できる（＝最高火力になってからの過熱を緩やかにする）。
constexpr float StrawberryHeatMax = 4.2f;        // 最大威力(レッドゾーン)到達まで約2.5秒
constexpr float StrawberryOverheatLock = 2.2f;   // オーバーヒート時の発射ロック秒数
constexpr float StrawberryRedline = 0.6f;        // この比率以上はレッドゾーン（最大火力＝金弾）
constexpr float StrawberryRedlineHeatMul = 0.6f;  // レッドゾーン中の過熱進行倍率（小さいほど長く維持できる）

enum class Weapon
{
    Strawberry = 0,
    Chocolate = 1,
    Cheese = 2,
    Roll = 3
};

// 通常攻撃の基礎性能です。
// キャラクター固有の補正は LoadoutPreset 側にあり、最終火力は両方を組み合わせます。
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
    { L"イチゴ", Berry, 0.19f, 15.0f, 18.0f, 0.13f, 0, 0 },
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
    { L"ショート", L"ヒート連射", L"止まって撃ち続け反射弾を強化する", Weapon::Strawberry, CharacterType::Shortcake, 92.0f, 6.15f, 0.92f, 0.82f, 18.0f, Berry },
    { L"チョコ", L"ヨーヨー", L"敵で跳ねる弾をコンボさせる", Weapon::Chocolate, CharacterType::Chocolate, 145.0f, 4.55f, 1.10f, 1.06f, 8.0f, Choco },
    { L"チーズ", L"敵弾キャッチ", L"壁で敵弾を味方弾へ変える", Weapon::Cheese, CharacterType::Cheese, 112.0f, 4.95f, 1.28f, 1.14f, 12.0f, Gold },
    { L"ロール", L"壁バウンド", L"壁と境界で跳ねる弾を操る", Weapon::Roll, CharacterType::Roll, 108.0f, 5.35f, 1.00f, 0.98f, 24.0f, Cream },
} };

// プレイヤー1人分の実行時状態です。
// pos/vel は2Dルール用、pos3/vel3 は3Dルール用で、DimensionRules.cpp で同期します。
// hp、ult、bombs などの戦闘値もここに集め、UI表示とゲーム判定の両方から参照します。
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
    float warpCd = 0.0f;
    float bombCharge = 0.0f;    // チョコ爆弾のチャージ量（長押し時間）
    float fireHeat = 0.0f;      // ショートのヒート（撃ち続けた時間。移動でリセット）
    float overheatT = 0.0f;     // オーバーヒート中の発射ロック残時間（>0なら撃てない）
    bool firing = false;        // ショートが発射入力中か（移動中=ヒート0でもゲージ表示するため）
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
    bool chargeFull = false;
    bool downed = false;
    bool alive = true;
};

// 雑魚敵の状態です。
// type が敵の種類、kind は旧実装互換の番号です。新しい処理では type を優先します。
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
    int id = 0;
    EnemyType type = EnemyType::Normal;
    int score = 100;
    Color color = Rose;
    bool dead = false;
    bool hiddenBossClone = false;
    bool caught = false;        // チョコ最大弾に巻き込まれて固定中
    V2 caughtOffset{};          // 弾中心からの相対位置
};

// 通常ボスと隠しボスで共通利用する状態です。
// 隠しボス固有のギミックは HiddenBossCore など別構造に分け、ここは本体HPと攻撃状態を持ちます。
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

struct BossGimmickState
{
    BossType type = BossType::Demon;
    BossPatternId nextPattern = BossPatternId::Radial;
    int patternStep = 0;
    int sealHits = 0;
    int mirrorIndex = 0;
    float timer = 0.0f;
    float vulnerableT = 0.0f;
    float guardAngle = 0.0f;
    float gravityT = 0.0f;
    float territoryT = 0.0f;
    bool mirrorOpen = false;
};

// 弾1発分の状態です。
// enemy=true は敵弾、false は味方弾です。反射すると enemy が false になり ownerIndex が入ります。
// hitBoss は貫通弾や斬撃波が同じボスへ毎フレーム多段ヒットしないための印です。
enum class ShotVisualKind
{
    Orb,
    Homing,
    Blade
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
    int yoyoCombo = 0;
    int lastHitEnemyId = -1;
    int reflectSplit = 0;       // 反射した瞬間に分裂する子弾数（ショート用）
    bool chocoBomb = false;     // チャージで撃つ爆弾弾（チョコ用）
    int growStage = 0;          // 爆弾のチャージ段階（0〜3）
    float angularVel = 0.0f;
    float accel = 0.0f;
    float homingStrength = 0.0f;
    float yoyoRetargetT = 0.0f;
    float warpCd = 0.0f;
    CharacterType sourceCharacter = CharacterType::Shortcake;
    bool enemy = false;
    bool dead = false;
    bool grazed = false;
    bool reflected = false;
    bool charged = false;
    bool hitBoss = false;
    bool yoyo = false;
    bool ultimateSource = false;
    bool hiddenBossAuraKey = false;
    ShotVisualKind visual = ShotVisualKind::Orb;
    Color color = Cream;
};

enum class SlashVisualMode
{
    Sector,
    Hidden,
    Line
};

// チョコの近接斬りなど、扇形の短時間判定です。
// 見た目をEffekseerや別スプライトに任せる場合でも、当たり判定だけはこの構造を使います。
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
    bool sweep = false;         // 薙ぎ払い：刃が弧を端から端へ振り抜ける演出
};

// フィールド上のアイテムです。
// 色だけでなく PickupType ごとに形も変え、敵や弾と見間違えにくくしています。
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

// ステージ障害物やチーズ壁を表します。
// cheeseWall=true の壁は敵弾を味方弾へ変換できるため、反射体験の重要な要素です。
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
    bool bumper = false;        // 反射ブースト台
    bool breakable = false;     // 破壊可能（壊すとドロップ）
    float maxHp = 140.0f;
    float flash = 0.0f;         // ヒット時の発光
    float spin = 0.0f;          // 見た目の回転
    float pushForce = 0.0f;     // コンベア/突風（vel方向へ押す）
    float gravity = 0.0f;       // 重力井戸（>0で引き寄せ）
    V2 orbitCenter{};           // 公転中心
    float orbitRadius = 0.0f;
    float orbitAngle = 0.0f;
    float orbitSpeed = 0.0f;    // 0以外で公転
    bool flipper = false;       // フリッパー（往復スイング）
    float swingBase = 0.0f;     // スイングの中心角
    float swingAmp = 0.0f;      // スイングの振れ幅
    int warpId = -1;            // ワープ対（同一idどうしが対）
};

// 軽量な粒子です。爆発、反射、被弾などの短い演出に使います。
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

// 隠しボス1ゲージ目の炎核ギミックです。
// core を壊すことで本体へ大きくダメージが通る攻撃チャンスを作ります。
struct HiddenBossCore
{
    V2 pos{};
    V3 pos3{};
    float angle = 0.0f;
    float orbitRadius = 2.7f;
    float radius = 0.34f;
    float hp = 220.0f;
    float maxHp = 220.0f;
    float flash = 0.0f;
    bool active = false;
};

// 加算表示用のリング/衝撃波です。
// Effekseerが読み込めない場合でも、ここで最低限の派手さを維持します。
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

// Sword9由来の剣エフェクトを補助する2D表示データです。
// angle/range/arc を Slash と揃えることで、見た目と攻撃判定の向きを一致させます。
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
