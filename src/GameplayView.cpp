#include "SweetsApp.h"

#include <filesystem>

// GameplayView.cpp は戦闘画面の表示を担当します。
// 2Dモードではスプライト、3Dモードではメッシュを使い、ゲームルール側の状態を見た目へ変換します。

namespace
{
// TAA用のジッター値です。2D/3Dどちらの戦闘画面にも使えます。
float Halton(int index, int base)
{
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0)
    {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }
    return r;
}

std::wstring FindAssetFile(const std::wstring& relativePath)
{
    namespace fs = std::filesystem;
    const fs::path rel(relativePath);

    std::array<fs::path, 5> candidates{};
    candidates[0] = fs::current_path() / rel;

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    const fs::path exeDir = fs::path(modulePath).parent_path();
    candidates[1] = exeDir / rel;
    candidates[2] = exeDir.parent_path() / rel;
    candidates[3] = exeDir.parent_path().parent_path() / rel;
    candidates[4] = rel;

    for (const auto& path : candidates)
    {
        std::error_code ec;
        if (fs::exists(path, ec))
        {
            return path.wstring();
        }
    }
    return relativePath;
}

bool PointInRect(float sx, float sy, float left, float top, float right, float bottom)
{
    return sx >= left && sx <= right && sy >= top && sy <= bottom;
}

float GameplayHalfHeight()
{
    return 11.5f;
}

float GameplayHalfWidth(UINT width, UINT height)
{
    const float aspect = static_cast<float>(std::max(1u, width)) / std::max(1.0f, static_cast<float>(height));
    return GameplayHalfHeight() * aspect;
}

const wchar_t* CharacterSpriteId(CharacterType type)
{
    switch (type)
    {
    case CharacterType::Chocolate: return L"2d_player_chocolate";
    case CharacterType::Cheese: return L"2d_player_cheese";
    case CharacterType::Roll: return L"2d_player_roll";
    case CharacterType::Shortcake:
    default: return L"2d_player_shortcake";
    }
}

const wchar_t* EnemySpriteId(EnemyType type)
{
    switch (type)
    {
    case EnemyType::Shield: return L"2d_enemy_shield";
    case EnemyType::Split: return L"2d_enemy_split";
    case EnemyType::Healer: return L"2d_enemy_healer";
    case EnemyType::Barrier: return L"2d_enemy_barrier";
    case EnemyType::Mirror: return L"2d_enemy_mirror";
    case EnemyType::Mine: return L"2d_enemy_mine";
    case EnemyType::Teleport: return L"2d_enemy_teleport";
    case EnemyType::Normal:
    default: return L"2d_enemy_normal";
    }
}

const wchar_t* PickupSpriteId(PickupType type)
{
    switch (type)
    {
    case PickupType::Slow: return L"2d_pickup_slow";
    case PickupType::Invincible: return L"2d_pickup_invincible";
    case PickupType::Magnet: return L"2d_pickup_magnet";
    case PickupType::BombDamage: return L"2d_pickup_bomb";
    case PickupType::Heal: return L"2d_pickup_heal";
    case PickupType::UltFull: return L"2d_pickup_ult";
    case PickupType::Spread: return L"2d_pickup_spread";
    case PickupType::Speed: return L"2d_pickup_speed";
    case PickupType::ScoreDouble: return L"2d_pickup_score";
    case PickupType::Attack:
    default: return L"2d_pickup_attack";
    }
}
}

