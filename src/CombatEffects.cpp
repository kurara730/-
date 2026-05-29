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

void SweetsApp::UpdateParticles(float dt)
{
    for (auto& p : particles_)
    {
        p.ttl -= dt;
        p.pos += p.vel * dt;
        p.y += p.vy * dt;
        p.vy -= 6.0f * dt;
        p.pos3 = Grounded3D(p.pos, p.y);
        p.vel3 = Grounded3D(p.vel, p.vy);
    }
}

void SweetsApp::UpdateEffectVisuals(float dt)
{
    if (screenFlashT_ > 0.0f)
    {
        screenFlashT_ = std::max(0.0f, screenFlashT_ - dt);
    }
    for (auto& pulse : effectPulses_)
    {
        pulse.ttl -= dt;
        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
    }
    effectPulses_.erase(
        std::remove_if(effectPulses_.begin(), effectPulses_.end(), [](const EffectPulse& pulse) { return pulse.ttl <= 0.0f; }),
        effectPulses_.end());
    for (auto& visual : swordEffectVisuals_)
    {
        visual.ttl -= dt;
        visual.pos3 = Grounded3D(visual.pos, visual.height);
    }
    swordEffectVisuals_.erase(
        std::remove_if(swordEffectVisuals_.begin(), swordEffectVisuals_.end(), [](const SwordEffectVisual& visual) { return visual.ttl <= 0.0f; }),
        swordEffectVisuals_.end());
}

void SweetsApp::SpawnEnemyShot(V2 pos, float angle, float speed, float damage, float radius, Color color, float ttl, float angularVel, float accel)
{
    if (screen_ == Screen::HiddenBoss)
    {
        const int enemyBullets = static_cast<int>(std::count_if(shots_.begin(), shots_.end(), [](const Shot& s) { return s.enemy && !s.dead; }));
        if (enemyBullets >= HiddenBossBulletCap) return;
        radius *= 1.14f;
    }
    else
    {
        const DifficultyDef& diff = CurrentDifficulty();
        speed *= diff.bulletSpeedMul;
        radius *= diff.enemyShotRadiusMul;
        if (CurrentEncounterProfile() == EncounterProfile::MobRelease)
        {
            speed *= 0.92f;
            damage *= 0.82f;
        }
    }

    Shot s{};
    s.enemy = true;
    s.pos = pos + FromAngle(angle) * 0.12f;
    s.vel = FromAngle(angle) * speed;
    s.radius = radius;
    s.damage = damage;
    s.ttl = ttl;
    s.color = color;
    s.angularVel = angularVel;
    s.accel = accel;
    s.ownerIndex = -1;
    if (Use3DRules())
    {
        s.height = ClampFloat(ShotBodyY + 0.20f * std::sin(gameTime_ * 2.1f + angle * 3.0f), 0.16f, 1.05f);
    }
    SyncShot3D(s);
    shots_.push_back(s);
}

void SweetsApp::Burst(V2 p, Color c, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const float a = Rand(0.0f, TwoPi);
        const float spd = Rand(0.6f, 3.0f);
        Particle q{};
        q.pos = p;
        q.vel = FromAngle(a) * spd;
        q.y = Rand(0.12f, 0.8f);
        q.vy = Rand(1.0f, 4.2f);
        q.pos3 = Grounded3D(q.pos, q.y);
        q.vel3 = Grounded3D(q.vel, q.vy);
        q.ttl = Rand(0.35f, 0.9f);
        q.color = c;
        particles_.push_back(q);
    }
}

