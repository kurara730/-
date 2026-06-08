#pragma once

#include <array>

constexpr int MaxPlayers = 4;

// スペースキーのブリンク（短距離テレポート回避）の調整値です。
constexpr float BlinkDistance = 3.8f;    // 通常ブリンク（ジャスト回避でない）の移動距離
constexpr float BlinkJustDistance = 5.8f; // ジャスト回避成功時の移動距離（成功を見分けやすく長め）
constexpr float BlinkInvuln = 0.35f;     // ブリンク直後の無敵時間（弾を抜けられる）
constexpr int BlinkMaxCharges = 2;       // 連続で使える最大回数（チャージ）
constexpr float BlinkChargeCooldown = 10.0f; // チャージ1回あたりの回復時間（秒）
constexpr float BlinkAttackLock = 0.3f;  // ブリンク直後に攻撃を出せない時間（攻撃とブリンクの同時発動を禁止）

// ジャスト回避（危険な状況でブリンク成功）時の演出。ヒットストップ＋自キャラへズーム。
constexpr float HitstopTime = 0.22f;     // ヒットストップ（強スロー）の実時間長
constexpr float HitstopScale = 0.06f;    // ヒットストップ中のゲーム内時間倍率
constexpr float JustZoomLife = 0.55f;    // ズーム演出の長さ（実時間）
constexpr float JustZoomPeak = 0.30f;    // ズーム量のピーク（1.0 + これ倍）
constexpr float JustDodgeBulletRange = 1.35f; // この距離以内に迫る敵弾があればジャスト判定

// ボスの貫通ビーム（パリィ不可・低頻度の強攻撃）の調整値です。
// 予兆(Warn)で地面に線を出して避けさせ、照射(Active)中に線分上のプレイヤーへダメージします。
constexpr float BossBeamWarnTime = 1.5f;      // 予兆時間（回避猶予）
constexpr float BossBeamActiveTime = 1.5f;    // 照射時間
constexpr float BossBeamHalfWidth = 1.24f;    // ビーム半幅（当たり判定）
constexpr float BossBeamLength = 26.0f;       // ビーム長（アリーナを貫く）
constexpr float BossBeamDamageMul = 2.2f;     // boss_.atk への倍率（強攻撃）
constexpr float BossBeamCooldownMin = 12.0f;  // 次のビームまでの最短秒数（発動頻度・低め）
constexpr float BossBeamCooldownVar = 5.0f;   // 上記に加算される乱数幅

// どの攻撃（弾幕・特殊技）も、終わった後この秒数は次の攻撃を始めない（攻撃の重なり防止＝間合い確保）。
constexpr float BossAttackRest = 0.7f;

// 貫通ビームの反射＆ブレイク（崩し）システム。
// チョコウォール（右クリックの壁）をビームの軌道上に置くと、ビームを反射してボスへダメージ。
// 反射ダメージは「ブレイクゲージ」に蓄積し、満タンでボスが一定時間動けなくなる（ブレイク）。
constexpr float BossBeamReflectDps = 1.6f;     // 反射中、ボスへ与える毎秒ダメージ（boss.atk倍率）
constexpr float BossBreakGaugeMax = 90.0f;     // ブレイクゲージ最大値（反射ダメージの蓄積で満タン＝約3回の全反射）
constexpr float BossBreakDuration = 10.0f;     // ブレイク状態（動けない）の継続時間（秒）

// ボスのフェーズ（SAO風の分割HPバー）。HPを BossGaugeCount 本のゲージに分割し、
// 1本削り切るたびにフェーズが上がり、行動が激化する（移動・攻撃頻度の上昇＋弾消し＋演出）。
constexpr int BossGaugeCount = 4;              // HPバーの分割数＝フェーズ数
constexpr float BossPhaseAggroPerPhase = 0.30f;// フェーズ毎の攻撃頻度上昇（特殊技CD短縮）
constexpr float BossPhaseSpeedPerPhase = 0.15f;// フェーズ毎の移動速度上昇
constexpr float BossPhaseIntroTime = 0.9f;     // フェーズ移行時の小休止（無防備の溜め＝ピーク演出）

// ブレイクコンボ：ブレイク中だけ有効。ヒットを重ねるほど与ダメージ倍率が上がり、ブレイク終了で必ずリセット。
constexpr float BreakComboDamagePerHit = 0.06f;// 1ヒットごとの倍率上昇（+6%）
constexpr float BreakComboMaxMul = 3.0f;       // 倍率の上限

// ボスの極太回転ビーム薙ぎ払い（パリィ不可）。極太ビームを出しながらゆっくり回す。
constexpr float BossMegaBeamWarnTime = 1.6f;
constexpr float BossMegaBeamActiveTime = 3.2f;   // 回しながら照射する時間
constexpr float BossMegaBeamHalfWidth = 2.1f;    // 極太
constexpr float BossMegaBeamLength = 28.0f;
constexpr float BossMegaBeamDamageMul = 1.5f;
constexpr float BossMegaBeamRotateSpeed = 0.55f; // 回転速度（ラジアン/秒）
constexpr float BossMegaBeamCooldownMin = 17.0f;
constexpr float BossMegaBeamCooldownVar = 6.0f;

