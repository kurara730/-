#include "SweetsApp.h"

namespace
{
// 特殊敵かどうかの判定です。
// 特殊敵は画面を支配しやすいので、通常Waveでは同時数を制限します。
bool IsEliteTypeLocal(EnemyType type)
{
    return type == EnemyType::Healer
        || type == EnemyType::Barrier
        || type == EnemyType::Mirror
        || type == EnemyType::Teleport;
}

// 隠しボス専用のダメージ補正です。
// チャージ攻撃や必殺が強すぎてゲージを一瞬で飛ばさないよう、攻撃種別で調整します。
float HiddenBossDamageMultiplier(BossDamageKind kind)
{
    switch (kind)
    {
    case BossDamageKind::ChargeShot: return 0.45f;
    case BossDamageKind::ChocolateCharge: return 0.35f;
    case BossDamageKind::Melee: return 0.75f;
    case BossDamageKind::Bomb: return 0.65f;
    case BossDamageKind::Ultimate: return 0.70f;
    case BossDamageKind::ReflectedShot: return 1.10f;
    case BossDamageKind::NormalShot:
    default: return 1.0f;
    }
}

// 1ヒットで削れる最大量です。
// 貫通弾や大技が多段ヒットした時でも、ボス戦として成立するHPの残り方にします。
float HiddenBossHitCap(float gaugeHp, BossDamageKind kind)
{
    switch (kind)
    {
    case BossDamageKind::Bomb:
    case BossDamageKind::Ultimate:
        return gaugeHp * 0.10f;
    case BossDamageKind::ReflectedShot:
        return gaugeHp * 0.07f;
    case BossDamageKind::NormalShot:
    case BossDamageKind::ChargeShot:
    case BossDamageKind::ChocolateCharge:
    case BossDamageKind::Melee:
    default:
        return gaugeHp * 0.045f;
    }
}

bool IsChargeBossDamage(BossDamageKind kind)
{
    return kind == BossDamageKind::ChargeShot || kind == BossDamageKind::ChocolateCharge;
}

float NormalBossHitCap(float maxHp, BossDamageKind kind, bool reflected)
{
    if (reflected || kind == BossDamageKind::ReflectedShot) return maxHp * 0.14f;
    switch (kind)
    {
    case BossDamageKind::Bomb:
    case BossDamageKind::Ultimate:
        return maxHp * 0.12f;
    case BossDamageKind::ChargeShot:
        return maxHp * 0.065f;
    case BossDamageKind::ChocolateCharge:
        return maxHp * 0.050f;
    case BossDamageKind::Melee:
        return maxHp * 0.045f;
    case BossDamageKind::NormalShot:
    default:
        return maxHp * 0.070f;
    }
}

BossPatternId PatternForBoss(BossType type, int step)
{
    switch (type)
    {
    case BossType::Demon:
    {
        constexpr std::array<BossPatternId, 4> seq{ BossPatternId::Seal, BossPatternId::Radial, BossPatternId::Aimed, BossPatternId::Curve };
        return seq[step % static_cast<int>(seq.size())];
    }
    case BossType::DonutKing:
    {
        constexpr std::array<BossPatternId, 4> seq{ BossPatternId::GuardRing, BossPatternId::Spiral, BossPatternId::Aimed, BossPatternId::Radial };
        return seq[step % static_cast<int>(seq.size())];
    }
    case BossType::MirrorMacaron:
    {
        constexpr std::array<BossPatternId, 4> seq{ BossPatternId::MirrorSplit, BossPatternId::Curve, BossPatternId::Aimed, BossPatternId::Spiral };
        return seq[step % static_cast<int>(seq.size())];
    }
    case BossType::GravityPudding:
    {
        constexpr std::array<BossPatternId, 4> seq{ BossPatternId::GravityWell, BossPatternId::Radial, BossPatternId::Curve, BossPatternId::Aimed };
        return seq[step % static_cast<int>(seq.size())];
    }
    case BossType::TerritoryCake:
    {
        constexpr std::array<BossPatternId, 4> seq{ BossPatternId::TerritoryZone, BossPatternId::Aimed, BossPatternId::Radial, BossPatternId::Curve };
        return seq[step % static_cast<int>(seq.size())];
    }
    case BossType::DemonParfait:
    {
        constexpr std::array<BossPatternId, 6> seq{ BossPatternId::Seal, BossPatternId::GuardRing, BossPatternId::MirrorSplit, BossPatternId::GravityWell, BossPatternId::TerritoryZone, BossPatternId::Spiral };
        return seq[step % static_cast<int>(seq.size())];
    }
    default:
        return static_cast<BossPatternId>(step % 4);
    }
}

int AttackIndexForPattern(BossPatternId pattern)
{
    switch (pattern)
    {
    case BossPatternId::Aimed:
    case BossPatternId::GuardRing:
    case BossPatternId::MirrorSplit:
        return 1;
    case BossPatternId::Spiral:
    case BossPatternId::Seal:
        return 2;
    case BossPatternId::Curve:
    case BossPatternId::GravityWell:
    case BossPatternId::TerritoryZone:
        return 3;
    case BossPatternId::Radial:
    default:
        return 0;
    }
}

Color PatternTelegraphColor(BossPatternId pattern)
{
    switch (pattern)
    {
    case BossPatternId::Seal: return Grape;
    case BossPatternId::GuardRing: return Gold;
    case BossPatternId::MirrorSplit: return Cream;
    case BossPatternId::GravityWell: return Sky;
    case BossPatternId::TerritoryZone: return Mint;
    case BossPatternId::Aimed: return Red;
    case BossPatternId::Spiral: return Gold;
    case BossPatternId::Curve: return Mint;
    case BossPatternId::Radial:
    default: return Grape;
    }
}
}

