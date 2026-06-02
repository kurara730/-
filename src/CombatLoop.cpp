#include "SweetsApp.h"
#include "ReflectionSystem.h"
#include "StageFactory.h"

#include <algorithm>

namespace
{
// 特殊敵は出しすぎると画面が支配されるため、通常Waveでは数を抑えます。
bool IsEliteType(EnemyType type)
{
    return type == EnemyType::Healer
        || type == EnemyType::Barrier
        || type == EnemyType::Mirror
        || type == EnemyType::Teleport;
}
}

// 指定範囲内の敵弾を味方弾へ変換する共通処理です。
// チーズ壁、ショートの反射フィールド、ロールの反射弾などがこの関数を使います。
void SweetsApp::ReflectEnemyShotsNear(V2 center, float radius, int ownerIndex, CharacterType source, Color color, float power)
{
    int reflected = 0;
    for (auto& s : shots_)
    {
        if (!s.enemy || s.dead) continue;
        if (RuleDistance(s.pos, s.height, center, ShotBodyY) > radius + s.radius) continue;
        V2 n = s.pos - center;
        if (LenSq(n) < 0.001f)
        {
            const Player& p = players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))];
            n = FromAngle(p.face);
        }
        // 反射後は敵弾ではなく味方弾として扱い、スコアや反射倍率にもつなげます。
        ApplyShotReflection(s, Normalize(n), power);
        s.enemy = false;
        s.ownerIndex = std::max(0, std::min(ownerIndex, MaxPlayers - 1));
        s.sourceCharacter = source;
        s.color = color;
        s.damage *= 1.18f;
        s.bounce = std::max(s.bounce, source == CharacterType::Roll ? 3 : 1);
        if (source == CharacterType::Shortcake)
        {
            s.homingStrength = std::max(s.homingStrength, 3.4f);
            s.ttl = std::max(s.ttl, 2.2f);
        }
        if (source == CharacterType::Chocolate)
        {
            s.yoyo = true;
            s.pierce = std::max(s.pierce, 3);
            s.ttl = std::max(s.ttl, 2.4f);
        }
        SyncShot3D(s);
        ++reflected;
        Burst(s.pos, color, 8);
    }
    if (reflected > 0)
    {
        audio_.PlaySoundEffect(SoundEffect::Reflect);
        AddScore(reflected * 24, &players_[std::max(0, std::min(ownerIndex, MaxPlayers - 1))]);
        message_ = L"反射";
        messageT_ = std::max(messageT_, 0.65f);
        effectPulses_.push_back({ center, Grounded3D(center, 0.24f), radius * 0.55f, radius * 1.55f, 0.22f, 0.22f, 0.24f, color });
    }
}

