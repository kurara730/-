#include "SweetsApp.h"

#include <Xinput.h>

namespace
{
// XInputのスティック値を -1.0 ～ 1.0 に変換します。
// 小さな入力はデッドゾーンとして0にし、意図しない微妙な移動を防ぎます。
float NormalizeThumb(SHORT value)
{
    constexpr float deadZone = 8200.0f;
    const float f = static_cast<float>(value);
    if (std::fabs(f) < deadZone) return 0.0f;
    return ClampFloat(f / 32767.0f, -1.0f, 1.0f);
}
}

// キャラ選択で選ばれたロードアウトをプレイヤー状態へ反映します。
// maxHpや速度など、CSVで上書きされた性能もここから実プレイヤーに入ります。
void SweetsApp::ApplyLoadout(Player& p, int loadoutIndex, int playerIndex, bool ai)
{
    const int safeIndex = std::max(0, std::min(loadoutIndex, static_cast<int>(Loadouts.size()) - 1));
    const LoadoutPreset& loadout = Loadouts[safeIndex];
    p = {};
    p.index = playerIndex;
    p.ai = ai;
    p.active = true;
    p.character = loadout.character;
    p.weapon = loadout.weapon;
    p.maxHp = loadout.maxHp;
    p.hp = loadout.maxHp;
    p.speed = loadout.speed;
    p.damageMul = loadout.damageMul;
    p.cooldownMul = loadout.cooldownMul;
    p.ult = loadout.ultStart;

    const float a = TwoPi * static_cast<float>(playerIndex) / MaxPlayers + Pi * 0.25f;
    p.pos = playerIndex == 0 ? V2{ 0.0f, 3.6f } : FromAngle(a) * 2.6f;
    SyncPlayer3D(p);
}

// 2P-4Pの更新入口です。
// Offは無視、AIは自動操作、Padは接続ゲームパッド入力で動かします。
void SweetsApp::UpdateCoopPlayers(float dt)
{
    for (int i = 1; i < MaxPlayers; ++i)
    {
        Player& p = players_[i];
        if (!p.active) continue;

        if (p.inv > 0.0f) p.inv -= dt;
        if (p.shieldT > 0.0f) p.shieldT -= dt;
        if (p.bombT > 0.0f) p.bombT -= dt;
        if (p.grazeFlash > 0.0f) p.grazeFlash -= dt;
        if (p.dmgBuffT > 0.0f) p.dmgBuffT -= dt;
        if (p.speedBuffT > 0.0f) p.speedBuffT -= dt;
        if (p.scoreDoubleT > 0.0f) p.scoreDoubleT -= dt;
        if (p.magnetT > 0.0f) p.magnetT -= dt;
        if (p.spreadT > 0.0f) p.spreadT -= dt;
        if (p.fireCd > 0.0f) p.fireCd -= dt;
        if (p.chargeCd > 0.0f) p.chargeCd -= dt;
        if (p.feverT > 0.0f) p.feverT -= dt;
        else p.fever = std::max(0.0f, p.fever - dt * 16.0f);

        if (p.downed) continue;

        if (coopSlotModes_[i] == CoopSlotMode::AI)
        {
            p.ai = true;
            UpdateAiPlayer(p, i, dt);
        }
        else if (coopSlotModes_[i] == CoopSlotMode::Pad)
        {
            p.ai = false;
            UpdateGamepadPlayer(p, i, dt);
        }
        if (p.character == CharacterType::Roll && p.dashT > 0.0f)
        {
            ReflectEnemyShotsNear(p.pos, p.radius + 0.72f, i, CharacterType::Roll, Cream, 1.30f);
        }
    }

    TryRevivePlayers(dt);
}

