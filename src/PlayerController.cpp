#include "SweetsApp.h"

#include <algorithm>

// AutoTarget用に、一定範囲内の最も近い敵またはボスを探します。
// マウス照準が忙しいという意見への対策として、攻撃方向を自動化できるようにしています。
bool SweetsApp::FindAimTarget(V2 pos, float range, V2& out) const
{
    bool found = false;
    float bestD = range * range;
    if (boss_.active)
    {
        const float d = LenSq(boss_.pos - pos);
        if (d <= bestD)
        {
            bestD = d;
            out = boss_.pos;
            found = true;
        }
    }
    for (const auto& e : enemies_)
    {
        if (e.dead) continue;
        const float d = LenSq(e.pos - pos);
        if (d <= bestD)
        {
            bestD = d;
            out = e.pos;
            found = true;
        }
    }
    return found;
}

// 現在の照準モードから、実際に攻撃する角度を決めます。
// 1Pだけが設定画面のAimModeを使い、AI/Padは基本的に移動方向やAI判断を使います。
float SweetsApp::ResolvePlayerAim(const Player& p, int ownerIndex, V2 moveDir, V2 cursorPoint) const
{
    if (ownerIndex == 0 && aimMode_ == AimMode::Cursor)
    {
        return AngleOf(cursorPoint - p.pos);
    }
    if (ownerIndex == 0 && aimMode_ == AimMode::AutoTarget)
    {
        V2 target{};
        if (FindAimTarget(p.pos, 13.5f, target))
        {
            return AngleOf(target - p.pos);
        }
        return p.face;
    }
    if (LenSq(moveDir) > 0.001f)
    {
        return AngleOf(moveDir);
    }
    return p.face;
}

// 必殺技や壁設置など「地点」を必要とする攻撃の狙い先を決めます。
// Cursor以外では近い敵を優先し、敵がいなければ自機前方へ出します。
V2 SweetsApp::ResolvePlayerAimPoint(const Player& p, int ownerIndex, V2 cursorPoint, float range) const
{
    if (ownerIndex == 0 && aimMode_ == AimMode::Cursor)
    {
        return cursorPoint;
    }
    V2 target{};
    if (FindAimTarget(p.pos, range, target))
    {
        return target;
    }
    return p.pos + FromAngle(p.face) * std::min(range, 5.0f);
}

// 1Pの移動、通常攻撃、チャージ攻撃を更新します。
// 2P-4Pは CoopController.cpp 側でAI/ゲームパッド操作として更新します。
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

    // Shift集中移動は速度を落として、弾幕を避けやすくする操作です。
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
    if (player_.character == CharacterType::Roll && player_.dashT > 0.0f)
    {
        ReflectEnemyShotsNear(player_.pos, player_.radius + 0.72f, 0, CharacterType::Roll, Cream, 1.30f);
    }

    for (const auto& o : obstacles_)
    {
        if (o.damageField) continue;
        V2 d = player_.pos - o.pos;
        const float l = RuleDistance(player_.pos, PlayerBodyY, o.pos, o.height);
        const float minD = player_.radius + o.radius;
        if (l > 0.0001f && l < minD)
        {
            const V2 n = Normalize(d);
            player_.pos = o.pos + n * minD;
            SyncPlayer3D(player_);
        }
    }

    // マウス座標をゲーム内座標へ変換して、照準モードに応じた向きを作ります。
    const V2 aimPoint = ScreenToWorld(mouseX_, mouseY_);
    player_.face = ResolvePlayerAim(player_, 0, dir, aimPoint);
    const V2 actionPoint = ResolvePlayerAimPoint(player_, 0, aimPoint, 8.0f);

    if (player_.fireCd > 0.0f) player_.fireCd -= dt;
    // 左クリック/Spaceは通常弾専用です。長押ししてもチャージ弾は出しません。
    const bool primaryHeld = mouseLeft_ || KeyDown(VK_SPACE);
    if (primaryHeld && player_.fireCd <= 0.0f)
    {
        FirePrimaryFor(player_, 0, player_.face);
    }

    // 右クリックはチャージ専用です。チーズだけ短押し時に壁設置へ分岐します。
    if (mouseRight_)
    {
        player_.chargeT += dt;
        player_.chargeReady = player_.chargeT >= 0.55f;
        player_.chargeFull = player_.chargeT >= 1.15f;
        player_.charging = true;
    }
    else if (mouseRightReleased_ || player_.charging)
    {
        if (player_.chargeReady && player_.chargeCd <= 0.0f)
        {
            FireCharged(player_, 0, player_.face, actionPoint);
        }
        else if (mouseRightReleased_ && player_.character == CharacterType::Cheese && obstacles_.size() < 20)
        {
            Obstacle wall{};
            wall.pos = actionPoint;
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
        player_.chargeFull = false;
        player_.chargeT = 0.0f;
    }
    mouseRightReleased_ = false;
    prevMouseLeft_ = primaryHeld;
}

