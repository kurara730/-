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

// チョコウォール（短押し壁・チャージ壁の両方）を最大 maxWalls 枚に保ちます。
// 既に上限に達している場合は、配列の前方ほど古い（=先に push_back された）性質を使って
// 一番古い chocoWall の ttl を 0 にし、StageFactory の既存クリーンアップで回収させます（FIFO）。
// cheeseWall フラグや reflectPower には触れないため、コアの反射判定には影響しません。
void SweetsApp::EnforceChocoWallLimit(size_t maxWalls)
{
    auto countActive = [this]() {
        size_t n = 0;
        for (const auto& o : obstacles_) if (o.chocoWall && o.ttl > 0.0f) ++n;
        return n;
    };
    while (countActive() >= maxWalls)
    {
        Obstacle* oldest = nullptr;
        for (auto& o : obstacles_)
        {
            if (o.chocoWall && o.ttl > 0.0f) { oldest = &o; break; }
        }
        if (!oldest) break;
        oldest->ttl = 0.0f; // 次の除去パスで回収される（その場で erase せず反射ループを乱さない）
    }
}

// ブリンクした位置が「危険だったか」を判定します（ジャスト回避演出のトリガー）。
// 迫る敵弾、ボスの照射ビーム帯、ダメージ床、地中噴出の円のいずれかに掛かっていれば true。
bool SweetsApp::IsBlinkJustDodge(V2 pos) const
{
    // (1) 近くに迫る敵弾
    for (const auto& s : shots_)
    {
        if (!s.enemy || s.dead) continue;
        if (RuleDistance(s.pos, s.height, pos, PlayerBodyY) > JustDodgeBulletRange + s.radius) continue;
        if (Dot(s.vel, pos - s.pos) > 0.0f) return true; // こちらへ向かっている
    }
    if (boss_.active)
    {
        // (2) ボスの照射ビーム帯の中
        if (boss_.beamActiveT > 0.0f)
        {
            const V2 bdir = FromAngle(boss_.beamAngle);
            const V2 rel = pos - boss_.pos;
            const float along = Dot(rel, bdir);
            if (along >= 0.0f && along <= BossBeamLength)
            {
                const V2 perp = rel - bdir * along;
                if (Len(perp) <= BossBeamHalfWidth + player_.radius + 0.4f) return true;
            }
        }
        // (3) 地中突き上げの噴出円（噴出直後）
        if (boss_.burrowEruptT > 0.0f)
        {
            for (int i = 0; i < boss_.burrowCount; ++i)
            {
                if (RuleDistance(pos, PlayerBodyY, boss_.burrowTargets[i], 0.0f) <= BossBurrowRadius + player_.radius + 0.4f) return true;
            }
        }
    }
    // (4) ダメージ床の中
    for (const auto& o : obstacles_)
    {
        if (!o.damageField) continue;
        if (RuleDistance(pos, PlayerBodyY, o.pos, o.height) <= o.radius + player_.radius) return true;
    }
    return false;
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
    // ボスのつかみで拘束中は操作不可（位置・ダメージはボス側で処理）。
    if (player_.grabbedT > 0.0f)
    {
        prevSpace_ = KeyDown(VK_SPACE);
        prevMouseLeft_ = mouseLeft_;
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

    // マウス座標をゲーム内座標へ変換して、照準モードに応じた向きを作ります。
    const V2 aimPoint = ScreenToWorld(mouseX_, mouseY_);
    player_.face = ResolvePlayerAim(player_, 0, dir, aimPoint);
    const V2 actionPoint = ResolvePlayerAimPoint(player_, 0, aimPoint, 8.0f);

    // スペースキーでブリンク（短距離テレポート＋短い無敵）。最大BlinkMaxChargesまで連続使用可。
    // チャージは1回ごとに BlinkChargeCooldown 秒かけて順番に回復する。
    if (player_.blinkCharges < BlinkMaxCharges)
    {
        player_.blinkRechargeT -= dt;
        if (player_.blinkRechargeT <= 0.0f)
        {
            ++player_.blinkCharges;
            player_.blinkRechargeT = (player_.blinkCharges < BlinkMaxCharges) ? BlinkChargeCooldown : 0.0f;
        }
    }
    const bool spaceHeld = KeyDown(VK_SPACE);
    const bool spacePressed = spaceHeld && !prevSpace_;
    if (spacePressed && player_.blinkCharges > 0)
    {
        const V2 fromPos = player_.pos;
        // ジャスト回避判定：危険な状況（迫る敵弾／ボスの照射ビーム帯／ダメージ床／地中噴出）でのブリンク。
        const bool justDodge = IsBlinkJustDodge(fromPos);
        // フル状態から使い始めるときだけ回復タイマーを起動（連続使用中は継続）。
        if (player_.blinkCharges == BlinkMaxCharges) player_.blinkRechargeT = BlinkChargeCooldown;
        --player_.blinkCharges;
        // 移動入力があればその方向、なければ向いている方向へ飛ぶ。
        const V2 blinkDir = LenSq(dir) > 0.001f ? dir : FromAngle(player_.face);
        const float blinkDist = justDodge ? BlinkJustDistance : BlinkDistance; // 成功時は長く飛ぶ
        Burst(fromPos, Sky, 14);                       // 出発点に残光
        player_.pos += blinkDir * blinkDist;
        ClampInside(player_.pos, player_.radius);
        player_.inv = std::max(player_.inv, BlinkInvuln);
        player_.dashT = 0.0f;                           // 既存ダッシュ移動はキャンセル
        // 攻撃とブリンクの同時発動を禁止：直後の攻撃をロックし、溜め中の攻撃もキャンセル。
        player_.blinkLockT = BlinkAttackLock;
        player_.bombCharge = 0.0f;
        player_.charging = false;
        player_.chargeReady = false;
        player_.chargeFull = false;
        player_.chargeT = 0.0f;
        SyncPlayer3D(player_);
        Burst(player_.pos, Cream, 18);                 // 到着点に出現エフェクト
        audio_.PlaySoundEffect(SoundEffect::Reflect);
        if (justDodge)
        {
            // ヒットストップ＋自キャラへズーム。
            hitstopT_ = HitstopTime;
            justZoomT_ = JustZoomLife;
            justZoomLife_ = JustZoomLife;
            Burst(fromPos, Gold, 26);
            message_ = L"ジャスト回避!";
            messageT_ = std::max(messageT_, 0.9f);
        }
    }
    prevSpace_ = spaceHeld;

    if (player_.fireCd > 0.0f) player_.fireCd -= dt;
    if (player_.blinkLockT > 0.0f) player_.blinkLockT -= dt;
    // ブリンク直後は攻撃ロック中。クリック攻撃（左/右）をこの間は受け付けない＝同時発動を防ぐ。
    const bool attackLocked = player_.blinkLockT > 0.0f;
    // 左クリックは通常弾専用です。長押ししてもチャージ弾は出しません。
    const bool primaryHeld = mouseLeft_ && !attackLocked;
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
    else if (player_.weapon == Weapon::Strawberry)
    {
        // ショート（ミニガン）：足を止めて撃ち続けるほどヒートが上がり、連射速度・威力・反射・サイズが増す。
        // 上限まで振り切るとオーバーヒートして一定時間撃てない。動くと熱が冷めてリセット。
        player_.firing = primaryHeld; // 発射中フラグ（ゲージ表示用）
        if (player_.overheatT > 0.0f)
        {
            // オーバーヒート中：発射ロック。ロック時間をかけて完全冷却する。
            player_.overheatT -= dt;
            player_.fireHeat = std::max(0.0f, player_.fireHeat - dt * (StrawberryHeatMax / StrawberryOverheatLock));
            if (player_.overheatT <= 0.0f)
            {
                player_.overheatT = 0.0f;
                player_.fireHeat = 0.0f;
            }
        }
        else if (primaryHeld)
        {
            // 移動中でもヒートは溜まる。レッドゾーンでは過熱の進みを落として最大火力を長く保つ。
            const float gain = player_.fireHeat >= StrawberryHeatMax * StrawberryRedline
                ? dt * StrawberryRedlineHeatMul
                : dt;
            player_.fireHeat += gain;
            if (player_.fireHeat >= StrawberryHeatMax)
            {
                // 振り切った→オーバーヒート発動。しばらく撃てなくなる隙。
                player_.fireHeat = StrawberryHeatMax;
                player_.overheatT = StrawberryOverheatLock;
                Burst(player_.pos, Gold, 26);
                PlayCombatEffect(L"sword_slash", player_.pos, 0.5f, player_.face, 1.0f, Red, 14);
                message_ = L"オーバーヒート!";
                messageT_ = std::max(messageT_, 1.0f);
            }
            else if (player_.fireCd <= 0.0f)
            {
                FireStrawberryHeat(player_, 0, player_.face);
            }
        }
        else
        {
            // 撃つのをやめると素早く冷める＝小休止での熱管理がしやすい
            player_.fireHeat = std::max(0.0f, player_.fireHeat - dt * 3.5f);
        }
    }
    else if (primaryHeld && player_.fireCd <= 0.0f)
    {
        FirePrimaryFor(player_, 0, player_.face);
    }

    // 右クリックはチャージ専用です。チョコだけ短押し時に壁設置へ分岐します。
    // ブリンク直後のロック中は受け付けない（攻撃とブリンクの同時発動を防ぐ）。
    if (attackLocked)
    {
        // ロック中は右クリックの状態を進めず、リリースも握りつぶす。
    }
    else if (mouseRight_)
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
        else if (mouseRightReleased_ && player_.character == CharacterType::Chocolate && obstacles_.size() < 20)
        {
            // 出す前に最大3枚へ調整（古い壁から消える=FIFO）。
            EnforceChocoWallLimit(3);
            Obstacle wall{};
            // チャージボムの発射方向（player_.face）と同じ向きへ出す。
            wall.pos = player_.pos + FromAngle(player_.face) * 1.1f;
            ClampInside(wall.pos, 0.8f);
            wall.radius = 0.52f;          // 判定（反射）は従来通りこの円のまま
            wall.hp = 110.0f + player_.level * 18.0f;
            wall.ttl = 7.5f;
            wall.kind = 2;
            wall.ownerIndex = 0;
            wall.reflectPower = 1.35f + player_.corePower;
            wall.cheeseWall = true;       // 反射挙動は従来通り
            wall.chocoWall = true;        // FIFO管理＆長方形描画の対象
            wall.spin = player_.face;     // 長方形の向き。chocoWall は spin を加算更新しないので固定される
            wall.color = Choco;
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

// ショートのヒート射撃。撃ち続けた時間(p.fireHeat)に応じて連射速度・威力・反射回数・弾サイズが上がり、
// 最大ヒートでは着弾/反射時に苺片へ分裂する。発射ごとに次弾までのクールダウンを設定する。
void SweetsApp::FireStrawberryHeat(Player& p, int ownerIndex, float aim)
{
    const float t = std::min(1.0f, p.fireHeat / StrawberryHeatMax); // 0..1 のヒート比率
    const int tier = t >= StrawberryRedline ? 3 : (t >= 0.38f ? 2 : (t >= 0.18f ? 1 : 0));

    float dmgScale = 1.0f + (p.level - 1) * 0.18f + p.corePower;
    dmgScale *= p.damageMul;
    if (p.dmgBuffT > 0.0f) dmgScale *= 1.6f;

    const WeaponDef& def = Weapons[static_cast<int>(Weapon::Strawberry)];

    Shot s{};
    s.pos = p.pos + FromAngle(aim) * (p.radius + 0.18f);
    s.vel = FromAngle(aim) * (def.speed + 6.0f * t);                 // ヒートで弾速も上昇
    s.radius = (0.12f + 0.20f * t) * (1.0f + p.level * 0.04f);       // ヒートで弾サイズ上昇
    s.damage = def.damage * dmgScale * (1.0f + 1.6f * t);            // 威力 ×1.0〜×2.6
    s.pierce = def.pierce + (p.level >= 5 ? 1 : 0) + static_cast<int>(p.corePierce);
    s.bounce = 3 + static_cast<int>(p.coreBounce);                  // 壁・ギミック反射は常に3回固定(+コア)
    s.ttl = 2.4f + 0.8f * t;
    s.color = tier >= 3 ? Gold : (tier >= 2 ? Red : (tier >= 1 ? Berry : Rose));
    s.ownerIndex = ownerIndex;
    s.sourceCharacter = CharacterType::Shortcake;
    s.homingStrength = 0.0f;
    s.reflectSplit = 0;                                             // 分裂は廃止
    SyncShot3D(s);
    shots_.push_back(s);

    // 連射クールダウン：ヒートで 0.22s → 0.06s まで短縮
    p.fireCd = (0.22f - 0.16f * t) * p.cooldownMul;
    if (tier >= 3) audio_.PlaySoundEffect(SoundEffect::Reflect);
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
    s.sweep = true; // 薙ぎ払いモーション
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
