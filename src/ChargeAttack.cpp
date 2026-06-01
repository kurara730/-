#include "SweetsApp.h"

// 右クリック長押しなどで発動するキャラ別チャージ攻撃です。
// 同じ入口からキャラごとの処理へ分岐し、ショート/チョコ/チーズ/ロールの個性を作っています。
void SweetsApp::FireCharged(Player& p, int ownerIndex, float aim, V2 aimPoint)
{
    (void)aimPoint;
    if (p.chargeCd > 0.0f || p.downed) return;
    p.chargeCd = 0.55f;
    p.fireCd = std::max(p.fireCd, 0.18f);

    // レベル、コア強化、キャラ火力、アイテムバフをまとめてダメージに掛けます。
    // 雑魚戦では成長の手応えが出ますが、ボス側では DamageBoss で別途上限を掛けます。
    const float dmgScale = (1.0f + (p.level - 1) * 0.20f + p.corePower) * p.damageMul * (p.dmgBuffT > 0.0f ? 1.6f : 1.0f);
    if (p.character == CharacterType::Shortcake)
    {
        // ショートは追尾し、命中時に分裂する大きな苺弾です。
        // charged=true の弾は CombatLoop 側で周囲の敵弾を反射するフィールドも持ちます。
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
        SyncShot3D(s);
        shots_.push_back(s);
        message_ = L"分裂チャージ";
    }
    else if (p.character == CharacterType::Chocolate)
    {
        // チョコは剣エフェクトを出しつつ、判定としては前方へ斬撃波を飛ばします。
        // 見た目はEffekseerと補助スプライト、ダメージはShotとして管理します。
        PlayCombatEffect(L"sword_slash", p.pos, 0.56f, aim, 1.45f, Choco, 28);
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
            s.yoyo = true;
            s.bounce = std::max(s.bounce, 4);
            s.ownerIndex = ownerIndex;
            s.sourceCharacter = CharacterType::Chocolate;
            s.color = Choco;
            SyncShot3D(s);
            shots_.push_back(s);
        }
        message_ = L"斬撃波";
    }
    else if (p.character == CharacterType::Cheese)
    {
        // チーズは移動する壁を置きます。
        // cheeseWall=true なので、敵弾が当たると味方弾へ反射できます。
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
        SyncObstacle3D(wall);
        obstacles_.push_back(wall);
        message_ = L"トゲ付き壁";
    }
    else
    {
        // ロールは最大溜めの時だけ突進と無敵が付きます。
        // 短い溜めでは反射弾だけを撃つため、短押し無敵でバランスが壊れないようにしています。
        const bool fullCharge = p.chargeFull || p.chargeT >= 1.15f;
        if (fullCharge)
        {
            p.dashT = 0.55f;
            p.dashVel = FromAngle(aim) * (15.0f + p.speed * 0.5f);
            p.inv = std::max(p.inv, 0.45f);
            ReflectEnemyShotsNear(p.pos, p.radius + 0.95f, ownerIndex, CharacterType::Roll, Cream, 1.35f);
        }
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
            SyncShot3D(s);
            shots_.push_back(s);
        }
        message_ = fullCharge ? L"転がり突進" : L"反射ロール弾";
    }
    messageT_ = 1.4f;
}

// ショートのチャージ弾が命中した時に分裂弾を作ります。
// 分裂後も追尾を持たせ、雑魚戦で成長火力を感じやすくしています。
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
        SyncShot3D(child);
        shots_.push_back(child);
    }
    Burst(at, Berry, 24);
}
