#include "SweetsApp.h"
#include "StageFactory.h"

// StageFactory.cpp は、Waveごとの地形やステージギミックを作ります。
// 敵の強さではなく、移動しやすさや障害物配置でプレイ感を変える役割です。

// ステージごとの基調色です。背景ラインや床の雰囲気に使います。
Color StageColor(StageType stage)
{
    switch (stage)
    {
    case StageType::Donut: return Choco;
    case StageType::TwinIsland: return Grape;
    case StageType::Pinball: return Gold;
    case StageType::RingCorridor: return Sky;
    case StageType::FourPillars: return Cream;
    case StageType::MovingIsland: return Mint;
    case StageType::ShrinkRing: return Red;
    case StageType::BossArena: return Rose;
    default: return Choco;
    }
}

// Wave番号からステージを決めます。ボスWaveは専用アリーナに固定します。
StageType StageForWave(int wave, bool bossWave)
{
    if (bossWave) return StageType::BossArena;
    return static_cast<StageType>((wave - 1) % 7);
}

namespace
{
FieldShape FieldShapeForStage(StageType stage)
{
    switch (stage)
    {
    case StageType::TwinIsland: return FieldShape::Rectangle;
    case StageType::Pinball: return FieldShape::Octagon;
    case StageType::RingCorridor: return FieldShape::Ring;
    case StageType::FourPillars: return FieldShape::Rectangle;
    case StageType::MovingIsland: return FieldShape::Corridor;
    case StageType::ShrinkRing: return FieldShape::ShrinkCircle;
    case StageType::Donut:
    case StageType::BossArena:
    default: return FieldShape::Circle;
    }
}
}