// 3Dモードの戦闘表示です。
// ルール側も3D判定に切り替わるため、高さ付きの敵弾やボス攻撃を確認できます。
void SweetsApp::DrawGameplay3D()
{
    float blendFactor[4]{ 0, 0, 0, 0 };
    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->PSSetShader(ps_.Get(), nullptr, 0);
    ID3D11Buffer* frameCb = frameCB_.Get();
    ID3D11Buffer* objectCb = objectCB_.Get();
    context_->VSSetConstantBuffers(0, 1, &frameCb);
    context_->PSSetConstantBuffers(0, 1, &frameCb);
    context_->VSSetConstantBuffers(1, 1, &objectCb);
    context_->PSSetConstantBuffers(1, 1, &objectCb);
    context_->RSSetState(rasterState_.Get());
    context_->OMSetDepthStencilState(depthState_.Get(), 0);
    context_->OMSetBlendState(alphaBlend_.Get(), blendFactor, 0xffffffff);

    DrawMesh(floorMesh_,
        XMMatrixScaling(ArenaRadius + 1.3f, 1.0f, ArenaRadius + 1.3f) *
        XMMatrixTranslation(0.0f, -0.035f, 0.0f),
        WithAlpha(Rose, 0.16f));
    DrawMesh(floorMesh_,
        XMMatrixScaling(ArenaRadius, 1.0f, ArenaRadius) *
        XMMatrixTranslation(0.0f, -0.025f, 0.0f),
        WithAlpha({ 0.18f, 0.07f, 0.12f, 1.0f }, 0.94f));
    DrawMesh(ringMesh_,
        XMMatrixScaling(ArenaRadius, 1.0f, ArenaRadius) *
        XMMatrixTranslation(0.0f, 0.015f, 0.0f),
        WithAlpha(Gold, 0.80f));
    DrawMesh(ringMesh_,
        XMMatrixScaling(ArenaRadius * 0.68f, 1.0f, ArenaRadius * 0.68f) *
        XMMatrixTranslation(0.0f, 0.025f, 0.0f),
        WithAlpha(Cream, 0.22f));

    auto ringAt = [&](V2 pos, float r, float y, Color c)
    {
        DrawMesh(ringMesh_,
            XMMatrixScaling(r, 1.0f, r) * XMMatrixTranslation(pos.x, y, pos.z), c);
    };
    for (const auto& o : obstacles_)
    {
        if (o.damageField)
        {
            DrawCylinder(o.pos, o.radius, 0.08f, WithAlpha(Red, 0.42f));
            ringAt(o.pos, o.radius, 0.16f, WithAlpha(Red, 0.78f));
        }
        else if (o.warpId >= 0)
        {
            // ワープポータル：回転する光輪
            DrawCylinder(o.pos, o.radius * 0.5f, 0.05f, WithAlpha(o.color, o.flash > 0.0f ? 0.8f : 0.35f));
            ringAt(o.pos, o.radius * 1.2f, 0.20f, WithAlpha(o.color, 0.85f));
            ringAt(o.pos, o.radius * (0.7f + 0.15f * std::sin(o.spin * 4.0f)), 0.24f, WithAlpha(Cream, 0.60f));
        }
        else if (o.chocoWall)
        {
            // チョコウォールは長方形（見た目のみ）。判定は radius の円のままなので、
            // 横幅は判定円から極端にはみ出さないサイズに抑える（広い面の半幅 ≈ radius）。
            const Color c = o.flash > 0.0f ? Cream : o.color;
            const float top = 0.82f;
            const float depth = o.radius * 0.85f; // 正面方向（厚み・薄い）
            const float width = o.radius * 1.9f;  // 正面に直交する向き（広い面）
            DrawMesh(cubeMesh_,
                XMMatrixScaling(depth, top, width) *
                XMMatrixRotationY(-o.spin) *
                XMMatrixTranslation(o.pos.x, top * 0.5f, o.pos.z),
                WithAlpha(c, 0.9f));
            ringAt(o.pos, o.radius * 1.05f, 0.6f, WithAlpha(Cream, 0.24f));
        }
        else
        {
            const Color c = o.flash > 0.0f ? Cream : o.color;
            const float top = o.cheeseWall ? 0.82f : (o.bumper ? 0.66f : 0.56f);
            DrawCylinder(o.pos, o.radius, top, WithAlpha(c, 0.86f));
            ringAt(o.pos, o.radius * 1.08f, 0.65f, WithAlpha(o.bumper ? Gold : Cream, o.bumper ? 0.55f : 0.30f));
        }
    }

    for (const auto& p : pickups_) DrawPickupShape3D(p);

    for (const auto& s : shots_)
    {
        if (s.chocoBomb)
        {
            // バウンドで巨大化する爆弾弾（段階1〜3）
            const int stage = s.growStage;
            const Color bc = stage >= 3 ? Gold : (stage >= 2 ? Berry : Choco);
            DrawSphere(s.pos, s.height, s.radius * 1.8f, bc);
            for (int r = 0; r < stage; ++r)
            {
                ringAt(s.pos, s.radius * (1.9f + r * 0.5f), s.height + 0.01f, WithAlpha(Cream, 0.55f - r * 0.12f));
            }
            continue;
        }
        if (s.visual == ShotVisualKind::Blade)
        {
            const float angle = AngleOf(s.vel);
            DrawMesh(wedgeMesh_,
                XMMatrixScaling(s.radius * 5.6f, 1.0f, s.radius * 2.4f) *
                XMMatrixRotationY(-angle) *
                XMMatrixTranslation(s.pos.x, s.height, s.pos.z),
                WithAlpha(Grape, 0.86f));
            DrawMesh(ringMesh_,
                XMMatrixScaling(s.radius * 2.45f, 1.0f, s.radius * 2.45f) *
                XMMatrixTranslation(s.pos.x, s.height + 0.015f, s.pos.z),
                WithAlpha(Red, 0.48f));
            continue;
        }
        const float scale = s.charged ? 1.55f : 1.0f;
        const Color shotColor = s.hiddenBossAuraKey ? Gold : (s.enemy && s.visual == ShotVisualKind::Homing ? Red : s.color);
        DrawSphere(s.pos, s.height, s.radius * 1.75f * scale * (s.visual == ShotVisualKind::Homing ? 1.12f : 1.0f), shotColor);
        if (s.enemy)
        {
            const Color ringColor = s.hiddenBossAuraKey ? Gold : (s.visual == ShotVisualKind::Homing ? Grape : Red);
            DrawMesh(ringMesh_,
                XMMatrixScaling(s.radius * (s.hiddenBossAuraKey ? 3.10f : (s.visual == ShotVisualKind::Homing ? 2.70f : 2.25f)), 1.0f, s.radius * (s.hiddenBossAuraKey ? 3.10f : (s.visual == ShotVisualKind::Homing ? 2.70f : 2.25f))) *
                XMMatrixTranslation(s.pos.x, s.height, s.pos.z),
                WithAlpha(ringColor, s.hiddenBossAuraKey ? 0.62f : 0.42f));
            if (s.hiddenBossAuraKey)
            {
                DrawMesh(ringMesh_,
                    XMMatrixScaling(s.radius * 2.30f, 1.0f, s.radius * 2.30f) *
                    XMMatrixTranslation(s.pos.x, s.height + 0.018f, s.pos.z),
                    WithAlpha(Cream, 0.42f));
            }
        }
        else
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(s.radius * 1.80f, 1.0f, s.radius * 1.80f) *
                XMMatrixTranslation(s.pos.x, s.height, s.pos.z),
                WithAlpha(Sky, 0.20f));
        }
        if (s.reflected)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(s.radius * 2.95f, 1.0f, s.radius * 2.95f) *
                XMMatrixTranslation(s.pos.x, s.height + 0.02f, s.pos.z),
                WithAlpha(Gold, 0.56f));
        }
    }

    for (const auto& e : enemies_)
    {
        if (e.dead) continue;
        Color c = e.flash > 0.0f ? Cream : e.color;
        if (e.barrierT > 0.0f) c = Sky;
        const float cloneScale = e.hiddenBossClone ? 1.12f : 1.0f;
        DrawSphere(e.pos, e.height, e.radius * cloneScale, c);
        DrawCylinder(e.pos, e.radius * (e.hiddenBossClone ? 0.74f : 0.62f), e.height, WithAlpha(c, e.hiddenBossClone ? 0.34f : 0.42f));
        if (e.hiddenBossClone)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(e.radius * 1.78f, 1.0f, e.radius * 1.78f) *
                XMMatrixTranslation(e.pos.x, 0.11f, e.pos.z),
                WithAlpha(Cream, 0.24f));
        }
    }

    if (boss_.active)
    {
        Color c = boss_.flash > 0.0f ? Cream : (boss_.bossType == BossType::HiddenBoss ? Grape : Navy);
        // 腕（ボス本体の一部・左右2本）。黒い節を連ねて先端に赤（掴み判定）。消滅中は描かない。
        if (boss_.burrowSubT <= 0.0f && boss_.flyT <= 0.0f && boss_.flyStrikeWarnT <= 0.0f && boss_.bossType != BossType::HiddenBoss)
        {
            for (int arm = 0; arm < 2; ++arm)
            {
                if (boss_.armDownT[arm] > 0.0f) continue;
                const V2 tip = boss_.armPos[arm];
                const V2 dd = tip - boss_.pos;
                const float ang = AngleOf(dd);
                // 黒い節（ボス側→先端へ、だんだん大きく）
                const float seg[4] = { 0.34f, 0.50f, 0.66f, 0.80f };
                const float segR[4] = { 0.20f, 0.27f, 0.36f, 0.50f };
                for (int s = 0; s < 4; ++s)
                {
                    const V2 sp = boss_.pos + dd * seg[s];
                    DrawSphere(sp, boss_.height * 0.6f, segR[s], Ink);
                }
                // 先端の赤（掴み＆ダメージ）。掴み中は紫に。
                const Color tipCol = (boss_.grabHoldT > 0.0f) ? Grape : Red;
                DrawSphere(tip, boss_.height * 0.6f, BossArmRadius, tipCol);
                DrawMesh(ringMesh_,
                    XMMatrixScaling(BossArmRadius * 1.08f, 1.0f, BossArmRadius * 1.08f) *
                    XMMatrixTranslation(tip.x, 0.13f, tip.z),
                    WithAlpha(tipCol, 0.7f));
                (void)ang;
            }
        }
        if (boss_.burrowSubT <= 0.0f) // 地中突き上げの潜行中は本体を隠す
        {
            DrawSphere(boss_.pos, boss_.height, boss_.radius, c);
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * 1.42f, 1.0f, boss_.radius * 1.42f) *
                XMMatrixTranslation(boss_.pos.x, 0.10f, boss_.pos.z),
                WithAlpha(Red, 0.45f));
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossForm_ >= 2)
        {
            const float pulse = 0.14f * std::sin(gameTime_ * (hiddenBossForm_ >= 3 ? 8.0f : 5.2f));
            const float alpha = hiddenBossForm_ >= 3 ? 0.88f : 0.64f;
#if defined(_DEBUG)
            const float auraFx = ClampFloat(debug_.hiddenBossAuraFx, 0.0f, 2.0f);
#else
            const float auraFx = 1.0f;
#endif
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (2.05f + pulse), 1.0f, boss_.radius * (2.05f + pulse)) *
                XMMatrixTranslation(boss_.pos.x, 0.16f, boss_.pos.z),
                WithAlpha(Gold, ClampFloat(alpha * auraFx, 0.0f, 1.0f)));
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (2.70f - pulse), 1.0f, boss_.radius * (2.70f - pulse)) *
                XMMatrixTranslation(boss_.pos.x, 0.18f, boss_.pos.z),
                WithAlpha(Cream, ClampFloat(alpha * 0.42f * auraFx, 0.0f, 1.0f)));
            const int flameCount = hiddenBossForm_ >= 3 ? 18 : 12;
            for (int i = 0; i < flameCount; ++i)
            {
                const float a = TwoPi * static_cast<float>(i) / static_cast<float>(flameCount) + gameTime_ * 0.8f;
                const float wave = std::sin(gameTime_ * 5.0f + i * 1.7f);
                const V2 pos = boss_.pos + FromAngle(a) * (boss_.radius * (1.55f + 0.12f * wave));
                DrawCylinder(pos, 0.045f + 0.018f * std::fabs(wave), 0.70f + 0.28f * std::fabs(wave), WithAlpha(Gold, ClampFloat(0.58f * auraFx, 0.0f, 1.0f)));
                DrawSphere(pos, BossBodyY + 0.88f + 0.18f * wave, 0.10f, WithAlpha(Cream, ClampFloat(0.45f * auraFx, 0.0f, 1.0f)));
            }
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossReflectT_ > 0.0f)
        {
            const float pulse = 0.20f * std::sin(gameTime_ * 16.0f);
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (2.92f + pulse), 1.0f, boss_.radius * (2.92f + pulse)) *
                XMMatrixTranslation(boss_.pos.x, 0.21f, boss_.pos.z),
                WithAlpha(Gold, 0.86f));
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (3.45f - pulse), 1.0f, boss_.radius * (3.45f - pulse)) *
                XMMatrixTranslation(boss_.pos.x, 0.24f, boss_.pos.z),
                WithAlpha(Cream, 0.52f));
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossDashWarnT_ > 0.0f)
        {
            const float warn = ClampFloat(hiddenBossDashWarnT_ / std::max(0.01f, hiddenBossDashWarnLife_), 0.0f, 1.0f);
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (1.45f + (1.0f - warn) * 0.80f), 1.0f, boss_.radius * (1.45f + (1.0f - warn) * 0.80f)) *
                XMMatrixTranslation(boss_.pos.x, 0.28f, boss_.pos.z),
                WithAlpha(Red, 0.74f));
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossForm_ == 1)
        {
            for (const auto& core : hiddenBossCores_)
            {
                if (!core.active) continue;
                DrawSphere(core.pos, ShotBodyY + 0.12f, core.radius, core.flash > 0.0f ? Cream : Gold);
                DrawMesh(ringMesh_,
                    XMMatrixScaling(core.radius * 1.55f, 1.0f, core.radius * 1.55f) *
                    XMMatrixTranslation(core.pos.x, ShotBodyY + 0.02f, core.pos.z),
                    WithAlpha(Red, 0.68f));
            }
        }
        if (boss_.telegraphT > 0.0f && boss_.telegraphLife > 0.0f)
        {
            const float t = 1.0f - ClampFloat(boss_.telegraphT / boss_.telegraphLife, 0.0f, 1.0f);
            const Color telegraph = boss_.telegraphAttack == 0 ? Grape : (boss_.telegraphAttack == 1 ? Red : (boss_.telegraphAttack == 2 ? Gold : Mint));
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (1.75f + t * 1.15f), 1.0f, boss_.radius * (1.75f + t * 1.15f)) *
                XMMatrixTranslation(boss_.pos.x, 0.14f, boss_.pos.z),
                WithAlpha(telegraph, 0.72f));
        }
        // 貫通ビーム：予兆線（点滅）と照射（発光する角柱）を3Dで描画。
        if (boss_.beamWarnT > 0.0f || boss_.beamActiveT > 0.0f)
        {
            const float ang = boss_.beamAngle;
            const V2 bdir = FromAngle(ang);
            const V2 center = boss_.pos + bdir * (BossBeamLength * 0.5f);
            if (boss_.beamActiveT > 0.0f)
            {
                const float pulse = 0.5f + 0.5f * std::sin(gameTime_ * 30.0f);
                const float h = 0.72f;
                DrawMesh(cubeMesh_,
                    XMMatrixScaling(BossBeamLength, h, BossBeamHalfWidth * 2.0f) *
                    XMMatrixRotationY(-ang) *
                    XMMatrixTranslation(center.x, h * 0.5f + 0.05f, center.z),
                    WithAlpha(Red, 0.62f));
                DrawMesh(cubeMesh_,
                    XMMatrixScaling(BossBeamLength, h * 0.42f, BossBeamHalfWidth) *
                    XMMatrixRotationY(-ang) *
                    XMMatrixTranslation(center.x, h * 0.5f + 0.06f, center.z),
                    WithAlpha(Cream, ClampFloat(0.55f + 0.25f * pulse, 0.0f, 1.0f)));
            }
            else
            {
                const float blink = 0.30f + 0.35f * std::sin(gameTime_ * 18.0f);
                DrawMesh(cubeMesh_,
                    XMMatrixScaling(BossBeamLength, 0.04f, BossBeamHalfWidth * 2.0f) *
                    XMMatrixRotationY(-ang) *
                    XMMatrixTranslation(center.x, 0.06f, center.z),
                    WithAlpha(Red, ClampFloat(blink, 0.0f, 0.7f)));
            }
        }
        // 地中突き上げ：潜行中は予測円（ロック前は点滅・ロック後は実線寄り）、噴出中は発光円。
        if (boss_.burrowSubT > 0.0f)
        {
            const bool locked = boss_.burrowSubT <= BossBurrowLockTime;
            const float blink = locked ? 0.78f : (0.30f + 0.40f * std::sin(gameTime_ * 16.0f));
            for (int i = 0; i < boss_.burrowCount; ++i)
            {
                const V2 at = boss_.burrowTargets[i];
                DrawMesh(ringMesh_,
                    XMMatrixScaling(BossBurrowRadius, 1.0f, BossBurrowRadius) *
                    XMMatrixTranslation(at.x, 0.08f, at.z),
                    WithAlpha(Grape, ClampFloat(blink, 0.0f, 0.85f)));
                if (locked)
                {
                    DrawMesh(ringMesh_,
                        XMMatrixScaling(BossBurrowRadius * 0.62f, 1.0f, BossBurrowRadius * 0.62f) *
                        XMMatrixTranslation(at.x, 0.09f, at.z),
                        WithAlpha(Red, 0.55f));
                }
            }
        }
        if (boss_.burrowEruptT > 0.0f)
        {
            const float g = ClampFloat(boss_.burrowEruptT / BossBurrowEruptTime, 0.0f, 1.0f);
            for (int i = 0; i < boss_.burrowCount; ++i)
            {
                const V2 at = boss_.burrowTargets[i];
                // 開始(g=1)で太く高く噴き上がり、減衰(g→0)で細く低くなる。
                DrawCylinder(at, BossBurrowRadius * (0.45f + 0.55f * g), 0.4f + 1.8f * g, WithAlpha(Grape, 0.3f + 0.5f * g));
            }
        }
        // 極太回転ビーム：予兆は薄い線、照射は極太の発光角柱（回転）。
        if (boss_.megaBeamWarnT > 0.0f || boss_.megaBeamActiveT > 0.0f)
        {
            const float ang = boss_.megaBeamAngle;
            const V2 bdir = FromAngle(ang);
            const V2 center = boss_.pos + bdir * (BossMegaBeamLength * 0.5f);
            if (boss_.megaBeamActiveT > 0.0f)
            {
                const float pulse = 0.5f + 0.5f * std::sin(gameTime_ * 26.0f);
                const float h = 0.95f;
                DrawMesh(cubeMesh_,
                    XMMatrixScaling(BossMegaBeamLength, h, BossMegaBeamHalfWidth * 2.0f) *
                    XMMatrixRotationY(-ang) *
                    XMMatrixTranslation(center.x, h * 0.5f + 0.05f, center.z),
                    WithAlpha(Red, 0.6f));
                DrawMesh(cubeMesh_,
                    XMMatrixScaling(BossMegaBeamLength, h * 0.5f, BossMegaBeamHalfWidth) *
                    XMMatrixRotationY(-ang) *
                    XMMatrixTranslation(center.x, h * 0.5f + 0.07f, center.z),
                    WithAlpha(Cream, ClampFloat(0.5f + 0.3f * pulse, 0.0f, 1.0f)));
            }
            else
            {
                const float blink = 0.3f + 0.35f * std::sin(gameTime_ * 18.0f);
                DrawMesh(cubeMesh_,
                    XMMatrixScaling(BossMegaBeamLength, 0.05f, BossMegaBeamHalfWidth * 2.0f) *
                    XMMatrixRotationY(-ang) *
                    XMMatrixTranslation(center.x, 0.06f, center.z),
                    WithAlpha(Red, ClampFloat(blink, 0.0f, 0.7f)));
            }
        }
        // つかみ：拘束中はボス→対象を結ぶ腕を強調表示（予兆は腕そのものが担う）。
        if (boss_.grabHoldT > 0.0f && boss_.grabTarget >= 0 && boss_.grabTarget < MaxPlayers)
        {
            const V2 tp = players_[boss_.grabTarget].pos;
            const V2 dd = tp - boss_.pos;
            const float len = std::max(0.1f, Len(dd));
            const V2 mid = (boss_.pos + tp) * 0.5f;
            DrawMesh(cubeMesh_,
                XMMatrixScaling(len, 0.4f, 0.45f) *
                XMMatrixRotationY(-AngleOf(dd)) *
                XMMatrixTranslation(mid.x, 0.38f, mid.z),
                WithAlpha(Grape, 0.82f));
        }
    }

    for (const auto& s : slashes_) DrawSector3D(s);

    for (const auto& p : players_)
    {
        if (!p.active) continue;
        const Color base = Loadouts[static_cast<int>(p.character)].color;
        Color body = p.downed ? WithAlpha(Red, 0.65f) : (p.inv > 0.0f ? Cream : base);
        if (screen_ == Screen::HiddenBoss && !p.downed) body = WithAlpha(body, 0.42f);
        DrawSphere(p.pos, PlayerBodyY, p.radius, body);
        const V2 nose = p.pos + FromAngle(p.face) * (p.radius * 0.75f);
        DrawSphere(nose, PlayerBodyY + 0.03f, p.radius * 0.16f, Cream);
        const V2 line = p.pos + FromAngle(p.face) * (p.radius * 1.55f);
        DrawCylinder(line, p.radius * 0.08f, 0.10f, WithAlpha(Cream, 0.65f));
        if (p.bombCharge > 0.0f && !p.downed)
        {
            // チョコ爆弾チャージのプレビュー（溜め段階で前方の弾が大きくなる）
            const int cs = p.bombCharge >= 1.0f ? 3 : (p.bombCharge >= 0.6f ? 2 : (p.bombCharge >= 0.3f ? 1 : 0));
            const Color cc = cs >= 3 ? Gold : (cs >= 2 ? Berry : Choco);
            const float cr = 0.26f + 0.28f * static_cast<float>(cs);
            const V2 at = p.pos + FromAngle(p.face) * (p.radius + 0.3f + cr);
            DrawSphere(at, PlayerBodyY, cr, cc);
            ringAt(at, cr * 1.25f, PlayerBodyY + 0.01f, WithAlpha(Cream, 0.55f));
        }
        if (p.charging && !p.downed)
        {
            // チャージ進行に応じて外側のリングが機体へ収束し、溜まり具合を可視化する。
            const float chargeProgress = ClampFloat(p.chargeT / 1.15f, 0.0f, 1.0f);
            const Color chargeColor = p.chargeFull ? Gold : (p.chargeReady ? Sky : base);
            const float outer = p.radius * (2.6f - 1.3f * chargeProgress);
            DrawMesh(ringMesh_,
                XMMatrixScaling(outer, 1.0f, outer) *
                XMMatrixTranslation(p.pos.x, 0.07f, p.pos.z),
                WithAlpha(chargeColor, 0.28f + 0.42f * chargeProgress));
            if (p.chargeReady)
            {
                // 発動可能になったら機体周りで脈動させ、撃てる合図を明確に出す。
                const float pulse = 0.45f + 0.35f * std::sin(p.chargeT * 18.0f);
                const float readyRadius = p.chargeFull ? p.radius * 1.85f : p.radius * 1.28f;
                DrawMesh(ringMesh_,
                    XMMatrixScaling(readyRadius, 1.0f, readyRadius) *
                    XMMatrixTranslation(p.pos.x, 0.075f, p.pos.z),
                    WithAlpha(p.chargeFull ? Gold : Cream, pulse));
            }
        }
        if ((p.focus || screen_ == Screen::HiddenBoss) && !p.downed)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.hitboxRadius, 1.0f, p.hitboxRadius) *
                XMMatrixTranslation(p.pos.x, 0.06f, p.pos.z),
                Red);
        }
        if (p.shieldT > 0.0f)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.radius * 2.1f, 1.0f, p.radius * 2.1f) *
                XMMatrixTranslation(p.pos.x, 0.08f, p.pos.z),
                WithAlpha(Sky, 0.45f));
        }
    }

    for (const auto& particle : particles_)
    {
        DrawSphere(particle.pos, std::max(0.03f, particle.y), 0.055f + particle.y * 0.04f, WithAlpha(particle.color, ClampFloat(particle.ttl * 2.0f, 0.0f, 1.0f)));
    }

