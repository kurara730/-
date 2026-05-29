#include "SweetsApp.h"

#include <Xinput.h>

namespace
{
float NormalizeThumb(SHORT value)
{
    constexpr float deadZone = 8200.0f;
    const float f = static_cast<float>(value);
    if (std::fabs(f) < deadZone) return 0.0f;
    return ClampFloat(f / 32767.0f, -1.0f, 1.0f);
}
}

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
    }

    TryRevivePlayers(dt);
}

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

    V2 aim{ NormalizeThumb(state.Gamepad.sThumbRX), NormalizeThumb(state.Gamepad.sThumbRY) };
    if (LenSq(aim) < 0.05f)
    {
        aim = Normalize(FindNearestEnemyOrBoss(p.pos) - p.pos);
    }
    if (LenSq(aim) > 0.001f) p.face = AngleOf(aim);

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
        p.charging = true;
    }
    else if (p.charging)
    {
        if (p.chargeReady && p.chargeCd <= 0.0f) FireCharged(p, playerIndex, p.face, p.pos + FromAngle(p.face) * 3.0f);
        p.charging = false;
        p.chargeReady = false;
        p.chargeT = 0.0f;
    }

    if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0) UseBombFor(p, playerIndex);
    if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0) UseUltimateFor(p, playerIndex);
}

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