void SweetsApp::FirePrimary()
{
    FirePrimaryFor(player_, 0, player_.face);
}

// 通常攻撃を1回発射します。
// チョコだけは弾ではなく近接斬りなので DoMeleeFor へ分岐します。
void SweetsApp::FirePrimaryFor(Player& p, int ownerIndex, float aim)
{
    if (p.fireCd > 0.0f || p.downed) return;
    const auto& def = Weapons[static_cast<int>(p.weapon)];
    float dmgScale = 1.0f + (p.level - 1) * 0.18f + p.corePower;
    dmgScale *= p.damageMul;
    if (p.dmgBuffT > 0.0f) dmgScale *= 1.6f;

    p.fireCd = def.cooldown * p.cooldownMul;
    // レベルや拡散アイテムで弾数と角度を増やし、雑魚戦で強化の気持ちよさを出します。
    int count = 1;
    float spread = 0.0f;
    if (p.weapon == Weapon::Chocolate)
    {
        count = p.level >= 4 ? 2 : 1;
        spread = p.level >= 4 ? 0.18f : 0.0f;
    }
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
    if (p.weapon == Weapon::Chocolate)
    {
        audio_.PlaySoundEffect(SoundEffect::ChocoSlash);
    }

    for (int n = 0; n < count; ++n)
    {
        const float a = aim + (count > 1 ? (static_cast<float>(n) / (count - 1) - 0.5f) * spread : 0.0f);
        Shot s{};
        s.pos = p.pos + FromAngle(a) * (p.radius + 0.18f);
        s.vel = FromAngle(a) * (p.weapon == Weapon::Chocolate ? 12.8f : def.speed);
        s.radius = (p.weapon == Weapon::Chocolate ? 0.20f : def.radius) * (1.0f + p.level * 0.04f);
        s.damage = (p.weapon == Weapon::Chocolate ? 24.0f : def.damage) * dmgScale;
        s.pierce = (p.weapon == Weapon::Chocolate ? 5 : def.pierce) + (p.level >= 5 ? 1 : 0) + static_cast<int>(p.corePierce);
        s.bounce = def.bounce + static_cast<int>(p.coreBounce) + (p.weapon == Weapon::Roll && p.level >= 3 ? 1 : 0);
        s.ttl = p.weapon == Weapon::Roll ? 3.4f : (p.weapon == Weapon::Chocolate ? 2.8f : 2.2f);
        s.color = def.color;
        s.ownerIndex = ownerIndex;
        s.sourceCharacter = p.character;
        s.homingStrength = p.character == CharacterType::Shortcake ? 1.7f : 0.0f;
        s.yoyo = p.character == CharacterType::Chocolate;
        if (s.yoyo)
        {
            s.bounce = std::max(s.bounce, 3);
            PlayCombatEffect(L"sword_slash", p.pos, 0.52f, a, 0.86f, Choco, 12);
        }
        SyncShot3D(s);
        shots_.push_back(s);
    }
}

void SweetsApp::DoMelee(float aim)
{
    DoMeleeFor(player_, 0, aim);
}

// チョコの通常剣攻撃です。
// Slash は当たり判定用で、見た目は Sword9 Effekseer と補助FXに任せています。
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
    s.visualMode = SlashVisualMode::Hidden;
    SyncSlash3D(s);
    slashes_.push_back(s);
    audio_.PlaySoundEffect(SoundEffect::ChocoSlash);
    PlayCombatEffect(L"sword_slash", p.pos, 0.52f, aim, 1.10f, Choco, 22);

    // 扇形判定。距離と角度の両方が範囲内なら命中です。
    // 3Dルール時は高さも含めた距離、2Dルール時は平面距離を使います。
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
        }
    }
    for (const auto& core : hiddenBossCores_)
    {
        if (core.active && inCone(core.pos, core.radius, ShotBodyY))
        {
            DamageHiddenBossCore(s.damage, core.pos, ownerIndex);
        }
    }
    if (boss_.active && inCone(boss_.pos, boss_.radius, boss_.height))
    {
        DamageBoss(s.damage * 0.8f, BossDamageKind::Melee, false, ownerIndex);
    }
}

void SweetsApp::ResolvePlayerHit(float dmg, float angle)
{
    ResolvePlayerHit(player_, dmg, angle);
}

