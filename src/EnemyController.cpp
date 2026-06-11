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
    case BossDamageKind::HiddenBossAuraKey:
    case BossDamageKind::ReflectedShot: return 1.10f;
    case BossDamageKind::NormalShot:
    default: return 0.92f;
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
        return gaugeHp * 0.085f;
    case BossDamageKind::ReflectedShot:
    case BossDamageKind::HiddenBossAuraKey:
        return gaugeHp * 0.060f;
    case BossDamageKind::NormalShot:
    case BossDamageKind::ChargeShot:
    case BossDamageKind::ChocolateCharge:
    case BossDamageKind::Melee:
    default:
        return gaugeHp * 0.036f;
    }
}

bool IsChargeBossDamage(BossDamageKind kind)
{
    return kind == BossDamageKind::ChargeShot || kind == BossDamageKind::ChocolateCharge;
}

float NormalBossHitCap(float maxHp, BossDamageKind kind, bool reflected)
{
    if (reflected || kind == BossDamageKind::ReflectedShot || kind == BossDamageKind::HiddenBossAuraKey) return maxHp * 0.14f;
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
    boss_.beamCd = BossBeamCooldownMin + Rand(0.0f, BossBeamCooldownVar); // 戦闘開始直後に撃たないよう余裕を持たせる
    boss_.sweepCd = BossSweepCooldownMin + Rand(0.0f, BossSweepCooldownVar);
    boss_.burrowCd = BossBurrowCooldownMin + Rand(0.0f, BossBurrowCooldownVar);
    boss_.megaBeamCd = BossMegaBeamCooldownMin + Rand(0.0f, BossMegaBeamCooldownVar);
    boss_.grabCd = BossGrabCooldownMin + Rand(0.0f, BossGrabCooldownVar);
    boss_.cloneCd = BossCloneCooldownMin + Rand(0.0f, BossCloneCooldownVar);
    boss_.cloneWarnT = boss_.cloneActiveT = 0.0f; boss_.cloneCount = 0;
    boss_.turretSpawnCd = BossTurretSpawnInterval;
    for (int i = 0; i < BossTurretMax; ++i) { boss_.turretActive[i] = false; boss_.turretHp[i] = 0.0f; boss_.turretFireT[i] = 0.0f; boss_.turretTier[i] = 0; }
    boss_.breakGauge = 0.0f;
    boss_.breakGaugeMax = BossBreakGaugeMax;
    boss_.breakT = 0.0f;
    breakCombo_ = 0;
    boss_.armAngle = Rand(0.0f, TwoPi);
    boss_.armWanderTarget = Rand(0.0f, TwoPi);
    boss_.armWanderCd = Rand(1.5f, 3.0f);
    if (gameMode_ == GameMode::BossOnlyDebug)
    {
        boss_.maxHp *= 2.0f; // デバッグ用：耐久を2倍にして技を試しやすく
        boss_.hp = boss_.maxHp;
    }
    for (int i = 0; i < 2; ++i)
    {
        boss_.armHp[i] = boss_.maxHp * BossArmHpRatio; // 腕HP＝ボス最大HPの10%
        boss_.armDownT[i] = 0.0f;
        boss_.armPos[i] = boss_.pos;
    }
    if (gameMode_ == GameMode::BossOnlyDebug)
    {
        // デバッグステージは各技をすぐ・短い間隔で確認できるよう初期CDを短縮。
        boss_.beamCd = 3.0f;
        boss_.megaBeamCd = 7.0f;
        boss_.grabCd = 4.0f;
        boss_.cloneCd = 5.0f;
        boss_.turretSpawnCd = 2.0f;
    }
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
// タレット（反射可能な砲台）の更新。ボスの攻撃中・ダウン中も常に動く＝合間に反射する的を供給。
void SweetsApp::UpdateBossTurrets(float dt)
{
    if (!boss_.active) return;
    Player* targetPlayer = FindNearestPlayer(boss_.pos);
    if (!targetPlayer) return;
    const float specialCdMul = (gameMode_ == GameMode::BossOnlyDebug) ? 0.3f : 1.0f;
    int activeTurrets = 0;
    for (int i = 0; i < BossTurretMax; ++i) if (boss_.turretActive[i]) ++activeTurrets;
    boss_.turretSpawnCd -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
    if (boss_.turretSpawnCd <= 0.0f && activeTurrets < BossTurretMax)
    {
        for (int i = 0; i < BossTurretMax; ++i)
        {
            if (boss_.turretActive[i]) continue;
            const float a = Rand(0.0f, TwoPi);
            V2 pos = targetPlayer->pos + FromAngle(a) * Rand(5.0f, 8.0f);
            ClampInside(pos, BossTurretRadius + 0.4f);
            // フェーズが進むほど強い砲台：phase1→tier0, phase2→tier1, phase3以降→tier2(ビーム)。
            const int tier = (boss_.phase >= 3) ? 2 : (boss_.phase >= 2 ? 1 : 0);
            boss_.turretPos[i] = pos;
            boss_.turretTier[i] = tier;
            boss_.turretHp[i] = BossTurretHp * (1.0f + 0.6f * static_cast<float>(tier)); // 上位tierは硬い
            boss_.turretFireT[i] = Rand(0.6f, BossTurretFireInterval);
            boss_.turretActive[i] = true;
            Burst(pos, tier >= 2 ? Berry : Grape, 18);
            break;
        }
        boss_.turretSpawnCd = BossTurretSpawnInterval * specialCdMul;
    }
    for (int i = 0; i < BossTurretMax; ++i)
    {
        if (!boss_.turretActive[i]) continue;
        // 反射されたプレイヤー弾で破壊される。
        for (auto& s : shots_)
        {
            if (s.dead || s.enemy) continue;
            if (RuleDistance(s.pos, s.height, boss_.turretPos[i], 0.0f) < BossTurretRadius + s.radius)
            {
                boss_.turretHp[i] -= s.damage;
                Burst(boss_.turretPos[i], Sky, 8);
                if (s.chainJumps > 0 && TryChainHop(s, boss_.turretPos[i])) { /* 次の的へ連鎖（生存） */ }
                else if (s.pierce <= 0) s.dead = true; else --s.pierce;
            }
        }
        if (boss_.turretHp[i] <= 0.0f)
        {
            boss_.turretActive[i] = false;
            Burst(boss_.turretPos[i], Cream, 30);
            continue;
        }
        // 発射：プレイヤーへ反射可能弾。tierが高いほど速く・強く・連射（ビーム化）。
        boss_.turretFireT[i] -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        if (boss_.turretFireT[i] <= 0.0f)
        {
            const int tier = boss_.turretTier[i];
            const float a = AngleOf(targetPlayer->pos - boss_.turretPos[i]);
            const float spd = BossTurretBulletSpeed * (1.0f + 0.25f * static_cast<float>(tier));
            const float dmg = boss_.atk * BossTurretDamageMul * (1.0f + 0.25f * static_cast<float>(tier));
            const Color col = (tier >= 2) ? Berry : Grape;
            if (tier >= 2)
            {
                // ビームタレット：高速弾を一直線に連射（ストリーム＝ビーム風）。反射可能。
                for (int k = 0; k < BossTurretBeamBurst; ++k)
                {
                    SpawnEnemyShot(boss_.turretPos[i] + FromAngle(a) * (0.3f * k), a, spd * 1.3f, dmg, 0.30f, col, 4.5f, 0.0f, 0.0f);
                }
                boss_.turretFireT[i] = BossTurretFireInterval * 0.9f;
            }
            else
            {
                SpawnEnemyShot(boss_.turretPos[i], a, spd, dmg, 0.30f, col, 4.5f, 0.0f, 0.0f);
                boss_.turretFireT[i] = BossTurretFireInterval * (tier >= 1 ? 0.7f : 1.0f);
            }
        }
    }
}

void SweetsApp::UpdateBoss(float dt)
{
    if (!boss_.active) return;

    boss_.spin += dt * (1.0f + wave_ * 0.03f);
    if (boss_.flash > 0.0f) boss_.flash -= dt;
    if (boss_.phaseIntroT > 0.0f) boss_.phaseIntroT -= dt; // フェーズ移行の溜め
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

    // タレットはボス本体が攻撃中でもダウン中でも常に稼働させる（合間＝反射でフィールド干渉する時間）。
    UpdateBossTurrets(dt);

    // 崩し中：停止せず弱点が露出する。その代わり行動が活発化し、被ダメージが増える（下のbreak判定で反映）。
    if (boss_.breakT > 0.0f)
    {
        boss_.breakT -= dt;
        if (boss_.breakT <= 0.0f)
        {
            boss_.breakT = 0.0f;
            breakCombo_ = 0; // 崩し終了でコンボは必ずリセット
            Burst(boss_.pos, Cream, 30);
            message_ = L"弱点が閉じた";
            messageT_ = std::max(messageT_, 0.8f);
        }
        // 以降の通常処理（移動・攻撃）を継続＝活発に動く。
    }
    // 崩し（体幹）発動：反射で崩しゲージが満タンになったらボスを一定時間ダウンさせる。
    if (boss_.breakT <= 0.0f && boss_.breakGaugeMax > 0.0f && boss_.breakGauge >= boss_.breakGaugeMax)
    {
        boss_.breakGauge = 0.0f;
        boss_.breakGaugeMax *= BossBreakGaugeGrowth; // 崩すたびに次の必要量を増やす（だんだん崩しづらく）
        boss_.breakT = BossBreakDuration;
        breakCombo_ = 0;
        boss_.beamWarnT = boss_.beamActiveT = 0.0f; boss_.beamReflectDist = 0.0f;
        boss_.megaBeamWarnT = boss_.megaBeamActiveT = 0.0f;
        Burst(boss_.pos, Gold, 80);
        screenFlashT_ = 0.25f; screenFlashLife_ = screenFlashT_; screenFlashColor_ = Sky;
        shakeMag_ = 0.6f; shakeLife_ = 0.4f; shakeT_ = shakeLife_;
        message_ = L"弱点露出! (反撃が激化)"; messageT_ = std::max(messageT_, 2.0f);
        audio_.PlaySoundEffect(SoundEffect::Reflect);
        SyncBoss3D(boss_);
        return;
    }

    Player* targetPlayer = FindNearestPlayer(boss_.pos);
    if (!targetPlayer) return;
    const V2 toP = targetPlayer->pos - boss_.pos;
    const float d = RuleDistance(boss_.pos, boss_.height, targetPlayer->pos, PlayerBodyY);
    const V2 n = Normalize(toP);
    // フェーズ移行直後の溜め中は攻撃も移動も止める（無防備のピーク演出）。
    const bool intro = boss_.phaseIntroT > 0.0f;
    // 特殊攻撃（ビーム等）の予兆・実行中は本体を止める（方向を固定し、避け先を読みやすくする）
    const bool busy = boss_.beamWarnT > 0.0f || boss_.beamActiveT > 0.0f
        || boss_.megaBeamWarnT > 0.0f || boss_.megaBeamActiveT > 0.0f
        || boss_.cloneWarnT > 0.0f
        || boss_.grabReachWarnT > 0.0f || boss_.grabReachT > 0.0f || boss_.grabHoldT > 0.0f;
    // フェーズが上がるほど攻撃頻度（特殊技CD短縮）と移動速度が上がる＝行動が激化する。
    const bool weakOpen = boss_.breakT > 0.0f; // 崩し中＝弱点露出＋行動活発化
    const float phaseAggro = 1.0f + static_cast<float>(boss_.phase - 1) * BossPhaseAggroPerPhase;
    const float phaseSpeed = (1.0f + static_cast<float>(boss_.phase - 1) * BossPhaseSpeedPerPhase) * (weakOpen ? BossBreakSpeedMul : 1.0f);
    // デバッグステージでは新技を短い間隔で繰り返し確認できるようCDを縮める。フェーズ・崩しでさらに短縮。
    const float specialCdMul = ((gameMode_ == GameMode::BossOnlyDebug) ? 0.3f : 1.0f) / phaseAggro * (weakOpen ? BossBreakAggroCdMul : 1.0f);
    // 攻撃の重なり防止：何か攻撃中（特殊技＝busy／通常弾幕の予兆中）は小休止をリセットし続け、
    // 攻撃が終わってから BossAttackRest 秒だけ次の攻撃開始を待たせる。
    if (busy || intro || boss_.telegraphT > 0.0f) boss_.attackRestT = BossAttackRest;
    else if (boss_.attackRestT > 0.0f) boss_.attackRestT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
    const bool canStart = !busy && !intro && boss_.telegraphT <= 0.0f && boss_.attackRestT <= 0.0f;
    boss_.vel = (busy || intro) ? V2{} : n * boss_.speed * phaseSpeed * (slowT_ > 0.0f ? 0.55f : 1.0f);
    boss_.pos += boss_.vel * dt;
    ClampInside(boss_.pos, boss_.radius);
    SyncBoss3D(boss_);

    // 腕（ボス本体の一部・左右2本）の更新。基準向きを最寄りプレイヤー方向へ追従。
    // 拘束中・つかみ突き出し中は基準向きを固定（避け先を読めるように）。
    const bool grabBusy = boss_.grabHoldT > 0.0f || boss_.grabReachWarnT > 0.0f || boss_.grabReachT > 0.0f;
    if (!grabBusy && boss_.burrowSubT <= 0.0f && boss_.flyT <= 0.0f)
    {
        // 通常時はプレイヤーを追わず、可変域をランダムに漂う（本体に弾が当たる隙を作る）。
        boss_.armWanderCd -= dt;
        if (boss_.armWanderCd <= 0.0f)
        {
            boss_.armWanderTarget = Rand(0.0f, TwoPi);
            boss_.armWanderCd = Rand(1.5f, 3.0f);
        }
        float dArm = boss_.armWanderTarget - boss_.armAngle;
        while (dArm > Pi) dArm -= TwoPi;
        while (dArm < -Pi) dArm += TwoPi;
        boss_.armAngle += ClampFloat(dArm, -BossArmTrackSpeed * dt, BossArmTrackSpeed * dt);
    }
    // 通常時の開き角を最大BossArmSpreadの範囲でゆっくり開閉させる。
    const float armSpread = BossArmSpread *
        (BossArmSpreadMinRatio + (1.0f - BossArmSpreadMinRatio) * (0.5f + 0.5f * std::sin(bossGimmick_.timer * BossArmSpreadSpeed)));
    // 各腕の先端位置。つかみ中の腕は予兆で引き、突き出しで前方へ伸びる。
    auto computeArmTip = [&](int i) -> V2 {
        float angle = boss_.armAngle + (i == 0 ? armSpread : -armSpread);
        float reach = BossArmReach;
        if (i == boss_.grabArm)
        {
            angle = boss_.grabReachAngle;
            if (boss_.grabReachWarnT > 0.0f) reach = BossArmReach * 0.6f;            // 引き
            else if (boss_.grabReachT > 0.0f)
            {
                const float p = 1.0f - ClampFloat(boss_.grabReachT / BossGrabThrustTime, 0.0f, 1.0f);
                reach = BossArmReach + (BossGrabReachMax - BossArmReach) * std::sin(p * Pi * 0.5f); // 突き出し
            }
            else if (boss_.grabHoldT > 0.0f)
            {
                angle = boss_.grabAngle;       // 捕獲した方向
                reach = boss_.grabHoldDist;    // 腕を伸ばしたまま捕獲位置で保持
            }
        }
        return boss_.pos + FromAngle(angle) * reach;
    };
    boss_.armPos[0] = computeArmTip(0);
    boss_.armPos[1] = computeArmTip(1);
    // 消滅中の腕は時間経過で復活（HP満タンに戻す）。
    for (int i = 0; i < 2; ++i)
    {
        if (boss_.armDownT[i] > 0.0f)
        {
            boss_.armDownT[i] -= dt;
            if (boss_.armDownT[i] <= 0.0f)
            {
                boss_.armDownT[i] = 0.0f;
                boss_.armHp[i] = boss_.maxHp * BossArmHpRatio;
                Burst(boss_.armPos[i], Red, 24);
            }
        }
    }

    // タレットはボスのダウン中も稼働させたいので、UpdateBoss冒頭の UpdateBossTurrets() で常時更新する。

    // === 貫通ビーム（パリィ不可・低頻度の強攻撃）===
    // 通常弾幕の予兆中(telegraphT)・特殊攻撃中はクールダウンを進めない＝攻撃が重ならない。
    if (!busy && boss_.telegraphT <= 0.0f)
    {
        boss_.beamCd -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
    }
    if (boss_.beamCd <= 0.0f && canStart)
    {
        // 予兆開始：方向をプレイヤーへロックし、地面に予測線を出す。
        boss_.beamWarnT = BossBeamWarnTime;
        boss_.beamAngle = AngleOf(toP);
        WorldTelegraph warn{};
        warn.pos = boss_.pos;
        warn.dir = FromAngle(boss_.beamAngle);
        warn.radius = BossBeamHalfWidth * 2.0f;
        warn.length = BossBeamLength;
        warn.ttl = BossBeamWarnTime;
        warn.life = BossBeamWarnTime;
        warn.color = Red;
        warn.pattern = BossPatternId::Beam;
        worldTelegraphs_.push_back(warn);
        boss_.flash = std::max(boss_.flash, BossBeamWarnTime);
        message_ = L"貫通ビーム!";
        messageT_ = std::max(messageT_, 1.0f);
    }
    if (boss_.beamWarnT > 0.0f)
    {
        boss_.beamWarnT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        if (boss_.beamWarnT <= 0.0f)
        {
            boss_.beamWarnT = 0.0f;
            boss_.beamActiveT = BossBeamActiveTime;
            beamWasReflecting_ = false; // 新しいビーム開始：反射カウントをリセット
            Burst(boss_.pos, Red, 30);
            audio_.PlaySoundEffect(SoundEffect::UltimateSlash);
        }
        return; // 予兆中は通常攻撃しない
    }
    if (boss_.beamActiveT > 0.0f)
    {
        boss_.beamActiveT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        const V2 bdir = FromAngle(boss_.beamAngle);
        // 反射：チョコウォール（右クリックの壁）が軌道上にあると、そこでビームを反射する。
        // 最も手前の壁までの距離を求め、その先（壁より奥）にはビームが届かないようにする。
        boss_.beamReflectDist = 0.0f;
        float reflectDist = -1.0f;
        for (const auto& o : obstacles_)
        {
            // リフレクションコア（chocoWall）。恒久コアは ttl=-1、旧時限壁は ttl>0。
            // 除去待ち（ttl が (-0.5, 0]）のものだけスキップする。
            if (!o.chocoWall) continue;
            if (o.ttl <= 0.0f && o.ttl > -0.5f) continue;
            const V2 rel = o.pos - boss_.pos;
            const float along = Dot(rel, bdir);
            if (along <= 0.0f || along > BossBeamLength) continue;
            const V2 perp = rel - bdir * along;
            if (Len(perp) > BossBeamHalfWidth + o.radius) continue;
            if (reflectDist < 0.0f || along < reflectDist) reflectDist = along;
        }
        const float effLen = reflectDist >= 0.0f ? reflectDist : BossBeamLength;
        // ビーム反射の「開始」を1回だけカウント（ネガポジ突入の進捗）。
        const bool reflectingNow = reflectDist >= 0.0f;
        if (reflectingNow && !beamWasReflecting_) RegisterReflectSuccess();
        beamWasReflecting_ = reflectingNow;
        // 照射：本体から beamAngle 方向の線分（壁まで）に乗ったプレイヤーへダメージ（貫通・パリィ不可）。
        for (auto& p : players_)
        {
            if (!p.active || p.downed || p.inv > 0.0f) continue;
            if (p.reflectShieldT > 0.0f) continue; // シールド展開中は被弾せず反射（シールド側で処理）
            const V2 rel = p.pos - boss_.pos;
            const float along = Dot(rel, bdir);
            if (along < 0.0f || along > effLen) continue; // 壁より奥は当たらない
            const V2 perp = rel - bdir * along;
            if (Len(perp) <= BossBeamHalfWidth + p.radius)
            {
                ResolvePlayerHit(p, boss_.atk * BossBeamDamageMul, boss_.beamAngle);
            }
        }
        // 壁で反射したら、ボスへダメージ＋ブレイクゲージ蓄積。
        if (reflectDist >= 0.0f)
        {
            boss_.beamReflectDist = reflectDist;
            const V2 wallPos = boss_.pos + bdir * reflectDist;
            const float refl = boss_.atk * BossBeamReflectDps * dt;
            DamageBoss(refl, true, 0);          // 反射＝HPダメージ（反射ボーナス込み）
            if (!boss_.active) return;          // 撃破した場合はここで終了
            boss_.breakGauge += refl;           // 反射ダメージを崩しゲージへ蓄積（発動はUpdateBoss冒頭で）
            Burst(wallPos, Sky, 2);
        }
        if (boss_.beamActiveT <= 0.0f)
        {
            boss_.beamActiveT = 0.0f;
            boss_.beamReflectDist = 0.0f;
            boss_.beamCd = (BossBeamCooldownMin + Rand(0.0f, BossBeamCooldownVar)) * specialCdMul;
        }
        return; // 照射中は通常攻撃しない
    }

    // === 分身（本体＋分身が反射可能な弾をまとめて撃つ）===
    if (!busy && boss_.telegraphT <= 0.0f) boss_.cloneCd -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
    if (boss_.cloneCd <= 0.0f && canStart)
    {
        // 予兆：本体の左右に分身を配置（出現エフェクト）。
        boss_.cloneWarnT = BossCloneWarnTime;
        boss_.cloneCount = BossCloneMax;
        for (int i = 0; i < BossCloneMax; ++i)
        {
            const float side = (i == 0) ? 1.0f : -1.0f;
            const float a = AngleOf(toP) + side * 1.2f;
            boss_.clonePos[i] = boss_.pos + FromAngle(a) * BossCloneOffset;
            ClampInside(boss_.clonePos[i], boss_.radius);
            Burst(boss_.clonePos[i], Grape, 18);
        }
        boss_.flash = std::max(boss_.flash, BossCloneWarnTime);
        message_ = L"分身!";
        messageT_ = std::max(messageT_, 1.0f);
    }
    if (boss_.cloneWarnT > 0.0f)
    {
        boss_.cloneWarnT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        if (boss_.cloneWarnT <= 0.0f)
        {
            boss_.cloneWarnT = 0.0f;
            boss_.cloneActiveT = BossCloneActiveTime;
            // 本体＋各分身からプレイヤーへ扇状の反射可能弾を発射。
            auto fanFrom = [&](V2 src)
            {
                const float baseA = AngleOf(targetPlayer->pos - src);
                for (int k = 0; k < BossCloneBulletCount; ++k)
                {
                    const float t = (BossCloneBulletCount > 1) ? (static_cast<float>(k) / (BossCloneBulletCount - 1) - 0.5f) : 0.0f;
                    const float a = baseA + t * BossCloneBulletSpread;
                    SpawnEnemyShot(src, a, BossCloneBulletSpeed, boss_.atk * BossCloneDamageMul, 0.32f, Grape, 4.0f, 0.0f, 0.0f);
                }
                Burst(src, Grape, 16);
            };
            fanFrom(boss_.pos);
            for (int i = 0; i < boss_.cloneCount; ++i) fanFrom(boss_.clonePos[i]);
            audio_.PlaySoundEffect(SoundEffect::UltimateSlash);
        }
        return; // 予兆中は通常攻撃しない
    }
    if (boss_.cloneActiveT > 0.0f)
    {
        boss_.cloneActiveT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        if (boss_.cloneActiveT <= 0.0f)
        {
            boss_.cloneActiveT = 0.0f;
            boss_.cloneCount = 0;
            boss_.cloneCd = (BossCloneCooldownMin + Rand(0.0f, BossCloneCooldownVar)) * specialCdMul;
        }
    }

    // === 極太回転ビーム薙ぎ払い（パリィ不可）===
    if (!busy && boss_.telegraphT <= 0.0f) boss_.megaBeamCd -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
    if (boss_.megaBeamCd <= 0.0f && canStart)
    {
        boss_.megaBeamWarnT = BossMegaBeamWarnTime;
        boss_.megaBeamAngle = AngleOf(toP);
        boss_.megaBeamDir = (Rand(0.0f, 1.0f) < 0.5f) ? 1.0f : -1.0f; // 時計/反時計をランダム
        boss_.flash = std::max(boss_.flash, BossMegaBeamWarnTime);
        message_ = L"極太ビーム!";
        messageT_ = std::max(messageT_, 1.0f);
    }
    if (boss_.megaBeamWarnT > 0.0f)
    {
        boss_.megaBeamWarnT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        if (boss_.megaBeamWarnT <= 0.0f)
        {
            boss_.megaBeamWarnT = 0.0f;
            boss_.megaBeamActiveT = BossMegaBeamActiveTime;
            Burst(boss_.pos, Red, 36);
            audio_.PlaySoundEffect(SoundEffect::UltimateSlash);
        }
        return;
    }
    if (boss_.megaBeamActiveT > 0.0f)
    {
        boss_.megaBeamActiveT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        boss_.megaBeamAngle += boss_.megaBeamDir * BossMegaBeamRotateSpeed * dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        const V2 bdir = FromAngle(boss_.megaBeamAngle);
        for (auto& p : players_)
        {
            if (!p.active || p.downed || p.inv > 0.0f) continue;
            const V2 rel = p.pos - boss_.pos;
            const float along = Dot(rel, bdir);
            if (along < 0.0f || along > BossMegaBeamLength) continue;
            const V2 perp = rel - bdir * along;
            if (Len(perp) <= BossMegaBeamHalfWidth + p.radius)
            {
                ResolvePlayerHit(p, boss_.atk * BossMegaBeamDamageMul, boss_.megaBeamAngle);
            }
        }
        if (boss_.megaBeamActiveT <= 0.0f)
        {
            boss_.megaBeamActiveT = 0.0f;
            boss_.megaBeamCd = (BossMegaBeamCooldownMin + Rand(0.0f, BossMegaBeamCooldownVar)) * specialCdMul;
        }
        return;
    }

    // === 腕（赤先端）：常時の接触ダメージ＋つかみ攻撃（腕を伸ばして掴む）===
    const bool bossPresent = boss_.burrowSubT <= 0.0f && boss_.flyT <= 0.0f && boss_.flyStrikeWarnT <= 0.0f;
    // 生存腕の赤先端に触れているプレイヤーへ継続ダメージ（ダメージ床相当・常時）。
    if (bossPresent && boss_.grabHoldT <= 0.0f)
    {
        for (int arm = 0; arm < 2; ++arm)
        {
            if (boss_.armDownT[arm] > 0.0f) continue;
            for (int i = 0; i < MaxPlayers; ++i)
            {
                Player& p = players_[i];
                if (!p.active || p.downed || p.inv > 0.0f) continue;
                if (RuleDistance(p.pos, PlayerBodyY, boss_.armPos[arm], 0.0f) <= BossArmRadius + p.radius)
                {
                    ResolvePlayerHit(p, BossArmChipPerSec * dt, AngleOf(p.pos - boss_.armPos[arm]));
                }
            }
        }
    }
    // つかみのクールダウン。
    if (boss_.grabHoldT <= 0.0f && boss_.telegraphT <= 0.0f) boss_.grabCd -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
    // つかみ開始：生存腕の中からプレイヤー方向に近い側を選び、腕を引いて溜める。
    if (boss_.grabCd <= 0.0f && canStart && boss_.grabReachWarnT <= 0.0f && boss_.grabReachT <= 0.0f
        && boss_.grabHoldT <= 0.0f && d <= BossGrabTriggerRange)
    {
        int g = -1; float best = 100.0f;
        for (int arm = 0; arm < 2; ++arm)
        {
            if (boss_.armDownT[arm] > 0.0f) continue;
            float da = AngleOf(toP) - (boss_.armAngle + (arm == 0 ? armSpread : -armSpread));
            while (da > Pi) da -= TwoPi;
            while (da < -Pi) da += TwoPi;
            if (std::fabs(da) < best) { best = std::fabs(da); g = arm; }
        }
        if (g >= 0)
        {
            boss_.grabArm = g;
            boss_.grabReachWarnT = BossGrabReachWarn;
            boss_.grabReachAngle = AngleOf(toP); // 突き出す方向をロック（避けられる）
            boss_.flash = std::max(boss_.flash, BossGrabReachWarn);
            message_ = L"つかみ!";
            messageT_ = std::max(messageT_, 0.9f);
        }
    }
    if (boss_.grabReachWarnT > 0.0f)
    {
        boss_.grabReachWarnT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        if (boss_.grabReachWarnT <= 0.0f)
        {
            boss_.grabReachWarnT = 0.0f;
            boss_.grabReachT = BossGrabThrustTime; // 突き出し開始
        }
        return;
    }
    if (boss_.grabReachT > 0.0f)
    {
        boss_.grabReachT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        // 伸びた腕先（赤）が触れたプレイヤーを捕獲。
        if (boss_.grabArm >= 0 && boss_.grabArm < 2 && boss_.armDownT[boss_.grabArm] <= 0.0f)
        {
            const V2 tip = boss_.armPos[boss_.grabArm];
            for (int i = 0; i < MaxPlayers; ++i)
            {
                Player& p = players_[i];
                if (!p.active || p.downed) continue;
                if (RuleDistance(p.pos, PlayerBodyY, tip, 0.0f) <= BossArmRadius + p.radius)
                {
                    boss_.grabTarget = i;
                    boss_.grabAngle = boss_.grabReachAngle;
                    boss_.grabHoldT = BossGrabHoldTime;
                    // 捕まえた距離を保持（本体に引き寄せず、腕を伸ばしたまま掴む）。
                    boss_.grabHoldDist = ClampFloat(Len(boss_.armPos[boss_.grabArm] - boss_.pos),
                        boss_.radius + p.radius + 0.2f, BossGrabReachMax);
                    p.grabbedT = BossGrabHoldTime;
                    boss_.grabReachT = 0.0f;
                    // grabArm は維持：拘束中も腕を伸ばした状態で描画・保持する。
                    Burst(p.pos, Grape, 24);
                    message_ = L"捕獲!";
                    messageT_ = std::max(messageT_, 1.0f);
                    return;
                }
            }
        }
        if (boss_.grabReachT <= 0.0f)
        {
            boss_.grabReachT = 0.0f;
            boss_.grabArm = -1;
            boss_.grabCd = (BossGrabCooldownMin + Rand(0.0f, BossGrabCooldownVar)) * specialCdMul;
        }
        return;
    }
    if (boss_.grabHoldT > 0.0f)
    {
        boss_.grabHoldT -= dt * (slowT_ > 0.0f ? 0.5f : 1.0f);
        Player* gp = (boss_.grabTarget >= 0 && boss_.grabTarget < MaxPlayers) ? &players_[boss_.grabTarget] : nullptr;
        if (!gp || !gp->active || gp->downed)
        {
            boss_.grabHoldT = 0.0f;
        }
        else
        {
            // 伸ばした腕の先（捕獲位置）に拘束（移動不可）。周期ダメージ（i-frameで間引き）。
            gp->pos = boss_.pos + FromAngle(boss_.grabAngle) * boss_.grabHoldDist;
            ClampInside(gp->pos, gp->radius);
            SyncPlayer3D(*gp);
            gp->grabbedT = std::max(gp->grabbedT, boss_.grabHoldT);
            if (gp->inv <= 0.0f) ResolvePlayerHit(*gp, boss_.atk * BossGrabTickDamageMul, boss_.grabAngle + Pi);
        }
        if (boss_.grabHoldT <= 0.0f)
        {
            boss_.grabHoldT = 0.0f;
            if (gp) { gp->grabbedT = 0.0f; gp->inv = std::max(gp->inv, 0.4f); Burst(gp->pos, Cream, 18); }
            boss_.grabTarget = -1;
            boss_.grabArm = -1; // 拘束終了で腕を戻す
            boss_.grabCd = (BossGrabCooldownMin + Rand(0.0f, BossGrabCooldownVar)) * specialCdMul;
        }
        return;
    }

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

    // ボスのみデバッグステージでは弾幕（通常パターン）を撃たず、新技だけを使う。
    if (gameMode_ == GameMode::BossOnlyDebug) return;

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
    if (boss_.attackCd <= 0.0f && boss_.attackRestT <= 0.0f)
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
            message_ = L"コンボボーナス x2";
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
// 腕（赤先端）へのダメージ。HP（ボス最大HPの10%）を削り切ると一定時間消滅する。
bool SweetsApp::DamageBossArm(int index, float dmg)
{
    if (!boss_.active || index < 0 || index > 1) return false;
    if (boss_.armDownT[index] > 0.0f) return false; // 既に消滅中
    boss_.armHp[index] -= dmg;
    Burst(boss_.armPos[index], Red, 6);
    SpawnDamageNumber(boss_.armPos[index], dmg, Red, false);
    if (boss_.armHp[index] <= 0.0f)
    {
        boss_.armHp[index] = 0.0f;
        boss_.armDownT[index] = BossArmDestroyTime;
        Burst(boss_.armPos[index], Cream, 44);
        message_ = L"腕を破壊!";
        messageT_ = std::max(messageT_, 1.4f);
    }
    return true;
}

void SweetsApp::DamageBoss(float dmg, BossDamageKind kind, bool reflected, int ownerIndex)
{
    if (!boss_.active) return;
    if (boss_.burrowSubT > 0.0f) return; // 地中突き上げの潜行中は無敵
    if (boss_.bossType == BossType::HiddenBoss)
    {
        if (hiddenBossPhaseIntroT_ > 0.0f) return;
        const int oldForm = hiddenBossForm_;
        const bool isAuraKey = kind == BossDamageKind::HiddenBossAuraKey;
        const bool isReflected = reflected || kind == BossDamageKind::ReflectedShot || isAuraKey;
        bool reducedByLock = false;
        bool attackChance = false;
        float appliedDmg = dmg * HiddenBossDamageMultiplier(kind);
        if (!hiddenBossPractice_ && kind == BossDamageKind::NormalShot && !isReflected)
        {
            appliedDmg *= 0.88f;
        }
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
        // 2ゲージ目は金色キー弾反射ギミック。キー反射が足りると一時的に攻撃チャンスになります。
        else if (hiddenBossForm_ == 2)
        {
            if (isAuraKey && hiddenBossAuraBreakT_ <= 0.0f)
            {
                ++hiddenBossReflectCount_;
                Burst(boss_.pos, Gold, 18);
                if (hiddenBossReflectCount_ >= HiddenBossReflectBreakCount)
                {
                    hiddenBossReflectCount_ = 0;
                    hiddenBossAuraBreakT_ = 5.5f;
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
        appliedDmg = std::min(appliedDmg, HiddenBossHitCap(hiddenBossGaugeHp_, isReflected ? BossDamageKind::ReflectedShot : kind));
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
            hiddenBossDashWarnT_ = 0.0f;
            hiddenBossDashWarnLife_ = 0.0f;
            hiddenBossDashT_ = 0.0f;
            hiddenBossDashLife_ = 0.0f;
            hiddenBossDashRecoverT_ = 0.0f;
            hiddenBossDashChainLeft_ = 0;
            hiddenBossDashChainGapT_ = 0.0f;
            hiddenBossDashChainWarn_ = 0.0f;
            hiddenBossDashChainDuration_ = 0.0f;
            hiddenBossDashChainSpeed_ = 0.0f;
            hiddenBossDashGlobalCd_ = 0.0f;
            hiddenBossReflectT_ = 0.0f;
            hiddenBossCloneCd_ = 0.0f;
            hiddenBossIdleBasePos_ = boss_.pos;
            hiddenBossFloatAnchorT_ = hiddenBossT_;
            hiddenBossDashVel_ = {};
            enemies_.clear();
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
            message_ = nextForm == 2 ? L"金色キー弾を弾き返せ" : L"最後は正面勝負";
            messageT_ = 1.8f;
        }
        return;
    }
    // ネガポジ中はボスへの攻撃が反転＝回復になる（受けに徹して終了時のお返しで決める）。
    // ※お返し自体は GameFlow 側で negaposiT_=0 にしてから呼ぶので通常ダメージとして通る。
    if (negaposiT_ > 0.0f)
    {
        boss_.hp = std::min(boss_.maxHp, boss_.hp + dmg);
        boss_.flash = 0.1f;
        Burst(boss_.pos, Mint, 6);
        return;
    }
    float appliedDmg = dmg;
    const bool isReflected = reflected || kind == BossDamageKind::ReflectedShot || kind == BossDamageKind::HiddenBossAuraKey;
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
        addNotice(L"弱点露出", color);
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
                addNotice(L"封印ヒット", Grape);
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
    // 崩し中＝弱点露出：被ダメージ倍率＋ブレイクコンボ（重ねるほど火力UP）。崩し中は通常の上限を緩める。
    bool breakHit = false;
    if (boss_.breakT > 0.0f)
    {
        const float comboMul = std::min(BreakComboMaxMul, 1.0f + static_cast<float>(breakCombo_) * BreakComboDamagePerHit);
        appliedDmg *= BossBreakWeakDamageMul * comboMul;
        ++breakCombo_;
        breakHit = true;
    }
    else
    {
        appliedDmg = std::min(appliedDmg, NormalBossHitCap(boss_.maxHp, kind, reflected));
    }
    boss_.hp -= appliedDmg;
    boss_.flash = 0.15f;
    // ダメージ数値（モンハンライズ風）。反射・ブレイクコンボは強調表示。
    SpawnDamageNumber(boss_.pos + V2{ 0.0f, boss_.radius * 0.6f }, appliedDmg,
        breakHit ? Gold : (isReflected ? Sky : Cream), breakHit || isReflected);
    if (isReflected)
    {
        Player& owner = players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))];
        AddScore(static_cast<int>(appliedDmg * 4.0f), &owner);
        addNotice(L"ボーナス", Gold);
    }
    // SAO風の分割HPゲージ：1本（=1/BossGaugeCount）削り切るたびにフェーズが上がり行動が激化。
    const float gaugeHp = boss_.maxHp / static_cast<float>(BossGaugeCount);
    const int depleted = static_cast<int>((boss_.maxHp - boss_.hp) / gaugeHp);
    const int nextPhase = std::min(BossGaugeCount, depleted + 1);
    if (nextPhase > boss_.phase)
    {
        boss_.phase = nextPhase;
        // 進行中の予兆・特殊技を一旦リセットして、フェーズ移行の溜め（無防備）を作る＝戦闘のピーク。
        boss_.attackCd = 0.9f;
        boss_.telegraphT = 0.0f;
        boss_.telegraphAttack = -1;
        boss_.telegraphAdd = false;
        boss_.telegraphMirror = false;
        boss_.telegraphField = false;
        boss_.beamWarnT = boss_.beamActiveT = 0.0f;
        boss_.megaBeamWarnT = boss_.megaBeamActiveT = 0.0f;
        boss_.sweepWarnT = boss_.sweepActiveT = 0.0f;
        boss_.grabReachWarnT = boss_.grabReachT = 0.0f;
        boss_.phaseIntroT = BossPhaseIntroTime;
        // 演出：敵弾を一掃して仕切り直し、ヒットストップ＋画面シェイク＋フラッシュ＋衝撃波で派手に。
        for (auto& s : shots_) if (s.enemy) s.dead = true;
        Burst(boss_.pos, Gold, 90);
        Burst(boss_.pos, Red, 60);
        hitstopT_ = std::max(hitstopT_, 0.18f);     // 一瞬スローでタメを作る
        shakeMag_ = 0.85f;                          // 画面を大きく揺らす
        shakeLife_ = 0.5f;
        shakeT_ = shakeLife_;
        screenFlashT_ = 0.30f;
        screenFlashLife_ = screenFlashT_;
        screenFlashColor_ = Red;
        message_ = L"PHASE " + std::to_wstring(boss_.phase) + L" / " + std::to_wstring(BossGaugeCount);
        messageT_ = std::max(messageT_, 2.0f);
        audio_.PlaySoundEffect(SoundEffect::UltimateSlash);
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