// ゲームパッド操作のプレイヤー更新です。
// Pad指定の枠は、未接続でも自動AIにはせず、明示的な入力だけを使います。
void SweetsApp::UpdateGamepadPlayer(Player& p, int playerIndex, float dt)
{
    XINPUT_STATE state{};
    if (XInputGetState(static_cast<DWORD>(playerIndex - 1), &state) != ERROR_SUCCESS) return;

    V2 move{ NormalizeThumb(state.Gamepad.sThumbLX), NormalizeThumb(state.Gamepad.sThumbLY) };
    move = Normalize(move);
    p.focus = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
    const float focusMul = p.focus ? 0.45f : 1.0f;

    if (p.dashT > 0.0f)
    {
        p.dashT -= dt;
        p.pos += p.dashVel * dt;
    }
    else
    {
        p.vel = move * p.speed * focusMul * (p.speedBuffT > 0.0f ? 1.55f : 1.0f);
        p.pos += p.vel * dt;
    }
    ClampInside(p.pos, p.radius);
    SyncPlayer3D(p);

    if (aimMode_ != AimMode::MoveDirection)
    {
        V2 aim{ NormalizeThumb(state.Gamepad.sThumbRX), NormalizeThumb(state.Gamepad.sThumbRY) };
        if (aimMode_ == AimMode::AutoTarget || LenSq(aim) < 0.05f) aim = Normalize(FindNearestEnemyOrBoss(p.pos) - p.pos);
        if (LenSq(aim) > 0.001f) p.face = AngleOf(aim);
    }
    else if (LenSq(move) > 0.001f)
    {
        p.face = AngleOf(move); // 攻撃方向＝向いている方向（移動方向）
    }

    const bool attackHeld = state.Gamepad.bRightTrigger > 40 || (state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
    if (attackHeld && p.fireCd <= 0.0f)
    {
        FirePrimaryFor(p, playerIndex, p.face);
    }

    const bool chargeHeld = (state.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
    if (chargeHeld)
    {
        p.chargeT += dt;
        p.chargeReady = p.chargeT >= 0.55f;
        p.chargeFull = p.chargeT >= 1.15f;
        p.charging = true;
    }
    else if (p.charging)
    {
        if (p.chargeReady && p.chargeCd <= 0.0f) FireCharged(p, playerIndex, p.face, p.pos + FromAngle(p.face) * 3.0f);
        p.charging = false;
        p.chargeReady = false;
        p.chargeFull = false;
        p.chargeT = 0.0f;
    }

    if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0) UseBombFor(p, playerIndex);
    if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0) UseUltimateFor(p, playerIndex);
}

// AI操作のプレイヤー更新です。
// 近い敵へ向かって攻撃し、危険な弾が近い時は逃げる簡易行動にしています。
void SweetsApp::UpdateAiPlayer(Player& p, int playerIndex, float dt)
{
    V2 target = FindNearestEnemyOrBoss(p.pos);
    V2 toTarget = target - p.pos;
    const float d = RuleDistance(p.pos, PlayerBodyY, target, EnemyBodyY);
    V2 move{};

    for (const auto& other : players_)
    {
        if (other.active && other.downed)
        {
            const float rd = RuleDistance(other.pos, PlayerBodyY, p.pos, PlayerBodyY);
            if (rd < 5.8f)
            {
                move = Normalize(other.pos - p.pos);
                break;
            }
        }
    }

    if (LenSq(move) < 0.01f)
    {
        if (d > 5.0f) move = Normalize(toTarget);
        else if (d < 2.8f) move = Normalize(toTarget) * -1.0f;
        else move = FromAngle(AngleOf(toTarget) + Pi * 0.5f) * 0.35f;
    }

    for (const auto& s : shots_)
    {
        if (!s.enemy) continue;
        const float bd = RuleDistance(s, p);
        if (bd < 1.25f)
        {
            move += Normalize(p.pos - s.pos) * (1.25f - bd) * 2.0f;
        }
    }

    p.focus = true;
    p.vel = Normalize(move) * p.speed * 0.72f;
    p.pos += p.vel * dt;
    ClampInside(p.pos, p.radius);
    SyncPlayer3D(p);

    if (d > 0.001f) p.face = AngleOf(toTarget);
    if (p.fireCd <= 0.0f && d < 9.5f)
    {
        FirePrimaryFor(p, playerIndex, p.face);
    }
    if (p.ult >= 100.0f && boss_.active)
    {
        UseUltimateFor(p, playerIndex);
    }
}

// ダウン中の味方救助処理です。
// 近くに生存プレイヤーが一定時間いると復帰します。
void SweetsApp::TryRevivePlayers(float dt)
{
    for (auto& downed : players_)
    {
        if (!downed.active || !downed.downed) continue;
        bool helperNear = false;
        for (const auto& helper : players_)
        {
            if (!helper.active || helper.downed) continue;
            if (RuleDistance(helper.pos, PlayerBodyY, downed.pos, PlayerBodyY) < 1.15f)
            {
                helperNear = true;
                break;
            }
        }

        if (helperNear)
        {
            downed.reviveT += dt;
            if (downed.reviveT >= 1.6f)
            {
                downed.downed = false;
                downed.alive = true;
                downed.hp = downed.maxHp * 0.45f;
                downed.inv = 1.4f;
                downed.reviveT = 0.0f;
                Burst(downed.pos, Mint, 36);
                message_ = L"味方を救助";
                messageT_ = 1.8f;
            }
        }
        else
        {
            downed.reviveT = std::max(0.0f, downed.reviveT - dt * 0.5f);
        }
    }
}

