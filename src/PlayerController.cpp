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
    SyncPlayer3D(player_);

    for (auto& o : obstacles_)
    {
        if (o.damageField || o.warpId >= 0) continue; // 床とポータルは通過可
        V2 d = player_.pos - o.pos;
        const float l = RuleDistance(player_.pos, PlayerBodyY, o.pos, o.height);
        const float minD = player_.radius + o.radius;
        if (l > 0.0001f && l < minD)
        {
            const V2 n = Normalize(d);
            player_.pos = o.pos + n * minD;
            if (o.bumper)
            {
                // バンパー：触れた自機を勢いよく弾き返す
                player_.dashVel = n * 11.0f;
                player_.dashT = std::max(player_.dashT, 0.16f);
                o.flash = 1.0f;
                Burst(player_.pos, Gold, 12);
            }
            SyncPlayer3D(player_);
        }
    }

    const V2 aimPoint = ScreenToWorld(mouseX_, mouseY_);
    player_.face = AngleOf(aimPoint - player_.pos);

    if (player_.fireCd > 0.0f) player_.fireCd -= dt;
    const bool primaryHeld = mouseLeft_ || KeyDown(VK_SPACE);
    const bool primaryPressed = primaryHeld && !prevMouseLeft_;
    const bool primaryReleased = !primaryHeld && prevMouseLeft_;
    if (player_.weapon == Weapon::Chocolate)
    {
        // 長押しでチャージ→離して発射（サイズはチャージ段階）。飛行中はクリックで爆発
        Shot* bomb = nullptr;
        for (auto& bs : shots_)
        {
            if (!bs.dead && bs.chocoBomb && bs.ownerIndex == 0) { bomb = &bs; break; }
        }
        if (bomb)
        {
            if (primaryPressed && player_.fireCd <= 0.0f)
            {
                DetonateChocoBomb(*bomb, 0);
                player_.fireCd = 0.2f;
            }
            player_.bombCharge = 0.0f;
        }
        else if (primaryHeld)
        {
            player_.bombCharge = std::min(1.3f, player_.bombCharge + dt);
        }
        else if (primaryReleased && player_.bombCharge > 0.05f && player_.fireCd <= 0.0f)
        {
            FireChocoBomb(player_, 0, player_.face, player_.bombCharge);
            player_.bombCharge = 0.0f;
            player_.fireCd = 0.15f;
        }
    }
    else if (primaryHeld && player_.fireCd <= 0.0f)
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
            SyncObstacle3D(wall);
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
        // 協力/AI向け：飛んでいる爆弾があれば爆発、なければ中チャージで発射
        p.fireCd = 0.5f * p.cooldownMul;
        Shot* bomb = nullptr;
        for (auto& bs : shots_)
        {
            if (!bs.dead && bs.chocoBomb && bs.ownerIndex == ownerIndex) { bomb = &bs; break; }
        }
        if (bomb) DetonateChocoBomb(*bomb, ownerIndex);
        else FireChocoBomb(p, ownerIndex, aim, 0.7f);
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
        s.homingStrength = p.character == CharacterType::Shortcake ? 0.8f : 0.0f;
        if (p.character == CharacterType::Shortcake)
        {
            // ショートは1回だけ反射でき、跳ね返った瞬間に分裂する
            s.bounce = std::max(s.bounce, 1);
            s.reflectSplit = 3;
        }
        SyncShot3D(s);
        shots_.push_back(s);
    }
}

int ChocoBombStage(float charge)
{
    return charge >= 1.0f ? 3 : (charge >= 0.6f ? 2 : (charge >= 0.3f ? 1 : 0));
}

