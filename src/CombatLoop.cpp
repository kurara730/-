#include "SweetsApp.h"
#include "ReflectionSystem.h"
#include "StageFactory.h"

namespace
{
bool IsEliteType(EnemyType type)
{
    return type == EnemyType::Healer
        || type == EnemyType::Barrier
        || type == EnemyType::Mirror
        || type == EnemyType::Teleport;
}
}

void SweetsApp::UpdateShots(float dt)
{
    std::vector<Shot> spawned; // 反射分裂などイテレーション中に増える弾を遅延追加
    for (auto& s : shots_)
    {
        if (s.dead) continue;
        s.ttl -= dt;
        if (s.ttl <= 0.0f) continue;
        const int reflectBefore = s.reflectedCount;
        if (s.enemy && (std::fabs(s.angularVel) > 0.0001f || std::fabs(s.accel) > 0.0001f))
        {
            float speed = Len(s.vel);
            float angle = AngleOf(s.vel) + s.angularVel * dt;
            speed = std::max(0.3f, speed + s.accel * dt);
            s.vel = FromAngle(angle) * speed;
        }
        else if (!s.enemy && s.homingStrength > 0.0f)
        {
            const V2 target = FindNearestEnemyOrBoss(s.pos);
            const V2 desired = Normalize(target - s.pos);
            if (LenSq(desired) > 0.001f)
            {
                const float speed = Len(s.vel);
                const V2 blended = Normalize(Normalize(s.vel) + desired * s.homingStrength * dt);
                s.vel = blended * speed;
            }
        }

        s.pos += s.vel * dt;
        if (Use3DRules() && s.enemy)
        {
            const float wave = std::sin(gameTime_ * 1.7f + s.pos.x * 0.37f + s.pos.z * 0.23f);
            s.height = ClampFloat(ShotBodyY + 0.26f * wave + 0.08f * static_cast<float>(s.reflectedCount), 0.16f, 1.15f);
        }
        SyncShot3D(s);
        const float dist = Len(s.pos);
        if (dist > ArenaRadius - s.radius)
        {
            if (s.bounce > 0)
            {
                V2 n = Normalize(s.pos);
                s.pos = n * (ArenaRadius - s.radius);
                ApplyShotReflection(s, n, 1.0f);
                --s.bounce;
            }
            else
            {
                s.dead = true;
                continue;
            }
        }

        for (auto& o : obstacles_)
        {
            V2 d = s.pos - o.pos;
            const float l = RuleDistance(s.pos, s.height, o.pos, o.height);
            if (l < s.radius + o.radius)
            {
                // ワープポータルは弾を通す（自機専用の回避ギミック）
                if (o.warpId >= 0)
                {
                    continue;
                }

                V2 n = Normalize(d);
                s.pos = o.pos + n * (s.radius + o.radius + 0.01f);

                // 破壊可能オブジェへのダメージ（プレイヤー弾のみ）
                if (o.breakable && !s.enemy)
                {
                    o.hp -= s.damage;
                    o.flash = 1.0f;
                }

                if (s.enemy && o.cheeseWall)
                {
                    ApplyShotReflection(s, n, std::max(1.15f, o.reflectPower));
                    s.enemy = false;
                    s.ownerIndex = std::max(0, o.ownerIndex);
                    s.sourceCharacter = CharacterType::Cheese;
                    s.color = Gold;
                    s.damage *= 1.25f;
                    s.bounce = std::max(s.bounce, 2);
                }
                else if (o.bumper)
                {
                    // バンパー：加速して大きく反射＋発光・スコア
                    ApplyShotReflection(s, n, std::max(1.55f, o.reflectPower));
                    const float sp = Len(s.vel);
                    s.vel = Normalize(s.vel) * std::min(sp * 1.3f + 1.5f, 26.0f);
                    o.flash = 1.0f;
                    Burst(s.pos, Gold, 10);
                    if (!s.enemy)
                    {
                        AddScore(12, &players_[std::max(0, std::min(s.ownerIndex, MaxPlayers - 1))]);
                        message_ = L"バンパー反射!";
                        messageT_ = std::max(messageT_, 0.6f);
                    }
                    if (s.bounce > 0) --s.bounce;
                }
                else if (s.bounce > 0 || s.enemy)
                {
                    ApplyShotReflection(s, n, o.reflectPower);
                    if (s.bounce > 0) --s.bounce;
                }
                else
                {
                    s.dead = true;
                }
                break;
            }
        }

        if (s.dead) continue;

        // ショート弾：反射した瞬間に小さな追尾弾へ分裂（面制圧）
        if (!s.enemy && s.reflectSplit > 0 && s.reflectedCount > reflectBefore)
        {
            const float baseAng = AngleOf(s.vel);
            const int n = s.reflectSplit;
            for (int i = 0; i < n; ++i)
            {
                const float a = baseAng + (static_cast<float>(i) / std::max(1, n - 1) - 0.5f) * 1.1f;
                Shot child{};
                child.pos = s.pos;
                child.vel = FromAngle(a) * 8.5f;
                child.radius = 0.11f;
                child.damage = s.damage * 0.42f;
                child.ttl = 1.4f;
                child.homingStrength = 1.6f;
                child.ownerIndex = s.ownerIndex;
                child.sourceCharacter = CharacterType::Shortcake;
                child.color = Berry;
                SyncShot3D(child);
                spawned.push_back(child);
            }
            Burst(s.pos, Berry, 12);
            s.reflectSplit = 0;
        }

        if (s.enemy)
        {
            for (auto& p : players_)
            {
                if (!p.active || p.downed) continue;
                const float hitDist = s.radius + p.hitboxRadius;
                const float grazeDist = s.radius + p.grazeRadius;
                const float d = RuleDistance(s, p);
                if (!s.grazed && d < grazeDist && d >= hitDist)
                {
                    s.grazed = true;
                    ++p.graze;
                    ++p.grazeChain;
                    p.grazeFlash = 0.18f;
                    p.ult = std::min(100.0f, p.ult + 0.45f);
                    AddScore(12 + std::min(p.grazeChain, 80), &p);
                }
                if (d < hitDist)
                {
                    ResolvePlayerHit(p, s.damage, AngleOf(p.pos - s.pos));
                    s.dead = true;
                    break;
                }
            }
        }
        else
        {
            for (auto& e : enemies_)
            {
                if (e.dead) continue;
                if (RuleDistance(s, e) < s.radius + e.radius)
                {
                    const bool wasChargedSplit = s.charged && s.sourceCharacter == CharacterType::Shortcake && s.splitCount > 0;
                    DamageEnemy(e, ReflectedDamage(s), s.pos, 1.0f, s.reflected, s.ownerIndex);
                    if (e.type == EnemyType::Mirror && !s.reflected)
                    {
                        Burst(e.pos, Sky, 10);
                    }
                    if (wasChargedSplit) SpawnSplitShots(s, e.pos);
                    if (s.pierce > 0) --s.pierce;
                    else s.dead = true;
                    break;
                }
            }
            if (!s.dead && boss_.active && RuleDistance(s, boss_) < s.radius + boss_.radius)
            {
                if (s.charged && s.sourceCharacter == CharacterType::Shortcake && s.splitCount > 0) SpawnSplitShots(s, boss_.pos);
                DamageBoss(ReflectedDamage(s), s.reflected, s.ownerIndex);
                if (s.pierce > 0) --s.pierce;
                else s.dead = true;
            }
        }
    }

    for (const auto& sp : spawned) shots_.push_back(sp);
}

