#include "SweetsApp.h"

void SweetsApp::FireCharged(Player& p, int ownerIndex, float aim, V2 aimPoint)
{
    (void)aimPoint;
    if (p.chargeCd > 0.0f || p.downed) return;
    p.chargeCd = 0.55f;
    p.fireCd = std::max(p.fireCd, 0.18f);

    const float dmgScale = (1.0f + (p.level - 1) * 0.20f + p.corePower) * p.damageMul * (p.dmgBuffT > 0.0f ? 1.6f : 1.0f);
    if (p.character == CharacterType::Shortcake)
    {
        Shot s{};
        s.pos = p.pos + FromAngle(aim) * (p.radius + 0.28f);
        s.vel = FromAngle(aim) * 11.5f;
        s.radius = 0.27f;
        s.damage = 52.0f * dmgScale;
        s.ttl = 2.8f;
        s.pierce = 1 + static_cast<int>(p.corePierce);
        s.bounce = 1 + static_cast<int>(p.coreBounce);
        s.homingStrength = 2.8f;
        s.splitCount = 5;
        s.charged = true;
        s.ownerIndex = ownerIndex;
        s.sourceCharacter = CharacterType::Shortcake;
        s.color = Berry;
        shots_.push_back(s);
        message_ = L"分裂チャージ";
    }
    else if (p.character == CharacterType::Chocolate)
    {
        for (int i = -1; i <= 1; ++i)
        {
            const float a = aim + i * 0.10f;
            Shot s{};
            s.pos = p.pos + FromAngle(a) * 0.7f;
            s.vel = FromAngle(a) * 14.5f;
            s.radius = 0.19f;
            s.damage = 46.0f * dmgScale;
            s.ttl = 1.15f;
            s.pierce = 5 + static_cast<int>(p.corePierce);
            s.bounce = static_cast<int>(p.coreBounce);
            s.charged = true;
            s.ownerIndex = ownerIndex;
            s.sourceCharacter = CharacterType::Chocolate;
            s.color = Choco;
            shots_.push_back(s);
        }
        message_ = L"斬撃波";
    }
    else if (p.character == CharacterType::Cheese)
    {
        Obstacle wall{};
        wall.pos = p.pos + FromAngle(aim) * 1.1f;
        wall.vel = FromAngle(aim) * 2.2f;
        wall.radius = 0.72f;
        wall.hp = 190.0f + p.level * 28.0f;
        wall.ttl = 5.4f;
        wall.kind = 2;
        wall.ownerIndex = ownerIndex;
        wall.reflectPower = 1.55f + p.corePower;
        wall.cheeseWall = true;
        wall.moving = true;
        wall.color = Gold;
        obstacles_.push_back(wall);
        message_ = L"トゲ付き壁";
    }
    else
    {
        p.dashT = 0.55f;
        p.dashVel = FromAngle(aim) * (15.0f + p.speed * 0.5f);
        p.inv = std::max(p.inv, 0.45f);
        for (int i = -1; i <= 1; ++i)
        {
            const float a = aim + i * 0.18f;
            Shot s{};
            s.pos = p.pos + FromAngle(a) * 0.55f;
            s.vel = FromAngle(a) * 12.0f;
            s.radius = 0.22f;
            s.damage = 42.0f * dmgScale;
            s.ttl = 2.2f;
            s.bounce = 4 + static_cast<int>(p.coreBounce);
            s.charged = true;
            s.ownerIndex = ownerIndex;
            s.sourceCharacter = CharacterType::Roll;
            s.color = Cream;
            shots_.push_back(s);
        }
        message_ = L"転がり突進";
    }
    messageT_ = 1.4f;
}

void SweetsApp::SpawnSplitShots(const Shot& source, V2 at)
{
    for (int i = 0; i < source.splitCount; ++i)
    {
        const float a = TwoPi * i / static_cast<float>(source.splitCount) + 0.2f;
        Shot child{};
        child.pos = at + FromAngle(a) * 0.18f;
        child.vel = FromAngle(a) * 9.0f;
        child.radius = 0.12f;
        child.damage = source.damage * 0.46f;
        child.ttl = 1.8f;
        child.bounce = 2;
        child.ownerIndex = source.ownerIndex;
        child.sourceCharacter = CharacterType::Shortcake;
        child.homingStrength = 2.2f;
        child.color = Berry;
        shots_.push_back(child);
    }
    Burst(at, Berry, 24);
}
