#include "SweetsApp.h"

#include <algorithm>

namespace
{
int HiddenBossFormFromHp(float hp)
{
    if (hp <= HiddenBossGaugeHp) return 3;
    if (hp <= HiddenBossGaugeHp * 2.0f) return 2;
    return 1;
}
}

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
    screenFlashT_ = 0.0f;
    stage_ = StageType::BossArena;
    stageTimer_ = 0.0f;
    shrinkRadius_ = ArenaRadius;

    boss_ = {};
    boss_.active = true;
    boss_.bossType = BossType::HiddenBoss;
    boss_.type = static_cast<int>(BossType::HiddenBoss);
    boss_.pos = { 0.0f, -4.8f };
    boss_.height = BossBodyY + 0.22f;
    boss_.radius = 1.05f;
    boss_.maxHp = HiddenBossTotalHp;
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
    hiddenBossPhaseIntroT_ = 0.0f;
    hiddenBossPhaseIntroLife_ = 0.0f;
    screen_ = Screen::HiddenBoss;
    message_ = L"Hidden Boss";
    messageT_ = 3.0f;
}

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

void SweetsApp::UpdateHiddenBossIntro(float dt)
{
    hiddenIntroT_ += dt;
    boss_.active = true;
    boss_.bossType = BossType::HiddenBoss;
    boss_.pos = { 0.0f, -4.8f + ClampFloat(hiddenIntroT_ / 2.2f, 0.0f, 1.0f) * 2.6f };
    boss_.radius = 1.05f;
    UpdateParticles(dt);
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ttl <= 0.0f || p.y < -0.1f; }), particles_.end());
    if (hiddenIntroT_ >= 2.2f)
    {
        StartHiddenBoss();
    }
}

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

    hiddenBossForm_ = HiddenBossFormFromHp(std::max(1.0f, boss_.hp));
    boss_.phase = hiddenBossForm_;
    boss_.spin += dt * (1.35f + hiddenBossForm_ * 0.20f);
    boss_.pos.x = std::sin(hiddenBossT_ * 0.55f) * 2.7f;
    boss_.pos.z = -3.0f + std::sin(hiddenBossT_ * 0.37f) * 0.55f;
    if (boss_.flash > 0.0f) boss_.flash -= dt;

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

    const int activeHiddenPhase = std::max(0, std::min(hiddenBossForm_ - 1, 2));
    if (activeHiddenPhase != hiddenBossPhase_)
    {
        hiddenBossPhase_ = activeHiddenPhase;
        hiddenPatternStep_ = 0;
        hiddenPatternCd_ = 0.22f;
        for (auto& s : shots_)
        {
            if (s.enemy) s.dead = true;
        }
        message_ = hiddenBossPhase_ == 0 ? L"Hidden Boss Phase 1" : (hiddenBossPhase_ == 1 ? L"Hidden Boss Phase 2" : L"Hidden Boss Phase 3");
        messageT_ = 1.6f;
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
        if (phase == 0)
        {
            for (int i = -2; i <= 2; ++i) spawn(aimed + i * 0.13f, 3.45f + 0.10f * std::abs(i), 0.075f, Sky, 5.8f);
            for (int i = 0; i < 14; ++i) spawn(boss_.spin + TwoPi * i / 14.0f, 2.20f, 0.070f, Grape, 6.2f, 0.05f);
            hiddenPatternCd_ = 0.56f;
        }
        else if (phase == 1)
        {
            for (int arm = 0; arm < 4; ++arm)
            {
                const float a = boss_.spin * 1.6f + arm * Pi * 0.5f;
                spawn(a, 2.8f + 0.08f * (hiddenPatternStep_ % 5), 0.072f, Gold, 6.4f, (arm & 1) ? -0.16f : 0.16f, 0.03f);
                spawn(a + 0.18f, 3.25f, 0.070f, Mint, 5.4f, (arm & 1) ? -0.10f : 0.10f);
            }
            if ((hiddenPatternStep_ % 3) == 0)
            {
                for (int i = -3; i <= 3; ++i) spawn(aimed + i * 0.09f, 4.2f, 0.068f, Red, 4.6f, 0.0f, -0.04f);
            }
            hiddenPatternCd_ = 0.32f;
        }
        else
        {
            const float gap = std::sin(hiddenBossT_ * 1.2f) * 0.9f;
            for (int i = 0; i < 22; ++i)
            {
                const float a = -Pi * 0.94f + i * (Pi * 1.88f / 21.0f);
                if (std::fabs(a - gap) < 0.22f) continue;
                spawn(a, 2.55f + (i % 3) * 0.18f, 0.066f, Cream, 6.8f, 0.03f * std::sin(i * 1.7f));
            }
            for (int i = 0; i < 16; ++i)
            {
                const float a = boss_.spin * 1.8f + i * (TwoPi / 16.0f);
                spawn(a, 3.05f + 0.08f * (i % 4), 0.066f, (i & 1) ? Gold : Grape, 5.2f, (i & 1) ? 0.17f : -0.17f, 0.04f);
            }
            for (int i = -3; i <= 3; ++i) spawn(aimed + i * 0.08f, 4.75f, 0.066f, Red, 4.0f, 0.0f, -0.03f);
            hiddenPatternCd_ = 0.58f;
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