// プレイヤー被弾処理です。
// ダメージ、無敵時間、ノックバック、HP0時のダウン状態をここでまとめて処理します。
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

// ボムは敵弾消去、短時間無敵、範囲ダメージをまとめた緊急回避手段です。
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
    DamageHiddenBossCoresInRadius(p.pos, 5.6f, 180.0f + wave_ * 12.0f, ownerIndex);
    if (boss_.active && RuleDistance(p.pos, PlayerBodyY, boss_.pos, boss_.height) < 6.4f)
    {
        DamageBoss(300.0f + wave_ * 18.0f, BossDamageKind::Bomb, false, ownerIndex);
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

// 必殺技です。
// 近くにULT満タンの味方がいる場合は合体必殺を優先し、いなければキャラ別必殺を出します。
void SweetsApp::UseUltimateFor(Player& p, int ownerIndex)
{
    if ((screen_ != Screen::Playing && screen_ != Screen::HiddenBoss) || p.ult < 100.0f || p.downed) return;
    for (auto& other : players_)
    {
        if (!other.active || other.index == ownerIndex || other.downed || other.ult < 100.0f) continue;
        if (RuleDistance(p.pos, PlayerBodyY, other.pos, PlayerBodyY) < 2.4f)
        {
            audio_.PlaySoundEffect(SoundEffect::UltimateSlash);
            p.ult = 0.0f;
            other.ult = 0.0f;
            for (auto& s : shots_)
            {
                if (s.enemy) s.dead = true;
            }
            suppressEnemyKillUltGain_ = true;
            for (auto& e : enemies_)
            {
                if (!e.dead) DamageEnemy(e, 240.0f + wave_ * 16.0f, p.pos, 2.4f, true, ownerIndex);
            }
            suppressEnemyKillUltGain_ = false;
            if (boss_.active) DamageBoss(540.0f + wave_ * 24.0f, BossDamageKind::Ultimate, false, ownerIndex);
            Burst((p.pos + other.pos) * 0.5f, Gold, 140);
            PlayCombatEffect(L"ult_chocolate", (p.pos + other.pos) * 0.5f, 0.55f, p.face, 1.95f, Gold, 80);
            message_ = L"合体必殺";
            messageT_ = 2.0f;
            return;
        }
    }
    audio_.PlaySoundEffect(SoundEffect::UltimateSlash);
    p.ult = 0.0f;
    const auto weapon = p.weapon;
    if (weapon == Weapon::Strawberry)
    {
        const V2 cursorTarget = ScreenToWorld(mouseX_, mouseY_);
        const V2 target = ownerIndex == 0 ? ResolvePlayerAimPoint(p, ownerIndex, cursorTarget, 14.0f) : FindNearestEnemyOrBoss(p.pos);
        suppressEnemyKillUltGain_ = true;
        for (auto& e : enemies_)
        {
            if (!e.dead && RuleDistance(e.pos, e.height, target, ShotBodyY) < 3.2f) DamageEnemy(e, 210.0f + wave_ * 18.0f, target, 2.0f, false, ownerIndex);
        }
        suppressEnemyKillUltGain_ = false;
        DamageHiddenBossCoresInRadius(target, 3.2f, 260.0f + wave_ * 18.0f, ownerIndex);
        if (boss_.active && RuleDistance(boss_.pos, boss_.height, target, ShotBodyY) < 3.6f) DamageBoss(460.0f + wave_ * 22.0f, BossDamageKind::Ultimate, false, ownerIndex);
        Burst(target, Berry, 90);
        PlayCombatEffect(L"ult_shortcake", target, 0.50f, 0.0f, 1.75f, Berry, 70);
        message_ = L"巨大メテオ";
    }
    else if (weapon == Weapon::Chocolate)
    {
        suppressEnemyKillUltGain_ = true;
        for (auto& e : enemies_) if (!e.dead) DamageEnemy(e, 160.0f + wave_ * 12.0f, p.pos, 2.0f, false, ownerIndex);
        suppressEnemyKillUltGain_ = false;
        DamageHiddenBossCoresInRadius(p.pos, 7.5f, 220.0f + wave_ * 12.0f, ownerIndex);
        if (boss_.active) DamageBoss(380.0f + wave_ * 18.0f, BossDamageKind::Ultimate, false, ownerIndex);
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
            s.ultimateSource = true;
            SyncShot3D(s);
            shots_.push_back(s);
        }
        Burst(p.pos, Cream, 70);
        PlayCombatEffect(L"ult_roll", p.pos, 0.48f, p.face, 1.80f, Cream, 70);
        message_ = L"全画面叩きつけ";
    }
    messageT_ = 2.0f;
}
