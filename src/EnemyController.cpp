#include "SweetsApp.h"

namespace
{
bool IsEliteTypeLocal(EnemyType type)
{
    return type == EnemyType::Healer
        || type == EnemyType::Barrier
        || type == EnemyType::Mirror
        || type == EnemyType::Teleport;
}
}

void SweetsApp::SpawnEnemy()
{
    Enemy e{};
    const int maxKind = std::min(7, wave_ / 2 + 1);
    const EncounterProfile profile = CurrentEncounterProfile();
    const EncounterTuning& tuning = CurrentEncounterTuning();
    auto allowed = [&](EnemyType type)
    {
        return static_cast<int>(type) <= maxKind;
    };
    auto pickMobType = [&]()
    {
        const bool eliteCapped = EliteEnemyCount() >= tuning.mobEliteCap;
        const int roll = RandInt(0, 99);
        EnemyType type = EnemyType::Normal;
        if (roll < 42) type = EnemyType::Normal;
        else if (roll < 66) type = EnemyType::Split;
        else if (roll < 82) type = EnemyType::Mine;
        else if (roll < 90) type = EnemyType::Shield;
        else if (roll < 94) type = EnemyType::Healer;
        else if (roll < 97) type = EnemyType::Barrier;
        else if (roll < 99) type = EnemyType::Mirror;
        else type = EnemyType::Teleport;

        if (!allowed(type) || (eliteCapped && IsEliteTypeLocal(type)))
        {
            const int simple = RandInt(0, 2);
            type = simple == 0 ? EnemyType::Normal : (simple == 1 ? EnemyType::Split : EnemyType::Mine);
            if (!allowed(type)) type = EnemyType::Normal;
        }
        return type;
    };
    auto pickBossAddType = [&]()
    {
        const int roll = RandInt(0, 99);
        if (roll < 50) return EnemyType::Normal;
        if (roll < 78 && allowed(EnemyType::Split)) return EnemyType::Split;
        if (roll < 94 && allowed(EnemyType::Mine)) return EnemyType::Mine;
        return allowed(EnemyType::Shield) ? EnemyType::Shield : EnemyType::Normal;
    };

    e.type = profile == EncounterProfile::MobRelease ? pickMobType() : pickBossAddType();
    e.kind = static_cast<int>(e.type);
    e.pos = RandInArena(0.8f);
    if (Len(e.pos) < 7.0f)
    {
        e.pos = Normalize(e.pos) * Rand(7.0f, 9.2f);
    }

    switch (e.type)
    {
    case EnemyType::Normal:
        e.radius = 0.34f; e.hp = 18.0f + wave_ * 4.0f; e.speed = 2.55f; e.atk = 7.0f; e.score = 120; e.color = Rose;
        break;
    case EnemyType::Shield:
        e.radius = 0.38f; e.hp = 28.0f + wave_ * 5.0f; e.speed = 2.0f; e.atk = 9.0f; e.score = 180; e.color = Mint;
        break;
    case EnemyType::Split:
        e.radius = 0.40f; e.hp = 24.0f + wave_ * 5.0f; e.speed = 1.75f; e.atk = 7.0f; e.score = 240; e.color = Gold;
        break;
    case EnemyType::Healer:
        e.radius = 0.38f; e.hp = 30.0f + wave_ * 5.0f; e.speed = 1.65f; e.atk = 6.0f; e.score = 320; e.color = Mint; e.shootCd = Rand(0.7f, 1.5f);
        break;
    case EnemyType::Barrier:
        e.radius = 0.44f; e.hp = 44.0f + wave_ * 7.0f; e.speed = 1.45f; e.atk = 7.0f; e.score = 420; e.color = Sky; e.shootCd = Rand(0.5f, 1.3f);
        break;
    case EnemyType::Mirror:
        e.radius = 0.43f; e.hp = 48.0f + wave_ * 8.0f; e.speed = 1.55f; e.atk = 8.0f; e.score = 520; e.color = Cream; e.shootCd = Rand(0.4f, 1.0f);
        break;
    case EnemyType::Mine:
        e.radius = 0.42f; e.hp = 28.0f + wave_ * 6.0f; e.speed = 2.20f; e.atk = 16.0f; e.score = 360; e.color = Red;
        break;
    default:
        e.radius = 0.38f; e.hp = 36.0f + wave_ * 7.0f; e.speed = 1.85f; e.atk = 8.0f; e.score = 460; e.color = Grape; e.shootCd = Rand(0.4f, 1.0f); e.teleportCd = Rand(1.2f, 2.2f);
        break;
    }
    const DifficultyDef& diff = CurrentDifficulty();
    e.hp *= diff.enemyHpMul;
    e.atk *= diff.enemyAtkMul;
    if (profile == EncounterProfile::MobRelease)
    {
        e.hp *= tuning.mobHpMul;
        e.atk *= 0.88f;
        if (IsEliteTypeLocal(e.type))
        {
            e.shootCd += Rand(0.4f, 0.9f);
        }
    }
    else if (profile == EncounterProfile::BossSkillCheck)
    {
        e.hp *= 0.85f;
        e.atk *= 0.90f;
    }
    e.maxHp = e.hp;
    if (Use3DRules())
    {
        e.height = (e.type == EnemyType::Teleport || e.type == EnemyType::Mirror) ? 0.82f : EnemyBodyY;
    }
    e.uid = nextEnemyUid_++;
    SyncEnemy3D(e);
    enemies_.push_back(e);
}