#if defined(_DEBUG)
    if (debug_.overlays)
    {
        for (const auto& p : players_)
        {
            if (!p.active) continue;
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.hitboxRadius, 1.0f, p.hitboxRadius) *
                XMMatrixTranslation(p.pos.x, 0.085f, p.pos.z),
                WithAlpha(Red, 0.85f));
        }
    }
#endif
}

// 2Dスプライトを1枚描く共通処理です。
// 画像が読み込めない時は、呼び出し側が図形フォールバックを出す前提です。
void SweetsApp::DrawSprite2D(const std::wstring& spriteId, V2 pos, V2 size, float rotation, Color tint, float depth)
{
    const SpriteAsset* sprite = spriteLibrary_.Find(spriteId);
    const TextureAsset* texture = sprite ? textureLibrary_.Find(sprite->textureId) : nullptr;
    ID3D11ShaderResourceView* srv = texture ? texture->shaderResource.Get() : nullptr;
    if (srv)
    {
        spriteCanvas_.DrawQuad(srv, pos, size, rotation, tint, depth);
        return;
    }

    const float radius = std::max(size.x, size.z) * 0.5f;
    spriteCanvas_.DrawCircle(pos, radius, tint, depth, 28);
    spriteCanvas_.DrawRing(pos, radius, std::max(0.025f, radius * 0.12f), WithAlpha(Cream, tint.a * 0.35f), depth - 0.01f, 28);
}