bool SweetsApp::AllPlayersDown() const
{
    bool anyActive = false;
    for (const auto& p : players_)
    {
        if (!p.active) continue;
        anyActive = true;
        if (!p.downed && p.hp > 0.0f) return false;
    }
    return anyActive;
}

int SweetsApp::ActivePlayerCount() const
{
    int count = 0;
    for (const auto& p : players_)
    {
        if (p.active) ++count;
    }
    return std::max(1, count);
}

// 協力人数による敵HP補正です。
// Offの枠は数えず、実際に参加しているAI/Padだけで増やします。
float SweetsApp::MultiplayerHpMultiplier() const
{
    switch (std::min(4, ActivePlayerCount()))
    {
    case 2: return 1.35f;
    case 3: return 1.65f;
    case 4: return 1.90f;
    default: return 1.0f;
    }
}

// Storyで隠しボスへ到達した時だけ、平均レベルに応じてHPを上げます。
// Practiceでは練習しやすいよう、この補正を掛けません。
float SweetsApp::HiddenBossLevelHpMultiplier() const
{
    if (hiddenBossPractice_) return 1.0f;
    float totalLevel = 0.0f;
    int count = 0;
    for (const auto& p : players_)
    {
        if (!p.active) continue;
        totalLevel += static_cast<float>(std::max(1, p.level));
        ++count;
    }
    if (count <= 0) return 1.0f;
    const float avgLevel = totalLevel / static_cast<float>(count);
    const float bonusLevels = std::min(std::max(0.0f, avgLevel - 1.0f), 20.0f);
    return std::min(1.7f, 1.0f + bonusLevels * 0.035f);
}

Player* SweetsApp::FindNearestPlayer(V2 pos)
{
    Player* best = nullptr;
    float bestD = 99999.0f;
    for (auto& p : players_)
    {
        if (!p.active || p.downed) continue;
        const float d = LenSq(p.pos - pos);
        if (d < bestD)
        {
            bestD = d;
            best = &p;
        }
    }
    return best;
}

const Player* SweetsApp::FindNearestPlayer(V2 pos) const
{
    const Player* best = nullptr;
    float bestD = 99999.0f;
    for (const auto& p : players_)
    {
        if (!p.active || p.downed) continue;
        const float d = LenSq(p.pos - pos);
        if (d < bestD)
        {
            bestD = d;
            best = &p;
        }
    }
    return best;
}

V2 SweetsApp::FindNearestEnemyOrBoss(V2 pos) const
{
    V2 best = boss_.active ? boss_.pos : pos + FromAngle(player_.face) * 4.0f;
    float bestD = boss_.active ? LenSq(best - pos) : 99999.0f;
    for (const auto& e : enemies_)
    {
        if (e.dead) continue;
        const float d = LenSq(e.pos - pos);
        if (d < bestD)
        {
            bestD = d;
            best = e.pos;
        }
    }
    return best;
}

// スコア加算の共通処理です。
// ScoreDouble中は倍率を掛け、最終的な加算値を返します。
float SweetsApp::AddScore(int base, const Player* source)
{
    float mul = 1.0f;
    if (source)
    {
        if (source->scoreDoubleT > 0.0f) mul *= 2.0f;
        if (source->feverT > 0.0f) mul *= 1.5f;
    }
    score_ += static_cast<int>(base * mul);
    return mul;
}

// ボス撃破報酬として、プレイヤーのコア性能を少し伸ばします。
// 雑魚戦の爽快感とボス戦を越えた成長感をつなぐ役割です。
void SweetsApp::GrantBossSkill(Player& p)
{
    ++p.skillCount;
    p.corePower += 0.10f;
    if (p.character == CharacterType::Shortcake)
    {
        p.spreadT = std::max(p.spreadT, 10.0f);
    }
    else if (p.character == CharacterType::Chocolate)
    {
        p.shieldT = std::max(p.shieldT, 5.0f);
    }
    else if (p.character == CharacterType::Cheese)
    {
        p.corePierce += 1.0f;
    }
    else
    {
        p.coreBounce += 1.0f;
    }
}