void SweetsApp::SpawnBoss()
{
    boss_ = {};
    boss_.active = true;
    boss_.pos = { 0.0f, -1.2f };
    boss_.radius = 1.15f + 0.06f * static_cast<float>(wave_ / 3);
    const DifficultyDef& diff = CurrentDifficulty();
    boss_.maxHp = (720.0f + wave_ * 260.0f) * diff.bossHpMul;
    boss_.hp = boss_.maxHp;
    boss_.speed = 1.2f + wave_ * 0.035f;
    boss_.atk = (13.0f + wave_ * 1.3f) * diff.enemyAtkMul;
    boss_.attackCd = 1.7f;
    boss_.spin = Rand(0.0f, TwoPi);
    boss_.type = (wave_ / 3) % 6;
    boss_.bossType = static_cast<BossType>(boss_.type);
    boss_.phase = 1;
    SyncBoss3D(boss_);
}

void SweetsApp::UpdateEnemies(float dt)
{
    const float slowMul = slowT_ > 0.0f ? 0.45f : 1.0f;
    const float shotCooldownMul = CurrentEncounterProfile() == EncounterProfile::MobRelease
        ? CurrentEncounterTuning().mobShotCooldownMul
        : 1.0f;
    for (auto& e : enemies_)
    {
        if (e.dead) continue;
        if (e.flash > 0.0f) e.flash -= dt;
        if (e.touchCd > 0.0f) e.touchCd -= dt;
        if (e.barrierT > 0.0f) e.barrierT -= dt;
        if (e.yoyoHitCd > 0.0f) e.yoyoHitCd -= dt;
        if (e.caught) { SyncEnemy3D(e); continue; } // 巻き込まれ中は行動停止（弾に固定）

        Player* targetPlayer = FindNearestPlayer(e.pos);
        if (!targetPlayer) continue;
        V2 toP = targetPlayer->pos - e.pos;
        const float d = RuleDistance(e.pos, e.height, targetPlayer->pos, PlayerBodyY);
        const V2 n = Normalize(toP);
        e.face = AngleOf(toP);

        if (e.type == EnemyType::Healer)
        {
            e.shootCd -= dt * slowMul;
            e.vel = (d < 4.0f ? n * -1.0f : n * 0.25f) * e.speed * slowMul;
            if (e.shootCd <= 0.0f)
            {
                for (auto& other : enemies_)
                {
                    if (!other.dead && RuleDistance(other.pos, other.height, e.pos, e.height) < 3.6f)
                    {
                        other.hp = std::min(other.maxHp, other.hp + 12.0f + wave_ * 0.8f);
                        other.flash = 0.08f;
                    }
                }
                Burst(e.pos, Mint, 10);
                e.shootCd = 2.0f * CurrentDifficulty().spawnIntervalMul * (shotCooldownMul > 1.0f ? 1.15f : 1.0f);
            }
        }
        else if (e.type == EnemyType::Barrier)
        {
            e.shootCd -= dt * slowMul;
            e.vel = (d < 4.6f ? n * -1.0f : n * 0.20f) * e.speed * slowMul;
            for (auto& other : enemies_)
            {
                if (!other.dead && RuleDistance(other.pos, other.height, e.pos, e.height) < 3.1f)
                {
                    other.barrierT = std::max(other.barrierT, 0.20f);
                }
            }
            if (e.shootCd <= 0.0f)
            {
                const int bullets = ScaledBulletCount(8);
                for (int i = 0; i < bullets; ++i)
                {
                    const float a = e.face + TwoPi * i / static_cast<float>(bullets);
                    SpawnEnemyShot(e.pos + FromAngle(a) * (e.radius + 0.15f), a, 3.1f + wave_ * 0.08f, e.atk * 0.70f, 0.085f, Sky, 5.0f);
                }
                e.shootCd = 1.8f * CurrentDifficulty().spawnIntervalMul * shotCooldownMul;
            }
        }
        else if (e.type == EnemyType::Mirror || e.type == EnemyType::Teleport)
        {
            e.shootCd -= dt * slowMul;
            e.teleportCd -= dt * slowMul;
            V2 move{};
            if (d > 5.0f) move = n;
            else if (d < 3.4f) move = n * -1.0f;
            e.vel = move * e.speed * slowMul;
            if (e.type == EnemyType::Teleport && e.teleportCd <= 0.0f)
            {
                e.pos = targetPlayer->pos + FromAngle(Rand(0.0f, TwoPi)) * Rand(3.8f, 6.8f);
                ClampInside(e.pos, e.radius);
                SyncEnemy3D(e);
                Burst(e.pos, Grape, 18);
                e.teleportCd = Rand(2.0f, 3.2f);
            }
            if (e.shootCd <= 0.0f)
            {
                const int count = ScaledBulletCount(e.type == EnemyType::Teleport ? std::min(7, 3 + wave_ / 3) : std::min(5, 1 + wave_ / 4));
                const float base = AngleOf(toP);
                for (int i = 0; i < count; ++i)
                {
                    const float fan = e.type == EnemyType::Teleport ? 0.50f : 0.28f;
                    const float a = base + (count > 1 ? (static_cast<float>(i) / (count - 1) - 0.5f) * fan : 0.0f);
                    const float curve = e.type == EnemyType::Teleport ? ((i % 2 == 0) ? 0.35f : -0.35f) : 0.0f;
                    SpawnEnemyShot(e.pos + FromAngle(a) * (e.radius + 0.2f), a, 4.8f + wave_ * 0.16f, e.atk, 0.105f, e.type == EnemyType::Teleport ? Grape : Cream, 5.2f, curve, 0.08f);
                }
                e.shootCd = (e.type == EnemyType::Teleport ? 1.45f : 1.05f) * CurrentDifficulty().spawnIntervalMul * shotCooldownMul;
            }
        }
        else if (e.type == EnemyType::Mine)
        {
            e.vel = n * e.speed * slowMul;
        }
        else
        {
            float hop = e.type == EnemyType::Shield ? (1.0f + 0.35f * std::sin(gameTime_ * 8.0f + e.pos.x)) : 1.0f;
            e.vel = n * e.speed * hop * slowMul;
        }

        e.pos += e.vel * dt;
        ClampInside(e.pos, e.radius);
        SyncEnemy3D(e);

        for (const auto& o : obstacles_)
        {
            V2 push = e.pos - o.pos;
            const float l = RuleDistance(e.pos, e.height, o.pos, o.height);
            const float minD = e.radius + o.radius;
            if (l > 0.0001f && l < minD)
            {
                e.pos = o.pos + Normalize(push) * minD;
                SyncEnemy3D(e);
            }
        }

        for (auto& p : players_)
        {
            if (!p.active || p.downed || p.dashT <= 0.0f) continue;
            if (RuleDistance(p, e) < p.radius + e.radius + 0.1f)
            {
                DamageEnemy(e, 58.0f + p.level * 8.0f, p.pos, 2.5f, true, p.index);
            }
        }

        if (!e.dead && d < e.radius + targetPlayer->radius + 0.05f && e.touchCd <= 0.0f)
        {
            if (e.type == EnemyType::Mine)
            {
                for (auto& p : players_)
                {
                    if (p.active && !p.downed && RuleDistance(p, e) < 2.2f)
                    {
                        ResolvePlayerHit(p, e.atk * 2.2f, AngleOf(p.pos - e.pos));
                    }
                }
                Burst(e.pos, Red, 22);
                e.dead = true;
            }
            else
            {
                ResolvePlayerHit(*targetPlayer, e.atk, AngleOf(toP));
                e.touchCd = 0.65f;
            }
        }
    }
}

