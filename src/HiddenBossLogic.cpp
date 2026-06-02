#include "SweetsApp.h"

#include <algorithm>
#include <sstream>

namespace
{
// HPから現在のゲージ/形態を求めます。
// 3ゲージ制なので、残りHPが2ゲージ分以下なら第2形態、1ゲージ分以下なら第3形態です。
int HiddenBossFormFromHp(float hp, float gaugeHp)
{
    if (hp <= gaugeHp) return 3;
    if (hp <= gaugeHp * 2.0f) return 2;
    return 1;
}

// 1ゲージ目の炎核がすべて壊れているか確認します。
bool HiddenBossCoresCleared(const std::array<HiddenBossCore, HiddenBossCoreCount>& cores)
{
    return std::none_of(cores.begin(), cores.end(), [](const HiddenBossCore& core) { return core.active; });
}

int HiddenBossCloneCount(const std::vector<Enemy>& enemies)
{
    return static_cast<int>(std::count_if(enemies.begin(), enemies.end(), [](const Enemy& e)
    {
        return !e.dead && (e.type == EnemyType::Mirror || e.type == EnemyType::Teleport);
    }));
}

V2 HiddenBossAimDir(V2 from, const Player* target, float fallbackAngle)
{
    const V2 toTarget = target ? target->pos - from : V2{};
    return LenSq(toTarget) > 0.0001f ? Normalize(toTarget) : FromAngle(fallbackAngle);
}

V2 HiddenBossFloatOffset(float t, int form)
{
    return {
        std::sin(t * (form >= 3 ? 0.80f : 0.55f)) * (form >= 3 ? 1.15f : 0.85f),
        std::sin(t * (form == 2 ? 0.55f : 0.37f)) * (form >= 3 ? 0.42f : 0.30f)
    };
}
}

// 隠しボス本戦を開始します。
// HPは1ゲージHP × 3ゲージ × 協力人数補正 × ストーリー到達時のレベル補正で決まります。
void SweetsApp::StartHiddenBoss()
{
    EnsureGameplayAssetsReady();
    PrepareHiddenBossResources();
    enemies_.clear();
    shots_.clear();
    slashes_.clear();
    pickups_.clear();
    obstacles_.clear();
    particles_.clear();
    effectPulses_.clear();
    swordEffectVisuals_.clear();
    hiddenBossCores_ = {};
    screenFlashT_ = 0.0f;
    stage_ = StageType::BossArena;
    fieldShape_ = FieldShape::Circle;
    stageTimer_ = 0.0f;
    shrinkRadius_ = ArenaRadius;
    // Practiceではレベル補正なし、Story到達では平均レベルに応じてHPが上がります。
    hiddenBossGaugeHp_ = HiddenBossBaseGaugeHp * MultiplayerHpMultiplier() * HiddenBossLevelHpMultiplier();
    hiddenBossTotalHp_ = hiddenBossGaugeHp_ * static_cast<float>(HiddenBossGaugeCount);

    boss_ = {};
    boss_.active = true;
    boss_.bossType = BossType::HiddenBoss;
    boss_.type = static_cast<int>(BossType::HiddenBoss);
    boss_.pos = { 0.0f, -3.0f };
    boss_.height = BossBodyY + 0.22f;
    boss_.radius = 1.05f;
    boss_.maxHp = hiddenBossTotalHp_;
    boss_.hp = boss_.maxHp;
    boss_.speed = 0.35f;
    boss_.atk = 15.0f;
    boss_.phase = 1;
    boss_.attackCd = 0.0f;
    SyncBoss3D(boss_);

    hiddenBossT_ = 0.0f;
    hiddenPatternCd_ = 0.0f;
    hiddenPatternStep_ = 0;
    hiddenBossPhase_ = -1;
    hiddenBossForm_ = 1;
    hiddenBossCoreOpenT_ = 0.0f;
    hiddenBossAuraBreakT_ = 0.0f;
    hiddenBossReflectCount_ = 0;
    hiddenBossPhaseIntroT_ = 0.0f;
    hiddenBossPhaseIntroLife_ = 0.0f;
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
    hiddenBossEdgePressureCd_ = 0.0f;
    hiddenBossReflectT_ = 0.0f;
    hiddenBossCloneCd_ = 0.0f;
    hiddenBossIdleBasePos_ = boss_.pos;
    hiddenBossFloatAnchorT_ = hiddenBossT_;
    hiddenBossDashVel_ = {};
    ResetHiddenBossCores();
    screen_ = Screen::HiddenBoss;
    message_ = L"炎核を見つけろ";
    messageT_ = 3.0f;
}

// 隠しボス突入時のプレイヤー準備です。
// 高難度戦に入る前にHP/ボム/ULTを整え、突入直後の理不尽な事故を防ぎます。
void SweetsApp::PrepareHiddenBossResources()
{
    for (auto& p : players_)
    {
        if (!p.active) continue;
        p.hp = p.maxHp;
        p.bombs = 2;
        p.ult = 100.0f;
        p.downed = false;
        p.alive = true;
        p.reviveT = 0.0f;
        p.inv = std::max(p.inv, 1.4f);
    }
}

// 隠しボス登場演出です。
// BGM開始から10秒間は上空を飛び、降下完了後に本戦へ移行します。
void SweetsApp::UpdateHiddenBossIntro(float dt)
{
    hiddenIntroT_ += dt;
    gameTime_ += dt;
    boss_.active = true;
    boss_.bossType = BossType::HiddenBoss;
    boss_.type = static_cast<int>(BossType::HiddenBoss);
    const float t = ClampFloat(hiddenIntroT_ / HiddenBossIntroDuration, 0.0f, 1.0f);
    const float hoverT = ClampFloat(t / 0.70f, 0.0f, 1.0f);
    const float dropT = ClampFloat((t - 0.70f) / 0.30f, 0.0f, 1.0f);
    boss_.pos = {
        std::sin(hiddenIntroT_ * 1.15f) * (1.8f * (1.0f - dropT)),
        -5.6f + hoverT * 0.75f + dropT * 2.6f
    };
    boss_.height = BossBodyY + 1.75f * (1.0f - dropT) + 0.22f;
    boss_.radius = 1.05f;
    boss_.spin += dt * (2.2f + dropT * 2.8f);
    if (Rand(0.0f, 1.0f) < dt * (32.0f + dropT * 36.0f))
    {
        Particle p{};
        const float a = Rand(0.0f, TwoPi);
        p.pos = boss_.pos + FromAngle(a) * Rand(0.35f, 1.55f + dropT * 1.25f);
        p.vel = FromAngle(a + Pi * 0.5f) * Rand(0.1f, 0.75f + dropT * 0.55f);
        p.y = Rand(0.2f, 1.7f);
        p.vy = Rand(0.35f, 1.55f + dropT * 0.9f);
        p.ttl = Rand(0.35f, 1.05f);
        p.color = Rand(0.0f, 1.0f) < 0.45f ? Gold : (Rand(0.0f, 1.0f) < 0.65f ? Grape : Cream);
        p.pos3 = Grounded3D(p.pos, p.y);
        particles_.push_back(p);
    }
    if (Rand(0.0f, 1.0f) < dt * (2.2f + dropT * 5.6f))
    {
        EffectPulse pulse{};
        const float r = Rand(0.8f, 1.8f + dropT * 2.0f);
        pulse.pos = boss_.pos + FromAngle(Rand(0.0f, TwoPi)) * Rand(0.0f, 0.55f + dropT * 0.8f);
        pulse.startRadius = r * 0.45f;
        pulse.endRadius = r * (1.3f + dropT * 0.8f);
        pulse.ttl = Rand(0.28f, 0.56f);
        pulse.life = pulse.ttl;
        pulse.y = 0.18f + dropT * 0.35f;
        pulse.color = dropT > 0.72f ? Red : Grape;
        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
        effectPulses_.push_back(pulse);
    }
    if (dropT > 0.86f && Rand(0.0f, 1.0f) < dt * 22.0f)
    {
        const float a = Rand(0.0f, TwoPi);
        Particle p{};
        p.pos = boss_.pos + FromAngle(a) * Rand(1.4f, 3.6f);
        p.vel = FromAngle(a) * Rand(0.9f, 2.2f);
        p.y = Rand(0.05f, 0.35f);
        p.vy = Rand(0.25f, 0.85f);
        p.ttl = Rand(0.25f, 0.55f);
        p.color = Rand(0.0f, 1.0f) < 0.55f ? Red : Gold;
        p.pos3 = Grounded3D(p.pos, p.y);
        particles_.push_back(p);
    }
    UpdateParticles(dt);
    UpdateCamera(dt);
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());
    if (hiddenIntroT_ >= HiddenBossIntroDuration)
    {
        StartHiddenBoss();
    }
}