// 全ての弾を更新します。
// 移動 → 壁反射 → 障害物 → 敵/プレイヤー命中、という順番で処理します。
void SweetsApp::UpdateShots(float dt)
{
    std::vector<Shot> spawned; // 反射分裂などイテレーション中に増える弾を遅延追加
    for (auto& s : shots_)
    {
        if (s.dead) continue;
        s.ttl -= dt;
        if (s.ttl <= 0.0f) continue;
        // 敵弾は角速度や加速度を持てます。曲がる弾や加速弾の弾幕に使います。
        const int reflectBefore = s.reflectedCount;

        if (s.chocoBomb)
        {
            // 壁・障害物では反射する（敵では反射しない）。サイズはチャージで確定済み
            s.pos += s.vel * dt;
            bool bounced = false;
            V2 n{};
            if (Len(s.pos) > ArenaRadius - s.radius)
            {
                n = Normalize(s.pos);
                s.pos = n * (ArenaRadius - s.radius);
                s.vel = s.vel - n * (2.0f * Dot(s.vel, n));
                bounced = true;
            }
            if (!bounced)
            {
                for (auto& o : obstacles_)
                {
                    if (o.damageField || o.warpId >= 0) continue;
                    if (RuleDistance(s.pos, s.height, o.pos, o.height) < s.radius + o.radius)
                    {
                        n = Normalize(s.pos - o.pos);
                        if (LenSq(n) < 0.001f) n = FromAngle(0.0f);
                        s.pos = o.pos + n * (s.radius + o.radius + 0.02f);
                        s.vel = s.vel - n * (2.0f * Dot(s.vel, n));
                        bounced = true;
                        break;
                    }
                }
            }
            if (bounced)
            {
                const float sp = std::max(7.0f, Len(s.vel));
                if (LenSq(s.vel) > 0.0001f) s.vel = Normalize(s.vel) * sp;
            }
            // 最大チャージ弾：近くの敵を巻き込み、一度掴んだら弾に固定して逃がさない（ボスは対象外）
            if (s.growStage >= 3)
            {
                const float catchR = s.radius + 0.6f;
                for (auto& e : enemies_)
                {
                    if (e.dead) continue;
                    if (!e.caught && RuleDistance(s, e) < catchR + e.radius)
                    {
                        e.caught = true;
                        e.caughtOffset = e.pos - s.pos; // 掴んだ瞬間の相対位置で固定開始
                    }
                    if (e.caught)
                    {
                        e.caughtOffset = e.caughtOffset * 0.9f; // 徐々に中心へ引き込む
                        e.pos = s.pos + e.caughtOffset;         // 弾に追従（逃げられない）
                        ClampInside(e.pos, e.radius);
                        SyncEnemy3D(e);
                    }
                }
            }
            SyncShot3D(s);
            continue;
        }

        if (s.enemy && (std::fabs(s.angularVel) > 0.0001f || std::fabs(s.accel) > 0.0001f))
        {
            float speed = Len(s.vel);
            float angle = AngleOf(s.vel) + s.angularVel * dt;
            speed = std::max(0.3f, speed + s.accel * dt);
            s.vel = FromAngle(angle) * speed;
        }
        // 味方弾の追尾は、最寄りの敵/ボス方向へ少しずつ速度を曲げます。
        else if (!s.enemy && s.homingStrength > 0.0f)
        {
            const V2 target = FindNearestEnemyOrBoss(s.pos);
            const V2 desired = Normalize(target - s.pos);
            if (LenSq(desired) > 0.001f)
            {
                const float speed = Len(s.vel);
                const V2 blended = Normalize(Normalize(s.vel) + desired * s.homingStrength * dt);
                s.vel = blended * speed;
            }
        }

        s.pos += s.vel * dt;
        if (Use3DRules() && s.enemy)
        {
            const float wave = std::sin(gameTime_ * 1.7f + s.pos.x * 0.37f + s.pos.z * 0.23f);
            s.height = ClampFloat(ShotBodyY + 0.26f * wave + 0.08f * static_cast<float>(s.reflectedCount), 0.16f, 1.15f);
        }
        SyncShot3D(s);
        // ショートとロールは、味方弾そのものが敵弾反射の入口になります。
        if (!s.enemy)
        {
            if (s.sourceCharacter == CharacterType::Shortcake && s.charged)
            {
                ReflectEnemyShotsNear(s.pos, s.radius + 0.70f, s.ownerIndex, CharacterType::Shortcake, Berry, 1.15f);
            }
            else if (s.sourceCharacter == CharacterType::Roll)
            {
                ReflectEnemyShotsNear(s.pos, s.radius + (s.charged ? 0.58f : 0.34f), s.ownerIndex, CharacterType::Roll, Cream, 1.12f);
            }
        }
        V2 fieldNormal{};
        // アリーナ外へ出た弾は、反射回数が残っていれば跳ね返り、無ければ消えます。
        if (ResolveFieldBoundary(s.pos, s.radius, fieldNormal))
        {
            if (s.bounce > 0)
            {
                ApplyShotReflection(s, fieldNormal, s.sourceCharacter == CharacterType::Roll ? 1.18f : 1.0f);
                --s.bounce;
                if (!s.enemy)
                {
                    Player& owner = players_[std::max(0, std::min(s.ownerIndex, MaxPlayers - 1))];
                    AddScore(20 + s.reflectedCount * 10, &owner);
                    if (s.sourceCharacter == CharacterType::Roll)
                    {
                        s.damage *= 1.08f;
                        message_ = L"壁反射";
                        messageT_ = std::max(messageT_, 0.55f);
                    }
                }
            }
            else
            {
                s.dead = true;
                continue;
            }
            SyncShot3D(s);
        }

        // チーズ壁などの障害物との衝突。
        // 敵弾がチーズ壁に当たった場合は味方弾へ変換されます。
        for (auto& o : obstacles_)
        {
            V2 d = s.pos - o.pos;
            const float l = RuleDistance(s.pos, s.height, o.pos, o.height);
            if (l < s.radius + o.radius)
            {
                // ワープポータルは弾を通す（自機専用の回避ギミック）
                if (o.warpId >= 0)
                {
                    continue;
                }

                V2 n = Normalize(d);
                s.pos = o.pos + n * (s.radius + o.radius + 0.01f);

                // 破壊可能オブジェへのダメージ（プレイヤー弾のみ）
                if (o.breakable && !s.enemy)
                {
                    o.hp -= s.damage;
                    o.flash = 1.0f;
                }

                if (s.enemy && o.cheeseWall)
                {
                    ApplyShotReflection(s, n, std::max(1.15f, o.reflectPower));
                    s.enemy = false;
                    s.ownerIndex = std::max(0, o.ownerIndex);
                    s.sourceCharacter = CharacterType::Cheese;
                    s.color = Gold;
                    s.damage *= 1.25f;
                    s.bounce = std::max(s.bounce, 2);
                }
                else if (o.bumper)
                {
                    // バンパー：加速して大きく反射＋発光・スコア
                    ApplyShotReflection(s, n, std::max(1.55f, o.reflectPower));
                    const float sp = Len(s.vel);
                    s.vel = Normalize(s.vel) * std::min(sp * 1.3f + 1.5f, 26.0f);
                    o.flash = 1.0f;
                    Burst(s.pos, Gold, 10);
                    if (!s.enemy)
                    {
                        AddScore(12, &players_[std::max(0, std::min(s.ownerIndex, MaxPlayers - 1))]);
                        message_ = L"バンパー反射!";
                        messageT_ = std::max(messageT_, 0.6f);
                    }
                    if (s.bounce > 0) --s.bounce;
                }
                else if (s.bounce > 0 || s.enemy)
                {
                    ApplyShotReflection(s, n, o.reflectPower);
                    if (s.bounce > 0) --s.bounce;
                }
                else
                {
                    s.dead = true;
                }
                break;
            }
        }

        if (s.dead) continue;

        // ショート弾：反射した瞬間に小さな追尾弾へ分裂（面制圧）
        if (!s.enemy && s.reflectSplit > 0 && s.reflectedCount > reflectBefore)
        {
            const float baseAng = AngleOf(s.vel);
            const int n = s.reflectSplit;
            for (int i = 0; i < n; ++i)
            {
                const float a = baseAng + (static_cast<float>(i) / std::max(1, n - 1) - 0.5f) * 1.1f;
                Shot child{};
                child.pos = s.pos;
                child.vel = FromAngle(a) * 8.5f;
                child.radius = 0.11f;
                child.damage = s.damage * 0.42f;
                child.ttl = 1.4f;
                child.homingStrength = 1.6f;
                child.ownerIndex = s.ownerIndex;
                child.sourceCharacter = CharacterType::Shortcake;
                child.color = Berry;
                SyncShot3D(child);
                spawned.push_back(child);
            }
            Burst(s.pos, Berry, 12);
            s.reflectSplit = 0;
        }

        if (s.enemy)
        {
            for (auto& p : players_)
            {
                if (!p.active || p.downed) continue;
                const float hitDist = s.radius + p.hitboxRadius;
                const float grazeDist = s.radius + p.grazeRadius;
                const float d = RuleDistance(s, p);
                // グレイズは「当たり判定には触れていないが近くをかすめた」時の報酬です。
                // 先にグレイズを見てから被弾を見ることで、弾幕回避の手応えを出しています。
                if (!s.grazed && d < grazeDist && d >= hitDist)
                {
                    s.grazed = true;
                    ++p.graze;
                    ++p.grazeChain;
                    p.grazeFlash = 0.18f;
                    p.ult = std::min(100.0f, p.ult + 0.45f);
                    AddScore(12 + std::min(p.grazeChain, 80), &p);
                }
                if (d < hitDist)
                {
                    ResolvePlayerHit(p, s.damage, AngleOf(p.pos - s.pos));
                    s.dead = true;
                    break;
                }
            }
        }
        else
        {
            if (DamageHiddenBossCore(ReflectedDamage(s), s.pos, s.ownerIndex))
            {
                if (s.pierce > 0) --s.pierce;
                else s.dead = true;
            }
            if (s.dead) continue;

            for (auto& e : enemies_)
            {
                if (e.dead) continue;
                if (RuleDistance(s, e) < s.radius + e.radius)
                {
                    const bool wasChargedSplit = s.charged && s.sourceCharacter == CharacterType::Shortcake && s.splitCount > 0;
                    const float comboMul = s.yoyo ? (1.0f + 0.16f * static_cast<float>(std::min(s.yoyoCombo, 6))) : 1.0f;
                    const bool prevSuppressUltGain = suppressEnemyKillUltGain_;
                    if (s.ultimateSource) suppressEnemyKillUltGain_ = true;
                    DamageEnemy(e, ReflectedDamage(s) * comboMul, s.pos, 1.0f, s.reflected || s.yoyo, s.ownerIndex);
                    suppressEnemyKillUltGain_ = prevSuppressUltGain;
                    if (e.type == EnemyType::Mirror && !s.reflected)
                    {
                        Burst(e.pos, Sky, 10);
                    }
                    if (wasChargedSplit) SpawnSplitShots(s, e.pos);
                    if (s.yoyo)
                    {
                        const bool sameTarget = s.lastHitEnemyId == e.id;
                        s.lastHitEnemyId = e.id;
                        s.yoyoCombo = std::min(8, s.yoyoCombo + (sameTarget ? 2 : 1));
                        ApplyShotReflection(s, Normalize(s.vel), 1.05f + 0.03f * s.yoyoCombo);
                        s.reflected = true;
                        s.reflectedCount = std::max(s.reflectedCount, s.yoyoCombo);
                        s.damage *= 1.03f;
                        --s.pierce;
                        Player& owner = players_[std::max(0, std::min(s.ownerIndex, MaxPlayers - 1))];
                        AddScore(70 + s.yoyoCombo * 35, &owner);
                        message_ = L"ヨーヨー反射コンボ x" + std::to_wstring(std::max(1, s.yoyoCombo));
                        messageT_ = 0.85f;
                        const V2 next = FindNearestEnemyOrBoss(s.pos);
                        const V2 dir = Normalize((LenSq(next - s.pos) > 0.02f ? next : owner.pos) - s.pos);
                        if (LenSq(dir) > 0.001f) s.vel = dir * std::max(9.0f, Len(s.vel));
                        s.pos += Normalize(s.vel) * (s.radius + e.radius + 0.08f);
                        if (s.pierce < 0) s.dead = true;
                    }
                    else
                    {
                        if (s.reflected)
                        {
                            Player& owner = players_[std::max(0, std::min(s.ownerIndex, MaxPlayers - 1))];
                            AddScore(36 + s.reflectedCount * 18, &owner);
                        }
                        if (s.pierce > 0) --s.pierce;
                        else s.dead = true;
                    }
                    break;
                }
            }
            // ボスへの貫通弾/斬撃波は hitBoss で多段ヒットを防ぎます。
            // これを入れないと、1発が毎フレーム当たり続けてHPを一瞬で削ってしまいます。
            if (!s.dead && !s.hitBoss && boss_.active && RuleDistance(s, boss_) < s.radius + boss_.radius)
            {
                if (s.charged && s.sourceCharacter == CharacterType::Shortcake && s.splitCount > 0) SpawnSplitShots(s, boss_.pos);
                BossDamageKind kind = BossDamageKind::NormalShot;
                if (s.reflected)
                {
                    kind = BossDamageKind::ReflectedShot;
                }
                else if (s.charged && s.sourceCharacter == CharacterType::Chocolate)
                {
                    kind = BossDamageKind::ChocolateCharge;
                }
                else if (s.charged)
                {
                    kind = BossDamageKind::ChargeShot;
                }
                DamageBoss(ReflectedDamage(s), kind, s.reflected, s.ownerIndex);
                s.hitBoss = true;
                if (s.pierce > 0) --s.pierce;
                else s.dead = true;
            }
        }
    }

    for (const auto& sp : spawned) shots_.push_back(sp);
}