void SweetsApp::UpdatePickups(float dt)
{
    for (auto& item : pickups_)
    {
        item.ttl -= dt;
        if (item.ttl <= 0.0f) continue;

        Player* taker = nullptr;
        float bestD = 99999.0f;
        for (auto& p : players_)
        {
            if (!p.active || p.downed) continue;
            const float d = RuleDistance(p, item);
            if (p.magnetT > 0.0f && d < 4.6f)
            {
                item.pos += Normalize(p.pos - item.pos) * (dt * 6.5f);
                SyncPickup3D(item);
            }
            if (d < item.radius + p.radius && d < bestD)
            {
                bestD = d;
                taker = &p;
            }
        }

        if (taker)
        {
            Player& p = *taker;
            switch (item.pickupType)
            {
            case PickupType::Heal:
                p.hp = std::min(p.maxHp, p.hp + 45.0f);
                message_ = L"回復アイテム";
                break;
            case PickupType::Attack:
                p.dmgBuffT = 8.0f;
                message_ = L"攻撃力アップ";
                break;
            case PickupType::Slow:
                slowT_ = 7.0f;
                message_ = L"敵スロー";
                break;
            case PickupType::Invincible:
                p.shieldT = 5.0f;
                p.inv = 2.0f;
                message_ = L"無敵";
                break;
            case PickupType::Magnet:
                p.magnetT = 10.0f;
                message_ = L"マグネット";
                break;
            case PickupType::BombDamage:
                for (auto& e : enemies_) DamageEnemy(e, 95.0f + wave_ * 6.0f, p.pos, 2.0f, false, p.index);
                if (boss_.active) DamageBoss(260.0f + wave_ * 18.0f, false, p.index);
                Burst(p.pos, Red, 60);
                message_ = L"爆発アイテム";
                break;
            case PickupType::UltFull:
                p.ult = 100.0f;
                message_ = L"必殺ゲージ満タン";
                break;
            case PickupType::Spread:
                p.spreadT = 10.0f;
                message_ = L"拡散ショット";
                break;
            case PickupType::Speed:
                p.speedBuffT = 8.0f;
                message_ = L"速度アップ";
                break;
            case PickupType::ScoreDouble:
                p.scoreDoubleT = 10.0f;
                message_ = L"スコア2倍";
                break;
            default:
                p.bombs = std::min(5, p.bombs + 1);
                p.inv = std::max(p.inv, 1.0f);
                message_ = L"ボム+1";
                break;
            }
            Burst(item.pos, item.color, 28);
            messageT_ = 2.0f;
            item.ttl = 0.0f;
        }
    }
}

