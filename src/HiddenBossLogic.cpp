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
    ResetHiddenBossCores();
    screen_ = Screen::HiddenBoss;
    message_ = L"Hidden Boss Phase 1";
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
    if (Rand(0.0f, 1.0f) < dt * 18.0f)
    {
        Particle p{};
        const float a = Rand(0.0f, TwoPi);
        p.pos = boss_.pos + FromAngle(a) * Rand(0.35f, 1.15f);
        p.vel = FromAngle(a + Pi * 0.5f) * Rand(0.1f, 0.55f);
        p.y = Rand(0.2f, 1.2f);
        p.vy = Rand(0.3f, 1.2f);
        p.ttl = Rand(0.35f, 0.85f);
        p.color = Rand(0.0f, 1.0f) < 0.65f ? Gold : Cream;
        p.pos3 = Grounded3D(p.pos, p.y);
        particles_.push_back(p);
    }
    UpdateParticles(dt);
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

    // HP残量から現在形態を再計算します。形態ごとにギミックと弾幕が変わります。
    hiddenBossForm_ = HiddenBossFormFromHp(std::max(1.0f, boss_.hp), hiddenBossGaugeHp_);
    boss_.phase = hiddenBossForm_;
    boss_.spin += dt * (1.35f + hiddenBossForm_ * 0.20f);
    boss_.pos.x = std::sin(hiddenBossT_ * (hiddenBossForm_ >= 3 ? 0.80f : 0.55f)) * (hiddenBossForm_ >= 3 ? 3.0f : 2.4f);
    boss_.pos.z = -3.0f + std::sin(hiddenBossT_ * (hiddenBossForm_ == 2 ? 0.55f : 0.37f)) * (hiddenBossForm_ >= 3 ? 0.70f : 0.48f);
    SyncBoss3D(boss_);
    if (boss_.flash > 0.0f) boss_.flash -= dt;

    // 形態移行演出中は入力/被弾/弾幕更新を止め、ボスへ視線を集めます。
    if (hiddenBossPhaseIntroT_ > 0.0f)
    {
        mouseRightReleased_ = false;
        hiddenBossPhaseIntroT_ = std::max(0.0f, hiddenBossPhaseIntroT_ - dt);
        boss_.flash = std::max(boss_.flash, 0.08f);
        boss_.spin += dt * (hiddenBossForm_ >= 3 ? 3.6f : 2.6f);
        UpdateParticles(dt);
        particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());
        return;
    }

    UpdateStage(dt);
    UpdatePlayer(dt);
    UpdateCoopPlayers(dt);
    UpdateHiddenBossCores(dt);

    const int activeHiddenPhase = std::max(0, std::min(hiddenBossForm_ - 1, 2));
    // 形態が切り替わった瞬間に敵弾を消し、事故を防いで次のルールをメッセージで知らせます。
    if (activeHiddenPhase != hiddenBossPhase_)
    {
        hiddenBossPhase_ = activeHiddenPhase;
        hiddenPatternStep_ = 0;
        hiddenPatternCd_ = 0.22f;
        hiddenBossReflectCount_ = 0;
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
        message_ = hiddenBossPhase_ == 0 ? L"炎核を壊せ" : (hiddenBossPhase_ == 1 ? L"金色弾を反射しろ" : L"回避して攻めろ");
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
                ss << L"金色弾を反射しろ " << hiddenBossReflectCount_ << L"/" << HiddenBossReflectBreakCount;
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

    hiddenPatternCd_ -= dt;
    if (hiddenPatternCd_ <= 0.0f)
    {
        const int enemyBullets = static_cast<int>(std::count_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.enemy && !s.dead; }));
        int remaining = std::max(0, HiddenBossBulletCap - enemyBullets);
        auto spawn = [&](float angle, float speed, float radius, Color color, float ttl, float curve = 0.0f, float accel = 0.0f)
        {
            if (remaining <= 0) return;
            SpawnEnemyShot(boss_.pos + FromAngle(angle) * (boss_.radius + 0.18f), angle, speed, boss_.atk * 0.62f, radius, color, ttl, curve, accel);
            --remaining;
        };

        const int phase = hiddenBossPhase_;
        const float aimed = AngleOf(player_.pos - boss_.pos);
        const int pattern = hiddenPatternStep_ % 3;
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
                hiddenPatternCd_ = open ? 0.68f : 0.95f;
            }
            else if (pattern == 1)
            {
                for (int lane = -2; lane <= 2; ++lane)
                {
                    const float a = aimed + lane * 0.22f + std::sin(hiddenBossT_ * 0.8f) * 0.08f;
                    spawn(a, open ? 3.00f : 2.25f, 0.070f, open ? Gold : Mint, 6.4f, 0.04f);
                    if (open && std::abs(lane) == 2) spawn(a + 0.32f, 2.25f, 0.068f, Sky, 6.2f, -0.04f);
                }
                hiddenPatternCd_ = open ? 0.62f : 0.90f;
            }
            else
            {
                const int count = open ? 16 : 10;
                for (int i = 0; i < count; ++i)
                {
                    const float speed = (i & 1) ? (open ? 3.05f : 2.35f) : (open ? 1.80f : 1.45f);
                    spawn(boss_.spin * 0.9f + TwoPi * i / static_cast<float>(count), speed, 0.072f, (i & 1) ? Gold : Grape, 6.8f, (i & 1) ? 0.08f : -0.04f);
                }
                hiddenPatternCd_ = open ? 0.72f : 1.05f;
            }
        }
        // 第2形態: 金色弾を反射してオーラを剥がすギミック。
        // 反射対象が分かりやすいように、金色弾を出す瞬間は他弾を少し控えめにします。
        else if (phase == 1)
        {
            const bool broken = hiddenBossAuraBreakT_ > 0.0f;
            if (!broken && pattern == 0)
            {
                for (int i = -1; i <= 1; ++i) spawn(aimed + i * 0.24f, 2.45f, 0.088f, Gold, 7.0f, 0.0f, -0.015f);
                hiddenPatternCd_ = 0.95f;
            }
            else if (pattern == 0)
            {
                for (int arm = 0; arm < 4; ++arm)
                {
                    const float a = boss_.spin * 1.35f + arm * (TwoPi / 4.0f);
                    spawn(a, 2.85f, 0.074f, Gold, 6.4f, (arm & 1) ? -0.14f : 0.14f, 0.02f);
                    spawn(a + 0.22f, 3.25f, 0.070f, Red, 5.4f, (arm & 1) ? -0.08f : 0.08f);
                }
                hiddenPatternCd_ = 0.66f;
            }
            else if (!broken && pattern == 1)
            {
                for (int i = 0; i < 8; ++i) spawn(boss_.spin + TwoPi * i / 8.0f, 1.85f, 0.070f, (i % 4 == 0) ? Gold : Mint, 7.0f, 0.08f);
                for (int i = -1; i <= 1; i += 2) spawn(aimed + i * 0.18f, 2.65f, 0.086f, Gold, 6.8f);
                hiddenPatternCd_ = 0.85f;
            }
            else if (pattern == 1)
            {
                for (int i = -4; i <= 4; ++i)
                {
                    if (i == 0) continue;
                    spawn(aimed + i * 0.095f, 3.85f, 0.068f, i % 3 == 0 ? Gold : Cream, 5.0f, 0.0f, -0.025f);
                }
                for (int i = 0; i < 8; ++i) spawn(boss_.spin + TwoPi * i / 8.0f, 2.0f, 0.072f, Gold, 6.6f, 0.10f);
                hiddenPatternCd_ = 0.62f;
            }
            else
            {
                const float wallBase = boss_.spin * 0.45f;
                const int count = broken ? 16 : 10;
                for (int i = 0; i < count; ++i)
                {
                    const float a = wallBase + TwoPi * i / static_cast<float>(count);
                    if (i % 5 == 0) continue;
                    spawn(a, broken ? 3.05f : 2.35f, 0.076f, (i % 4 == 0) ? Gold : Mint, 6.2f);
                }
                hiddenPatternCd_ = broken ? 0.68f : 0.95f;
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
                hiddenPatternCd_ = 0.72f;
            }
            else if (pattern == 1)
            {
                for (int i = 0; i < 16; ++i)
                {
                    const float a = boss_.spin * 1.6f + i * (TwoPi / 16.0f);
                    spawn(a, 2.85f + 0.08f * (i % 4), 0.066f, (i & 1) ? Gold : Grape, 5.2f, (i & 1) ? 0.14f : -0.14f, 0.03f);
                    if (i % 2 == 0) spawn(a + 0.09f, 2.20f, 0.064f, Cream, 6.0f, (i & 1) ? -0.08f : 0.08f);
                }
                hiddenPatternCd_ = 0.68f;
            }
            else
            {
                for (int i = -4; i <= 4; ++i) spawn(aimed + i * 0.075f, 3.85f + (std::abs(i) % 3) * 0.24f, 0.066f, i == 0 ? Red : Gold, 4.8f, 0.018f * static_cast<float>(i), -0.035f);
                for (int i = 0; i < 14; ++i)
                {
                    const float a = boss_.spin * -1.15f + i * (TwoPi / 14.0f);
                    spawn(a, 2.75f, 0.066f, (i % 3 == 0) ? Grape : Cream, 6.0f, -0.14f);
                }
                hiddenPatternCd_ = 0.75f;
            }
        }
        ++hiddenPatternStep_;
    }

    UpdateShots(dt);
    UpdatePickups(dt);
    UpdateParticles(dt);
    for (auto& s : slashes_)
    {
        s.ttl -= dt;
    }

    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.dead || s.ttl <= 0.0f; }), shots_.end());
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