// 現在のWaveからステージ種類を選び、障害物や境界ルールを初期化します。
void SweetsApp::BuildStage()
{
    obstacles_.clear();
    stage_ = StageForWave(wave_, bossWave_);
    fieldShape_ = FieldShapeForStage(stage_);
    shrinkRadius_ = ArenaRadius;
    stageTimer_ = 0.0f;

    // ステージ移動時はショートのヒートをリセット（持ち越さない）
    for (auto& p : players_)
    {
        p.fireHeat = 0.0f;
        p.overheatT = 0.0f;
    }

    auto push = [&](const Obstacle& o)
    {
        Obstacle copy = o;
        SyncObstacle3D(copy);
        obstacles_.push_back(copy);
    };

    auto addObstacle = [&](V2 pos, float radius, int kind, Color color)
    {
        Obstacle o{};
        o.pos = pos;
        o.radius = radius;
        o.kind = kind;
        o.hp = 140.0f;
        o.maxHp = 140.0f;
        o.color = color;
        o.bumper = kind == 1;
        o.reflectPower = kind == 1 ? 1.55f : 1.0f;
        push(o);
    };

    auto addBumper = [&](V2 pos, float radius, Color color)
    {
        addObstacle(pos, radius, 1, color);
    };

    // ダメージ床（シロップ沼/溶岩）。移動し、敵・ボスにも効く共有ハザード
    auto addHazard = [&](V2 pos, float radius, V2 vel, Color color)
    {
        Obstacle o{};
        o.pos = pos;
        o.radius = radius;
        o.kind = 3;
        o.color = color;
        o.damageField = true;
        o.moving = true;
        o.vel = vel;
        push(o);
    };

    if (stage_ == StageType::BossArena)
    {
        // ボス戦でも使える、動くダメージ床
        addHazard({ -3.0f, -1.5f }, 1.3f, { 1.6f, 0.9f }, Red);
        addHazard({ 3.2f, 1.8f }, 1.15f, { -1.1f, 1.4f }, Gold);
        return;
    }

    if (stage_ == StageType::Donut)
    {
        addObstacle({ 0.0f, 0.0f }, 1.85f, 0, Choco);
        // 中央島を公転するバンパーリング
        for (int i = 0; i < 4; ++i)
        {
            Obstacle o{};
            const float a = TwoPi * i / 4.0f;
            o.orbitCenter = { 0.0f, 0.0f };
            o.orbitRadius = 4.6f;
            o.orbitAngle = a;
            o.orbitSpeed = 0.55f;
            o.pos = FromAngle(a) * o.orbitRadius;
            o.radius = 0.5f;
            o.kind = 1;
            o.bumper = true;
            o.reflectPower = 1.55f;
            o.color = Gold;
            push(o);
        }
    }
    else if (stage_ == StageType::TwinIsland)
    {
        addObstacle({ -2.7f, 0.0f }, 1.25f, 0, Grape);
        addObstacle({ 2.7f, 0.0f }, 1.25f, 0, Grape);
    }
    else if (stage_ == StageType::Pinball)
    {
        // フリッパーのように往復スイングするバンパー
        for (int i = 0; i < 6; ++i)
        {
            const float base = TwoPi * i / 6.0f + 0.28f;
            Obstacle o{};
            o.orbitCenter = { 0.0f, 0.0f };
            o.orbitRadius = 4.1f;
            o.swingBase = base;
            o.swingAmp = 0.42f;            // 振れ幅
            o.orbitSpeed = 3.4f;           // スイング速度
            o.orbitAngle = base;
            o.pos = FromAngle(base) * o.orbitRadius;
            o.radius = 0.55f;
            o.kind = 1;
            o.bumper = true;
            o.flipper = true;
            o.reflectPower = 1.55f;
            o.color = Gold;
            push(o);
        }
        addBumper({ 0.0f, 0.0f }, 0.7f, Gold);
    }
    else if (stage_ == StageType::RingCorridor)
    {
        addObstacle({ 0.0f, 0.0f }, 2.55f, 0, Sky);
        for (int i = 0; i < 10; ++i)
        {
            if ((i % 5) == 0) continue;
            addObstacle(FromAngle(TwoPi * i / 10.0f) * 3.2f, 0.46f, 0, Sky);
        }
        // ワープポータルの対
        Obstacle a{};
        a.pos = { -6.4f, 0.0f };
        a.radius = 0.6f;
        a.kind = 5;
        a.warpId = 0;
        a.color = Sky;
        push(a);
        Obstacle b{};
        b.pos = { 6.4f, 0.0f };
        b.radius = 0.6f;
        b.kind = 5;
        b.warpId = 0;
        b.color = Berry;
        push(b);
    }
    else if (stage_ == StageType::FourPillars)
    {
        // 柱は破壊可能（壊すとアイテムドロップ）
        const V2 corners[4] = { { -4.1f, -4.1f }, { 4.1f, -4.1f }, { -4.1f, 4.1f }, { 4.1f, 4.1f } };
        for (const V2& c : corners)
        {
            Obstacle o{};
            o.pos = c;
            o.radius = 0.75f;
            o.kind = 0;
            o.hp = 5000.0f;
            o.maxHp = 5000.0f;
            o.breakable = true;
            o.color = Cream;
            push(o);
        }
        // 中央の動くダメージ床
        addHazard({ 0.0f, 0.0f }, 1.6f, { 1.2f, 0.8f }, Red);
    }
    else if (stage_ == StageType::MovingIsland)
    {
        for (int i = 0; i < 5; ++i)
        {
            Obstacle o{};
            const float a = TwoPi * i / 5.0f;
            o.pos = FromAngle(a) * 3.4f;
            o.radius = 0.62f;
            o.hp = 130.0f;
            o.maxHp = 130.0f;
            o.kind = 0;
            o.color = Mint;
            o.moving = true;
            o.vel = FromAngle(a + Pi * 0.5f) * 0.75f;
            push(o);
        }
    }
    else if (stage_ == StageType::ShrinkRing)
    {
        for (int i = 0; i < 8; ++i)
        {
            addBumper(FromAngle(TwoPi * i / 8.0f) * 5.5f, 0.42f, Red);
        }
        // 動くシロップのダメージ床
        for (int i = 0; i < 3; ++i)
        {
            const float a = TwoPi * i / 3.0f + 0.5f;
            addHazard(FromAngle(a) * 2.6f, 0.95f, FromAngle(a + Pi * 0.5f) * 1.3f, Red);
        }
    }
}