// 1ゲージ目の炎核を配置し直します。
// ボス周囲を回る核を壊すと、本体へ通るダメージが大きくなります。
void SweetsApp::ResetHiddenBossCores()
{
    const float hp = 360.0f + 38.0f * static_cast<float>(std::max(1, player_.level));
    for (int i = 0; i < HiddenBossCoreCount; ++i)
    {
        HiddenBossCore& core = hiddenBossCores_[i];
        core = {};
        core.active = true;
        core.angle = boss_.spin + TwoPi * static_cast<float>(i) / static_cast<float>(HiddenBossCoreCount);
        core.orbitRadius = 2.35f + 0.18f * static_cast<float>(i & 1);
        core.radius = 0.34f;
        core.maxHp = hp;
        core.hp = hp;
        core.pos = boss_.pos + FromAngle(core.angle) * core.orbitRadius;
        core.pos3 = Grounded3D(core.pos, ShotBodyY + 0.12f);
    }
}

// 炎核の回転と位置同期です。
// 第2形態以降では使わないので、配列を空に戻します。
void SweetsApp::UpdateHiddenBossCores(float dt)
{
    if (hiddenBossForm_ != 1)
    {
        hiddenBossCores_ = {};
        return;
    }

    if (hiddenBossCoreOpenT_ <= 0.0f && HiddenBossCoresCleared(hiddenBossCores_))
    {
        ResetHiddenBossCores();
    }

    for (int i = 0; i < HiddenBossCoreCount; ++i)
    {
        HiddenBossCore& core = hiddenBossCores_[i];
        if (!core.active) continue;
        if (core.flash > 0.0f) core.flash = std::max(0.0f, core.flash - dt);
        core.angle += dt * (0.62f + 0.08f * static_cast<float>(i));
        core.pos = boss_.pos + FromAngle(core.angle) * core.orbitRadius;
        core.pos3 = Grounded3D(core.pos, ShotBodyY + 0.12f);
    }
}

// 炎核への命中処理です。
// すべて壊れたら敵弾を消し、短い攻撃チャンスを発生させます。
bool SweetsApp::DamageHiddenBossCore(float dmg, V2 from, int ownerIndex)
{
    if (hiddenBossForm_ != 1 || hiddenBossCoreOpenT_ > 0.0f) return false;
    for (auto& core : hiddenBossCores_)
    {
        if (!core.active) continue;
        if (RuleDistance(from, ShotBodyY, core.pos, ShotBodyY) > core.radius + 0.24f) continue;
        core.hp -= dmg;
        core.flash = 0.12f;
        Burst(core.pos, Gold, 14);
        if (core.hp <= 0.0f)
        {
            core.active = false;
            AddScore(500, &players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))]);
            Burst(core.pos, Red, 34);
            if (HiddenBossCoresCleared(hiddenBossCores_))
            {
                hiddenBossCoreOpenT_ = 7.0f;
                for (auto& s : shots_) if (s.enemy) s.dead = true;
                screenFlashT_ = 0.18f;
                screenFlashLife_ = screenFlashT_;
                screenFlashColor_ = Gold;
                message_ = L"炎核破壊: 攻撃チャンス";
                messageT_ = 2.0f;
            }
        }
        return true;
    }
    return false;
}

// ボムや必殺など、範囲攻撃で炎核をまとめて削るための処理です。
void SweetsApp::DamageHiddenBossCoresInRadius(V2 center, float radius, float dmg, int ownerIndex)
{
    if (hiddenBossForm_ != 1 || hiddenBossCoreOpenT_ > 0.0f) return;
    for (auto& core : hiddenBossCores_)
    {
        if (!core.active) continue;
        if (RuleDistance(center, ShotBodyY, core.pos, ShotBodyY) <= radius + core.radius)
        {
            DamageHiddenBossCore(dmg, core.pos, ownerIndex);
        }
    }
}

