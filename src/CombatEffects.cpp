#include "SweetsApp.h"
#include "ReflectionSystem.h"
#include "StageFactory.h"

// CombatEffects.cpp は、弾や攻撃の見た目を支える軽量演出を扱います。
// Effekseerが出ない環境でも、ここで発光リングや粒子を出して攻撃が見えるようにします。

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

// 通常の小粒子更新です。寿命が減り、速度と高さを反映して移動します。
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

// 剣FXや発光リングなど、寿命付きの見た目データを更新します。
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
    for (auto& notice : combatNotices_)
    {
        notice.ttl -= dt;
    }
    combatNotices_.erase(
        std::remove_if(combatNotices_.begin(), combatNotices_.end(), [](const CombatNotice& notice) { return notice.ttl <= 0.0f; }),
        combatNotices_.end());
    for (auto& telegraph : worldTelegraphs_)
    {
        telegraph.ttl -= dt;
    }
    worldTelegraphs_.erase(
        std::remove_if(worldTelegraphs_.begin(), worldTelegraphs_.end(), [](const WorldTelegraph& telegraph) { return telegraph.ttl <= 0.0f; }),
        worldTelegraphs_.end());
    // ダメージ数値：上へ流れながら減速し、寿命で消える。
    for (auto& dn : damageNumbers_)
    {
        dn.ttl -= dt;
        dn.pos += dn.vel * dt;
        dn.vel = dn.vel * 0.90f; // だんだん止まる
    }
    damageNumbers_.erase(
        std::remove_if(damageNumbers_.begin(), damageNumbers_.end(), [](const DamageNumber& dn) { return dn.ttl <= 0.0f; }),
        damageNumbers_.end());
}

// ダメージ数値（モンハンライズ風）を1つ湧かせる。小さなダメージや連続ヒットは直近の数値へ合算する。
void SweetsApp::SpawnDamageNumber(V2 pos, float value, Color color, bool crit)
{
    if (value < 1.0f) return; // 微小な連続ダメージ（反射の毎フレーム等）は出さない
    // 近くに出たばかりの数値があれば合算（数字の洪水を防ぐ）。
    for (auto& dn : damageNumbers_)
    {
        if (dn.ttl > dn.life - 0.18f && Len(dn.pos - pos) < 1.1f)
        {
            dn.value += value;
            dn.ttl = dn.life;          // 寿命リフレッシュ
            dn.crit = dn.crit || crit;
            if (crit) dn.color = color;
            return;
        }
    }
    if (damageNumbers_.size() > 60) return; // 上限
    DamageNumber dn{};
    dn.pos = pos + V2{ Rand(-0.4f, 0.4f), Rand(-0.2f, 0.2f) };
    dn.vel = V2{ Rand(-0.6f, 0.6f), 2.6f }; // 上（+z）へ
    dn.value = value;
    dn.life = crit ? 0.95f : 0.7f;
    dn.ttl = dn.life;
    dn.color = color;
    dn.crit = crit;
    damageNumbers_.push_back(dn);
}

// 敵弾を1発作る共通処理です。
// 難易度ごとの弾速、弾サイズ、弾数調整はここへ集約しています。
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

// 爆発や反射時に使う短い粒子発生です。
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

// 攻撃エフェクト再生入口です。
// Effekseer再生に成功しても、同時に補助FXを出して「見えない」状態を避けます。
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