// ステージ固有ギミックの毎フレーム更新です。
// 動く島、縮むリング、ダメージ床など、敵とは別の環境変化を扱います。
void SweetsApp::UpdateStage(float dt)
{
    stageTimer_ += dt;
    for (auto& p : players_)
    {
        if (p.warpCd > 0.0f) p.warpCd -= dt;
    }
    if (stage_ == StageType::ShrinkRing)
    {
        shrinkRadius_ = std::max(5.4f, ArenaRadius - stageTimer_ * 0.11f);
        for (auto& p : players_)
        {
            if (!p.active || p.downed) continue;
            if (Len(p.pos) > shrinkRadius_ && p.inv <= 0.0f)
            {
                ResolvePlayerHit(p, 4.0f * dt, AngleOf(p.pos) + Pi);
            }
        }
    }

    for (auto& o : obstacles_)
    {
        if (o.ttl > 0.0f)
        {
            o.ttl -= dt;
        }
        if (o.flash > 0.0f)
        {
            o.flash = std::max(0.0f, o.flash - dt * 4.0f);
        }
        o.spin += dt * (o.bumper ? 2.4f : 1.0f);

        // 破壊可能オブジェ：HPが尽きたら衝撃波＋ドロップして消滅
        if (o.breakable && o.hp <= 0.0f && o.ttl < -0.5f)
        {
            Burst(o.pos, o.color, 60);
            SpawnPickupAt(o.pos);
            // 破壊の衝撃波：周囲の敵・ボスへ範囲ダメージ
            const float blastR = 3.4f;
            const float blastDmg = 400.0f;
            for (auto& e : enemies_)
            {
                if (!e.dead && RuleDistance(e.pos, e.height, o.pos, o.height) < blastR + e.radius)
                {
                    DamageEnemy(e, blastDmg, o.pos, 3.0f, true, 0);
                }
            }
            if (boss_.active && RuleDistance(boss_.pos, boss_.height, o.pos, o.height) < blastR + boss_.radius)
            {
                DamageBoss(blastDmg, true, 0);
            }
            o.ttl = -0.25f; // 既存の除去パスで回収させる
            continue;
        }

        // ワープポータル：触れた自機を対の穴へ転送（回避用、クールダウン付き）
        if (o.warpId >= 0)
        {
            for (auto& p : players_)
            {
                if (!p.active || p.downed || p.warpCd > 0.0f) continue;
                if (!RuleCircleHit(p.pos, PlayerBodyY, p.radius, o.pos, o.height, o.radius)) continue;
                const Obstacle* partner = nullptr;
                for (auto& q : obstacles_)
                {
                    if (&q != &o && q.warpId == o.warpId) { partner = &q; break; }
                }
                if (partner)
                {
                    V2 dir = Normalize(p.pos - o.pos);
                    if (LenSq(dir) < 0.001f) dir = { 1.0f, 0.0f };
                    p.pos = partner->pos + dir * (partner->radius + p.radius + 0.12f);
                    ClampInside(p.pos, p.radius);
                    p.warpCd = 0.7f;
                    p.inv = std::max(p.inv, 0.25f); // 抜けた直後の軽い無敵
                    Burst(o.pos, o.color, 16);
                    Burst(partner->pos, partner->color, 16);
                    SyncPlayer3D(p);
                }
            }
        }

        if (o.damageField)
        {
            for (auto& p : players_)
            {
                if (p.active && !p.downed && RuleCircleHit(p.pos, PlayerBodyY, p.radius, o.pos, o.height, o.radius))
                {
                    ResolvePlayerHit(p, 8.0f * dt, AngleOf(p.pos - o.pos));
                }
            }
            // 敵・ボスにも継続ダメージ（ボス戦の共有ハザード）
            for (auto& e : enemies_)
            {
                if (!e.dead && RuleCircleHit(e.pos, e.height, e.radius, o.pos, o.height, o.radius))
                {
                    DamageEnemy(e, 22.0f * dt, o.pos, 0.0f, false, 0);
                }
            }
            if (boss_.active && RuleCircleHit(boss_.pos, boss_.height, boss_.radius, o.pos, o.height, o.radius))
            {
                DamageBoss(30.0f * dt, false, 0);
            }
        }

        if (o.flipper)
        {
            // フリッパー：基準角を中心に往復スイング
            o.orbitAngle = o.swingBase + o.swingAmp * std::sin(stageTimer_ * o.orbitSpeed);
            o.pos = o.orbitCenter + FromAngle(o.orbitAngle) * o.orbitRadius;
            SyncObstacle3D(o);
        }
        else if (o.orbitSpeed != 0.0f)
        {
            o.orbitAngle += o.orbitSpeed * dt;
            o.pos = o.orbitCenter + FromAngle(o.orbitAngle) * o.orbitRadius;
            SyncObstacle3D(o);
        }
        else if (o.moving)
        {
            o.pos += o.vel * dt;
            const float maxR = ArenaRadius - o.radius - 0.2f;
            const float d = Len(o.pos);
            if (d > maxR && d > 0.0001f)
            {
                V2 n = Normalize(o.pos);
                o.pos = n * maxR;
                o.vel -= n * (2.0f * Dot(o.vel, n));
            }
            SyncObstacle3D(o);
        }
    }

    obstacles_.erase(std::remove_if(obstacles_.begin(), obstacles_.end(),
        [](const Obstacle& o) { return o.ttl <= 0.0f && o.ttl > -0.5f; }), obstacles_.end());
}