void SweetsApp::PlayCombatEffect(const std::wstring& id, V2 position, float y, float rotationY, float scale, Color fallbackColor, int fallbackCount)
{
    const bool sword = id == L"sword_slash";
    const bool shortcake = id == L"ult_shortcake";
    const bool chocolate = id == L"ult_chocolate";
    const bool cheese = id == L"ult_cheese";
    const bool roll = id == L"ult_roll";
    const bool ultimate = shortcake || chocolate || cheese || roll;

    const bool chargedSword = sword && scale >= 1.30f;
#if defined(_DEBUG)
    const float effectFx = sword ? ClampFloat(debug_.swordFx, 0.0f, 2.0f) : (ultimate ? ClampFloat(debug_.ultimateFx, 0.0f, 2.0f) : 1.0f);
#else
    const float effectFx = 1.0f;
#endif
    const float boostedScale = scale * (sword ? (chargedSword ? 2.35f : 1.85f) : (ultimate ? 2.5f : 1.8f)) * effectFx;
    const bool played = effekseer_.Play(id, position, y, rotationY, boostedScale);

    auto addPulse = [&](float startRadius, float endRadius, float life, Color color, float pulseY)
    {
        EffectPulse pulse{};
        pulse.pos = position;
        pulse.startRadius = startRadius;
        pulse.endRadius = endRadius;
        pulse.ttl = life;
        pulse.life = life;
        pulse.y = pulseY;
        pulse.color = color;
        pulse.pos3 = Grounded3D(pulse.pos, pulse.y);
        effectPulses_.push_back(pulse);
    };

    auto addFlash = [&](Color color, float life)
    {
        if (life > screenFlashT_)
        {
            screenFlashT_ = life;
            screenFlashLife_ = std::max(0.01f, life);
            screenFlashColor_ = color;
        }
    };

    auto addSlashVisual = [&](float range, float arc, float life, Color color, float angle)
    {
        Slash visual{};
        visual.pos = position;
        visual.angle = angle;
        visual.range = range;
        visual.arc = arc;
        visual.life = life;
        visual.ttl = life;
        visual.damage = 0.0f;
        visual.color = color;
        SyncSlash3D(visual);
        slashes_.push_back(visual);
    };

    auto addSwordVisual = [&](bool charged)
    {
        SwordEffectVisual visual{};
        visual.pos = position;
        visual.angle = rotationY;
        visual.scale = scale;
        visual.range = charged ? 3.35f : 2.20f;
        visual.arc = charged ? 0.70f : 0.52f;
        visual.height = y;
        visual.life = charged ? 0.34f : 0.22f;
        visual.ttl = visual.life;
        visual.charged = charged;
        visual.pos3 = Grounded3D(visual.pos, visual.height);
        swordEffectVisuals_.push_back(visual);
    };

    if (sword)
    {
        addSwordVisual(chargedSword);
    }
    else if (shortcake)
    {
        addPulse(0.6f, 4.2f * scale, 0.42f, Berry, y + 0.04f);
        addPulse(1.1f, 6.4f * scale, 0.55f, Gold, y + 0.05f);
        addPulse(0.2f, 2.6f * scale, 0.28f, Cream, y + 0.10f);
        Burst(position, Berry, fallbackCount + 120);
        Burst(position, Gold, 52);
        addFlash(Berry, 0.22f);
    }
    else if (chocolate)
    {
        addPulse(1.0f, ArenaRadius * 0.94f, 0.48f, Choco, y + 0.04f);
        addPulse(0.3f, 4.8f * scale, 0.34f, Gold, y + 0.08f);
        for (int i = 0; i < 8; ++i)
        {
            addSlashVisual(ArenaRadius * 0.92f, 0.22f, 0.32f, (i & 1) ? Choco : Cream, rotationY + TwoPi * i / 8.0f);
        }
        Burst(position, Choco, fallbackCount + 110);
        Burst(position, Cream, 36);
        addFlash(Choco, 0.20f);
    }
    else if (cheese)
    {
        addPulse(0.8f, 2.4f * scale, 0.34f, Gold, y + 0.05f);
        addPulse(1.4f, 3.6f * scale, 0.52f, Cream, y + 0.08f);
        addPulse(2.1f, 5.0f * scale, 0.70f, Gold, y + 0.10f);
        Burst(position, Gold, fallbackCount + 95);
        Burst(position, Cream, 34);
        addFlash(Gold, 0.18f);
    }
    else if (roll)
    {
        addPulse(0.9f, 4.0f * scale, 0.34f, Cream, y + 0.04f);
        addPulse(1.6f, ArenaRadius * 0.98f, 0.58f, Sky, y + 0.05f);
        addPulse(2.8f, ArenaRadius * 1.18f, 0.76f, Cream, y + 0.08f);
        for (int i = 0; i < 6; ++i)
        {
            addSlashVisual(ArenaRadius * 0.78f, 0.18f, 0.30f, Cream, rotationY + TwoPi * i / 6.0f);
        }
        Burst(position, Cream, fallbackCount + 115);
        Burst(position, Sky, 46);
        addFlash(Cream, 0.24f);
    }
    else
    {
        addPulse(0.4f, 2.0f * scale, 0.28f, fallbackColor, y + 0.04f);
        Burst(position, fallbackColor, fallbackCount + 18);
        addFlash(fallbackColor, 0.08f);
    }

    if (!played && !sword)
    {
        Burst(position, fallbackColor, fallbackCount + (ultimate ? 80 : 20));
    }
}