// ボスの腕（ダメージ床）。ボス本体の一部として最寄りプレイヤー方向へ伸び、触れると継続ダメージ。
// 腕に触れた瞬間、つかみ(下記)が可能ならそのプレイヤーを捕獲する。
constexpr float BossArmRadius = 1.1f;        // 腕先端（赤＝掴み判定）の半径
constexpr float BossArmReach = 3.6f;         // ボス中心から腕先端（赤）までの距離
constexpr float BossArmTrackSpeed = 2.2f;    // 腕がプレイヤー方向へ向き直る速さ（ラジアン/秒）
constexpr float BossArmChipPerSec = 8.0f;    // 腕に触れている間の継続ダメージ（毎秒・従来のダメージ床と同等）
constexpr float BossArmSpread = 0.7f;        // 左右2本の腕の開き角の最大値（ラジアン）
constexpr float BossArmSpreadMinRatio = 0.3f;// 開き角の最小＝最大×この割合（この範囲で開閉する）
constexpr float BossArmSpreadSpeed = 1.2f;   // 開閉のゆっくりした速さ（ラジアン/秒）
constexpr float BossArmHpRatio = 0.10f;      // 腕のHP＝ボス最大HPのこの割合。これだけ削ると腕が消滅
constexpr float BossArmDestroyTime = 20.0f;  // 腕が消滅してから復活するまでの時間（秒）

// ボスのつかみ攻撃（回避専用）。腕に触れると捕獲し、拘束ダメージ→解放。
constexpr float BossGrabWarnTime = 0.9f;
constexpr float BossGrabReachTime = 0.35f;       // 手を伸ばす判定時間（外したら終了）
constexpr float BossGrabRange = 6.5f;
constexpr float BossGrabArc = 0.7f;              // 正面コーンの全角（ラジアン）
constexpr float BossGrabHoldTime = 2.2f;         // 拘束時間
constexpr float BossGrabTickDamageMul = 0.275f;  // 拘束中の周期ダメージ（i-frameで約0.45s間引き／旧0.55の1/2）
constexpr float BossGrabCooldownMin = 6.0f;
constexpr float BossGrabCooldownVar = 2.0f;
// つかみ攻撃で腕を「伸ばす」動き。
constexpr float BossGrabTriggerRange = 9.5f;     // この距離以内にプレイヤーがいると掴みを試みる
constexpr float BossGrabReachWarn = 0.55f;       // 腕を引いて溜める予兆時間
constexpr float BossGrabThrustTime = 0.42f;      // 腕を突き出す時間
constexpr float BossGrabReachMax = 9.0f;         // 突き出した腕先（赤）の最大到達距離

// ボスの飛行必殺（フィールドを一周してから大範囲円に強攻撃）。
constexpr float BossFlyWarnTime = 1.2f;          // 飛び立つ前の予兆
constexpr float BossFlyLapTime = 3.0f;           // フィールドを一周する時間
constexpr float BossFlyStrikeWarnTime = 1.5f;    // 着弾円の予兆
constexpr float BossFlyStrikeTime = 0.6f;        // 着弾（判定/演出）
constexpr float BossFlyStrikeRadius = 5.9f;      // 着弾円の半径（フィールドの約1/3）
constexpr float BossFlyDamageMul = 3.0f;         // 必殺の威力
constexpr float BossFlyCooldownMin = 22.0f;
constexpr float BossFlyCooldownVar = 6.0f;

// ボスの薙ぎ払い（近接・回避専用＝パリィ不可）の調整値です。
// プレイヤーが近いときに前方の扇を予告→振り抜く。接近を咎める対近接攻撃。
constexpr float BossSweepWarnTime = 0.85f;     // 予兆（短め＝近接の圧）
constexpr float BossSweepActiveTime = 0.4f;    // 振り抜きの見た目・判定が残る時間
constexpr float BossSweepRange = 5.6f;         // 扇の半径（リーチ）
constexpr float BossSweepArc = 2.7f;           // 扇の全角（ラジアン, 約155度）
constexpr float BossSweepDamageMul = 1.8f;     // boss_.atk への倍率
constexpr float BossSweepCooldownMin = 6.0f;   // 最短クールダウン
constexpr float BossSweepCooldownVar = 2.5f;   // 加算乱数幅
constexpr float BossSweepTriggerRange = 6.6f;  // この距離以内にプレイヤーがいると発動

// ボスの地中突き上げ（Burrow→Eruption・回避専用＝パリィ不可）の調整値です。
// 潜行予兆→地中に潜って無敵→各プレイヤー足元の予測円が追尾→ロック→地面から噴出。
constexpr float BossBurrowWarnTime = 0.8f;     // 潜る前の予兆（地上で発光）
constexpr float BossBurrowSubmergeTime = 2.2f; // 潜行（無敵・非表示）の時間
constexpr float BossBurrowLockTime = 0.7f;     // 潜行残りこの秒数で予測円をロック（以降は追尾しない）
constexpr float BossBurrowEruptTime = 0.5f;    // 噴出の判定/演出が残る時間
constexpr float BossBurrowRadius = 2.3f;       // 噴出1発の半径
constexpr float BossBurrowDamageMul = 2.0f;    // boss_.atk への倍率（強攻撃）
constexpr float BossBurrowCooldownMin = 14.0f; // 最短クールダウン（低頻度）
constexpr float BossBurrowCooldownVar = 4.0f;  // 加算乱数幅

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
    ReflectedShot,
    HiddenBossAuraKey
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
    { L"チョコ", L"CH", L"チャージボム", L"チョコウォール", L"時計斬り" },
    { L"チーズ", L"CZ", L"チーズショット", L"ストレッチダッシュ", L"無敵要塞" },
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