void SweetsApp::DrawMesh(const Mesh& mesh, const XMMATRIX& world, Color tint)
{
    ObjectCB object{};
    object.world = world;
    object.tint = XMFLOAT4(tint.r, tint.g, tint.b, tint.a);
    context_->UpdateSubresource(objectCB_.Get(), 0, nullptr, &object, 0, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ID3D11Buffer* vb = mesh.vb.Get();
    context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context_->IASetIndexBuffer(mesh.ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    context_->DrawIndexed(mesh.indexCount, 0, 0);
}

void SweetsApp::DrawSphere(V2 p, float y, float r, Color c)
{
    DrawMesh(sphereMesh_, XMMatrixScaling(r, r, r) * XMMatrixTranslation(p.x, y, p.z), c);
}

void SweetsApp::DrawCylinder(V2 p, float radius, float height, Color c)
{
    DrawMesh(cylinderMesh_, XMMatrixScaling(radius, height, radius) * XMMatrixTranslation(p.x, 0.0f, p.z), c);
}

// 2Dアイテム表示です。
// 色だけでなく形を変え、敵や弾と見間違えにくくしています。
void SweetsApp::DrawPickupShape(const Pickup& p)
{
    const float bob = 0.34f + 0.10f * std::sin(gameTime_ * 5.0f + p.pos.x * 1.7f);
    const float spin = gameTime_ * 1.65f + p.pos.x * 0.41f + p.pos.z * 0.19f;
    const float r = p.radius;
    const Color body = p.color;
    const Color glow = WithAlpha(body, 0.50f);
    const Color pale = WithAlpha(Cream, 0.82f);

    DrawMesh(ringMesh_,
        XMMatrixScaling(r * 1.95f, 1.0f, r * 1.95f) *
        XMMatrixTranslation(p.pos.x, 0.075f, p.pos.z),
        glow);
    DrawMesh(ringMesh_,
        XMMatrixScaling(r * 1.22f, 1.0f, r * 1.22f) *
        XMMatrixRotationY(-spin) *
        XMMatrixTranslation(p.pos.x, 0.105f, p.pos.z),
        WithAlpha(Cream, 0.32f));

    auto cube = [&](float sx, float sy, float sz, V2 offset, float yOffset, float yaw, Color c)
    {
        DrawMesh(cubeMesh_,
            XMMatrixScaling(sx, sy, sz) *
            XMMatrixRotationY(yaw) *
            XMMatrixTranslation(p.pos.x + offset.x, bob + yOffset, p.pos.z + offset.z),
            c);
    };

    auto sphere = [&](V2 offset, float yOffset, float radius, Color c)
    {
        DrawSphere(p.pos + offset, bob + yOffset, radius, c);
    };

    auto wedge = [&](float scale, float yaw, Color c, float alpha = 0.68f)
    {
        DrawMesh(wedgeMesh_,
            XMMatrixScaling(scale, 1.0f, scale) *
            XMMatrixRotationY(yaw) *
            XMMatrixTranslation(p.pos.x, bob + 0.01f, p.pos.z),
            WithAlpha(c, alpha));
    };

    switch (p.pickupType)
    {
    case PickupType::Attack:
        for (int i = 0; i < 5; ++i)
        {
            wedge(r * 1.16f, spin + TwoPi * i / 5.0f, Gold, 0.72f);
        }
        sphere({ 0.0f, 0.0f }, 0.02f, r * 0.46f, Berry);
        break;
    case PickupType::Slow:
        for (int i = 0; i < 3; ++i)
        {
            cube(r * 1.85f, r * 0.18f, r * 0.16f, { 0.0f, 0.0f }, 0.0f, spin + TwoPi * i / 3.0f, Sky);
        }
        sphere({ 0.0f, 0.0f }, 0.03f, r * 0.26f, pale);
        break;
    case PickupType::Invincible:
        cube(r * 0.82f, r * 0.22f, r * 1.16f, { 0.0f, 0.0f }, 0.0f, spin + Pi * 0.25f, Gold);
        cube(r * 0.56f, r * 0.20f, r * 0.76f, FromAngle(spin + Pi) * (r * 0.16f), -0.01f, spin + Pi * 0.25f, pale);
        DrawMesh(ringMesh_,
            XMMatrixScaling(r * 1.50f, 1.0f, r * 1.50f) *
            XMMatrixRotationY(spin) *
            XMMatrixTranslation(p.pos.x, bob + 0.03f, p.pos.z),
            WithAlpha(Gold, 0.62f));
        break;
    case PickupType::Magnet:
    {
        const V2 side = FromAngle(spin + Pi * 0.5f);
        const V2 forward = FromAngle(spin);
        cube(r * 0.22f, r * 0.22f, r * 1.02f, side * (r * 0.50f), 0.0f, spin, Red);
        cube(r * 0.22f, r * 0.22f, r * 1.02f, side * (-r * 0.50f), 0.0f, spin, Sky);
        cube(r * 0.98f, r * 0.20f, r * 0.22f, FromAngle(spin + Pi) * (r * 0.46f), -0.02f, spin, pale);
        sphere(side * (r * 0.50f) + forward * (r * 0.58f), 0.04f, r * 0.18f, Red);
        sphere(side * (-r * 0.50f) + forward * (r * 0.58f), 0.04f, r * 0.18f, Sky);
        break;
    }
    case PickupType::BombDamage:
        cube(r * 0.92f, r * 0.22f, r * 0.92f, { 0.0f, 0.0f }, 0.0f, spin + Pi * 0.25f, Red);
        for (int i = 0; i < 6; ++i)
        {
            wedge(r * 0.64f, spin + TwoPi * i / 6.0f, Gold, 0.58f);
        }
        sphere({ 0.0f, 0.0f }, 0.05f, r * 0.24f, Cream);
        break;
    case PickupType::Heal:
        cube(r * 1.25f, r * 0.28f, r * 0.22f, { 0.0f, 0.0f }, 0.0f, spin, Mint);
        cube(r * 1.25f, r * 0.28f, r * 0.22f, { 0.0f, 0.0f }, 0.0f, spin + Pi * 0.5f, Mint);
        sphere({ 0.0f, 0.0f }, 0.04f, r * 0.20f, pale);
        break;
    case PickupType::UltFull:
        cube(r * 0.92f, r * 0.24f, r * 0.92f, { 0.0f, 0.0f }, -0.02f, spin + Pi * 0.25f, Grape);
        for (int i = -1; i <= 1; ++i)
        {
            const float a = spin + Pi * 0.5f + i * 0.42f;
            sphere(FromAngle(a) * (r * 0.50f), 0.22f + 0.04f * (1 - std::abs(i)), r * 0.20f, Gold);
        }
        break;
    case PickupType::Spread:
        for (int i = -1; i <= 1; ++i)
        {
            wedge(r * 0.88f, spin + i * 0.45f, Sky, 0.56f);
            sphere(FromAngle(spin + i * 0.45f) * (r * 0.88f), 0.06f, r * 0.16f, Gold);
        }
        sphere({ 0.0f, 0.0f }, 0.02f, r * 0.18f, pale);
        break;
    case PickupType::Speed:
        wedge(r * 1.15f, spin, Mint, 0.76f);
        cube(r * 0.92f, r * 0.22f, r * 0.18f, FromAngle(spin + Pi) * (r * 0.42f), -0.02f, spin, Mint);
        cube(r * 0.52f, r * 0.14f, r * 0.14f, FromAngle(spin + Pi) * (r * 0.92f), -0.03f, spin, pale);
        break;
    case PickupType::ScoreDouble:
        DrawMesh(ringMesh_,
            XMMatrixScaling(r * 0.96f, 1.0f, r * 0.96f) *
            XMMatrixRotationY(spin) *
            XMMatrixTranslation(p.pos.x - r * 0.32f, bob + 0.02f, p.pos.z),
            Gold);
        DrawMesh(ringMesh_,
            XMMatrixScaling(r * 0.96f, 1.0f, r * 0.96f) *
            XMMatrixRotationY(spin) *
            XMMatrixTranslation(p.pos.x + r * 0.32f, bob + 0.08f, p.pos.z),
            pale);
        sphere({ -r * 0.32f, 0.0f }, 0.02f, r * 0.18f, Gold);
        sphere({ r * 0.32f, 0.0f }, 0.08f, r * 0.18f, pale);
        break;
    default:
        sphere({ 0.0f, 0.0f }, 0.0f, r * 0.50f, body);
        break;
    }
}

// 3Dモード用のアイテム表示です。
// 2Dと同じ意味の形を、既存メッシュの組み合わせで表現します。
void SweetsApp::DrawPickupShape3D(const Pickup& p)
{
    DrawPickupShape(p);
}

// 2Dの扇形斬撃表示です。
// チョコ剣攻撃はEffekseerを主表示にするため、Hidden指定ならここでは描きません。
void SweetsApp::DrawSector(const Slash& s)
{
    const float alpha = ClampFloat(s.ttl / s.life, 0.0f, 1.0f) * 0.60f;
    if (s.visualMode == SlashVisualMode::Hidden)
    {
        return;
    }

    if (s.sweep)
    {
        // 薙ぎ払い：刃が弧の端から端へ振り抜ける
        const float prog = 1.0f - ClampFloat(s.ttl / s.life, 0.0f, 1.0f);
        const float bladeAng = s.angle - s.arc * 0.5f + s.arc * prog;
        const V2 bc = s.pos + FromAngle(bladeAng) * (s.range * 0.5f);
        DrawSprite2D(L"2d_slash", bc, { s.range * 1.25f, s.range * 0.72f }, bladeAng, WithAlpha(s.color, alpha), 0.08f);
        spriteCanvas_.DrawArc(s.pos, s.range * 0.60f, s.range * 0.34f, s.angle, s.arc, WithAlpha(Cream, alpha * 0.5f), 0.07f, 32);
        return;
    }

    const V2 center = s.pos + FromAngle(s.angle) * (s.range * 0.45f);
    if (s.visualMode == SlashVisualMode::Line)
    {
        DrawSprite2D(L"2d_shot_player", center, { s.range * 0.20f, s.range * 1.75f }, s.angle - Pi * 0.5f, WithAlpha(s.color, alpha), 0.08f);
        return;
    }

    DrawSprite2D(L"2d_slash", center, { s.range * 1.65f, s.range * 0.92f }, s.angle, WithAlpha(s.color, alpha), 0.08f);
    spriteCanvas_.DrawArc(s.pos, s.range * 0.60f, s.range * 0.34f, s.angle, s.arc, WithAlpha(Cream, alpha * 0.58f), 0.07f, 32);
}

void SweetsApp::DrawSector3D(const Slash& s)
{
    if (s.visualMode == SlashVisualMode::Hidden) return;
    const float alpha = ClampFloat(s.ttl / s.life, 0.0f, 1.0f) * 0.55f;
    if (s.sweep)
    {
        // 薙ぎ払い：刃が弧の端から端へ振り抜ける
        const float prog = 1.0f - ClampFloat(s.ttl / s.life, 0.0f, 1.0f);
        const float bladeAng = s.angle - s.arc * 0.5f + s.arc * prog;
        const V2 c = s.pos + FromAngle(bladeAng) * (s.range * 0.5f);
        DrawMesh(wedgeMesh_,
            XMMatrixScaling(s.range * 0.55f, 1.0f, s.range * 0.55f) *
            XMMatrixRotationY(-bladeAng) *
            XMMatrixTranslation(c.x, s.height, c.z),
            WithAlpha(s.color, alpha + 0.10f));
        DrawMesh(ringMesh_,
            XMMatrixScaling(s.range * 0.62f, 1.0f, s.range * 0.62f) *
            XMMatrixTranslation(s.pos.x, s.height + 0.015f, s.pos.z),
            WithAlpha(Cream, alpha * 0.25f));
        return;
    }
    const V2 center = s.pos + FromAngle(s.angle) * (s.range * 0.42f);
    DrawMesh(wedgeMesh_,
        XMMatrixScaling(s.range * 0.82f, 1.0f, s.range * 0.82f) *
        XMMatrixRotationY(-s.angle) *
        XMMatrixTranslation(center.x, s.height, center.z),
        WithAlpha(s.color, alpha));
    DrawMesh(ringMesh_,
        XMMatrixScaling(s.range * 0.60f, 1.0f, s.range * 0.60f) *
        XMMatrixTranslation(s.pos.x, s.height + 0.015f, s.pos.z),
        WithAlpha(Cream, alpha * 0.28f));
}

// 必殺技の範囲プレビューです。
// 「どこまで当たるか分からない」問題を減らすため、発動前に範囲を見せます。
void SweetsApp::DrawUltimatePreview(const Player& p, int ownerIndex)
{
    if (!p.active || p.downed || p.ult < 100.0f)
    {
        return;
    }

#if defined(_DEBUG)
    const float ultimateFx = ClampFloat(debug_.ultimateFx, 0.0f, 2.0f);
#else
    const float ultimateFx = 1.0f;
#endif
    auto drawRing = [&](V2 center, float radius, Color color, float alpha)
    {
        spriteCanvas_.DrawRing(center, radius, 0.10f + radius * 0.015f, WithAlpha(color, ClampFloat(alpha * ultimateFx, 0.0f, 1.0f)), 0.09f, 96);
    };

    const Color accent = Loadouts[static_cast<int>(p.character)].color;
    bool comboReady = false;
    for (const auto& other : players_)
    {
        if (!other.active || other.index == ownerIndex || other.downed || other.ult < 100.0f) continue;
        comboReady = true;
        break;
    }
    if (comboReady)
    {
        drawRing(p.pos, 2.4f, Gold, 0.35f);
    }

    switch (p.weapon)
    {
    case Weapon::Strawberry:
    {
        const V2 cursorTarget = ScreenToWorld(mouseX_, mouseY_);
        const V2 target = ownerIndex == 0 ? ResolvePlayerAimPoint(p, ownerIndex, cursorTarget, 14.0f) : FindNearestEnemyOrBoss(p.pos);
        drawRing(target, 3.4f, Berry, 0.62f);
        drawRing(target, 1.7f, Cream, 0.32f);
        spriteCanvas_.DrawCircle(target, 0.11f, WithAlpha(Berry, 0.80f), 0.08f, 18);
        break;
    }
    case Weapon::Chocolate:
        drawRing({ 0.0f, 0.0f }, ArenaRadius * 0.96f, Choco, 0.52f);
        drawRing(p.pos, 3.2f, accent, 0.35f);
        break;
    case Weapon::Cheese:
        drawRing(p.pos, 2.1f, Gold, 0.58f);
        drawRing(p.pos, 3.0f, Cream, 0.28f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = TwoPi * i / 8.0f;
            spriteCanvas_.DrawCircle(p.pos + FromAngle(a) * 2.1f, 0.08f, WithAlpha(Gold, 0.70f), 0.08f, 12);
        }
        break;
    case Weapon::Roll:
        drawRing({ 0.0f, 0.0f }, ArenaRadius * 0.96f, Cream, 0.48f);
        drawRing(p.pos, 4.4f, accent, 0.34f);
        break;
    default:
        drawRing(p.pos, 3.2f, accent, 0.45f);
        break;
    }
}

// マウス座標をゲーム内座標へ変換します。
// 2Dでは正射影、3Dではカメラレイと地面の交点として扱います。
V2 SweetsApp::ScreenToWorld(float sx, float sy) const
{
    const float nx = (2.0f * sx / std::max(1u, width_)) - 1.0f;
    const float ny = 1.0f - (2.0f * sy / std::max(1u, height_));
    if (Use3DRules())
    {
        const XMMATRIX view = XMMatrixLookAtLH(
            XMVectorSet(camera_.center.x, 15.8f, camera_.center.z - 17.8f, 1.0f),
            XMVectorSet(camera_.center.x, 0.0f, camera_.center.z + 0.8f, 1.0f),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        const XMMATRIX proj = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(48.0f),
            static_cast<float>(std::max(1u, width_)) / std::max(1.0f, static_cast<float>(height_)),
            0.1f,
            80.0f);
        const XMMATRIX inv = XMMatrixInverse(nullptr, view * proj);
        XMVECTOR nearP = XMVector3TransformCoord(XMVectorSet(nx, ny, 0.0f, 1.0f), inv);
        XMVECTOR farP = XMVector3TransformCoord(XMVectorSet(nx, ny, 1.0f, 1.0f), inv);
        XMVECTOR dir = XMVector3Normalize(farP - nearP);
        const float nearY = XMVectorGetY(nearP);
        const float dirY = XMVectorGetY(dir);
        float t = 0.0f;
        if (std::fabs(dirY) > 0.0001f)
        {
            t = -nearY / dirY;
        }
        XMVECTOR hit = nearP + dir * std::max(0.0f, t);
        V2 out{ XMVectorGetX(hit), XMVectorGetZ(hit) };
        ClampInside(out, 0.0f);
        return out;
    }
    V2 out{ camera_.center.x + nx * GameplayViewHalfWidth(), camera_.center.z + ny * GameplayViewHalfHeight() };
    ClampInside(out, 0.0f);
    return out;
}

