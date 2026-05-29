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

    for (const auto& o : obstacles_)
    {
        if (o.damageField)
        {
            DrawCylinder(o.pos, o.radius, 0.08f, WithAlpha(Red, 0.42f));
            DrawMesh(ringMesh_,
                XMMatrixScaling(o.radius, 1.0f, o.radius) *
                XMMatrixTranslation(o.pos.x, 0.16f, o.pos.z),
                WithAlpha(Red, 0.78f));
        }
        else
        {
            DrawCylinder(o.pos, o.radius, o.cheeseWall ? 0.82f : 0.56f, WithAlpha(o.color, 0.86f));
            DrawMesh(ringMesh_,
                XMMatrixScaling(o.radius * 1.08f, 1.0f, o.radius * 1.08f) *
                XMMatrixTranslation(o.pos.x, 0.65f, o.pos.z),
                WithAlpha(Cream, 0.30f));
        }
    }

    for (const auto& p : pickups_) DrawPickupShape3D(p);

    for (const auto& s : shots_)
    {
        const float scale = s.charged ? 1.55f : 1.0f;
        DrawSphere(s.pos, s.height, s.radius * 1.75f * scale, s.color);
        if (s.enemy)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(s.radius * 2.25f, 1.0f, s.radius * 2.25f) *
                XMMatrixTranslation(s.pos.x, s.height, s.pos.z),
                WithAlpha(Cream, 0.26f));
        }
    }

    for (const auto& e : enemies_)
    {
        if (e.dead) continue;
        Color c = e.flash > 0.0f ? Cream : e.color;
        if (e.barrierT > 0.0f) c = Sky;
        DrawSphere(e.pos, e.height, e.radius, c);
        DrawCylinder(e.pos, e.radius * 0.62f, e.height, WithAlpha(c, 0.42f));
    }

    if (boss_.active)
    {
        Color c = boss_.flash > 0.0f ? Cream : (boss_.bossType == BossType::HiddenBoss ? Grape : Rose);
        DrawSphere(boss_.pos, boss_.height, boss_.radius, c);
        DrawMesh(ringMesh_,
            XMMatrixScaling(boss_.radius * 1.42f, 1.0f, boss_.radius * 1.42f) *
            XMMatrixTranslation(boss_.pos.x, 0.10f, boss_.pos.z),
            WithAlpha(Red, 0.45f));
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossForm_ >= 2)
        {
            const float pulse = 0.14f * std::sin(gameTime_ * (hiddenBossForm_ >= 3 ? 8.0f : 5.2f));
            const float alpha = hiddenBossForm_ >= 3 ? 0.88f : 0.64f;
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (2.05f + pulse), 1.0f, boss_.radius * (2.05f + pulse)) *
                XMMatrixTranslation(boss_.pos.x, 0.16f, boss_.pos.z),
                WithAlpha(Gold, alpha));
            DrawMesh(ringMesh_,
                XMMatrixScaling(boss_.radius * (2.70f - pulse), 1.0f, boss_.radius * (2.70f - pulse)) *
                XMMatrixTranslation(boss_.pos.x, 0.18f, boss_.pos.z),
                WithAlpha(Cream, alpha * 0.42f));
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
        if ((p.focus || screen_ == Screen::HiddenBoss) && !p.downed)
        {
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.hitboxRadius, 1.0f, p.hitboxRadius) *
                XMMatrixTranslation(p.pos.x, 0.06f, p.pos.z),
                Red);
            DrawMesh(ringMesh_,
                XMMatrixScaling(p.grazeRadius, 1.0f, p.grazeRadius) *
                XMMatrixTranslation(p.pos.x, 0.055f, p.pos.z),
                WithAlpha(Sky, p.grazeFlash > 0.0f ? 0.70f : 0.30f));
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

void SweetsApp::DrawPickupShape3D(const Pickup& p)
{
    DrawPickupShape(p);
}

void SweetsApp::DrawSector(const Slash& s)
{
    const float alpha = ClampFloat(s.ttl / s.life, 0.0f, 1.0f) * 0.60f;
    if (s.visualMode == SlashVisualMode::Hidden)
    {
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

void SweetsApp::DrawUltimatePreview(const Player& p, int ownerIndex)
{
    if (!p.active || p.downed || p.ult < 100.0f)
    {
        return;
    }

    auto drawRing = [&](V2 center, float radius, Color color, float alpha)
    {
        spriteCanvas_.DrawRing(center, radius, 0.10f + radius * 0.015f, WithAlpha(color, alpha), 0.09f, 96);
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
        const V2 target = ownerIndex == 0 ? ScreenToWorld(mouseX_, mouseY_) : FindNearestEnemyOrBoss(p.pos);
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

V2 SweetsApp::ScreenToWorld(float sx, float sy) const
{
    const float nx = (2.0f * sx / std::max(1u, width_)) - 1.0f;
    const float ny = 1.0f - (2.0f * sy / std::max(1u, height_));
    if (Use3DRules())
    {
        const XMMATRIX view = XMMatrixLookAtLH(
            XMVectorSet(0.0f, 15.8f, -17.8f, 1.0f),
            XMVectorSet(0.0f, 0.0f, 0.8f, 1.0f),
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
    V2 out{ nx * GameplayHalfWidth(width_, height_), ny * GameplayHalfHeight() };
    ClampInside(out, 0.0f);
    return out;
}