void SweetsApp::UpdateBoss(float dt)
{
    if (!boss_.active) return;

    boss_.spin += dt * (1.0f + wave_ * 0.03f);
    if (boss_.flash > 0.0f) boss_.flash -= dt;

    Player* targetPlayer = FindNearestPlayer(boss_.pos);
    if (!targetPlayer) return;
    const V2 toP = targetPlayer->pos - boss_.pos;
    const float d = RuleDistance(boss_.pos, boss_.height, targetPlayer->pos, PlayerBodyY);
    const V2 n = Normalize(toP);
    boss_.vel = n * boss_.speed * (slowT_ > 0.0f ? 0.55f : 1.0f);
    boss_.pos += boss_.vel * dt;
    ClampInside(boss_.pos, boss_.radius);
    SyncBoss3D(boss_);

    if (boss_.bossType == BossType::GravityPudding)
    {
        for (auto& p : players_)
        {
            if (!p.active || p.downed) continue;
            const V2 pull = boss_.pos - p.pos;
            p.pos += Normalize(pull) * dt * (Len(pull) < 6.5f ? 1.1f : 0.25f);
            SyncPlayer3D(p);
        }
    }

    if (d < boss_.radius + targetPlayer->radius && targetPlayer->inv <= 0.0f)
    {
        ResolvePlayerHit(*targetPlayer, boss_.atk, AngleOf(toP));
    }

    const EncounterTuning& tuning = CurrentEncounterTuning();
    auto fireBossAttack = [&]()
    {
        const int attack = std::max(0, boss_.telegraphAttack);
        if (boss_.telegraphAdd && BossAddCount() < tuning.bossAddCap)
        {
            SpawnEnemy();
        }
        if (boss_.telegraphMirror && BossAddCount() < tuning.bossAddCap)
        {
            Enemy mirror{};
            mirror.type = EnemyType::Mirror;
            mirror.kind = static_cast<int>(mirror.type);
            mirror.pos = boss_.pos + FromAngle(Rand(0.0f, TwoPi)) * 2.2f;
            ClampInside(mirror.pos, 0.5f);
            mirror.height = Use3DRules() ? 0.82f : EnemyBodyY;
            mirror.radius = 0.38f;
            mirror.hp = (28.0f + wave_ * 6.0f) * CurrentDifficulty().enemyHpMul;
            mirror.maxHp = mirror.hp;
            mirror.speed = 1.5f;
            mirror.atk = boss_.atk * 0.5f;
            mirror.score = 350;
            mirror.color = Cream;
            mirror.shootCd = 1.1f;
            mirror.uid = nextEnemyUid_++;
            SyncEnemy3D(mirror);
            enemies_.push_back(mirror);
        }
        if (boss_.telegraphField)
        {
            Obstacle field{};
            field.pos = targetPlayer->pos + FromAngle(Rand(0.0f, TwoPi)) * Rand(0.5f, 2.0f);
            ClampInside(field.pos, 1.0f);
            field.radius = 0.70f + 0.08f * boss_.phase;
            field.ttl = 3.0f;
            field.color = Red;
            field.damageField = true;
            SyncObstacle3D(field);
            obstacles_.push_back(field);
        }
        if (attack == 0)
        {
            const int count = ScaledBulletCount(12 + boss_.phase * 4);
            const float base = boss_.spin;
            for (int i = 0; i < count; ++i)
            {
                const float a = base + TwoPi * i / count;
                const float curve = ((i & 1) ? -0.08f : 0.08f) * boss_.phase;
                SpawnEnemyShot(boss_.pos + FromAngle(a) * (boss_.radius + 0.15f), a, 2.9f + boss_.phase * 0.35f, boss_.atk * 0.62f, 0.085f, boss_.type == 1 ? Sky : Grape, 5.2f, curve, 0.03f);
            }
        }
        else if (attack == 1)
        {
            const float base = AngleOf(toP);
            const int lanes = std::max(2, ScaledBulletCount(3 + boss_.phase * 2));
            for (int i = 0; i < lanes; ++i)
            {
                const float a = base + (static_cast<float>(i) / (lanes - 1) - 0.5f) * 0.58f;
                SpawnEnemyShot(boss_.pos + FromAngle(a) * (boss_.radius + 0.2f), a, 3.8f + boss_.phase * 0.42f, boss_.atk * 0.85f, 0.095f, Red, 3.8f, 0.0f, -0.05f);
            }
        }
        else if (attack == 2)
        {
            const int count = ScaledBulletCount(10 + boss_.phase * 3);
            for (int i = 0; i < count; ++i)
            {
                const float a = boss_.spin + i * 0.62f;
                const float speed = 2.3f + 0.05f * i + boss_.phase * 0.18f;
                const float curve = (boss_.type % 2 == 0 ? 0.18f : -0.18f);
                SpawnEnemyShot(boss_.pos + FromAngle(a) * (boss_.radius + 0.15f), a, speed, boss_.atk * 0.60f, 0.082f, Gold, 5.6f, curve, 0.05f);
            }
        }
        else
        {
            const int petals = ScaledBulletCount(6 + boss_.phase * 2);
            for (int i = 0; i < petals; ++i)
            {
                const float a = -boss_.spin * 0.65f + TwoPi * i / petals;
                SpawnEnemyShot(boss_.pos + FromAngle(a) * (boss_.radius + 0.2f), a, 2.0f + boss_.phase * 0.18f, boss_.atk * 0.45f, 0.072f, Mint, 5.5f, -0.10f, 0.06f);
            }
        }

        boss_.telegraphAttack = -1;
        boss_.telegraphAdd = false;
        boss_.telegraphMirror = false;
        boss_.telegraphField = false;
        boss_.attackCd = std::max(0.95f, (2.15f - boss_.phase * 0.12f - wave_ * 0.008f) * CurrentDifficulty().spawnIntervalMul * tuning.bossShotRestMul);
    };

    if (boss_.telegraphT > 0.0f)
    {
        boss_.telegraphT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        if (boss_.telegraphT <= 0.0f)
        {
            fireBossAttack();
        }
        return;
    }

    boss_.attackCd -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
    if (boss_.attackCd <= 0.0f)
    {
        const int attack = RandInt(0, boss_.phase >= 3 ? 3 : 2);
        const bool addBoss = boss_.bossType == BossType::DonutKing || boss_.bossType == BossType::DemonParfait;
        const bool mirrorBoss = boss_.bossType == BossType::MirrorMacaron || boss_.bossType == BossType::DemonParfait;
        const bool territoryBoss = boss_.bossType == BossType::TerritoryCake || boss_.bossType == BossType::DemonParfait;
        boss_.telegraphAttack = attack;
        boss_.telegraphAdd = addBoss && BossAddCount() < tuning.bossAddCap;
        boss_.telegraphMirror = mirrorBoss && BossAddCount() < tuning.bossAddCap && Rand(0.0f, 1.0f) < 0.25f;
        boss_.telegraphField = territoryBoss && Rand(0.0f, 1.0f) < 0.22f;
        boss_.telegraphLife = tuning.bossTelegraphTime;
        boss_.telegraphT = tuning.bossTelegraphTime;
        boss_.flash = std::max(boss_.flash, tuning.bossTelegraphTime);
        EffectPulse pulse{};
        pulse.pos = boss_.pos;
        pulse.startRadius = boss_.radius * 1.1f;
        pulse.endRadius = boss_.radius * (2.6f + 0.2f * boss_.phase);
        pulse.ttl = tuning.bossTelegraphTime;
        pulse.life = tuning.bossTelegraphTime;
        pulse.y = 0.22f;
        pulse.color = attack == 0 ? Grape : (attack == 1 ? Red : (attack == 2 ? Gold : Mint));
        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
        effectPulses_.push_back(pulse);
        switch (attack)
        {
        case 0: message_ = L"ボス予兆: 放射弾"; break;
        case 1: message_ = L"ボス予兆: 狙い撃ち"; break;
        case 2: message_ = L"ボス予兆: 回転弾"; break;
        default: message_ = L"ボス予兆: 曲がる弾"; break;
        }
        if (boss_.telegraphAdd || boss_.telegraphMirror) message_ += L" + 召喚";
        if (boss_.telegraphField) message_ += L" + 領域";
        messageT_ = tuning.bossTelegraphTime + 0.55f;
        if (boss_.telegraphT <= 0.0f)
        {
            fireBossAttack();
        }
    }
}

