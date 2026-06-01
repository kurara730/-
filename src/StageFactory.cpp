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

    auto addObstacle = [&](V2 pos, float radius, int kind, Color color)
    {
        Obstacle o{};
        o.pos = pos;
        o.radius = radius;
        o.kind = kind;
        o.hp = 140.0f;
        o.color = color;
        o.reflectPower = kind == 1 ? 1.35f : 1.0f;
        SyncObstacle3D(o);
        obstacles_.push_back(o);
    };

    if (stage_ == StageType::BossArena)
    {
        return;
    }

    if (stage_ == StageType::Donut)
    {
        addObstacle({ 0.0f, 0.0f }, 1.85f, 0, Choco);
    }
    else if (stage_ == StageType::TwinIsland)
    {
        addObstacle({ -2.7f, 0.0f }, 1.25f, 0, Grape);
        addObstacle({ 2.7f, 0.0f }, 1.25f, 0, Grape);
    }
    else if (stage_ == StageType::Pinball)
    {
        for (int i = 0; i < 6; ++i)
        {
            const float a = TwoPi * i / 6.0f + 0.28f;
            addObstacle(FromAngle(a) * 4.1f, 0.55f, 1, Gold);
        }
    }
    else if (stage_ == StageType::RingCorridor)
    {
        addObstacle({ 0.0f, 0.0f }, 2.55f, 0, Sky);
        for (int i = 0; i < 10; ++i)
        {
            if ((i % 5) == 0) continue;
            addObstacle(FromAngle(TwoPi * i / 10.0f) * 3.2f, 0.46f, 0, Sky);
        }
    }
    else if (stage_ == StageType::FourPillars)
    {
        addObstacle({ -4.1f, -4.1f }, 0.75f, 0, Cream);
        addObstacle({ 4.1f, -4.1f }, 0.75f, 0, Cream);
        addObstacle({ -4.1f, 4.1f }, 0.75f, 0, Cream);
        addObstacle({ 4.1f, 4.1f }, 0.75f, 0, Cream);
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
            o.kind = 0;
            o.color = Mint;
            o.moving = true;
            o.vel = FromAngle(a + Pi * 0.5f) * 0.75f;
            SyncObstacle3D(o);
            obstacles_.push_back(o);
        }
    }
    else if (stage_ == StageType::ShrinkRing)
    {
        for (int i = 0; i < 8; ++i)
        {
            addObstacle(FromAngle(TwoPi * i / 8.0f) * 5.5f, 0.42f, 1, Red);
        }
    }
}

// ステージ固有ギミックの毎フレーム更新です。
// 動く島、縮むリング、ダメージ床など、敵とは別の環境変化を扱います。
void SweetsApp::UpdateStage(float dt)
{
    stageTimer_ += dt;
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
        if (o.damageField)
        {
            for (auto& p : players_)
            {
                if (p.active && !p.downed && RuleCircleHit(p.pos, PlayerBodyY, p.radius, o.pos, o.height, o.radius))
                {
                    ResolvePlayerHit(p, 8.0f * dt, AngleOf(p.pos - o.pos));
                }
            }
        }
        if (o.moving)
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
