#include "SweetsApp.h"

void SweetsApp::UpdatePlayer(float dt)
{
    if (player_.downed)
    {
        mouseRightReleased_ = false;
        return;
    }

    V2 dir{};
    if (KeyDown('W') || KeyDown(VK_UP)) dir.z += 1.0f;
    if (KeyDown('S') || KeyDown(VK_DOWN)) dir.z -= 1.0f;
    if (KeyDown('A') || KeyDown(VK_LEFT)) dir.x -= 1.0f;
    if (KeyDown('D') || KeyDown(VK_RIGHT)) dir.x += 1.0f;
    dir = Normalize(dir);

    player_.focus = KeyDown(VK_SHIFT);
    const float focusMul = player_.focus ? 0.42f : 1.0f;
    const float spd = player_.speed * focusMul * (player_.speedBuffT > 0.0f ? 1.55f : 1.0f);
    if (player_.dashT > 0.0f)
    {
        player_.dashT -= dt;
        player_.pos += player_.dashVel * dt;
    }
    else
    {
        player_.vel = dir * spd;
        player_.pos += player_.vel * dt;
    }
    ClampInside(player_.pos, player_.radius);

    for (const auto& o : obstacles_)
    {
        if (o.damageField) continue;
        V2 d = player_.pos - o.pos;
        const float l = Len(d);
        const float minD = player_.radius + o.radius;
        if (l > 0.0001f && l < minD)
        {
            player_.pos = o.pos + d / l * minD;
        }
    }

    const V2 aimPoint = ScreenToWorld(mouseX_, mouseY_);
    player_.face = AngleOf(aimPoint - player_.pos);

    if (player_.fireCd > 0.0f) player_.fireCd -= dt;
    const bool primaryHeld = mouseLeft_ || KeyDown(VK_SPACE);
    if (primaryHeld && player_.fireCd <= 0.0f)
    {
        FirePrimaryFor(player_, 0, player_.face);
    }

    if (mouseRight_)
    {
        player_.chargeT += dt;
        player_.chargeReady = player_.chargeT >= 0.55f;
        player_.charging = true;
    }
    else if (mouseRightReleased_ || player_.charging)
    {
        if (player_.chargeReady && player_.chargeCd <= 0.0f)
        {
            FireCharged(player_, 0, player_.face, aimPoint);
        }
        else if (mouseRightReleased_ && player_.character == CharacterType::Cheese && obstacles_.size() < 20)
        {
            Obstacle wall{};
            wall.pos = aimPoint;
            ClampInside(wall.pos, 0.8f);
            wall.radius = 0.52f;
            wall.hp = 110.0f + player_.level * 18.0f;
            wall.ttl = 7.5f;
            wall.kind = 2;
            wall.ownerIndex = 0;
            wall.reflectPower = 1.35f + player_.corePower;
            wall.cheeseWall = true;
            wall.color = Gold;
            obstacles_.push_back(wall);
        }
        player_.charging = false;
        player_.chargeReady = false;
        player_.chargeT = 0.0f;
    }
    mouseRightReleased_ = false;
    prevMouseLeft_ = primaryHeld;
}

void SweetsApp::FirePrimary()
{
    FirePrimaryFor(player_, 0, player_.face);
}

void SweetsApp::FirePrimaryFor(Player& p, int ownerIndex, float aim)
{
    if (p.fireCd > 0.0f || p.downed) return;
    const auto& def = Weapons[static_cast<int>(p.weapon)];
    float dmgScale = 1.0f + (p.level - 1) * 0.18f + p.corePower;
    dmgScale *= p.damageMul;
    if (p.dmgBuffT > 0.0f) dmgScale *= 1.6f;

    p.fireCd = def.cooldown * p.cooldownMul;
    if (p.weapon == Weapon::Chocolate)
    {
        DoMeleeFor(p, ownerIndex, aim);
        return;
    }

    int count = 1;
    float spread = 0.0f;
    if (p.weapon == Weapon::Strawberry && p.level >= 3)
    {
        count = 2;
        spread = 0.12f;
    }
    if (p.weapon == Weapon::Cheese && p.level >= 4)
    {
        count = 2;
        spread = 0.20f;
    }
    if (p.spreadT > 0.0f)
    {
        count = std::max(count, 3);
        spread = std::max(spread, 0.42f);
    }

    for (int n = 0; n < count; ++n)
    {
        const float a = aim + (count > 1 ? (static_cast<float>(n) / (count - 1) - 0.5f) * spread : 0.0f);
        Shot s{};
        s.pos = p.pos + FromAngle(a) * (p.radius + 0.18f);
        s.vel = FromAngle(a) * def.speed;
        s.radius = def.radius * (1.0f + p.level * 0.04f);
        s.damage = def.damage * dmgScale;
        s.pierce = def.pierce + (p.level >= 5 ? 1 : 0) + static_cast<int>(p.corePierce);
        s.bounce = def.bounce + static_cast<int>(p.coreBounce) + (p.weapon == Weapon::Roll && p.level >= 3 ? 1 : 0);
        s.ttl = p.weapon == Weapon::Roll ? 3.4f : 2.2f;
        s.color = def.color;
        s.ownerIndex = ownerIndex;
        s.sourceCharacter = p.character;
        s.homingStrength = p.character == CharacterType::Shortcake ? 1.7f : 0.0f;
        shots_.push_back(s);
    }
}