void SweetsApp::DamageEnemy(Enemy& e, float dmg, V2 from, float knock)
{
    DamageEnemy(e, dmg, from, knock, false, 0);
}

void SweetsApp::DamageEnemy(Enemy& e, float dmg, V2 from, float knock, bool reflected, int ownerIndex)
{
    const float incoming = dmg;
    if (e.type == EnemyType::Mirror && !reflected)
    {
        dmg *= 0.22f;
    }
    if (e.type == EnemyType::Shield)
    {
        const float front = Dot(FromAngle(e.face), Normalize(from - e.pos));
        if (front > 0.25f) dmg = std::max(dmg * 0.35f, incoming * 0.12f);
    }
    if (e.barrierT > 0.0f)
    {
        dmg = std::max(dmg * 0.30f, incoming * 0.10f);
    }
    dmg = std::max(dmg, 1.0f);

    e.hp -= dmg;
    e.flash = 0.12f;
    V2 push = Normalize(e.pos - from);
    e.pos += push * (0.08f * knock);
    ClampInside(e.pos, e.radius);
    SyncEnemy3D(e);
    if (e.hp <= 0.0f && !e.dead)
    {
        e.dead = true;
        Player& owner = players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))];
        AddScore(e.score, &owner);
        if (reflected)
        {
            AddScore(e.score, &owner);
            ++reflectKills_;
            message_ = L"反射キル x2";
            messageT_ = 1.2f;
        }
        owner.kills++;
        const bool mobRelease = CurrentEncounterProfile() == EncounterProfile::MobRelease;
        owner.ult = std::min(100.0f, owner.ult + (mobRelease ? 8.5f : 5.0f));
        owner.xp += mobRelease ? 2 : 1;
        owner.fever = std::min(100.0f, owner.fever + (mobRelease ? 14.0f : 7.0f));
        if (owner.fever >= 100.0f)
        {
            owner.feverT = 8.0f;
        }
        if (owner.xp >= 7 + owner.level * 5)
        {
            owner.xp = 0;
            owner.level = std::min(5, owner.level + 1);
            owner.maxHp += 18.0f;
            owner.hp = std::min(owner.maxHp, owner.hp + 18.0f);
            message_ = L"レベルアップ";
            messageT_ = 1.8f;
            Burst(owner.pos, Gold, 34);
        }
        if (e.type == EnemyType::Split)
        {
            for (int i = 0; i < 2; ++i)
            {
                Enemy child{};
                child.type = EnemyType::Normal;
                child.kind = static_cast<int>(child.type);
                child.pos = e.pos + FromAngle(Rand(0.0f, TwoPi)) * 0.45f;
                child.radius = 0.25f;
                child.hp = 12.0f + wave_ * 2.0f;
                child.maxHp = child.hp;
                child.speed = 2.8f;
                child.atk = 5.0f;
                child.score = 70;
                child.color = Gold;
                child.uid = nextEnemyUid_++;
                SyncEnemy3D(child);
                enemies_.push_back(child);
            }
        }
        if (e.type == EnemyType::Mine)
        {
            for (auto& p : players_)
            {
                if (p.active && !p.downed && RuleDistance(p, e) < 2.0f)
                {
                    ResolvePlayerHit(p, e.atk * 1.3f, AngleOf(p.pos - e.pos));
                }
            }
        }
        Burst(e.pos, e.color, 18);
    }
}

