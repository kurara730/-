#include "SweetsApp.h"
#include "ReflectionSystem.h"
#include "StageFactory.h"

#include <algorithm>

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

void SweetsApp::ReflectEnemyShotsNear(V2 center, float radius, int ownerIndex, CharacterType source, Color color, float power)
{
    int reflected = 0;
    for (auto& s : shots_)
    {
        if (!s.enemy || s.dead) continue;
        if (RuleDistance(s.pos, s.height, center, ShotBodyY) > radius + s.radius) continue;
        V2 n = s.pos - center;
        if (LenSq(n) < 0.001f)
        {
            const Player& p = players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))];
            n = FromAngle(p.face);
        }
        ApplyShotReflection(s, Normalize(n), power);
        s.enemy = false;
        s.ownerIndex = std::max(0, std::min(ownerIndex, MaxPlayers - 1));
        s.sourceCharacter = source;
        s.color = color;
        s.damage *= 1.18f;
        s.bounce = std::max(s.bounce, source == CharacterType::Roll ? 3 : 1);
        SyncShot3D(s);
        ++reflected;
        Burst(s.pos, color, 8);
    }
    if (reflected > 0)
    {
        audio_.PlaySoundEffect(SoundEffect::Reflect);
        AddScore(reflected * 24, &players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))]);
        message_ = L"反射";
        messageT_ = std::max(messageT_, 0.65f);
        effectPulses_.push_back({ center, Grounded3D(center, 0.24f), radius * 0.55f, radius * 1.55f, 0.22f, 0.22f, 0.24f, color });
    }
}

void SweetsApp::UpdateShots(float dt)
{
    for (auto& s : shots_)
    {
        if (s.dead) continue;
        s.ttl -= dt;
        if (s.ttl <= 0.0f) continue;

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
        if (!s.enemy)
        {
            if (s.sourceCharacter == CharacterType::Shortcake && s.charged)
            {
                ReflectEnemyShotsNear(s.pos, s.radius + 0.70f, s.ownerIndex, CharacterType::Shortcake, Berry, 1.15f);
            }
            else if (s.sourceCharacter == CharacterType::Roll)
            {
                ReflectEnemyShotsNear(s.pos, s.radius + (s.charged ? 0.58f : 0.34f), s.ownerIndex, CharacterType::Roll, Cream, 1.12f);
            }
        }
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

        for (const auto& o : obstacles_)
        {
            V2 d = s.pos - o.pos;
            const float l = RuleDistance(s.pos, s.height, o.pos, o.height);
            if (l < s.radius + o.radius)
            {
                V2 n = Normalize(d);
                s.pos = o.pos + n * (s.radius + o.radius + 0.01f);
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
            if (DamageHiddenBossCore(ReflectedDamage(s), s.pos, s.ownerIndex))
            {
                if (s.pierce > 0) --s.pierce;
                else s.dead = true;
            }
            if (s.dead) continue;

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
            if (!s.dead && !s.hitBoss && boss_.active && RuleDistance(s, boss_) < s.radius + boss_.radius)
            {
                if (s.charged && s.sourceCharacter == CharacterType::Shortcake && s.splitCount > 0) SpawnSplitShots(s, boss_.pos);
                BossDamageKind kind = BossDamageKind::NormalShot;
                if (s.reflected)
                {
                    kind = BossDamageKind::ReflectedShot;
                }
                else if (s.charged && s.sourceCharacter == CharacterType::Chocolate)
                {
                    kind = BossDamageKind::ChocolateCharge;
                }
                else if (s.charged)
                {
                    kind = BossDamageKind::ChargeShot;
                }
                DamageBoss(ReflectedDamage(s), kind, s.reflected, s.ownerIndex);
                s.hitBoss = true;
                if (s.pierce > 0) --s.pierce;
                else s.dead = true;
            }
        }
    }
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
                if (boss_.active) DamageBoss(260.0f + wave_ * 18.0f, BossDamageKind::Bomb, false, p.index);
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