void SweetsApp::DoMelee(float aim)
{
    DoMeleeFor(player_, 0, aim);
}

void SweetsApp::DoMeleeFor(Player& p, int ownerIndex, float aim)
{
    float dmgScale = 1.0f + (p.level - 1) * 0.18f + p.corePower;
    dmgScale *= p.damageMul;
    if (p.dmgBuffT > 0.0f) dmgScale *= 1.6f;
    Slash s{};
    s.pos = p.pos;
    s.angle = aim;
    s.range = 2.05f + (p.level >= 4 ? 0.35f : 0.0f);
    s.arc = 1.45f;
    s.damage = 44.0f * dmgScale;
    s.color = Choco;
    slashes_.push_back(s);

    auto inCone = [&](V2 target, float r)
    {
        V2 d = target - p.pos;
        const float l = Len(d);
        if (l > s.range + r) return false;
        float da = AngleOf(d) - aim;
        while (da > Pi) da -= TwoPi;
        while (da < -Pi) da += TwoPi;
        return std::fabs(da) <= s.arc * 0.5f;
    };

    for (auto& e : enemies_)
    {
        if (!e.dead && inCone(e.pos, e.radius))
        {
            DamageEnemy(e, s.damage, p.pos, 1.8f, false, ownerIndex);
        }
    }
    if (boss_.active && inCone(boss_.pos, boss_.radius))
    {
        DamageBoss(s.damage * 0.8f, false, ownerIndex);
    }
}

void SweetsApp::ResolvePlayerHit(float dmg, float angle)
{
    ResolvePlayerHit(player_, dmg, angle);
}

void SweetsApp::ResolvePlayerHit(Player& p, float dmg, float angle)
{
    if (p.inv > 0.0f || p.downed) return;
    if (p.shieldT > 0.0f) dmg *= 0.35f;
    p.hp -= dmg;
    p.inv = 0.45f;
    p.grazeChain = 0;
    p.pos += FromAngle(angle) * 0.24f;
    ClampInside(p.pos, p.radius);
    Burst(p.pos, Red, 12);
    if (p.hp <= 0.0f)
    {
        p.hp = 0.0f;
        p.downed = true;
        p.alive = false;
        p.reviveT = 0.0f;
        Burst(p.pos, Red, 28);
        message_ = p.index == 0 ? L"プレイヤーがダウン" : L"味方がダウン";
        messageT_ = 2.0f;
    }
}

void SweetsApp::UseBomb()
{
    UseBombFor(player_, 0);
}

void SweetsApp::UseBombFor(Player& p, int ownerIndex)
{
    if (screen_ != Screen::Playing || p.bombs <= 0 || p.bombT > 0.0f || p.downed) return;

    --p.bombs;
    p.bombT = 1.8f;
    p.inv = std::max(p.inv, 1.8f);
    p.shieldT = std::max(p.shieldT, 1.8f);
    p.grazeChain = 0;

    int cleared = 0;
    for (auto& s : shots_)
    {
        if (s.enemy && Len(s.pos - p.pos) < 9.5f)
        {
            s.dead = true;
            ++cleared;
        }
    }

    for (auto& e : enemies_)
    {
        if (!e.dead && Len(e.pos - p.pos) < 5.6f)
        {
            DamageEnemy(e, 120.0f + wave_ * 12.0f, p.pos, 2.0f, false, ownerIndex);
        }
    }
    if (boss_.active && Len(boss_.pos - p.pos) < 6.4f)
    {
        DamageBoss(300.0f + wave_ * 18.0f, false, ownerIndex);
    }

    AddScore(cleared * 18, &p);
    Burst(p.pos, Sky, 90);
    message_ = L"ボム: 敵弾を消去";
    messageT_ = 2.0f;
}