void SweetsApp::FireChocoBomb(Player& p, int ownerIndex, float aim, float charge)
{
    float dmgScale = 1.0f + (p.level - 1) * 0.18f + p.corePower;
    dmgScale *= p.damageMul;
    if (p.dmgBuffT > 0.0f) dmgScale *= 1.6f;
    const int stage = ChocoBombStage(charge);
    Shot s{};
    s.pos = p.pos + FromAngle(aim) * (p.radius + 0.2f);
    s.vel = FromAngle(aim) * 11.0f;
    s.radius = 0.26f + 0.28f * static_cast<float>(stage); // チャージ段階でサイズ決定（0.26/0.54/0.82/1.10）
    s.damage = 24.0f * dmgScale;
    s.ttl = 12.0f;
    s.color = Choco;
    s.ownerIndex = ownerIndex;
    s.sourceCharacter = CharacterType::Chocolate;
    s.chocoBomb = true;
    s.growStage = stage;
    SyncShot3D(s);
    shots_.push_back(s);
}

void SweetsApp::DetonateChocoBomb(Shot& bomb, int ownerIndex)
{
    const Player& p = players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))];
    float dmgScale = 1.0f + (p.level - 1) * 0.18f + p.corePower;
    dmgScale *= p.damageMul;
    if (p.dmgBuffT > 0.0f) dmgScale *= 1.6f;
    const int stage = bomb.growStage;                          // 0..3
    const float mult = 1.0f + 0.1f * static_cast<float>(stage); // 1.0/1.1/1.2/1.3倍
    const float exDmg = 90.0f * dmgScale * mult;
    const float exR = 2.8f * mult;

    // ボスへのダメージ：ボスと95%以上重なって爆発するとクリティカル（1.25〜2倍）
    bool didCrit = false;
    auto damageBossMaybeCrit = [&]()
    {
        if (!boss_.active) return;
        const float d = RuleDistance(boss_.pos, boss_.height, bomb.pos, bomb.height);
        if (d >= exR + boss_.radius) return;
        const float inside = (boss_.radius + bomb.radius - d) / (2.0f * bomb.radius); // 弾がボスに入っている割合
        if (inside >= 0.95f)
        {
            const float crit = Rand(1.25f, 2.0f);
            DamageBoss(exDmg * crit, false, ownerIndex);
            Burst(boss_.pos, Gold, 50);
            didCrit = true;
        }
        else
        {
            DamageBoss(exDmg, false, ownerIndex);
        }
    };

    if (stage >= 3)
    {
        // 最大チャージ：巻き込んだ敵ほど一体あたりのダメージがアップ＋生存敵を吹き飛ばす
        std::vector<int> caught;
        for (int i = 0; i < static_cast<int>(enemies_.size()); ++i)
        {
            const Enemy& e = enemies_[i];
            if (!e.dead && RuleDistance(e.pos, e.height, bomb.pos, bomb.height) < exR + e.radius) caught.push_back(i);
        }
        // ザコは巻き込むほど一体あたりのダメージがアップ（最大3倍）
        const int count = static_cast<int>(caught.size());
        const float per = exDmg * std::min(3.0f, 1.0f + 0.2f * static_cast<float>(std::max(0, count - 1)));
        for (int i : caught)
        {
            if (enemies_[i].dead) continue;
            DamageEnemy(enemies_[i], per, bomb.pos, 1.0f, false, ownerIndex);
            if (!enemies_[i].dead)
            {
                V2 away = Normalize(enemies_[i].pos - bomb.pos);
                if (LenSq(away) < 0.001f) away = FromAngle(0.0f);
                enemies_[i].pos += away * 3.2f; // 吹き飛ばし
                ClampInside(enemies_[i].pos, enemies_[i].radius);
                SyncEnemy3D(enemies_[i]);
            }
        }
        for (auto& e : enemies_) e.caught = false; // 爆発で全解放
        damageBossMaybeCrit(); // ボスは巻き込まないが範囲内なら巻き添え（クリティカル対象）
    }
    else
    {
        for (auto& e : enemies_)
        {
            if (!e.dead && RuleDistance(e.pos, e.height, bomb.pos, bomb.height) < exR + e.radius)
            {
                DamageEnemy(e, exDmg, bomb.pos, 2.5f, false, ownerIndex);
            }
        }
        damageBossMaybeCrit();
    }
    // 爆発範囲内の敵弾を消去
    for (auto& bs : shots_)
    {
        if (bs.enemy && !bs.dead && RuleDistance(bs.pos, bs.height, bomb.pos, bomb.height) < exR)
        {
            bs.dead = true;
        }
    }
    const Color exCol = stage >= 3 ? Gold : (stage >= 2 ? Berry : Choco);
    EffectPulse shock{};
    shock.pos = bomb.pos;
    shock.startRadius = exR * 0.25f;
    shock.endRadius = exR;
    shock.ttl = 0.34f;
    shock.life = 0.34f;
    shock.y = bomb.height + 0.05f;
    shock.color = exCol;
    shock.pos3 = Grounded3D(shock.pos, shock.y);
    effectPulses_.push_back(shock);
    EffectPulse core{};
    core.pos = bomb.pos;
    core.startRadius = exR * 0.6f;
    core.endRadius = exR * 0.15f;
    core.ttl = 0.20f;
    core.life = 0.20f;
    core.y = bomb.height + 0.10f;
    core.color = Cream;
    core.pos3 = Grounded3D(core.pos, core.y);
    effectPulses_.push_back(core);
    Burst(bomb.pos, exCol, 18 + stage * 34);
    Burst(bomb.pos, Cream, 8 + stage * 6);
    bomb.dead = true;
    message_ = didCrit ? L"クリティカル爆発!!" : (stage >= 3 ? L"特大チョコ爆発!!" : (stage >= 2 ? L"大チョコ爆発!" : L"チョコ爆発"));
    messageT_ = std::max(messageT_, didCrit ? 1.0f : 0.7f);
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
    s.visualMode = SlashVisualMode::Sector;
    s.sweep = true; // 薙ぎ払いモーション
    SyncSlash3D(s);
    slashes_.push_back(s);

    auto inCone = [&](V2 target, float r, float targetY)
    {
        V2 d = target - p.pos;
        const float l = Use3DRules() ? RuleDistance(p.pos, PlayerBodyY, target, targetY) : Len(d);
        if (l > s.range + r) return false;
        float da = AngleOf(d) - aim;
        while (da > Pi) da -= TwoPi;
        while (da < -Pi) da += TwoPi;
        return std::fabs(da) <= s.arc * 0.5f;
    };

    for (auto& e : enemies_)
    {
        if (!e.dead && inCone(e.pos, e.radius, e.height))
        {
            DamageEnemy(e, s.damage, p.pos, 1.8f, false, ownerIndex);
            p.ult = std::min(100.0f, p.ult + 1.0f); // 近接ヒットで必殺ゲージ
            if (e.dead)
            {
                // 近接キル報酬：HP回復＋必殺ゲージ加算（切り込むほど強くなる）
                p.hp = std::min(p.maxHp, p.hp + 6.0f);
                p.ult = std::min(100.0f, p.ult + 4.0f);
                Burst(e.pos, Choco, 14);
            }
        }
    }
    if (boss_.active && inCone(boss_.pos, boss_.radius, boss_.height))
    {
        DamageBoss(s.damage * 0.8f, false, ownerIndex);
        p.ult = std::min(100.0f, p.ult + 1.0f);
    }

    // 薙ぎ払いで弾幕を弾き返す：範囲内の敵弾をプレイヤー弾に変えて撃ち返す
    for (auto& bs : shots_)
    {
        if (!bs.enemy || bs.dead) continue;
        if (!inCone(bs.pos, bs.radius, bs.height)) continue;
        bs.vel = FromAngle(aim) * std::max(10.0f, Len(bs.vel) * 1.25f);
        bs.enemy = false;
        bs.ownerIndex = ownerIndex;
        bs.sourceCharacter = CharacterType::Chocolate;
        bs.reflected = true;
        bs.reflectedCount = std::max(1, bs.reflectedCount + 1);
        bs.damage = std::max(bs.damage, 16.0f);
        bs.bounce = std::max(bs.bounce, 1);
        bs.pierce = std::max(bs.pierce, 1);
        bs.ttl = std::max(bs.ttl, 1.6f);
        bs.color = Choco;
        SyncShot3D(bs);
        Burst(bs.pos, Choco, 6);
    }
}

