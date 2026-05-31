#include "SweetsApp.h"

#include <filesystem>

namespace
{
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

void SweetsApp::PresentFrame()
{
    DrawScene();
    effekseer_.Draw(view_ * proj_);
    DrawAdditiveScene();
    CompositeScene();
    DrawHud();
    swapChain_->Present(1, 0);
}

void SweetsApp::DrawScene()
{
    const float clear[4] = { 0.12f, 0.045f, 0.085f, 1.0f };
    ID3D11RenderTargetView* sceneTarget = sceneColorRtv_ ? sceneColorRtv_.Get() : rtv_.Get();
    context_->ClearRenderTargetView(sceneTarget, clear);
    context_->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    const float halfH = GameplayHalfHeight();
    const float halfW = GameplayHalfWidth(width_, height_);
    if (Use3DRules())
    {
        SyncAll3DState();
        const XMVECTOR eye = XMVectorSet(0.0f, 15.8f, -17.8f, 1.0f);
        const XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.8f, 1.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        view_ = XMMatrixLookAtLH(eye, at, up);
        proj_ = XMMatrixPerspectiveFovLH(XMConvertToRadians(48.0f), static_cast<float>(std::max(1u, width_)) / std::max(1.0f, static_cast<float>(height_)), 0.1f, 80.0f);
        cameraPos_ = { 0.0f, 15.8f, -17.8f };
    }
    else
    {
        view_ = XMMatrixIdentity();
        proj_ = XMMatrixOrthographicOffCenterLH(-halfW, halfW, -halfH, halfH, 0.0f, 10.0f);
        cameraPos_ = { 0.0f, 15.5f, -18.5f };
    }
#if defined(_DEBUG)
    if (debug_.taa)
    {
        const int frame = (debug_.taaFrame % 8) + 1;
        const float jitterX = (Halton(frame, 2) - 0.5f) * 2.0f / std::max(1.0f, static_cast<float>(width_));
        const float jitterY = (Halton(frame, 3) - 0.5f) * 2.0f / std::max(1.0f, static_cast<float>(height_));
        proj_ = proj_ * XMMatrixTranslation(jitterX, jitterY, 0.0f);
    }
#endif

    FrameCB frame{};
    frame.viewProj = view_ * proj_;
    frame.lightDir = XMFLOAT4(-0.35f, -1.0f, 0.55f, 0.0f);
    frame.cameraPos = XMFLOAT4(cameraPos_.x, cameraPos_.y, cameraPos_.z, 1.0f);
    context_->UpdateSubresource(frameCB_.Get(), 0, nullptr, &frame, 0, 0);

    ID3D11RenderTargetView* rtv = sceneTarget;
    context_->OMSetRenderTargets(1, &rtv, dsv_.Get());
    if (Use3DRules())
    {
        DrawGameplay3D();
        return;
    }
    spriteCanvas_.Begin(view_ * proj_, false);

    spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, ArenaRadius + 1.3f, WithAlpha(Rose, 0.24f), 0.95f, 96);
    spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, ArenaRadius, WithAlpha({ 0.18f, 0.07f, 0.12f, 1.0f }, 0.92f), 0.94f, 96);
    spriteCanvas_.DrawRing({ 0.0f, 0.0f }, ArenaRadius, 0.16f, WithAlpha(Gold, 0.76f), 0.20f, 128);
    spriteCanvas_.DrawRing({ 0.0f, 0.0f }, ArenaRadius * 0.68f, 0.035f, WithAlpha(Cream, 0.18f), 0.30f, 96);
    spriteCanvas_.DrawRing({ 0.0f, 0.0f }, ArenaRadius * 0.36f, 0.035f, WithAlpha(Cream, 0.13f), 0.30f, 72);

    for (const auto& o : obstacles_)
    {
        if (o.damageField)
        {
            spriteCanvas_.DrawCircle(o.pos, o.radius, WithAlpha(Red, 0.22f), 0.33f, 40);
            spriteCanvas_.DrawRing(o.pos, o.radius, 0.18f, WithAlpha(Red, 0.68f), 0.32f);
        }
        else if (o.warpId >= 0)
        {
            // ワープポータル
            spriteCanvas_.DrawCircle(o.pos, o.radius * 0.6f, WithAlpha(o.color, o.flash > 0.0f ? 0.6f : 0.30f), 0.33f, 32);
            spriteCanvas_.DrawRing(o.pos, o.radius * 1.1f, 0.10f, WithAlpha(o.color, 0.85f), 0.31f, 48);
            spriteCanvas_.DrawRing(o.pos, o.radius * (0.7f + 0.15f * std::sin(o.spin * 4.0f)), 0.07f, WithAlpha(Cream, 0.6f), 0.30f, 40);
        }
        else
        {
            const Color c = o.flash > 0.0f ? Cream : o.color;
            const float size = o.radius * (o.cheeseWall ? 2.35f : 2.05f);
            DrawSprite2D(L"2d_obstacle_wall", o.pos, { size, size }, o.ttl * 0.35f + o.spin * 0.2f, WithAlpha(c, 0.92f), 0.38f);
            spriteCanvas_.DrawRing(o.pos, o.radius, 0.08f, WithAlpha(o.bumper ? Gold : Cream, o.bumper ? 0.55f : 0.28f), 0.36f);
        }
    }
    for (const auto& p : pickups_)
    {
        const float bob = 1.0f + 0.10f * std::sin(gameTime_ * 5.0f + p.pos.x);
        DrawSprite2D(PickupSpriteId(p.pickupType), p.pos, { p.radius * 2.8f * bob, p.radius * 2.8f * bob }, gameTime_ * 1.6f, p.color, 0.34f);
        spriteCanvas_.DrawRing(p.pos, p.radius * 1.55f, 0.045f, WithAlpha(p.color, 0.46f), 0.37f, 40);
    }
    for (const auto& s : shots_)
    {
        const wchar_t* id = s.enemy ? L"2d_shot_enemy" : L"2d_shot_player";
        const float size = s.radius * (s.enemy ? 3.35f : 3.05f) * (s.charged ? 1.55f : 1.0f);
        DrawSprite2D(id, s.pos, { size, size }, AngleOf(s.vel), s.color, s.enemy ? 0.25f : 0.24f);
        if (s.enemy)
        {
            spriteCanvas_.DrawRing(s.pos, s.radius * 1.75f, 0.035f, WithAlpha(Cream, 0.35f), 0.27f, 24);
        }
        else if (s.reflected)
        {
            spriteCanvas_.DrawRing(s.pos, s.radius * (2.0f + 0.35f * s.reflectedCount), 0.045f, WithAlpha(Gold, 0.52f), 0.26f, 28);
        }
    }
    for (const auto& e : enemies_)
    {
        Color c = e.flash > 0.0f ? Cream : e.color;
        if (e.barrierT > 0.0f) c = Sky;
        DrawSprite2D(EnemySpriteId(e.type), e.pos, { e.radius * 2.45f, e.radius * 2.45f }, e.face, c, 0.22f);
        if (e.type == EnemyType::Teleport || e.type == EnemyType::Mirror || e.type == EnemyType::Barrier)
        {
            const Player* target = FindNearestPlayer(e.pos);
            const float face = target ? AngleOf(target->pos - e.pos) : e.face;
            DrawSprite2D(L"2d_shot_enemy", e.pos + FromAngle(face) * (e.radius * 0.68f), { e.radius * 0.55f, e.radius * 0.55f }, face, Red, 0.21f);
        }
    }
    if (boss_.active)
    {
        Color c = boss_.flash > 0.0f ? Cream : (boss_.bossType == BossType::HiddenBoss ? Grape : (boss_.bossType == BossType::DonutKing ? Sky : (boss_.bossType == BossType::MirrorMacaron ? Gold : Rose)));
        DrawSprite2D(boss_.bossType == BossType::HiddenBoss ? L"2d_boss_hidden" : L"2d_boss_normal", boss_.pos, { boss_.radius * 2.85f, boss_.radius * 2.85f }, boss_.spin * 0.35f, c, 0.18f);
        spriteCanvas_.DrawRing(boss_.pos, boss_.radius * 1.42f, 0.10f, WithAlpha(Red, 0.45f), 0.19f, 72);
        if (boss_.telegraphT > 0.0f && boss_.telegraphLife > 0.0f)
        {
            const float t = 1.0f - ClampFloat(boss_.telegraphT / boss_.telegraphLife, 0.0f, 1.0f);
            const Color telegraph = boss_.telegraphAttack == 0 ? Grape : (boss_.telegraphAttack == 1 ? Red : (boss_.telegraphAttack == 2 ? Gold : Mint));
            spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (1.75f + t * 1.15f), 0.12f, WithAlpha(telegraph, 0.72f), 0.16f, 96);
            spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (2.45f + t * 0.65f), 0.05f, WithAlpha(Cream, 0.48f), 0.15f, 96);
            if (boss_.telegraphAdd || boss_.telegraphMirror || boss_.telegraphField)
            {
                spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (3.10f + t * 0.55f), 0.045f, WithAlpha(Sky, 0.42f), 0.15f, 96);
            }
        }
    }

    for (const auto& s : slashes_)
    {
        DrawSector(s);
    }

    for (int i = 0; i < MaxPlayers; ++i)
    {
        DrawUltimatePreview(players_[i], i);
    }

    for (const auto& p : players_)
    {
        if (!p.active) continue;
        const Color playerColor = Loadouts[static_cast<int>(p.character)].color;
        Color bodyColor = p.downed ? WithAlpha(Red, 0.65f) : (p.inv > 0.0f ? Cream : playerColor);
        Color faceColor = Cream;
        if (screen_ == Screen::HiddenBoss && !p.downed)
        {
            bodyColor = WithAlpha(bodyColor, 0.42f);
            faceColor = WithAlpha(faceColor, 0.45f);
        }
        DrawSprite2D(CharacterSpriteId(p.character), p.pos, { p.radius * 2.45f, p.radius * 2.45f }, p.face - Pi * 0.5f, bodyColor, 0.14f);
        DrawSprite2D(L"2d_shot_player", p.pos + FromAngle(p.face) * (p.radius * 0.88f), { p.radius * 0.55f, p.radius * 0.55f }, p.face, faceColor, 0.13f);
        if (p.charging && !p.downed)
        {
            // チャージ進行に応じて外側のリングが機体へ収束し、溜まり具合を可視化する。
            const float chargeProgress = ClampFloat(p.chargeT / 0.55f, 0.0f, 1.0f);
            const Color chargeColor = p.chargeReady ? Gold : playerColor;
            const float outer = p.radius * (2.6f - 1.3f * chargeProgress);
            spriteCanvas_.DrawRing(p.pos, outer, 0.05f + 0.07f * chargeProgress,
                WithAlpha(chargeColor, 0.30f + 0.45f * chargeProgress), 0.125f, 48);
            if (p.chargeReady)
            {
                // 発動可能になったら機体周りで脈動させ、撃てる合図を明確に出す。
                const float pulse = 0.45f + 0.35f * std::sin(gameTime_ * 18.0f);
                spriteCanvas_.DrawRing(p.pos, p.radius * 1.28f, 0.09f, WithAlpha(Cream, pulse), 0.122f, 48);
            }
        }
        if ((p.focus || screen_ == Screen::HiddenBoss) && !p.downed)
        {
            spriteCanvas_.DrawCircle(p.pos, p.hitboxRadius, Red, 0.11f, 20);
            spriteCanvas_.DrawRing(p.pos, p.grazeRadius, 0.045f, WithAlpha(Sky, p.grazeFlash > 0.0f ? 0.70f : 0.30f), 0.12f, 48);
        }
        if (p.shieldT > 0.0f)
        {
            spriteCanvas_.DrawRing(p.pos, p.radius * 2.1f, 0.10f, WithAlpha(Sky, 0.45f), 0.12f, 64);
        }
        if (p.bombT > 0.0f)
        {
            const float t = p.bombT / 1.8f;
            spriteCanvas_.DrawRing(p.pos, 1.5f + (1.0f - t) * 5.8f, 0.16f, WithAlpha(Sky, 0.55f * t), 0.10f, 96);
        }
    }

    for (const auto& p : particles_)
    {
        spriteCanvas_.DrawCircle(p.pos, 0.055f + p.y * 0.04f, WithAlpha(p.color, ClampFloat(p.ttl * 2.0f, 0.0f, 1.0f)), 0.09f, 12);
    }