void SweetsApp::DamageBoss(float dmg)
{
    DamageBoss(dmg, false, 0);
}

void SweetsApp::DamageBoss(float dmg, bool reflected, int ownerIndex)
{
    if (!boss_.active) return;
    if (boss_.bossType == BossType::HiddenBoss)
    {
        boss_.flash = 0.08f;
        AddScore(reflected ? 18 : 9, &players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))]);
        return;
    }
    boss_.hp -= dmg;
    boss_.flash = 0.15f;
    const float hpPct = boss_.hp / boss_.maxHp;
    const int nextPhase = hpPct < 0.25f ? 4 : (hpPct < 0.50f ? 3 : (hpPct < 0.75f ? 2 : 1));
    if (nextPhase > boss_.phase)
    {
        boss_.phase = nextPhase;
        boss_.attackCd = 0.9f;
        boss_.telegraphT = 0.0f;
        boss_.telegraphAttack = -1;
        boss_.telegraphAdd = false;
        boss_.telegraphMirror = false;
        boss_.telegraphField = false;
        Burst(boss_.pos, Red, 50);
    }
    if (boss_.hp <= 0.0f)
    {
        Player& owner = players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))];
        AddScore(6000 + wave_ * 750 + (reflected ? 1500 : 0), &owner);
        for (auto& p : players_)
        {
            if (p.active)
            {
                p.ult = std::min(100.0f, p.ult + 35.0f);
                if (!p.downed)
                {
                    p.hp = std::min(p.maxHp, p.hp + 35.0f);
                    p.fever = 100.0f;
                    p.feverT = std::max(p.feverT, 5.0f);
                }
            }
        }
        Burst(boss_.pos, Gold, 90);
        boss_.active = false;
    }
}
