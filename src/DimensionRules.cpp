#include "SweetsApp.h"

// 3Dルールを使うかどうかの判定です。
// Releaseでは基本2D固定、Debug UIから3Dへ切り替えた時だけ高さ付き判定を使います。
bool SweetsApp::Use3DRules() const
{
#if defined(_DEBUG)
    return gameplayDimension_ == GameplayDimension::ThreeD;
#else
    return false;
#endif
}

// 2D座標を3D空間の地面(XZ平面)へ載せるヘルパーです。
// V2のzは、3DではZ軸として扱います。
V3 SweetsApp::Grounded3D(V2 pos, float y) const
{
    return { pos.x, ClampFloat(y, GameplayYMin, GameplayYMax), pos.z };
}

// 2D側のpos/velを3D側へ同期します。
// 3Dモードでも入力は地上移動なので、基本はXZ平面へ変換するだけです。
void SweetsApp::SyncPlayer3D(Player& p)
{
    p.pos3 = Grounded3D(p.pos, PlayerBodyY);
    p.vel3 = Grounded3D(p.vel, 0.0f);
    p.dashVel3 = Grounded3D(p.dashVel, 0.0f);
}

void SweetsApp::SyncEnemy3D(Enemy& e)
{
    e.height = ClampFloat(e.height <= 0.0f ? EnemyBodyY : e.height, EnemyBodyY, GameplayYMax);
    e.pos3 = Grounded3D(e.pos, e.height);
    e.vel3 = Grounded3D(e.vel, 0.0f);
}

void SweetsApp::SyncBoss3D(Boss& b)
{
    b.height = ClampFloat(b.height <= 0.0f ? BossBodyY : b.height, BossBodyY, GameplayYMax);
    b.pos3 = Grounded3D(b.pos, b.height);
    b.vel3 = Grounded3D(b.vel, 0.0f);
}

void SweetsApp::SyncShot3D(Shot& s)
{
    s.height = ClampFloat(s.height <= 0.0f ? ShotBodyY : s.height, 0.10f, GameplayYMax);
    s.pos3 = Grounded3D(s.pos, s.height);
    s.vel3 = Grounded3D(s.vel, 0.0f);
}

void SweetsApp::SyncPickup3D(Pickup& p)
{
    p.height = ClampFloat(p.height <= 0.0f ? PickupBodyY : p.height, PickupBodyY, GameplayYMax);
    p.pos3 = Grounded3D(p.pos, p.height);
}

void SweetsApp::SyncObstacle3D(Obstacle& o)
{
    o.height = ClampFloat(o.height <= 0.0f ? ObstacleBodyY : o.height, ObstacleBodyY, GameplayYMax);
    o.pos3 = Grounded3D(o.pos, o.height);
    o.vel3 = Grounded3D(o.vel, 0.0f);
}

void SweetsApp::SyncSlash3D(Slash& s)
{
    s.height = ClampFloat(s.height <= 0.0f ? 0.74f : s.height, 0.10f, GameplayYMax);
    s.pos3 = Grounded3D(s.pos, s.height);
}

// 既存オブジェクトをまとめて3D座標へ同期します。
// 2D/3D切替時やゲームリセット時に、見た目と判定のズレを防ぐために呼びます。
void SweetsApp::SyncAll3DState()
{
    for (auto& p : players_) SyncPlayer3D(p);
    for (auto& e : enemies_) SyncEnemy3D(e);
    SyncBoss3D(boss_);
    for (auto& s : shots_) SyncShot3D(s);
    for (auto& p : pickups_) SyncPickup3D(p);
    for (auto& o : obstacles_) SyncObstacle3D(o);
    for (auto& s : slashes_) SyncSlash3D(s);
    for (auto& p : particles_)
    {
        p.pos3 = Grounded3D(p.pos, p.y);
        p.vel3 = Grounded3D(p.vel, p.vy);
    }
    for (auto& pulse : effectPulses_) pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
    for (auto& visual : swordEffectVisuals_) visual.pos3 = Grounded3D(visual.pos, visual.height);
}

// 2D/3Dルール切替です。
// プレイ中のオブジェクトを無理に変換せず、同じ設定でランを再開始する方針です。
void SweetsApp::SetGameplayDimension(GameplayDimension dimension)
{
#if defined(_DEBUG)
    if (gameplayDimension_ == dimension) return;
    gameplayDimension_ = dimension;
    SyncAll3DState();
    message_ = dimension == GameplayDimension::ThreeD ? L"DEBUG: 座標/ルール 3D" : L"DEBUG: 座標/ルール 2D";
    messageT_ = 1.6f;
    if (screen_ == Screen::Playing || screen_ == Screen::HiddenBoss || screen_ == Screen::Paused)
    {
        RestartCurrentRun();
    }
#else
    (void)dimension;
#endif
}

float SweetsApp::RuleDistance(const Player& p, const Pickup& item) const
{
    return RuleDistance(p.pos, PlayerBodyY, item.pos, item.height);
}

// ゲームルール用の距離計算です。
// 2Dでは平面距離、3Dでは高さ差も含めた距離へ切り替わります。
float SweetsApp::RuleDistance(V2 a, float ay, V2 b, float by) const
{
    if (!Use3DRules()) return Len(a - b);
    return Len(Grounded3D(a, ay) - Grounded3D(b, by));
}

bool SweetsApp::RuleCircleHit(V2 a, float ay, float ar, V2 b, float by, float br) const
{
    return RuleDistance(a, ay, b, by) < ar + br;
}

float SweetsApp::RuleDistance(const Shot& s, const Player& p) const
{
    return RuleDistance(s.pos, s.height, p.pos, PlayerBodyY);
}

float SweetsApp::RuleDistance(const Shot& s, const Enemy& e) const
{
    return RuleDistance(s.pos, s.height, e.pos, e.height);
}

float SweetsApp::RuleDistance(const Shot& s, const Boss& b) const
{
    return RuleDistance(s.pos, s.height, b.pos, b.height);
}

float SweetsApp::RuleDistance(const Player& p, const Enemy& e) const
{
    return RuleDistance(p.pos, PlayerBodyY, e.pos, e.height);
}