#if defined(_DEBUG)
    if (debug_.overlays)
    {
        for (const auto& p : players_)
        {
            if (!p.active) continue;
            spriteCanvas_.DrawRing(p.pos, p.hitboxRadius, 0.03f, WithAlpha(Red, 0.85f), 0.06f, 28);
            spriteCanvas_.DrawRing(p.pos, p.grazeRadius, 0.04f, WithAlpha(Sky, 0.55f), 0.07f, 48);
        }
        for (const auto& e : enemies_)
        {
            if (e.dead) continue;
            spriteCanvas_.DrawRing(e.pos, e.radius, 0.04f, WithAlpha(Cream, 0.45f), 0.08f, 40);
        }
    }
#endif
    spriteCanvas_.End();
}

void SweetsApp::DrawAdditiveScene()
{
    if (!additiveRtv_) return;
    const float clear[4] = { 0, 0, 0, 1 };
    context_->ClearRenderTargetView(additiveRtv_.Get(), clear);

    ID3D11RenderTargetView* rtv = additiveRtv_.Get();
    context_->OMSetRenderTargets(1, &rtv, dsv_.Get());
    spriteCanvas_.Begin(view_ * proj_, true);

    for (const auto& s : shots_)
    {
        const float glow = s.enemy ? 2.1f : (s.reflected ? 2.8f : 1.7f);
        spriteCanvas_.DrawCircle(s.pos, s.radius * glow, WithAlpha(s.color, s.enemy ? 0.42f : 0.55f), 0.06f, 24);
    }
    for (const auto& s : slashes_)
    {
        DrawSector(s);
    }
    for (const auto& pulse : effectPulses_)
    {
        const float life = std::max(0.01f, pulse.life);
        const float progress = ClampFloat(1.0f - pulse.ttl / life, 0.0f, 1.0f);
        const float fade = ClampFloat(pulse.ttl / life, 0.0f, 1.0f);
        const float radius = pulse.startRadius + (pulse.endRadius - pulse.startRadius) * progress;
        spriteCanvas_.DrawRing(pulse.pos, radius, 0.15f + radius * 0.025f, WithAlpha(pulse.color, 0.88f * fade), 0.05f, 96);
        spriteCanvas_.DrawRing(pulse.pos, radius * 0.62f, 0.09f, WithAlpha(Cream, 0.34f * fade), 0.04f, 72);
    }
    for (const auto& visual : swordEffectVisuals_)
    {
        const float life = std::max(0.01f, visual.life);
        const float progress = ClampFloat(1.0f - visual.ttl / life, 0.0f, 1.0f);
        const float fade = ClampFloat(visual.ttl / life, 0.0f, 1.0f);
        const float baseScale = std::max(0.1f, visual.scale);
        const float length = visual.range * baseScale * (visual.charged ? 1.05f : 0.96f);
        const float width = std::max(0.18f, std::tan(visual.arc * 0.5f) * visual.range * 0.36f) * baseScale;
        const V2 forward = FromAngle(visual.angle);
        const V2 side{ -forward.z, forward.x };
        const V2 root = visual.pos + forward * (0.12f * baseScale);
        const V2 center = root + forward * (length * 0.48f);
        const float rotation = visual.angle - Pi * 0.5f;
        const float sweep = 1.0f + 0.16f * std::sin(progress * Pi);

        DrawSprite2D(L"effect_sword_thunder", center, { width * sweep, length * (1.0f + progress * 0.10f) }, rotation, WithAlpha(Cream, 0.96f * fade), 0.035f);
        DrawSprite2D(L"effect_sword_line", center - side * (0.16f * baseScale), { width * 0.72f, length * 0.92f }, rotation + 0.05f, WithAlpha(Sky, 0.58f * fade), 0.034f);
        DrawSprite2D(L"effect_sword_line", center + side * (0.18f * baseScale), { width * 0.52f, length * 0.76f }, rotation - 0.07f, WithAlpha(Choco, 0.46f * fade), 0.033f);
        if (visual.charged)
        {
            DrawSprite2D(L"effect_sword_ring", root + forward * (0.42f * baseScale), { 1.55f * baseScale, 1.55f * baseScale }, gameTime_ * 5.0f, WithAlpha(Sky, 0.72f * fade), 0.032f);
            DrawSprite2D(L"effect_sword_thunder", center + forward * (0.32f * baseScale), { width * 1.22f, length * 1.10f }, rotation, WithAlpha(Gold, 0.42f * fade), 0.031f);
        }

        const int sparkCount = visual.charged ? 9 : 5;
        for (int i = 0; i < sparkCount; ++i)
        {
            const float t = (static_cast<float>(i) + 0.35f) / static_cast<float>(sparkCount);
            const float wave = std::sin((t + progress) * TwoPi * 1.7f);
            const V2 sparkPos = root + forward * (length * t) + side * (wave * width * 0.42f);
            const float sparkSize = (visual.charged ? 0.34f : 0.24f) * baseScale * (1.0f - 0.35f * t);
            DrawSprite2D(L"effect_sword_particle", sparkPos, { sparkSize, sparkSize }, visual.angle + t * TwoPi, WithAlpha((i & 1) ? Gold : Cream, 0.82f * fade), 0.030f);
        }
    }
    for (const auto& p : players_)
    {
        if (!p.active) continue;
        if (p.bombT > 0.0f)
        {
            const float t = p.bombT / 1.8f;
            spriteCanvas_.DrawRing(p.pos, 1.5f + (1.0f - t) * 5.8f, 0.24f, WithAlpha(Sky, 0.9f * t), 0.04f, 120);
        }
        if (p.grazeFlash > 0.0f)
        {
            spriteCanvas_.DrawRing(p.pos, p.grazeRadius, 0.08f, WithAlpha(Sky, 0.75f), 0.04f, 64);
        }
    }
    for (const auto& p : particles_)
    {
        spriteCanvas_.DrawCircle(p.pos, 0.10f + p.y * 0.04f, WithAlpha(p.color, ClampFloat(p.ttl * 2.5f, 0.0f, 1.0f)), 0.04f, 16);
    }
    spriteCanvas_.End();
}