void SweetsApp::ReleaseCaughtIfNoBomb()
{
    // 巻き込み弾が無くなったら（爆発/消滅）捕まっていた敵を解放する
    bool engulf = false;
    for (const auto& s : shots_)
    {
        if (!s.dead && s.chocoBomb && s.growStage >= 3 && s.ttl > 0.0f) { engulf = true; break; }
    }
    if (!engulf)
    {
        for (auto& e : enemies_) e.caught = false;
    }
}

// アイテムの寿命、磁石吸引、取得効果を処理します。
// 効果時間系は Player の各タイマーへ入れ、毎フレーム減らしていきます。
void SweetsApp::UpdatePickups(float dt)
{
    for (auto& item : pickups_)
    {
        item.ttl -= dt;
        if (item.ttl <= 0.0f) continue;

        Player* taker = nullptr;
        float bestD = 99999.0f;
        for (auto& p : players_)
        {
            if (!p.active || p.downed) continue;
            const float d = RuleDistance(p, item);
            if (p.magnetT > 0.0f && d < 4.6f)
            {
                item.pos += Normalize(p.pos - item.pos) * (dt * 6.5f);
                SyncPickup3D(item);
            }
            if (d < item.radius + p.radius && d < bestD)
            {
                bestD = d;
                taker = &p;
            }
        }

        if (taker)
        {
            Player& p = *taker;
            switch (item.pickupType)
            {
            case PickupType::Heal:
                p.hp = std::min(p.maxHp, p.hp + 45.0f);
                message_ = L"回復アイテム";
                break;
            case PickupType::Attack:
                p.dmgBuffT = 8.0f;
                message_ = L"攻撃力アップ";
                break;
            case PickupType::Slow:
                slowT_ = 7.0f;
                message_ = L"敵スロー";
                break;
            case PickupType::Invincible:
                p.shieldT = 5.0f;
                p.inv = 2.0f;
                message_ = L"無敵";
                break;
            case PickupType::Magnet:
                p.magnetT = 10.0f;
                message_ = L"マグネット";
                break;
            case PickupType::BombDamage:
                for (auto& e : enemies_) DamageEnemy(e, 95.0f + wave_ * 6.0f, p.pos, 2.0f, false, p.index);
                if (boss_.active) DamageBoss(260.0f + wave_ * 18.0f, BossDamageKind::Bomb, false, p.index);
                Burst(p.pos, Red, 60);
                message_ = L"爆発アイテム";
                break;
            case PickupType::UltFull:
                p.ult = 100.0f;
                message_ = L"必殺ゲージ満タン";
                break;
            case PickupType::Spread:
                p.spreadT = 10.0f;
                message_ = L"拡散ショット";
                break;
            case PickupType::Speed:
                p.speedBuffT = 8.0f;
                message_ = L"速度アップ";
                break;
            case PickupType::ScoreDouble:
                p.scoreDoubleT = 10.0f;
                message_ = L"スコア2倍";
                break;
            default:
                p.bombs = std::min(5, p.bombs + 1);
                p.inv = std::max(p.inv, 1.0f);
                message_ = L"ボム+1";
                break;
            }
            Burst(item.pos, item.color, 28);
            messageT_ = 2.0f;
            item.ttl = 0.0f;
        }
    }
}