void SweetsApp::UseUltimate()
{
    UseUltimateFor(player_, 0);
}

void SweetsApp::UseUltimateFor(Player& p, int ownerIndex)
{
    if (screen_ != Screen::Playing || p.ult < 100.0f || p.downed) return;
    for (auto& other : players_)
    {
        if (!other.active || other.index == ownerIndex || other.downed || other.ult < 100.0f) continue;
        if (Len(other.pos - p.pos) < 2.4f)
        {
            p.ult = 0.0f;
            other.ult = 0.0f;
            for (auto& s : shots_)
            {
                if (s.enemy) s.dead = true;
            }
            for (auto& e : enemies_)
            {
                if (!e.dead) DamageEnemy(e, 240.0f + wave_ * 16.0f, p.pos, 2.4f, true, ownerIndex);
            }
            if (boss_.active) DamageBoss(540.0f + wave_ * 24.0f, true, ownerIndex);
            Burst((p.pos + other.pos) * 0.5f, Gold, 140);
            message_ = L"合体必殺";
            messageT_ = 2.0f;
            return;
        }
    }
    p.ult = 0.0f;
    const auto weapon = p.weapon;
    if (weapon == Weapon::Strawberry)
    {
        const V2 target = ownerIndex == 0 ? ScreenToWorld(mouseX_, mouseY_) : FindNearestEnemyOrBoss(p.pos);
        for (auto& e : enemies_)
        {
            if (!e.dead && Len(e.pos - target) < 3.2f) DamageEnemy(e, 210.0f + wave_ * 18.0f, target, 2.0f, false, ownerIndex);
        }
        if (boss_.active && Len(boss_.pos - target) < 3.6f) DamageBoss(460.0f + wave_ * 22.0f, false, ownerIndex);
        Burst(target, Berry, 90);
        message_ = L"巨大メテオ";
    }
    else if (weapon == Weapon::Chocolate)
    {
        for (auto& e : enemies_) if (!e.dead) DamageEnemy(e, 160.0f + wave_ * 12.0f, p.pos, 2.0f, false, ownerIndex);
        if (boss_.active) DamageBoss(380.0f + wave_ * 18.0f, false, ownerIndex);
        Burst(p.pos, Choco, 80);
        message_ = L"時計斬り";
    }
    else if (weapon == Weapon::Cheese)
    {
        p.shieldT = 9.0f;
        p.inv = 1.2f;
        for (int i = 0; i < 8; ++i)
        {
            Obstacle o{};
            const float a = TwoPi * i / 8.0f;
            o.pos = p.pos + FromAngle(a) * 2.1f;
            ClampInside(o.pos, 0.5f);
            o.radius = 0.40f;
            o.hp = 120.0f;
            o.ttl = 9.0f;
            o.kind = 2;
            o.ownerIndex = ownerIndex;
            o.reflectPower = 1.5f + p.corePower;
            o.cheeseWall = true;
            o.color = Gold;
            obstacles_.push_back(o);
        }
        Burst(p.pos, Gold, 70);
        message_ = L"無敵要塞";
    }
    else
    {
        for (int i = 0; i < 24; ++i)
        {
            const float a = TwoPi * i / 24.0f;
            Shot s{};
            s.pos = p.pos + FromAngle(a) * 0.5f;
            s.vel = FromAngle(a) * 10.5f;
            s.radius = 0.17f;
            s.damage = 54.0f + wave_ * 3.0f;
            s.bounce = 6 + static_cast<int>(p.coreBounce);
            s.ttl = 3.2f;
            s.color = Cream;
            s.ownerIndex = ownerIndex;
            s.sourceCharacter = CharacterType::Roll;
            shots_.push_back(s);
        }
        Burst(p.pos, Cream, 70);
        message_ = L"全画面叩きつけ";
    }
    messageT_ = 2.0f;
}