void SweetsApp::CompositeScene()
{
    if (!resolvedRtv_ || !postVs_ || !postPs_ || !sceneColorSrv_ || !additiveSrv_ || !historySrv_)
    {
        return;
    }

    const float clear[4] = { 0, 0, 0, 1 };
    context_->ClearRenderTargetView(resolvedRtv_.Get(), clear);
    ID3D11RenderTargetView* rtv = resolvedRtv_.Get();
    context_->OMSetRenderTargets(1, &rtv, nullptr);
    float blendFactor[4]{ 0,0,0,0 };
    context_->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(postVs_.Get(), nullptr, 0);
    context_->PSSetShader(postPs_.Get(), nullptr, 0);
    context_->RSSetState(rasterState_.Get());

    PostCB post{};
#if defined(_DEBUG)
    const bool useTaa = debug_.taa && debug_.taaFrame > 0 && !debug_.additiveView;
    post.params = XMFLOAT4(useTaa ? 0.18f : 0.0f, 1.0f, debug_.additiveView ? 1.0f : 0.0f, 0.0f);
#else
    post.params = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
#endif
    context_->UpdateSubresource(postCB_.Get(), 0, nullptr, &post, 0, 0);
    ID3D11Buffer* pcb = postCB_.Get();
    context_->PSSetConstantBuffers(0, 1, &pcb);
    ID3D11ShaderResourceView* srvs[3] = { sceneColorSrv_.Get(), additiveSrv_.Get(), historySrv_.Get() };
    context_->PSSetShaderResources(0, 3, srvs);
    ID3D11SamplerState* sampler = postSampler_.Get();
    context_->PSSetSamplers(0, 1, &sampler);
    context_->Draw(3, 0);
    ID3D11ShaderResourceView* nullSrvs[3] = { nullptr, nullptr, nullptr };
    context_->PSSetShaderResources(0, 3, nullSrvs);

    if (historyTex_ && resolvedTex_)
    {
        context_->CopyResource(historyTex_.Get(), resolvedTex_.Get());
    }
    if (backBufferTex_ && resolvedTex_)
    {
        context_->CopyResource(backBufferTex_.Get(), resolvedTex_.Get());
    }
#if defined(_DEBUG)
    ++debug_.taaFrame;
#endif
}

void SweetsApp::DrawScreenFlashOverlay()
{
    if (screenFlashT_ <= 0.0f || screenFlashLife_ <= 0.0f)
    {
        return;
    }

    const float t = ClampFloat(screenFlashT_ / std::max(0.01f, screenFlashLife_), 0.0f, 1.0f);
    const float alpha = 0.34f * t * t;
    const D2D1_RECT_F full = D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_));

    textBrush_->SetColor(D2D1::ColorF(screenFlashColor_.r, screenFlashColor_.g, screenFlashColor_.b, alpha));
    d2dContext_->FillRectangle(full, textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.28f));
    d2dContext_->FillRectangle(full, textBrush_.Get());
}