void SweetsApp::ResolvePlayerHit(float dmg, float angle)
{
    ResolvePlayerHit(player_, dmg, angle);
}

void SweetsApp::ResolvePlayerHit(Player& p, float dmg, float angle)
{
#if defined(_DEBUG)
    if (debug_.invincible) return;
#endif
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
    if ((screen_ != Screen::Playing && screen_ != Screen::HiddenBoss) || p.bombs <= 0 || p.bombT > 0.0f || p.downed) return;

    --p.bombs;
    p.bombT = 1.8f;
    p.inv = std::max(p.inv, 1.8f);
    p.shieldT = std::max(p.shieldT, 1.8f);
    p.grazeChain = 0;

    int cleared = 0;
    for (auto& s : shots_)
    {
        if (s.enemy && RuleDistance(s.pos, s.height, p.pos, PlayerBodyY) < 9.5f)
        {
            s.dead = true;
            ++cleared;
        }
    }

    for (auto& e : enemies_)
    {
        if (!e.dead && RuleDistance(p, e) < 5.6f)
        {
            DamageEnemy(e, 120.0f + wave_ * 12.0f, p.pos, 2.0f, false, ownerIndex);
        }
    }
    if (boss_.active && RuleDistance(p.pos, PlayerBodyY, boss_.pos, boss_.height) < 6.4f)
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
    if ((screen_ != Screen::Playing && screen_ != Screen::HiddenBoss) || p.ult < 100.0f || p.downed) return;
    for (auto& other : players_)
    {
        if (!other.active || other.index == ownerIndex || other.downed || other.ult < 100.0f) continue;
        if (RuleDistance(p.pos, PlayerBodyY, other.pos, PlayerBodyY) < 2.4f)
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
            PlayCombatEffect(L"ult_chocolate", (p.pos + other.pos) * 0.5f, 0.55f, p.face, 1.95f, Gold, 80);
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
            if (!e.dead && RuleDistance(e.pos, e.height, target, ShotBodyY) < 3.2f) DamageEnemy(e, 210.0f + wave_ * 18.0f, target, 2.0f, false, ownerIndex);
        }
        if (boss_.active && RuleDistance(boss_.pos, boss_.height, target, ShotBodyY) < 3.6f) DamageBoss(460.0f + wave_ * 22.0f, false, ownerIndex);
        Burst(target, Berry, 90);
        PlayCombatEffect(L"ult_shortcake", target, 0.50f, 0.0f, 1.75f, Berry, 70);
        message_ = L"巨大メテオ";
    }
    else if (weapon == Weapon::Chocolate)
    {
        for (auto& e : enemies_) if (!e.dead) DamageEnemy(e, 160.0f + wave_ * 12.0f, p.pos, 2.0f, false, ownerIndex);
        if (boss_.active) DamageBoss(380.0f + wave_ * 18.0f, false, ownerIndex);
        Burst(p.pos, Choco, 80);
        PlayCombatEffect(L"ult_chocolate", p.pos, 0.55f, p.face, 1.85f, Choco, 70);
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
            SyncObstacle3D(o);
            obstacles_.push_back(o);
        }
        Burst(p.pos, Gold, 70);
        PlayCombatEffect(L"ult_cheese", p.pos, 0.50f, 0.0f, 1.85f, Gold, 70);
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
            SyncShot3D(s);
            shots_.push_back(s);
        }
        Burst(p.pos, Cream, 70);
        PlayCombatEffect(L"ult_roll", p.pos, 0.48f, p.face, 1.80f, Cream, 70);
        message_ = L"全画面叩きつけ";
    }
    messageT_ = 2.0f;
}