// 雑魚敵を1体生成します。
// 通常Waveは倒しやすい敵を多め、ボスWaveの追加敵は少なめにして役割を分けています。
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
    // 雑魚戦は「薙ぎ倒す解放感」が目的なので、Normal/Split/Mineを多めに抽選します。
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
    // ボス戦の追加敵は、弾幕の読み合いを邪魔しないよう軽めの敵に寄せます。
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
    // 最終HPは、敵の基礎HP × 難易度 × 協力人数 × 戦闘目的補正で決まります。
    const DifficultyDef& diff = CurrentDifficulty();
    e.hp *= diff.enemyHpMul;
    e.hp *= MultiplayerHpMultiplier();
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
    e.id = ++enemySerial_;
    if (Use3DRules())
    {
        e.height = (e.type == EnemyType::Teleport || e.type == EnemyType::Mirror) ? 0.82f : EnemyBodyY;
    }
    SyncEnemy3D(e);
    enemies_.push_back(e);
}

int SweetsApp::SpawnEnemyFormation()
{
    const int pattern = RandInt(0, 4);
    const int count = pattern == 4 ? 5 : RandInt(3, 6);
    const size_t start = enemies_.size();
    for (int i = 0; i < count; ++i)
    {
        SpawnEnemy();
    }

    const float side = Rand(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
    const float lane = Rand(-4.4f, 4.4f);
    const V2 center = pattern == 2 ? RandInArena(2.2f) : V2{ side * 8.7f, lane };
    const float face = AngleOf({ -side, -0.15f * lane });
    for (int i = 0; i < count; ++i)
    {
        Enemy& e = enemies_[start + static_cast<size_t>(i)];
        V2 offset{};
        if (pattern == 0)
        {
            offset = { 0.0f, (static_cast<float>(i) - (count - 1) * 0.5f) * 0.72f };
        }
        else if (pattern == 1)
        {
            const float row = static_cast<float>(i);
            offset = { -side * row * 0.42f, (row - (count - 1) * 0.5f) * 0.54f };
        }
        else if (pattern == 2)
        {
            offset = FromAngle(TwoPi * i / static_cast<float>(count)) * 1.25f;
        }
        else if (pattern == 3)
        {
            const float row = static_cast<float>(i);
            offset = { -side * row * 0.58f, std::sin(row * 1.2f) * 0.95f };
        }
        else
        {
            const float a = i == 0 ? 0.0f : (TwoPi * (i - 1) / static_cast<float>(count - 1));
            offset = i == 0 ? V2{} : FromAngle(a) * 1.05f;
            if (i == 0)
            {
                e.type = EnemyType::Shield;
                e.kind = static_cast<int>(e.type);
                e.hp *= 1.25f;
                e.maxHp = e.hp;
                e.color = Mint;
            }
        }
        e.pos = center + offset;
        e.face = face;
        ClampInside(e.pos, e.radius);
        SyncEnemy3D(e);
    }
    message_ = L"編隊出現";
    messageT_ = std::max(messageT_, 0.8f);
    return count;
}

// 通常ボスを生成します。
// HPは EnemyController.cpp のこの式が入口で、難易度補正と協力人数補正もここで掛かります。
void SweetsApp::SpawnBoss()
{
    boss_ = {};
    boss_.active = true;
    boss_.pos = { 0.0f, -1.2f };
    boss_.radius = 1.15f + 0.06f * static_cast<float>(wave_ / 3);
    const DifficultyDef& diff = CurrentDifficulty();
    boss_.maxHp = (1200.0f + wave_ * 420.0f) * diff.bossHpMul * MultiplayerHpMultiplier();
    boss_.hp = boss_.maxHp;
    boss_.speed = 1.2f + wave_ * 0.035f;
    boss_.atk = (13.0f + wave_ * 1.3f) * diff.enemyAtkMul;
    boss_.attackCd = 1.25f;
    boss_.spin = Rand(0.0f, TwoPi);
    boss_.type = (wave_ / 3) % 6;
    boss_.bossType = static_cast<BossType>(boss_.type);
    boss_.phase = 1;
    bossGimmick_ = {};
    bossGimmick_.type = boss_.bossType;
    bossGimmick_.nextPattern = PatternForBoss(boss_.bossType, 0);
    bossGimmick_.guardAngle = boss_.spin;
    SyncBoss3D(boss_);
}

// 雑魚敵全体のAI更新です。
// 敵の種類ごとに移動、射撃、回復、バリア、テレポートなどの役割を分岐します。
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
        if (e.caught) { SyncEnemy3D(e); continue; } // 巻き込まれ中は行動停止（弾に固定）

        // 敵は最も近い生存プレイヤーを狙います。協力プレイでもターゲットが分散します。
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

// 通常ボスの移動、予兆、弾幕を更新します。
// 攻撃前に telegraphT を挟むことで、初見でも次の攻撃を読めるようにしています。
void SweetsApp::UpdateBoss(float dt)
{
    if (!boss_.active) return;

    boss_.spin += dt * (1.0f + wave_ * 0.03f);
    if (boss_.flash > 0.0f) boss_.flash -= dt;
    bossGimmick_.timer += dt;
    bossGimmick_.guardAngle += dt * (0.95f + boss_.phase * 0.18f);
    const bool wasVulnerable = bossGimmick_.vulnerableT > 0.0f;
    if (bossGimmick_.vulnerableT > 0.0f) bossGimmick_.vulnerableT = std::max(0.0f, bossGimmick_.vulnerableT - dt);
    if (wasVulnerable && bossGimmick_.vulnerableT <= 0.0f)
    {
        bossGimmick_.mirrorOpen = false;
    }
    if (bossGimmick_.gravityT > 0.0f) bossGimmick_.gravityT = std::max(0.0f, bossGimmick_.gravityT - dt);
    if (bossGimmick_.territoryT > 0.0f) bossGimmick_.territoryT = std::max(0.0f, bossGimmick_.territoryT - dt);

    Player* targetPlayer = FindNearestPlayer(boss_.pos);
    if (!targetPlayer) return;
    const V2 toP = targetPlayer->pos - boss_.pos;
    const float d = RuleDistance(boss_.pos, boss_.height, targetPlayer->pos, PlayerBodyY);
    const V2 n = Normalize(toP);
    boss_.vel = n * boss_.speed * (slowT_ > 0.0f ? 0.55f : 1.0f);
    boss_.pos += boss_.vel * dt;
    ClampInside(boss_.pos, boss_.radius);
    SyncBoss3D(boss_);

    if (boss_.bossType == BossType::GravityPudding || boss_.bossType == BossType::DemonParfait)
    {
        for (auto& p : players_)
        {
            if (!p.active || p.downed) continue;
            const V2 pull = boss_.pos - p.pos;
            const float gravityMul = bossGimmick_.gravityT > 0.0f ? 1.55f : 1.0f;
            p.pos += Normalize(pull) * dt * gravityMul * (Len(pull) < 7.4f ? 1.1f : 0.25f);
            SyncPlayer3D(p);
        }
    }

    if (d < boss_.radius + targetPlayer->radius && targetPlayer->inv <= 0.0f)
    {
        ResolvePlayerHit(*targetPlayer, boss_.atk, AngleOf(toP));
    }

    const EncounterTuning& tuning = CurrentEncounterTuning();
    // 予兆が終わった瞬間に実際の攻撃を出すラムダです。
    // 召喚、フィールド設置、弾幕パターンをここへ集約しています。
    auto fireBossAttack = [&]()
    {
        const BossPatternId pattern = bossGimmick_.nextPattern;
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
            const float mirrorAngle = boss_.spin + 0.75f * static_cast<float>(bossGimmick_.patternStep);
            mirror.pos = boss_.pos + FromAngle(mirrorAngle) * 2.2f;
            ClampInside(mirror.pos, 0.5f);
            mirror.height = Use3DRules() ? 0.82f : EnemyBodyY;
            mirror.radius = 0.38f;
            mirror.hp = (28.0f + wave_ * 6.0f) * CurrentDifficulty().enemyHpMul * MultiplayerHpMultiplier();
            mirror.maxHp = mirror.hp;
            mirror.speed = 1.5f;
            mirror.atk = boss_.atk * 0.5f;
            mirror.score = 350;
            mirror.color = Cream;
            mirror.shootCd = 1.1f;
            mirror.id = ++enemySerial_;
            SyncEnemy3D(mirror);
            enemies_.push_back(mirror);
        }
        if (boss_.telegraphField)
        {
            Obstacle field{};
            const float fieldAngle = AngleOf(toP) + (pattern == BossPatternId::GravityWell ? Pi * 0.35f : -Pi * 0.28f);
            field.pos = targetPlayer->pos + FromAngle(fieldAngle) * 1.35f;
            ClampInside(field.pos, 1.0f);
            field.radius = (pattern == BossPatternId::GravityWell ? 0.90f : 0.78f) + 0.08f * boss_.phase;
            field.ttl = 3.0f;
            field.color = pattern == BossPatternId::GravityWell ? Sky : Mint;
            field.damageField = true;
            SyncObstacle3D(field);
            obstacles_.push_back(field);
            if (pattern == BossPatternId::GravityWell) bossGimmick_.gravityT = 3.8f;
            if (pattern == BossPatternId::TerritoryZone) bossGimmick_.territoryT = 4.2f;
        }
        if (pattern == BossPatternId::Seal)
        {
            bossGimmick_.sealHits = 0;
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
        boss_.attackCd = std::max(0.75f, (1.72f - boss_.phase * 0.10f - wave_ * 0.006f) * CurrentDifficulty().spawnIntervalMul * tuning.bossShotRestMul);
    };

    // 予兆中は弾を撃たず、表示とメッセージだけで次の攻撃を知らせます。
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
        const BossPatternId pattern = PatternForBoss(boss_.bossType, bossGimmick_.patternStep++);
        const int attack = AttackIndexForPattern(pattern);
        bossGimmick_.nextPattern = pattern;
        boss_.telegraphAttack = attack;
        boss_.telegraphAdd = pattern == BossPatternId::GuardRing && BossAddCount() < tuning.bossAddCap;
        boss_.telegraphMirror = pattern == BossPatternId::MirrorSplit && BossAddCount() < tuning.bossAddCap;
        boss_.telegraphField = pattern == BossPatternId::GravityWell || pattern == BossPatternId::TerritoryZone;
        boss_.telegraphLife = tuning.bossTelegraphTime;
        boss_.telegraphT = tuning.bossTelegraphTime;
        boss_.flash = std::max(boss_.flash, tuning.bossTelegraphTime);
        const Color telegraphColor = PatternTelegraphColor(pattern);
        EffectPulse pulse{};
        pulse.pos = boss_.pos;
        pulse.startRadius = boss_.radius * 1.1f;
        pulse.endRadius = boss_.radius * (2.6f + 0.2f * boss_.phase);
        pulse.ttl = tuning.bossTelegraphTime;
        pulse.life = tuning.bossTelegraphTime;
        pulse.y = 0.22f;
        pulse.color = telegraphColor;
        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
        effectPulses_.push_back(pulse);

        WorldTelegraph telegraph{};
        telegraph.pos = boss_.pos;
        telegraph.dir = Normalize(toP);
        telegraph.radius = boss_.radius * (pattern == BossPatternId::GuardRing ? 3.2f : 2.4f);
        telegraph.length = pattern == BossPatternId::Aimed ? 7.0f : 0.0f;
        telegraph.ttl = tuning.bossTelegraphTime;
        telegraph.life = tuning.bossTelegraphTime;
        telegraph.color = telegraphColor;
        telegraph.pattern = pattern;
        worldTelegraphs_.push_back(telegraph);

        if (pattern == BossPatternId::Seal)
        {
            for (int i = 0; i < 3; ++i)
            {
                WorldTelegraph seal{};
                seal.pos = boss_.pos + FromAngle(boss_.spin + TwoPi * i / 3.0f) * (boss_.radius + 1.35f);
                seal.radius = 0.55f;
                seal.ttl = tuning.bossTelegraphTime + 1.0f;
                seal.life = seal.ttl;
                seal.color = Grape;
                seal.pattern = BossPatternId::Seal;
                worldTelegraphs_.push_back(seal);
            }
        }
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

// 雑魚敵へのダメージ処理です。
// Mirror/Shield/Barrier は軽減しますが、最低ダメージを保証して倒せない敵を防ぎます。
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
    // 撃破時はスコア、経験値、フィーバー、反射キル報酬をまとめて処理します。
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
        if (!suppressEnemyKillUltGain_)
        {
            owner.ult = std::min(100.0f, owner.ult + (mobRelease ? 8.5f : 5.0f));
        }
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
                child.hp = (12.0f + wave_ * 2.0f) * MultiplayerHpMultiplier();
                child.maxHp = child.hp;
                child.speed = 2.8f;
                child.atk = 5.0f;
                child.score = 70;
                child.color = Gold;
                child.id = ++enemySerial_;
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
    DamageBoss(dmg, BossDamageKind::NormalShot, false, 0);
}

void SweetsApp::DamageBoss(float dmg, bool reflected, int ownerIndex)
{
    DamageBoss(dmg, reflected ? BossDamageKind::ReflectedShot : BossDamageKind::NormalShot, reflected, ownerIndex);
}

// ボスへのダメージ処理です。
// 通常ボスと隠しボスで必要な補正が違うため、ここで分岐します。
void SweetsApp::DamageBoss(float dmg, BossDamageKind kind, bool reflected, int ownerIndex)
{
    if (!boss_.active) return;
    if (boss_.bossType == BossType::HiddenBoss)
    {
        if (hiddenBossPhaseIntroT_ > 0.0f) return;
        const int oldForm = hiddenBossForm_;
        const bool isReflected = reflected || kind == BossDamageKind::ReflectedShot;
        bool reducedByLock = false;
        bool attackChance = false;
        float appliedDmg = dmg * HiddenBossDamageMultiplier(kind);
        // 1ゲージ目は炎核ギミック。核を壊すまでは本体ダメージを大きく軽減します。
        if (hiddenBossForm_ == 1)
        {
            if (hiddenBossCoreOpenT_ > 0.0f)
            {
                attackChance = true;
            }
            else
            {
                reducedByLock = true;
                appliedDmg *= 0.22f;
            }
        }
        // 2ゲージ目は金色弾反射ギミック。反射回数が足りると一時的に攻撃チャンスになります。
        else if (hiddenBossForm_ == 2)
        {
            if (isReflected && hiddenBossAuraBreakT_ <= 0.0f)
            {
                ++hiddenBossReflectCount_;
                Burst(boss_.pos, Gold, 18);
                if (hiddenBossReflectCount_ >= HiddenBossReflectBreakCount)
                {
                    hiddenBossReflectCount_ = 0;
                    hiddenBossAuraBreakT_ = 7.0f;
                    for (auto& s : shots_) if (s.enemy) s.dead = true;
                    screenFlashT_ = 0.22f;
                    screenFlashLife_ = screenFlashT_;
                    screenFlashColor_ = Gold;
                    message_ = L"金色オーラ解除: 攻撃チャンス";
                    messageT_ = 2.0f;
                }
            }
            if (hiddenBossAuraBreakT_ > 0.0f)
            {
                attackChance = true;
            }
            else
            {
                reducedByLock = true;
                appliedDmg *= isReflected ? 0.45f : 0.15f;
            }
        }
        appliedDmg = std::min(appliedDmg, HiddenBossHitCap(hiddenBossGaugeHp_, kind == BossDamageKind::ReflectedShot || isReflected ? BossDamageKind::ReflectedShot : kind));
        if (IsChargeBossDamage(kind) && messageT_ < 0.35f)
        {
            message_ = reducedByLock ? L"本体ダメージ軽減中" : (attackChance ? L"チャージ有効" : L"チャージ命中");
            messageT_ = 0.75f;
        }
        boss_.hp -= appliedDmg;
        boss_.flash = 0.15f;
        Player& owner = players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))];
        AddScore(static_cast<int>(appliedDmg * (isReflected ? 2.0f : 1.0f)), &owner);
        if (boss_.hp <= 0.0f)
        {
            SaveProgress();
            AddScore(50000 + (isReflected ? 5000 : 0), &owner);
            shots_.clear();
            Burst(boss_.pos, Gold, 160);
            boss_.active = false;
            screen_ = Screen::CompleteClear;
            message_ = L"Complete Clear";
            messageT_ = 999.0f;
            return;
        }

        int nextForm = 1;
        if (boss_.hp <= hiddenBossGaugeHp_) nextForm = 3;
        else if (boss_.hp <= hiddenBossGaugeHp_ * 2.0f) nextForm = 2;
        if (nextForm > oldForm)
        {
            const float boundaryHp = hiddenBossGaugeHp_ * static_cast<float>(HiddenBossGaugeCount - nextForm + 1);
            boss_.hp = std::max(boss_.hp, boundaryHp);
            hiddenBossForm_ = nextForm;
            boss_.phase = nextForm;
            hiddenBossPhase_ = nextForm - 1;
            hiddenPatternStep_ = 0;
            hiddenPatternCd_ = nextForm == 2 ? 1.25f : 0.85f;
            hiddenBossCoreOpenT_ = 0.0f;
            hiddenBossAuraBreakT_ = 0.0f;
            hiddenBossReflectCount_ = 0;
            hiddenBossCores_ = {};
            hiddenBossPhaseIntroLife_ = nextForm == 2 ? 1.6f : 1.0f;
            hiddenBossPhaseIntroT_ = hiddenBossPhaseIntroLife_;
            for (auto& s : shots_)
            {
                if (s.enemy) s.dead = true;
            }
            Burst(boss_.pos, Gold, nextForm == 2 ? 130 : 170);
            screenFlashT_ = nextForm == 2 ? 0.28f : 0.20f;
            screenFlashLife_ = screenFlashT_;
            screenFlashColor_ = Gold;
            message_ = nextForm == 2 ? L"金色弾を弾き返せ" : L"最後は正面勝負";
            messageT_ = 1.8f;
        }
        return;
    }
    float appliedDmg = dmg;
    const bool isReflected = reflected || kind == BossDamageKind::ReflectedShot;
    auto addNotice = [&](const std::wstring& text, Color color)
    {
        CombatNotice notice{};
        notice.text = text;
        notice.ttl = 1.25f;
        notice.life = notice.ttl;
        notice.color = color;
        combatNotices_.push_back(notice);
    };
    auto openWeakness = [&](float seconds, Color color)
    {
        bossGimmick_.vulnerableT = std::max(bossGimmick_.vulnerableT, seconds);
        Burst(boss_.pos, color, 42);
        addNotice(L"反射で弱点露出", color);
    };
    if (isReflected)
    {
        appliedDmg *= 1.35f;
        switch (boss_.bossType)
        {
        case BossType::Demon:
        case BossType::DemonParfait:
            ++bossGimmick_.sealHits;
            if (bossGimmick_.sealHits >= 3)
            {
                bossGimmick_.sealHits = 0;
                openWeakness(4.5f, Grape);
            }
            else
            {
                addNotice(L"封印に反射ヒット", Grape);
            }
            break;
        case BossType::DonutKing:
            openWeakness(3.2f, Gold);
            break;
        case BossType::MirrorMacaron:
            bossGimmick_.mirrorOpen = true;
            openWeakness(4.0f, Cream);
            break;
        case BossType::GravityPudding:
            bossGimmick_.gravityT = 0.0f;
            openWeakness(3.6f, Sky);
            break;
        case BossType::TerritoryCake:
            bossGimmick_.territoryT = 0.0f;
            openWeakness(3.6f, Mint);
            break;
        default:
            break;
        }
    }
    if (bossGimmick_.vulnerableT <= 0.0f)
    {
        switch (boss_.bossType)
        {
        case BossType::Demon:
            appliedDmg *= isReflected ? 0.95f : 0.72f;
            break;
        case BossType::DonutKing:
            appliedDmg *= isReflected ? 0.95f : 0.62f;
            break;
        case BossType::MirrorMacaron:
            appliedDmg *= (isReflected || bossGimmick_.mirrorOpen) ? 0.95f : 0.55f;
            break;
        case BossType::GravityPudding:
            appliedDmg *= isReflected ? 0.95f : 0.70f;
            break;
        case BossType::TerritoryCake:
            appliedDmg *= isReflected ? 0.95f : (bossGimmick_.territoryT > 0.0f ? 0.68f : 0.82f);
            break;
        case BossType::DemonParfait:
            appliedDmg *= isReflected ? 0.95f : 0.62f;
            break;
        default:
            break;
        }
    }
    appliedDmg = std::min(appliedDmg, NormalBossHitCap(boss_.maxHp, kind, reflected));
    boss_.hp -= appliedDmg;
    boss_.flash = 0.15f;
    if (isReflected)
    {
        Player& owner = players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))];
        AddScore(static_cast<int>(appliedDmg * 4.0f), &owner);
        addNotice(L"反射ボーナス", Gold);
    }
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