// 隠しボス本戦の更新です。
// 通常ボスとは違い、3ゲージごとのギミック、専用BGM、専用弾幕をここで管理します。
void SweetsApp::UpdateHiddenBoss(float dt)
{
    gameTime_ += dt;
    hiddenBossT_ += dt;
    if (messageT_ > 0.0f) messageT_ -= dt;
    if (player_.inv > 0.0f) player_.inv -= dt;
    if (player_.shieldT > 0.0f) player_.shieldT -= dt;
    if (player_.bombT > 0.0f) player_.bombT -= dt;
    if (player_.grazeFlash > 0.0f) player_.grazeFlash -= dt;
    if (player_.dmgBuffT > 0.0f) player_.dmgBuffT -= dt;
    if (player_.speedBuffT > 0.0f) player_.speedBuffT -= dt;
    if (player_.scoreDoubleT > 0.0f) player_.scoreDoubleT -= dt;
    if (player_.magnetT > 0.0f) player_.magnetT -= dt;
    if (player_.spreadT > 0.0f) player_.spreadT -= dt;
    if (player_.chargeCd > 0.0f) player_.chargeCd -= dt;
    if (player_.feverT > 0.0f) player_.feverT -= dt;
    else player_.fever = std::max(0.0f, player_.fever - dt * 16.0f);
    if (hiddenBossCoreOpenT_ > 0.0f) hiddenBossCoreOpenT_ = std::max(0.0f, hiddenBossCoreOpenT_ - dt);
    if (hiddenBossAuraBreakT_ > 0.0f) hiddenBossAuraBreakT_ = std::max(0.0f, hiddenBossAuraBreakT_ - dt);
    if (hiddenBossReflectT_ > 0.0f) hiddenBossReflectT_ = std::max(0.0f, hiddenBossReflectT_ - dt);
    if (hiddenBossCloneCd_ > 0.0f) hiddenBossCloneCd_ = std::max(0.0f, hiddenBossCloneCd_ - dt);
    if (hiddenBossDashRecoverT_ > 0.0f) hiddenBossDashRecoverT_ = std::max(0.0f, hiddenBossDashRecoverT_ - dt);
    if (hiddenBossDashChainGapT_ > 0.0f) hiddenBossDashChainGapT_ = std::max(0.0f, hiddenBossDashChainGapT_ - dt);
    if (hiddenBossDashGlobalCd_ > 0.0f) hiddenBossDashGlobalCd_ = std::max(0.0f, hiddenBossDashGlobalCd_ - dt);
    if (hiddenBossEdgePressureCd_ > 0.0f) hiddenBossEdgePressureCd_ = std::max(0.0f, hiddenBossEdgePressureCd_ - dt);

    const bool dashWasActive = hiddenBossDashT_ > 0.0f;
    bool dashActive = hiddenBossDashT_ > 0.0f;
    bool dashEnded = false;
    if (hiddenBossDashWarnT_ > 0.0f)
    {
        hiddenBossDashWarnT_ = std::max(0.0f, hiddenBossDashWarnT_ - dt);
        boss_.flash = std::max(boss_.flash, 0.06f);
        if (hiddenBossDashWarnT_ <= 0.0f)
        {
            hiddenBossDashT_ = hiddenBossDashLife_;
            dashActive = hiddenBossDashT_ > 0.0f;
            boss_.flash = std::max(boss_.flash, 0.18f);
            screenFlashT_ = std::max(screenFlashT_, 0.06f);
            screenFlashLife_ = std::max(screenFlashLife_, screenFlashT_);
            screenFlashColor_ = Red;
            Burst(boss_.pos, Red, hiddenBossForm_ >= 3 ? 28 : 18);
        }
    }
    else if (hiddenBossDashT_ > 0.0f)
    {
        hiddenBossDashT_ = std::max(0.0f, hiddenBossDashT_ - dt);
        dashEnded = hiddenBossDashT_ <= 0.0f;
        dashActive = hiddenBossDashT_ > 0.0f;
    }

    // HP残量から現在形態を再計算します。形態ごとにギミックと弾幕が変わります。
    hiddenBossForm_ = HiddenBossFormFromHp(std::max(1.0f, boss_.hp), hiddenBossGaugeHp_);
    boss_.phase = hiddenBossForm_;
    boss_.spin += dt * (1.35f + hiddenBossForm_ * 0.20f);
    if (dashWasActive || dashActive)
    {
        boss_.pos += hiddenBossDashVel_ * dt;
        ClampInside(boss_.pos, boss_.radius);
    }
    else if (!dashEnded && hiddenBossDashWarnT_ <= 0.0f && hiddenBossDashRecoverT_ <= 0.0f)
    {
        const V2 floatOffset = HiddenBossFloatOffset(hiddenBossT_ - hiddenBossFloatAnchorT_, hiddenBossForm_);
        boss_.pos = hiddenBossIdleBasePos_ + floatOffset;
        ClampInside(boss_.pos, boss_.radius);
        hiddenBossIdleBasePos_ = boss_.pos - floatOffset;
    }
    SyncBoss3D(boss_);
    if (boss_.flash > 0.0f) boss_.flash -= dt;
    if (dashEnded)
    {
        hiddenBossIdleBasePos_ = boss_.pos;
        hiddenBossFloatAnchorT_ = hiddenBossT_;
        ClampInside(hiddenBossIdleBasePos_, boss_.radius);

        const bool finalDash = hiddenBossDashChainLeft_ <= 0;
        if (finalDash)
        {
            const int count = hiddenBossForm_ >= 3 ? 18 : 12;
            for (int i = 0; i < count; ++i)
            {
                const float a = boss_.spin + TwoPi * i / static_cast<float>(count);
                SpawnEnemyShot(boss_.pos + FromAngle(a) * (boss_.radius + 0.12f), a, hiddenBossForm_ >= 3 ? 3.1f : 2.45f, boss_.atk * 0.58f, 0.080f, hiddenBossForm_ >= 3 ? Grape : Red, 5.0f, (i & 1) ? 0.08f : -0.08f);
            }
        }
        EffectPulse pulse{};
        pulse.pos = boss_.pos;
        pulse.startRadius = boss_.radius * (finalDash ? 0.8f : 0.55f);
        pulse.endRadius = boss_.radius * (finalDash ? 3.4f : 1.9f);
        pulse.ttl = finalDash ? 0.34f : 0.20f;
        pulse.life = pulse.ttl;
        pulse.y = 0.18f;
        pulse.color = Red;
        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
        effectPulses_.push_back(pulse);
        hiddenBossDashRecoverT_ = finalDash ? 0.36f : 0.12f;
        if (!finalDash)
        {
            hiddenBossDashChainGapT_ = 0.16f;
        }
        else
        {
            hiddenPatternCd_ = std::min(hiddenPatternCd_, 0.55f);
        }
        Burst(boss_.pos, Red, finalDash ? (hiddenBossForm_ >= 3 ? 42 : 28) : 12);
    }

    // 形態移行演出中は入力/被弾/弾幕更新を止め、ボスへ視線を集めます。
    if (hiddenBossPhaseIntroT_ > 0.0f)
    {
        mouseRightReleased_ = false;
        hiddenBossPhaseIntroT_ = std::max(0.0f, hiddenBossPhaseIntroT_ - dt);
        boss_.flash = std::max(boss_.flash, 0.08f);
        boss_.spin += dt * (hiddenBossForm_ >= 3 ? 3.6f : 2.6f);
        UpdateParticles(dt);
        UpdateCamera(dt);
        particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());
        return;
    }

    UpdateStage(dt);
    UpdatePlayer(dt);
    UpdateCoopPlayers(dt);
    UpdateHiddenBossCores(dt);
    UpdateEnemies(dt);
    if (hiddenBossEdgePressureCd_ <= 0.0f
        && hiddenBossDashWarnT_ <= 0.0f
        && hiddenBossDashT_ <= 0.0f
        && hiddenBossPhaseIntroT_ <= 0.0f)
    {
        Player* edgeTarget = nullptr;
        float edgeDist = 0.0f;
        for (auto& p : players_)
        {
            if (!p.active || p.downed) continue;
            const float d = Len(p.pos);
            if (d > edgeDist)
            {
                edgeDist = d;
                edgeTarget = &p;
            }
        }
        const float trigger = ArenaRadius * (hiddenBossForm_ >= 3 ? 0.70f : 0.76f);
        if (edgeTarget && edgeDist >= trigger)
        {
            V2 radial = Normalize(edgeTarget->pos);
            if (LenSq(radial) <= 0.001f) radial = FromAngle(boss_.spin);
            const V2 tangent{ -radial.z, radial.x };
            const float orbitSign = Dot(edgeTarget->vel, tangent) >= 0.0f ? 1.0f : -1.0f;
            const int lanes = hiddenBossForm_ >= 3 ? 7 : 5;
            const float speed = hiddenBossForm_ >= 3 ? 3.85f : 3.25f;
            const Color color = hiddenBossForm_ >= 3 ? Red : (hiddenBossForm_ == 2 ? Gold : Sky);
            const V2 centerCut = edgeTarget->pos - radial * (hiddenBossForm_ >= 3 ? 3.1f : 2.4f);
            for (int side = -1; side <= 1; side += 2)
            {
                V2 origin = edgeTarget->pos + tangent * (orbitSign * side * 4.2f) + radial * 1.8f;
                ClampInside(origin, 0.70f);
                const float base = AngleOf(centerCut - origin);
                const int half = lanes / 2;
                for (int i = -half; i <= half; ++i)
                {
                    SpawnEnemyShot(origin, base + i * 0.115f, speed + 0.05f * static_cast<float>(std::abs(i)),
                        boss_.atk * 0.50f, 0.066f, color, 6.2f, 0.0f, -0.018f);
                }
            }
            EffectPulse pulse{};
            pulse.pos = edgeTarget->pos;
            pulse.startRadius = 0.45f;
            pulse.endRadius = hiddenBossForm_ >= 3 ? 2.8f : 2.25f;
            pulse.ttl = 0.30f;
            pulse.life = pulse.ttl;
            pulse.y = 0.22f;
            pulse.color = color;
            pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
            effectPulses_.push_back(pulse);
            hiddenBossEdgePressureCd_ = hiddenBossForm_ >= 3 ? 0.82f : 1.12f;
        }
    }
    if (hiddenBossDashT_ > 0.0f)
    {
        for (auto& p : players_)
        {
            if (!p.active || p.downed) continue;
            if (RuleDistance(p.pos, PlayerBodyY, boss_.pos, boss_.height) < p.hitboxRadius + boss_.radius + 0.28f)
            {
                ResolvePlayerHit(p, boss_.atk * (hiddenBossForm_ >= 3 ? 1.55f : 1.35f), AngleOf(p.pos - boss_.pos));
            }
        }
    }

    const int activeHiddenPhase = std::max(0, std::min(hiddenBossForm_ - 1, 2));
    // 形態が切り替わった瞬間に敵弾を消し、事故を防いで次のルールをメッセージで知らせます。
    if (activeHiddenPhase != hiddenBossPhase_)
    {
        hiddenBossPhase_ = activeHiddenPhase;
        hiddenPatternStep_ = 0;
        hiddenPatternCd_ = 0.22f;
        hiddenBossReflectCount_ = 0;
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
        hiddenBossEdgePressureCd_ = 0.0f;
        hiddenBossReflectT_ = 0.0f;
        hiddenBossCloneCd_ = 0.0f;
        hiddenBossIdleBasePos_ = boss_.pos;
        hiddenBossFloatAnchorT_ = hiddenBossT_;
        ClampInside(hiddenBossIdleBasePos_, boss_.radius);
        hiddenBossDashVel_ = {};
        if (hiddenBossForm_ == 1)
        {
            hiddenBossCoreOpenT_ = 0.0f;
            ResetHiddenBossCores();
        }
        else
        {
            hiddenBossCoreOpenT_ = 0.0f;
            hiddenBossCores_ = {};
        }
        if (hiddenBossForm_ == 2) hiddenBossAuraBreakT_ = 0.0f;
        for (auto& s : shots_)
        {
            if (s.enemy) s.dead = true;
        }
        enemies_.clear();
        message_ = hiddenBossPhase_ == 0 ? L"炎核を壊せ" : (hiddenBossPhase_ == 1 ? L"金色キー弾を反射しろ" : L"回避して攻めろ");
        messageT_ = 2.0f;
    }

    if (messageT_ <= 0.0f)
    {
        if (hiddenBossForm_ == 1)
        {
            message_ = hiddenBossCoreOpenT_ > 0.0f ? L"本体へ攻撃チャンス" : L"炎核を壊せ";
            messageT_ = hiddenBossCoreOpenT_ > 0.0f ? 1.1f : 1.4f;
        }
        else if (hiddenBossForm_ == 2)
        {
            if (hiddenBossAuraBreakT_ > 0.0f)
            {
                message_ = L"オーラ解除中: 本体を削れ";
            }
            else
            {
                std::wostringstream ss;
                ss << L"金色キー弾を反射しろ " << hiddenBossReflectCount_ << L"/" << HiddenBossReflectBreakCount;
                message_ = ss.str();
            }
            messageT_ = 1.2f;
        }
        else
        {
            message_ = L"最終ゲージ: 回避して攻めろ";
            messageT_ = 1.1f;
        }
    }

    auto reflectPlayerShotsNearBoss = [&](float radius, int maxReflect)
    {
        int reflected = 0;
        for (auto& s : shots_)
        {
            if (reflected >= maxReflect) break;
            if (s.dead || s.enemy || s.chocoBomb || s.ultimateSource || s.charged || s.reflected) continue;
            if (Len(s.pos - boss_.pos) > radius + s.radius) continue;
            Player* target = FindNearestPlayer(s.pos);
            const V2 dir = HiddenBossAimDir(s.pos, target, boss_.spin);
            const float speed = std::max(3.9f, std::min(6.8f, Len(s.vel) * 0.72f + 1.4f));
            s.enemy = true;
            s.ownerIndex = -1;
            s.reflected = true;
            s.reflectedCount = std::max(1, s.reflectedCount);
            s.pierce = 0;
            s.bounce = 0;
            s.homingStrength = 0.0f;
            s.visual = ShotVisualKind::Orb;
            s.yoyo = false;
            s.grazed = false;
            s.hitBoss = false;
            s.vel = dir * speed;
            s.damage = boss_.atk * (hiddenBossForm_ >= 3 ? 0.58f : 0.46f);
            s.radius = std::max(s.radius, hiddenBossForm_ >= 3 ? 0.095f : 0.085f);
            s.ttl = ClampFloat(s.ttl, 2.2f, 4.2f);
            s.color = hiddenBossForm_ >= 3 ? Grape : Gold;
            SyncShot3D(s);
            Burst(s.pos, s.color, 6);
            ++reflected;
        }
        if (reflected > 0 && messageT_ < 0.35f)
        {
            message_ = L"弾反射";
            messageT_ = 0.65f;
        }
    };

    auto beginHiddenBossDash = [&]()
    {
        Player* target = FindNearestPlayer(boss_.pos);
        const V2 dir = HiddenBossAimDir(boss_.pos, target, boss_.spin);
        hiddenBossDashWarnT_ = hiddenBossDashChainWarn_;
        hiddenBossDashWarnLife_ = hiddenBossDashChainWarn_;
        hiddenBossDashT_ = 0.0f;
        hiddenBossDashLife_ = hiddenBossDashChainDuration_;
        hiddenBossDashRecoverT_ = 0.0f;
        if (hiddenBossDashChainLeft_ > 0) --hiddenBossDashChainLeft_;
        const float wantedDistance = target ? Len(target->pos - boss_.pos) + (hiddenBossForm_ >= 3 ? 3.2f : 2.4f) : hiddenBossDashChainSpeed_ * hiddenBossDashChainDuration_;
        const float speedCap = hiddenBossForm_ >= 3 ? 18.5f : 16.5f;
        const float fittedSpeed = std::min(speedCap, std::max(hiddenBossDashChainSpeed_, wantedDistance / std::max(0.05f, hiddenBossDashChainDuration_)));
        hiddenBossDashVel_ = dir * fittedSpeed;
        boss_.flash = std::max(boss_.flash, hiddenBossDashChainWarn_);

        WorldTelegraph telegraph{};
        telegraph.pos = boss_.pos;
        telegraph.dir = dir;
        telegraph.radius = boss_.radius * (hiddenBossForm_ >= 3 ? 1.55f : 1.35f);
        telegraph.length = std::min(ArenaRadius * 1.45f, wantedDistance);
        telegraph.ttl = hiddenBossDashChainWarn_;
        telegraph.life = hiddenBossDashChainWarn_;
        telegraph.color = hiddenBossForm_ >= 3 ? Red : Gold;
        telegraph.pattern = BossPatternId::Aimed;
        worldTelegraphs_.push_back(telegraph);

        EffectPulse pulse{};
        pulse.pos = boss_.pos;
        pulse.startRadius = boss_.radius * 0.65f;
        pulse.endRadius = boss_.radius * 2.15f;
        pulse.ttl = hiddenBossDashChainWarn_;
        pulse.life = hiddenBossDashChainWarn_;
        pulse.y = 0.24f;
        pulse.color = telegraph.color;
        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
        effectPulses_.push_back(pulse);

        if (messageT_ < 0.75f)
        {
            message_ = L"突進予兆";
            messageT_ = hiddenBossDashChainWarn_ + 0.35f;
        }
    };

    auto startHiddenBossDashChain = [&](float warn, float duration, float speed) -> bool
    {
        if (hiddenBossDashGlobalCd_ > 0.0f) return false;
        if (hiddenBossDashWarnT_ > 0.0f || hiddenBossDashT_ > 0.0f || hiddenBossDashRecoverT_ > 0.0f) return false;
        hiddenBossDashChainLeft_ = 3;
        hiddenBossDashChainGapT_ = 0.0f;
        hiddenBossDashChainWarn_ = warn;
        hiddenBossDashChainDuration_ = duration;
        hiddenBossDashChainSpeed_ = speed;
        hiddenBossDashGlobalCd_ = 10.0f;
        beginHiddenBossDash();
        return true;
    };

    auto summonHiddenBossClones = [&](EnemyType type, int desired)
    {
        if (hiddenBossCloneCd_ > 0.0f) return;
        const int maxClones = hiddenBossForm_ >= 3 ? 4 : 3;
        int cloneCount = HiddenBossCloneCount(enemies_);
        const int spawnCount = std::max(0, std::min(desired, maxClones - cloneCount));
        for (int i = 0; i < spawnCount; ++i)
        {
            Enemy clone{};
            clone.type = type;
            clone.kind = static_cast<int>(clone.type);
            const float a = boss_.spin + TwoPi * (static_cast<float>(i) + 0.35f) / static_cast<float>(std::max(1, spawnCount));
            clone.pos = boss_.pos + FromAngle(a) * (type == EnemyType::Teleport ? 3.25f : 2.45f);
            ClampInside(clone.pos, 0.48f);
            clone.height = Use3DRules() ? 0.82f : EnemyBodyY;
            clone.radius = type == EnemyType::Teleport ? 0.38f : 0.40f;
            clone.hp = (type == EnemyType::Teleport ? 130.0f : 105.0f) * CurrentDifficulty().enemyHpMul * MultiplayerHpMultiplier();
            if (hiddenBossForm_ >= 3) clone.hp *= 1.18f;
            clone.maxHp = clone.hp;
            clone.speed = type == EnemyType::Teleport ? 2.05f : 1.70f;
            clone.atk = boss_.atk * (hiddenBossForm_ >= 3 ? 0.64f : 0.54f);
            clone.score = type == EnemyType::Teleport ? 120 : 90;
            clone.color = Color{ 0.48f, 0.50f, 0.55f, 1.0f };
            clone.hiddenBossClone = true;
            clone.shootCd = 0.45f + 0.18f * static_cast<float>(i);
            clone.teleportCd = Rand(1.35f, 2.35f);
            clone.id = ++enemySerial_;
            SyncEnemy3D(clone);
            enemies_.push_back(clone);
            Burst(clone.pos, clone.color, 18);
            ++cloneCount;
        }
        if (spawnCount > 0)
        {
            hiddenBossCloneCd_ = hiddenBossForm_ >= 3 ? 4.4f : 6.2f;
            message_ = type == EnemyType::Teleport ? L"分身転移" : L"鏡分身";
            messageT_ = 1.0f;
        }
    };

    if (hiddenBossReflectT_ > 0.0f)
    {
        reflectPlayerShotsNearBoss(hiddenBossForm_ >= 3 ? 2.95f : 2.55f, hiddenBossForm_ >= 3 ? 5 : 3);
    }

    if (hiddenBossDashChainLeft_ > 0
        && hiddenBossDashChainGapT_ <= 0.0f
        && hiddenBossDashWarnT_ <= 0.0f
        && hiddenBossDashT_ <= 0.0f
        && hiddenBossDashRecoverT_ <= 0.0f)
    {
        beginHiddenBossDash();
    }

    const bool dashSequenceBusy = hiddenBossDashWarnT_ > 0.0f
        || hiddenBossDashT_ > 0.0f
        || hiddenBossDashRecoverT_ > 0.0f
        || hiddenBossDashChainGapT_ > 0.0f
        || hiddenBossDashChainLeft_ > 0;
    if (dashSequenceBusy)
    {
        hiddenPatternCd_ = std::max(hiddenPatternCd_, 0.16f);
    }
    else
    {
        hiddenPatternCd_ -= dt;
    }
    if (hiddenPatternCd_ <= 0.0f && !dashSequenceBusy)
    {
        const int enemyBullets = static_cast<int>(std::count_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.enemy && !s.dead; }));
        int remaining = std::max(0, HiddenBossBulletCap - enemyBullets);
        auto spawnAt = [&](V2 origin, float angle, float speed, float radius, Color color, float ttl, float curve = 0.0f, float accel = 0.0f)
        {
            if (remaining <= 0) return;
            SpawnEnemyShot(origin, angle, speed, boss_.atk * 0.62f, radius, color, ttl, curve, accel);
            --remaining;
        };
        auto spawn = [&](float angle, float speed, float radius, Color color, float ttl, float curve = 0.0f, float accel = 0.0f)
        {
            spawnAt(boss_.pos + FromAngle(angle) * (boss_.radius + 0.18f), angle, speed, radius, color, ttl, curve, accel);
        };
        auto spawnVisualAt = [&](V2 origin, float angle, float speed, float radius, Color color, float ttl, ShotVisualKind visual, float damageMul, float homing = 0.0f, bool auraKey = false, float curve = 0.0f, float accel = 0.0f)
        {
            if (remaining <= 0) return;
            Shot s{};
            s.enemy = true;
            s.pos = origin + FromAngle(angle) * 0.14f;
            s.vel = FromAngle(angle) * speed;
            s.radius = radius * 1.14f;
            s.damage = boss_.atk * damageMul;
            s.ttl = ttl;
            s.color = color;
            s.ownerIndex = -1;
            s.homingStrength = homing;
            s.angularVel = curve;
            s.accel = accel;
            s.visual = visual;
            s.hiddenBossAuraKey = auraKey;
            if (Use3DRules())
            {
                s.height = ClampFloat(ShotBodyY + 0.18f * std::sin(gameTime_ * 2.4f + angle * 2.6f), 0.18f, 1.04f);
            }
            SyncShot3D(s);
            shots_.push_back(s);
            --remaining;
        };
        auto spawnVisual = [&](float angle, float speed, float radius, Color color, float ttl, ShotVisualKind visual, float damageMul, float homing = 0.0f)
        {
            spawnVisualAt(boss_.pos + FromAngle(angle) * (boss_.radius + 0.34f), angle, speed, radius, color, ttl, visual, damageMul, homing);
        };
        auto spawnBlade = [&](float angle, float speed, float offset)
        {
            spawnVisual(angle + offset, speed, 0.108f, WithAlpha(Grape, 0.96f), 4.9f, ShotVisualKind::Blade, 0.70f);
        };
        auto spawnHoming = [&](float angle, float speed, float homing)
        {
            spawnVisual(angle, speed, 0.078f, WithAlpha(Red, 0.98f), 6.2f, ShotVisualKind::Homing, 0.50f, homing);
        };
        auto spawnAuraKey = [&](V2 origin, float angle, float speed, float ttl, float curve = 0.0f, float accel = 0.0f)
        {
            spawnVisualAt(origin, angle, speed, 0.090f, Gold, ttl, ShotVisualKind::Orb, 0.58f, 0.0f, true, curve, accel);
        };

        Player* targetPlayer = FindNearestPlayer(boss_.pos);
        const V2 targetPos = targetPlayer ? targetPlayer->pos : player_.pos;
        auto edgeOrigin = [&](float angleOffset)
        {
            V2 origin = targetPos + FromAngle(AngleOf(targetPos - boss_.pos) + angleOffset) * 7.8f;
            ClampInside(origin, 0.75f);
            return origin;
        };
        auto spawnPressureFan = [&](float sideAngle, int lanes, Color color, float speed, float ttl)
        {
            const V2 origin = edgeOrigin(sideAngle);
            const float toTarget = AngleOf(targetPos - origin);
            const int half = lanes / 2;
            for (int i = -half; i <= half; ++i)
            {
                spawnAt(origin, toTarget + i * 0.105f, speed + 0.06f * std::abs(i), 0.066f, color, ttl, 0.0f, -0.018f);
            }
        };

        const int phase = hiddenBossPhase_;
        Player* attackTarget = targetPlayer;
        const float aimed = AngleOf(targetPos - boss_.pos);
        const int pattern = hiddenPatternStep_ % 5;
        // 第1形態: 炎核を壊すギミック。
        // 核破壊前は弾密度を抑え、核を見つけて壊す余裕を残します。
        if (phase == 0)
        {
            const bool open = hiddenBossCoreOpenT_ > 0.0f;
            if (pattern == 0)
            {
                const int fan = open ? 2 : 1;
                for (int i = -fan; i <= fan; ++i) spawn(aimed + i * 0.15f, open ? 3.45f : 2.70f, 0.073f, open ? Gold : Sky, 5.8f);
                const int ringCount = open ? 14 : 8;
                for (int i = 0; i < ringCount; ++i) spawn(boss_.spin + TwoPi * i / static_cast<float>(ringCount), open ? 2.20f : 1.65f, 0.070f, open ? Red : Grape, 6.2f, open ? 0.06f : 0.03f);
                if ((hiddenPatternStep_ % 2) == 0) spawnPressureFan(Pi * 0.56f, open ? 7 : 5, Mint, open ? 3.95f : 3.20f, 7.0f);
                hiddenPatternCd_ = open ? 0.58f : 0.82f;
            }
            else if (pattern == 1)
            {
                for (int lane = -2; lane <= 2; ++lane)
                {
                    const float a = aimed + lane * 0.22f + std::sin(hiddenBossT_ * 0.8f) * 0.08f;
                    spawn(a, open ? 3.00f : 2.25f, 0.070f, open ? Gold : Mint, 6.4f, 0.04f);
                    if (open && std::abs(lane) == 2) spawn(a + 0.32f, 2.25f, 0.068f, Sky, 6.2f, -0.04f);
                }
                spawnPressureFan(-Pi * 0.62f, open ? 7 : 5, open ? Sky : Grape, open ? 3.85f : 3.15f, 6.8f);
                hiddenPatternCd_ = open ? 0.54f : 0.78f;
            }
            else if (pattern == 2)
            {
                const int count = open ? 16 : 10;
                for (int i = 0; i < count; ++i)
                {
                    const float speed = (i & 1) ? (open ? 3.05f : 2.35f) : (open ? 1.80f : 1.45f);
                    spawn(boss_.spin * 0.9f + TwoPi * i / static_cast<float>(count), speed, 0.072f, (i & 1) ? Gold : Grape, 6.8f, (i & 1) ? 0.08f : -0.04f);
                }
                if (open) spawnPressureFan((hiddenPatternStep_ & 1) ? Pi * 0.70f : -Pi * 0.70f, 7, Cream, 3.60f, 7.2f);
                hiddenPatternCd_ = open ? 0.62f : 0.90f;
            }
            else if (pattern == 3)
            {
                if (open)
                {
                    for (int i = -2; i <= 2; ++i)
                    {
                        spawn(aimed + i * 0.18f, 2.85f, 0.068f, (i & 1) ? Sky : Mint, 5.3f, i * 0.018f);
                    }
                }
                else
                {
                    int coreIndex = 0;
                    for (const auto& core : hiddenBossCores_)
                    {
                        if (!core.active) continue;
                        const float radial = AngleOf(core.pos - boss_.pos);
                        const float toPlayer = AngleOf((attackTarget ? attackTarget->pos : player_.pos) - core.pos);
                        spawnAt(core.pos, toPlayer, 2.65f, 0.070f, Sky, 5.9f, 0.035f * std::sin(hiddenBossT_ + coreIndex));
                        spawnAt(core.pos, radial + Pi * 0.5f, 2.10f, 0.066f, Gold, 6.2f, 0.045f);
                        spawnAt(core.pos, radial - Pi * 0.5f, 2.10f, 0.066f, Gold, 6.2f, -0.045f);
                        for (int k = 0; k < 3; ++k)
                        {
                            spawnAt(core.pos, boss_.spin * 0.55f + TwoPi * k / 3.0f + coreIndex * 0.42f, 1.42f, 0.062f, Mint, 6.8f);
                        }
                        EffectPulse pulse{};
                        pulse.pos = core.pos;
                        pulse.startRadius = core.radius * 0.65f;
                        pulse.endRadius = core.radius * 3.4f;
                        pulse.ttl = 0.34f;
                        pulse.life = pulse.ttl;
                        pulse.y = 0.23f;
                        pulse.color = Sky;
                        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
                        effectPulses_.push_back(pulse);
                        ++coreIndex;
                    }
                }
                hiddenPatternCd_ = open ? 0.82f : 1.15f;
            }
            else
            {
                if (open)
                {
                    const int lanes = 5;
                    for (int i = 0; i < lanes; ++i)
                    {
                        const float tLane = static_cast<float>(i) / static_cast<float>(lanes - 1) - 0.5f;
                        spawn(aimed + tLane * 0.48f, 2.95f, 0.068f, (i & 1) ? Sky : Mint, 5.2f, tLane * 0.035f);
                    }
                }
                else
                {
                    int coreIndex = 0;
                    for (const auto& core : hiddenBossCores_)
                    {
                        if (!core.active) continue;
                        EffectPulse pulse{};
                        pulse.pos = core.pos;
                        pulse.startRadius = core.radius * 0.80f;
                        pulse.endRadius = core.radius * 4.8f;
                        pulse.ttl = 0.42f;
                        pulse.life = pulse.ttl;
                        pulse.y = 0.24f;
                        pulse.color = Gold;
                        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
                        effectPulses_.push_back(pulse);
                        const int petals = 5;
                        for (int k = 0; k < petals; ++k)
                        {
                            const float a = core.angle + TwoPi * k / static_cast<float>(petals) + coreIndex * 0.18f;
                            spawnAt(core.pos, a, 1.85f + 0.12f * static_cast<float>(k & 1), 0.066f, (k & 1) ? Red : Gold, 6.6f, (k & 1) ? 0.055f : -0.035f);
                        }
                        ++coreIndex;
                    }
                }
                hiddenPatternCd_ = open ? 0.78f : 1.22f;
            }
        }
        // 第2形態: 金色キー弾を反射してオーラを剥がすギミック。
        // 反射対象が分かりやすいように、キー弾を出す瞬間は他弾を少し控えめにします。
        else if (phase == 1)
        {
            const bool broken = hiddenBossAuraBreakT_ > 0.0f;
            if (!broken && pattern == 0)
            {
                for (int i = -1; i <= 1; ++i)
                {
                    const float a = aimed + i * 0.24f;
                    spawnAuraKey(boss_.pos + FromAngle(a) * (boss_.radius + 0.32f), a, 2.45f, 7.0f, i * 0.025f, -0.015f);
                }
                for (int i = -2; i <= 2; i += 4)
                {
                    spawn(aimed + i * 0.18f, 2.15f, 0.066f, Mint, 6.0f, -i * 0.020f);
                }
                spawnPressureFan(Pi * 0.54f, 5, Mint, 3.20f, 7.0f);
                hiddenPatternCd_ = 0.86f;
            }
            else if (pattern == 0)
            {
                for (int arm = 0; arm < 4; ++arm)
                {
                    const float a = boss_.spin * 1.35f + arm * (TwoPi / 4.0f);
                    spawn(a, 2.85f, 0.074f, Gold, 6.4f, (arm & 1) ? -0.14f : 0.14f, 0.02f);
                    spawn(a + 0.22f, 3.25f, 0.070f, Red, 5.4f, (arm & 1) ? -0.08f : 0.08f);
                }
                spawnPressureFan(-Pi * 0.58f, 7, Gold, 3.90f, 6.8f);
                hiddenPatternCd_ = 0.56f;
            }
            else if (!broken && pattern == 1)
            {
                for (int i = 0; i < 8; ++i)
                {
                    const float a = boss_.spin + TwoPi * i / 8.0f;
                    if (i % 4 == 0) spawnAuraKey(boss_.pos + FromAngle(a) * (boss_.radius + 0.32f), a, 1.92f, 7.0f, 0.08f);
                    else spawn(a, 1.78f, 0.066f, (i & 1) ? Mint : Sky, 7.0f, 0.06f);
                }
                for (int i = -1; i <= 1; i += 2)
                {
                    const float a = aimed + i * 0.18f;
                    spawnAuraKey(boss_.pos + FromAngle(a) * (boss_.radius + 0.32f), a, 2.65f, 6.8f, i * 0.030f);
                }
                spawnPressureFan(-Pi * 0.64f, 5, Sky, 3.25f, 7.0f);
                hiddenPatternCd_ = 0.78f;
            }
            else if (pattern == 1)
            {
                for (int i = -4; i <= 4; ++i)
                {
                    if (i == 0) continue;
                    spawn(aimed + i * 0.095f, 3.85f, 0.068f, i % 3 == 0 ? Gold : Cream, 5.0f, 0.0f, -0.025f);
                }
                for (int i = 0; i < 8; ++i) spawn(boss_.spin + TwoPi * i / 8.0f, 2.0f, 0.072f, Gold, 6.6f, 0.10f);
                spawnPressureFan(Pi * 0.64f, 7, Red, 4.05f, 6.4f);
                hiddenPatternCd_ = 0.54f;
            }
            else if (pattern == 2)
            {
                const float wallBase = boss_.spin * 0.45f;
                const int count = broken ? 16 : 10;
                for (int i = 0; i < count; ++i)
                {
                    const float a = wallBase + TwoPi * i / static_cast<float>(count);
                    if (i % 5 == 0) continue;
                    if (!broken && i % 4 == 0)
                    {
                        spawnAuraKey(boss_.pos + FromAngle(a) * (boss_.radius + 0.32f), a, 2.35f, 6.6f, (i & 1) ? 0.075f : -0.075f);
                    }
                    else
                    {
                        spawn(a, broken ? 3.05f : 2.35f, 0.074f, broken ? ((i % 4 == 0) ? Gold : Mint) : ((i & 1) ? Mint : Sky), 6.2f);
                    }
                }
                spawnPressureFan((hiddenPatternStep_ & 1) ? Pi * 0.72f : -Pi * 0.72f, broken ? 7 : 5, Cream, broken ? 3.85f : 3.10f, 7.0f);
                hiddenPatternCd_ = broken ? 0.58f : 0.82f;
            }
            else if (pattern == 3)
            {
                hiddenBossReflectT_ = std::max(hiddenBossReflectT_, broken ? 0.72f : 1.15f);
                EffectPulse pulse{};
                pulse.pos = boss_.pos;
                pulse.startRadius = boss_.radius * 0.95f;
                pulse.endRadius = boss_.radius * (broken ? 2.35f : 2.85f);
                pulse.ttl = broken ? 0.52f : 0.85f;
                pulse.life = pulse.ttl;
                pulse.y = 0.26f;
                pulse.color = Gold;
                pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
                effectPulses_.push_back(pulse);
                const int count = broken ? 10 : 8;
                for (int i = 0; i < count; ++i)
                {
                    const float a = boss_.spin + TwoPi * i / static_cast<float>(count);
                    if (!broken && i % 2 == 0)
                    {
                        spawnAuraKey(boss_.pos + FromAngle(a) * (boss_.radius + 0.32f), a, 2.18f, 6.4f, (i & 1) ? 0.10f : -0.10f);
                    }
                    else
                    {
                        spawn(a, broken ? 3.15f : 2.10f, 0.072f, broken ? Gold : Mint, 6.0f, (i & 1) ? 0.10f : -0.10f);
                    }
                }
                hiddenPatternCd_ = broken ? 0.74f : 1.08f;
            }
            else
            {
                summonHiddenBossClones(EnemyType::Mirror, broken ? 1 : 2);
                for (int i = -2; i <= 2; ++i)
                {
                    const float a = aimed + i * 0.14f;
                    if (!broken && i == 0)
                    {
                        spawnAuraKey(boss_.pos + FromAngle(a) * (boss_.radius + 0.32f), a, 2.65f, 5.8f);
                    }
                    else
                    {
                        spawn(a, broken ? 3.45f : 2.55f, 0.070f, broken && i == 0 ? Gold : Cream, 5.4f);
                    }
                }
                hiddenPatternCd_ = broken ? 0.78f : 1.20f;
            }
        }
        // 第3形態: ギミック無しの殴り合いです。
        // 中央へ戻れる隙間を残しつつ、高密度の弾幕で回避力を試します。
        else
        {
            if (pattern == 0)
            {
                const float gap = std::sin(hiddenBossT_ * 1.2f) * 0.9f;
                for (int i = 0; i < 18; ++i)
                {
                    const float a = -Pi * 0.86f + i * (Pi * 1.72f / 17.0f);
                    if (std::fabs(a - gap) < 0.30f) continue;
                    spawn(a, 2.35f + (i % 3) * 0.16f, 0.066f, Cream, 6.8f, 0.025f * std::sin(i * 1.7f));
                }
                for (int i = -2; i <= 2; ++i) spawn(aimed + i * 0.10f, 4.20f, 0.066f, Red, 4.0f, 0.0f, -0.025f);
                spawnPressureFan(Pi * 0.50f, 7, Sky, 4.15f, 6.2f);
                hiddenPatternCd_ = 0.58f;
            }
            else if (pattern == 1)
            {
                for (int i = 0; i < 16; ++i)
                {
                    const float a = boss_.spin * 1.6f + i * (TwoPi / 16.0f);
                    spawn(a, 2.85f + 0.08f * (i % 4), 0.066f, (i & 1) ? Gold : Grape, 5.2f, (i & 1) ? 0.14f : -0.14f, 0.03f);
                    if (i % 2 == 0) spawn(a + 0.09f, 2.20f, 0.064f, Cream, 6.0f, (i & 1) ? -0.08f : 0.08f);
                }
                spawnPressureFan(-Pi * 0.50f, 7, Gold, 4.00f, 6.4f);
                hiddenPatternCd_ = 0.56f;
            }
            else if (pattern == 2)
            {
                for (int i = -3; i <= 3; ++i)
                {
                    spawnBlade(aimed, 3.45f + 0.16f * static_cast<float>(std::abs(i)), i * 0.13f);
                }
                for (int i = 0; i < 10; ++i)
                {
                    const float a = boss_.spin * -1.15f + i * (TwoPi / 10.0f);
                    spawn(a, 2.40f, 0.066f, (i % 3 == 0) ? Grape : Cream, 6.0f, -0.09f);
                }
                hiddenPatternCd_ = 0.88f;
            }
            else if (pattern == 3)
            {
                hiddenBossReflectT_ = std::max(hiddenBossReflectT_, 1.05f);
                for (int i = -2; i <= 2; ++i)
                {
                    spawnHoming(aimed + i * 0.22f, 2.65f + 0.10f * static_cast<float>(std::abs(i)), 1.34f);
                }
                for (int i = 0; i < 8; ++i)
                {
                    const float a = boss_.spin + TwoPi * i / 8.0f;
                    spawn(a, 2.15f, 0.064f, (i & 1) ? Grape : Red, 5.5f, (i & 1) ? 0.08f : -0.08f);
                }
                hiddenPatternCd_ = 1.02f;
            }
            else
            {
                summonHiddenBossClones(EnemyType::Teleport, 2);
                const bool dashStarted = startHiddenBossDashChain(0.54f, 0.58f, 16.2f);
                if (dashStarted)
                {
                    const int count = 10;
                    for (int i = 0; i < count; ++i)
                    {
                        const float a = boss_.spin * 0.8f + TwoPi * i / static_cast<float>(count);
                        spawn(a, (i & 1) ? 2.95f : 2.15f, 0.066f, (i % 3 == 0) ? Red : Grape, 5.4f, (i & 1) ? 0.11f : -0.08f);
                    }
                }
                else
                {
                    for (int i = -2; i <= 2; ++i)
                    {
                        spawnBlade(aimed, 3.35f + 0.12f * static_cast<float>(std::abs(i)), i * 0.16f);
                    }
                    for (int i = -1; i <= 1; ++i)
                    {
                        spawnHoming(aimed + i * 0.28f, 2.55f, 1.05f);
                    }
                    for (int i = 0; i < 8; ++i)
                    {
                        const float a = boss_.spin * 0.9f + TwoPi * i / 8.0f;
                        spawn(a, 2.35f, 0.064f, (i & 1) ? Grape : Cream, 5.8f, (i & 1) ? 0.08f : -0.08f);
                    }
                }
                spawnPressureFan((hiddenPatternStep_ & 1) ? Pi * 0.62f : -Pi * 0.62f, dashStarted ? 7 : 5, Red, dashStarted ? 3.75f : 3.35f, 6.2f);
                hiddenPatternCd_ = dashStarted ? 1.18f : 1.05f;
            }
        }
        ++hiddenPatternStep_;
    }

    UpdateShots(dt);
    ReleaseCaughtIfNoBomb();
    UpdatePickups(dt);
    UpdateParticles(dt);
    UpdateCamera(dt);
    for (auto& s : slashes_)
    {
        s.ttl -= dt;
    }

    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.dead || s.ttl <= 0.0f; }), shots_.end());
    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) { return e.dead; }), enemies_.end());
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& p) { return p.ttl <= 0.0f; }), pickups_.end());
    slashes_.erase(std::remove_if(slashes_.begin(), slashes_.end(), [](const Slash& s) { return s.ttl <= 0.0f; }), slashes_.end());
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());

    NormalizePlayerLifeStates();
    if (AllPlayersDown())
    {
        screen_ = Screen::GameOver;
        gameOverChoice_ = GameOverChoice::Retry;
        message_ = L"Game Over";
        messageT_ = 999.0f;
    }
}
