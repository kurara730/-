#include "SweetsApp.h"

#include <filesystem>

// FrameView.cpp は1フレーム分の画面出力をまとめます。
// ゲーム本体を描いた後、加算FX、ブルーム、TAA、HUD、画面フラッシュを順に重ねます。

namespace
{
// TAA用のジッター値です。フレームごとに少し違う位置で描いて履歴と混ぜます。
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

// 毎フレーム最後に呼ばれる表示入口です。
// DrawSceneで画面を作り、swapChain_->Presentでウィンドウへ出します。
void SweetsApp::PresentFrame()
{
    DrawScene();
    effekseer_.Draw(view_ * proj_);
    DrawAdditiveScene();
    RenderBloom();
    CompositeScene();
    DrawHud();
    swapChain_->Present(1, 0);
}

// 画面状態に応じて、タイトル、メニュー、ゲーム、動画などを描き分けます。
// メニュー画面ではゲーム中オブジェクトを描かないようにし、背景の混乱を防ぎます。
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

    const float zoom = CameraZoom(); // ジャスト回避時に >1 となり寄る
    const float halfH = GameplayViewHalfHeight() / zoom;
    const float halfW = GameplayViewHalfWidth() / zoom;
    if (Use3DRules())
    {
        SyncAll3DState();
        // 注視点(at)からの視点オフセットを 1/zoom 倍してカメラを近づける。
        const float eyeY = 15.8f / zoom;
        const float eyeZ = (camera_.center.z + 0.8f) - 18.6f / zoom;
        const XMVECTOR eye = XMVectorSet(camera_.center.x, eyeY, eyeZ, 1.0f);
        const XMVECTOR at = XMVectorSet(camera_.center.x, 0.0f, camera_.center.z + 0.8f, 1.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        view_ = XMMatrixLookAtLH(eye, at, up);
        proj_ = XMMatrixPerspectiveFovLH(XMConvertToRadians(48.0f), static_cast<float>(std::max(1u, width_)) / std::max(1.0f, static_cast<float>(height_)), 0.1f, 80.0f);
        cameraPos_ = { camera_.center.x, eyeY, eyeZ };
    }
    else
    {
        // 画面シェイク：残り時間に応じて揺れ幅を減衰させ、カメラ中心をずらす。
        float shx = 0.0f, shz = 0.0f;
        if (shakeT_ > 0.0f)
        {
            const float k = ClampFloat(shakeT_ / std::max(0.01f, shakeLife_), 0.0f, 1.0f);
            const float amp = shakeMag_ * k * k;
            shx = std::sin(gameTime_ * 91.0f) * amp;
            shz = std::cos(gameTime_ * 77.0f) * amp;
        }
        view_ = XMMatrixTranslation(-(camera_.center.x + shx), -(camera_.center.z + shz), 0.0f);
        proj_ = XMMatrixOrthographicOffCenterLH(-halfW, halfW, -halfH, halfH, 0.0f, 10.0f);
        cameraPos_ = { camera_.center.x, 15.5f, camera_.center.z - 18.5f };
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
    const bool showGameplayScene =
        screen_ == Screen::Playing ||
        screen_ == Screen::Paused ||
        screen_ == Screen::Clear ||
        screen_ == Screen::HiddenBossIntro ||
        screen_ == Screen::HiddenBoss ||
        screen_ == Screen::CompleteClear;
    if (!showGameplayScene)
    {
        return;
    }
    if (Use3DRules())
    {
        DrawGameplay3D();
        return;
    }
    spriteCanvas_.Begin(view_ * proj_, false);

    auto drawBoundaryLine = [&](V2 a, V2 b, Color color, float thickness)
    {
        const V2 mid = (a + b) * 0.5f;
        const V2 d = b - a;
        spriteCanvas_.DrawQuad(nullptr, mid, { thickness, Len(d) }, AngleOf(d) - Pi * 0.5f, color, 0.20f);
    };

    const Color fieldFill{ 0.18f, 0.07f, 0.12f, 1.0f };
    if (fieldShape_ == FieldShape::Rectangle || fieldShape_ == FieldShape::Corridor)
    {
        const float halfX = fieldShape_ == FieldShape::Corridor ? 5.8f : 12.6f;
        const float halfZ = fieldShape_ == FieldShape::Corridor ? 13.4f : 8.8f;
        spriteCanvas_.DrawQuad(nullptr, { 0.0f, 0.0f }, { halfX * 2.0f, halfZ * 2.0f }, 0.0f, WithAlpha(fieldFill, 0.92f), 0.94f);
        drawBoundaryLine({ -halfX, -halfZ }, { halfX, -halfZ }, WithAlpha(Gold, 0.76f), 0.16f);
        drawBoundaryLine({ halfX, -halfZ }, { halfX, halfZ }, WithAlpha(Gold, 0.76f), 0.16f);
        drawBoundaryLine({ halfX, halfZ }, { -halfX, halfZ }, WithAlpha(Gold, 0.76f), 0.16f);
        drawBoundaryLine({ -halfX, halfZ }, { -halfX, -halfZ }, WithAlpha(Gold, 0.76f), 0.16f);
        if (fieldShape_ == FieldShape::Corridor)
        {
            drawBoundaryLine({ -halfX * 0.52f, -halfZ }, { -halfX * 0.52f, halfZ }, WithAlpha(Cream, 0.16f), 0.045f);
            drawBoundaryLine({ halfX * 0.52f, -halfZ }, { halfX * 0.52f, halfZ }, WithAlpha(Cream, 0.16f), 0.045f);
        }
    }
    else if (fieldShape_ == FieldShape::Octagon)
    {
        spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, ArenaRadius + 0.6f, WithAlpha(Rose, 0.18f), 0.95f, 96);
        spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, ArenaRadius * 0.96f, WithAlpha(fieldFill, 0.92f), 0.94f, 96);
        const float r = ArenaRadius * 0.88f;
        for (int i = 0; i < 8; ++i)
        {
            const V2 a = FromAngle(TwoPi * i / 8.0f + Pi * 0.125f) * r;
            const V2 b = FromAngle(TwoPi * (i + 1) / 8.0f + Pi * 0.125f) * r;
            drawBoundaryLine(a, b, WithAlpha(Gold, 0.78f), 0.15f);
        }
    }
    else if (fieldShape_ == FieldShape::Ring)
    {
        spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, ArenaRadius + 1.3f, WithAlpha(Rose, 0.20f), 0.95f, 96);
        spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, ArenaRadius, WithAlpha(fieldFill, 0.92f), 0.94f, 96);
        spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, 4.15f, WithAlpha({ 0.04f, 0.02f, 0.03f, 1.0f }, 0.98f), 0.93f, 72);
        spriteCanvas_.DrawRing({ 0.0f, 0.0f }, ArenaRadius, 0.16f, WithAlpha(Gold, 0.76f), 0.20f, 128);
        spriteCanvas_.DrawRing({ 0.0f, 0.0f }, 4.15f, 0.14f, WithAlpha(Sky, 0.66f), 0.21f, 96);
    }
    else
    {
        const float fieldRadius = fieldShape_ == FieldShape::ShrinkCircle ? shrinkRadius_ : ArenaRadius;
        spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, fieldRadius + 1.3f, WithAlpha(Rose, 0.24f), 0.95f, 96);
        spriteCanvas_.DrawCircle({ 0.0f, 0.0f }, fieldRadius, WithAlpha(fieldFill, 0.92f), 0.94f, 96);
        spriteCanvas_.DrawRing({ 0.0f, 0.0f }, fieldRadius, 0.16f, WithAlpha(Gold, 0.76f), 0.20f, 128);
        spriteCanvas_.DrawRing({ 0.0f, 0.0f }, fieldRadius * 0.68f, 0.035f, WithAlpha(Cream, 0.18f), 0.30f, 96);
        spriteCanvas_.DrawRing({ 0.0f, 0.0f }, fieldRadius * 0.36f, 0.035f, WithAlpha(Cream, 0.13f), 0.30f, 72);
    }

    for (const auto& telegraph : worldTelegraphs_)
    {
        const float t = 1.0f - ClampFloat(telegraph.ttl / std::max(0.01f, telegraph.life), 0.0f, 1.0f);
        const float alpha = 0.22f + 0.46f * t;
        if (telegraph.length > 0.01f)
        {
            const V2 mid = telegraph.pos + Normalize(telegraph.dir) * (telegraph.length * 0.5f);
            spriteCanvas_.DrawQuad(nullptr, mid, { telegraph.radius, telegraph.length }, AngleOf(telegraph.dir) - Pi * 0.5f, WithAlpha(telegraph.color, alpha), 0.29f);
        }
        else
        {
            spriteCanvas_.DrawRing(telegraph.pos, telegraph.radius * (0.72f + t * 0.34f), 0.08f + 0.04f * t, WithAlpha(telegraph.color, alpha), 0.29f, 72);
            spriteCanvas_.DrawCircle(telegraph.pos, telegraph.radius * 0.42f, WithAlpha(telegraph.color, alpha * 0.18f), 0.30f, 48);
        }
    }

    // 貫通ビームの照射本体（2D）。予兆線は上の WorldTelegraph で表示済み。
    if (boss_.active && boss_.beamActiveT > 0.0f)
    {
        const V2 bdir = FromAngle(boss_.beamAngle);
        const float pulse = 0.5f + 0.5f * std::sin(gameTime_ * 30.0f);
        // 反射中は壁までで切り詰める（壁より奥にはビームを描かない）。
        const bool reflected = boss_.beamReflectDist > 0.0f;
        const float len = reflected ? boss_.beamReflectDist : BossBeamLength;
        const V2 mid = boss_.pos + bdir * (len * 0.5f);
        spriteCanvas_.DrawQuad(nullptr, mid, { BossBeamHalfWidth * 2.0f, len }, boss_.beamAngle - Pi * 0.5f, WithAlpha(Red, 0.6f), 0.19f);
        spriteCanvas_.DrawQuad(nullptr, mid, { BossBeamHalfWidth, len }, boss_.beamAngle - Pi * 0.5f, WithAlpha(Cream, ClampFloat(0.5f + 0.3f * pulse, 0.0f, 1.0f)), 0.185f);
        if (reflected)
        {
            // 反射点の光と、壁→ボスへ跳ね返る反射光（水色）を描く。
            const V2 wallPos = boss_.pos + bdir * boss_.beamReflectDist;
            const V2 back = (boss_.pos + wallPos) * 0.5f;
            spriteCanvas_.DrawQuad(nullptr, back, { BossBeamHalfWidth * 0.9f, boss_.beamReflectDist }, boss_.beamAngle - Pi * 0.5f, WithAlpha(Sky, ClampFloat(0.45f + 0.35f * pulse, 0.0f, 0.9f)), 0.20f);
            spriteCanvas_.DrawRing(wallPos, BossBeamHalfWidth * (1.1f + 0.25f * pulse), 0.06f, WithAlpha(Sky, 0.85f), 0.205f, 28);
        }
    }
    // 極太回転ビーム（2D）。予兆は細い点滅線、照射は極太＋回転。
    if (boss_.active && (boss_.megaBeamWarnT > 0.0f || boss_.megaBeamActiveT > 0.0f))
    {
        const V2 mid = boss_.pos + FromAngle(boss_.megaBeamAngle) * (BossMegaBeamLength * 0.5f);
        const float rot = boss_.megaBeamAngle - Pi * 0.5f;
        if (boss_.megaBeamActiveT > 0.0f)
        {
            const float pulse = 0.5f + 0.5f * std::sin(gameTime_ * 26.0f);
            spriteCanvas_.DrawQuad(nullptr, mid, { BossMegaBeamHalfWidth * 2.0f, BossMegaBeamLength }, rot, WithAlpha(Red, 0.62f), 0.19f);
            spriteCanvas_.DrawQuad(nullptr, mid, { BossMegaBeamHalfWidth, BossMegaBeamLength }, rot, WithAlpha(Cream, ClampFloat(0.5f + 0.3f * pulse, 0.0f, 1.0f)), 0.185f);
        }
        else
        {
            const float blink = 0.3f + 0.35f * std::sin(gameTime_ * 18.0f);
            spriteCanvas_.DrawQuad(nullptr, mid, { BossMegaBeamHalfWidth * 2.0f, BossMegaBeamLength }, rot, WithAlpha(Red, ClampFloat(blink, 0.0f, 0.6f)), 0.19f);
        }
    }
    // ブレイク（崩し）中：ボスの足元に明滅リング＝動けない攻撃チャンスを盤面でも明示。
    if (boss_.active && boss_.breakT > 0.0f)
    {
        const float pulse = 0.5f + 0.5f * std::sin(gameTime_ * 8.0f);
        spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (1.6f + 0.25f * pulse), 0.08f, WithAlpha(Gold, ClampFloat(0.55f + 0.35f * pulse, 0.0f, 1.0f)), 0.05f, 48);
        spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (2.1f + 0.3f * pulse), 0.05f, WithAlpha(Sky, 0.45f), 0.05f, 48);
    }
    // フェーズ移行中：フェーズ色のオーラ＋外へ広がる衝撃波リングで派手に演出。
    if (boss_.active && boss_.phaseIntroT > 0.0f)
    {
        const Color phaseCols[4] = { {0.35f,0.95f,0.45f,1.0f}, {1.0f,0.9f,0.3f,1.0f}, {1.0f,0.6f,0.2f,1.0f}, {1.0f,0.3f,0.35f,1.0f} };
        const Color pc = phaseCols[std::max(0, std::min(3, boss_.phase - 1))];
        const float prog = ClampFloat(1.0f - boss_.phaseIntroT / BossPhaseIntroTime, 0.0f, 1.0f);
        // 3重の衝撃波が時間差で外へ広がる。
        for (int r = 0; r < 3; ++r)
        {
            const float local = ClampFloat(prog * 1.2f - r * 0.18f, 0.0f, 1.0f);
            if (local <= 0.0f) continue;
            const float radius = boss_.radius * (1.0f + local * 9.0f);
            const float alpha = (1.0f - local) * 0.7f;
            spriteCanvas_.DrawRing(boss_.pos, radius, 0.10f + 0.10f * (1.0f - local), WithAlpha(pc, alpha), 0.045f, 64);
        }
        // 足元の発光オーラ（点滅）。
        const float glow = 0.5f + 0.5f * std::sin(gameTime_ * 30.0f);
        spriteCanvas_.DrawCircle(boss_.pos, boss_.radius * (1.5f + 0.3f * glow), WithAlpha(pc, 0.30f + 0.25f * glow), 0.04f, 48);
    }

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
        else if (o.chocoWall)
        {
            // 長方形（見た目のみ）。x=厚み(正面方向)・y=横幅(直交)。spin が正面角度。
            const Color c = o.flash > 0.0f ? Cream : o.color;
            DrawSprite2D(L"2d_obstacle_wall", o.pos, { o.radius * 0.85f, o.radius * 1.9f }, o.spin, WithAlpha(c, 0.92f), 0.38f);
            spriteCanvas_.DrawRing(o.pos, o.radius, 0.08f, WithAlpha(Cream, 0.24f), 0.36f);
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
        if (s.chocoBomb)
        {
            // バウンドで巨大化する爆弾弾（専用イラスト・段階で大きさと色が変わる）
            const int stage = s.growStage;
            const Color bc = stage >= 3 ? Gold : (stage >= 2 ? Berry : Choco);
            const float bsize = s.radius * 3.4f;
            DrawSprite2D(L"2d_pickup_bomb", s.pos, { bsize, bsize }, gameTime_ * 3.0f, bc, 0.24f);
            for (int r = 0; r < stage; ++r)
            {
                spriteCanvas_.DrawRing(s.pos, s.radius * (1.4f + r * 0.5f), 0.06f, WithAlpha(Cream, 0.6f - r * 0.12f), 0.25f, 32);
            }
            continue;
        }
        if (s.visual == ShotVisualKind::Blade)
        {
            const float angle = AngleOf(s.vel);
            DrawSprite2D(L"effect_sword_line", s.pos, { s.radius * 4.2f, s.radius * 15.5f }, angle - Pi * 0.5f, WithAlpha(Grape, 0.88f), 0.247f);
            DrawSprite2D(L"2d_slash", s.pos + FromAngle(angle) * (s.radius * 1.6f), { s.radius * 8.0f, s.radius * 4.6f }, angle, WithAlpha(Red, 0.58f), 0.246f);
            spriteCanvas_.DrawRing(s.pos, s.radius * 2.05f, 0.045f, WithAlpha(Red, 0.58f), 0.245f, 28);
            if (s.reflected)
            {
                spriteCanvas_.DrawRing(s.pos, s.radius * (2.3f + 0.35f * s.reflectedCount), 0.055f, WithAlpha(Gold, 0.66f), 0.244f, 32);
            }
            continue;
        }
        const wchar_t* id = s.enemy ? L"2d_shot_enemy" : L"2d_shot_player";
        const float size = s.radius * (s.enemy ? 3.35f : 3.05f) * (s.charged ? 1.55f : 1.0f) * (s.visual == ShotVisualKind::Homing ? 1.14f : 1.0f);
        const Color shotColor = s.hiddenBossAuraKey ? WithAlpha(Gold, 1.0f) : (s.enemy && s.visual == ShotVisualKind::Homing ? WithAlpha(Red, 0.98f) : s.color);
        DrawSprite2D(id, s.pos, { size, size }, AngleOf(s.vel), shotColor, s.enemy ? 0.25f : 0.24f);
        if (s.enemy)
        {
            const Color outer = s.hiddenBossAuraKey ? Gold : (s.visual == ShotVisualKind::Homing ? Grape : Red);
            spriteCanvas_.DrawRing(s.pos, s.radius * (s.hiddenBossAuraKey ? 2.35f : (s.visual == ShotVisualKind::Homing ? 2.15f : 1.85f)), 0.045f, WithAlpha(outer, s.hiddenBossAuraKey ? 0.76f : 0.58f), 0.27f, 28);
            spriteCanvas_.DrawRing(s.pos, s.radius * 1.28f, 0.026f, WithAlpha(Cream, 0.30f), 0.269f, 24);
            if (s.hiddenBossAuraKey)
            {
                spriteCanvas_.DrawRing(s.pos, s.radius * 2.95f, 0.035f, WithAlpha(Cream, 0.58f), 0.268f, 32);
            }
        }
        else
        {
            spriteCanvas_.DrawRing(s.pos, s.radius * 1.45f, 0.026f, WithAlpha(Sky, 0.26f), 0.269f, 24);
        }
        if (s.reflected)
        {
            spriteCanvas_.DrawRing(s.pos, s.radius * (2.0f + 0.35f * s.reflectedCount), 0.045f, WithAlpha(Gold, 0.52f), 0.26f, 28);
        }
    }
    for (const auto& e : enemies_)
    {
        Color c = e.flash > 0.0f ? Cream : e.color;
        if (e.barrierT > 0.0f) c = Sky;
        const wchar_t* enemySprite = e.hiddenBossClone ? L"2d_boss_hidden" : EnemySpriteId(e.type);
        const float enemySize = e.radius * (e.hiddenBossClone ? 3.05f : 2.45f);
        DrawSprite2D(enemySprite, e.pos, { enemySize, enemySize }, e.face, c, 0.22f);
        if (e.hiddenBossClone)
        {
            spriteCanvas_.DrawRing(e.pos, e.radius * 1.62f, 0.060f, WithAlpha(Cream, 0.26f), 0.219f, 56);
            spriteCanvas_.DrawRing(e.pos, e.radius * 1.98f, 0.035f, WithAlpha(Color{ 0.62f, 0.64f, 0.70f, 1.0f }, 0.42f), 0.218f, 56);
        }
        else if (e.type == EnemyType::Teleport || e.type == EnemyType::Mirror || e.type == EnemyType::Barrier)
        {
            const Player* target = FindNearestPlayer(e.pos);
            const float face = target ? AngleOf(target->pos - e.pos) : e.face;
            DrawSprite2D(L"2d_shot_enemy", e.pos + FromAngle(face) * (e.radius * 0.68f), { e.radius * 0.55f, e.radius * 0.55f }, face, Red, 0.21f);
        }
    }
    if (boss_.active)
    {
        Color c = boss_.flash > 0.0f ? Cream : (boss_.bossType == BossType::HiddenBoss ? Grape : (boss_.bossType == BossType::DonutKing ? Sky : (boss_.bossType == BossType::MirrorMacaron ? Gold : Rose)));
        // 腕（左右2本）の2D表示。黒い節＋赤先端。消滅中は描かない。
        if (boss_.burrowSubT <= 0.0f && boss_.flyT <= 0.0f && boss_.flyStrikeWarnT <= 0.0f && boss_.bossType != BossType::HiddenBoss)
        {
            const Color tipCol = boss_.grabHoldT > 0.0f ? Grape : Red;
            for (int arm = 0; arm < 2; ++arm)
            {
                if (boss_.armDownT[arm] > 0.0f) continue;
                const V2 tip = boss_.armPos[arm];
                const V2 dd = tip - boss_.pos;
                const float seg[4] = { 0.34f, 0.50f, 0.66f, 0.80f };
                const float segR[4] = { 0.20f, 0.27f, 0.36f, 0.50f };
                for (int s = 0; s < 4; ++s)
                {
                    spriteCanvas_.DrawCircle(boss_.pos + dd * seg[s], segR[s], WithAlpha(Ink, 0.95f), 0.185f, 24);
                }
                spriteCanvas_.DrawCircle(tip, BossArmRadius, WithAlpha(tipCol, 0.9f), 0.18f, 36);
                spriteCanvas_.DrawRing(tip, BossArmRadius, 0.12f, WithAlpha(Cream, 0.5f), 0.178f, 40);
            }
        }
        if (boss_.burrowSubT <= 0.0f) // 地中突き上げの潜行中は本体を隠す
        {
            DrawSprite2D(boss_.bossType == BossType::HiddenBoss ? L"2d_boss_hidden" : L"2d_boss_normal", boss_.pos, { boss_.radius * 2.85f, boss_.radius * 2.85f }, boss_.spin * 0.35f, c, 0.18f);
            spriteCanvas_.DrawRing(boss_.pos, boss_.radius * 1.42f, 0.10f, WithAlpha(Red, 0.45f), 0.19f, 72);
        }
        // 地中突き上げ：潜行中は予測円、噴出中は発光円（2D）。
        if (boss_.burrowSubT > 0.0f)
        {
            const bool locked = boss_.burrowSubT <= BossBurrowLockTime;
            const float blink = locked ? 0.8f : (0.3f + 0.4f * std::sin(gameTime_ * 16.0f));
            for (int i = 0; i < boss_.burrowCount; ++i)
            {
                const V2 at = boss_.burrowTargets[i];
                spriteCanvas_.DrawRing(at, BossBurrowRadius, 0.10f + (locked ? 0.06f : 0.0f), WithAlpha(locked ? Red : Grape, ClampFloat(blink, 0.0f, 0.85f)), 0.17f, 48);
            }
        }
        if (boss_.burrowEruptT > 0.0f)
        {
            const float g = ClampFloat(boss_.burrowEruptT / BossBurrowEruptTime, 0.0f, 1.0f);
            for (int i = 0; i < boss_.burrowCount; ++i)
            {
                spriteCanvas_.DrawCircle(boss_.burrowTargets[i], BossBurrowRadius * (0.5f + 0.5f * g), WithAlpha(Grape, 0.3f + 0.5f * g), 0.17f, 40);
            }
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossForm_ >= 2)
        {
            const float pulse = 0.12f * std::sin(gameTime_ * (hiddenBossForm_ >= 3 ? 7.0f : 4.8f));
            const float strength = hiddenBossForm_ >= 3 ? 0.78f : 0.56f;
#if defined(_DEBUG)
            const float auraFx = ClampFloat(debug_.hiddenBossAuraFx, 0.0f, 2.0f);
#else
            const float auraFx = 1.0f;
#endif
            spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (1.85f + pulse), 0.14f, WithAlpha(Gold, ClampFloat(strength * auraFx, 0.0f, 1.0f)), 0.17f, 96);
            spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (2.28f - pulse), 0.08f, WithAlpha(Cream, ClampFloat(strength * 0.48f * auraFx, 0.0f, 1.0f)), 0.16f, 96);
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossReflectT_ > 0.0f)
        {
            const float pulse = 0.16f * std::sin(gameTime_ * 16.0f);
            spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (2.60f + pulse), 0.18f, WithAlpha(Gold, 0.82f), 0.155f, 112);
            spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (3.02f - pulse), 0.08f, WithAlpha(Cream, 0.58f), 0.154f, 112);
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossDashWarnT_ > 0.0f)
        {
            const float warn = ClampFloat(hiddenBossDashWarnT_ / std::max(0.01f, hiddenBossDashWarnLife_), 0.0f, 1.0f);
            spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (1.30f + (1.0f - warn) * 0.75f), 0.10f, WithAlpha(Red, 0.76f), 0.153f, 80);
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossForm_ == 1)
        {
            for (const auto& core : hiddenBossCores_)
            {
                if (!core.active) continue;
                const Color coreColor = core.flash > 0.0f ? Cream : Gold;
                spriteCanvas_.DrawCircle(core.pos, core.radius * 1.15f, WithAlpha(coreColor, 0.84f), 0.165f, 32);
                spriteCanvas_.DrawRing(core.pos, core.radius * 1.45f, 0.045f, WithAlpha(Red, 0.68f), 0.164f, 48);
            }
        }
        if (boss_.bossType == BossType::HiddenBoss && hiddenBossPhaseIntroT_ > 0.0f)
        {
            const float fade = ClampFloat(hiddenBossPhaseIntroT_ / std::max(0.01f, hiddenBossPhaseIntroLife_), 0.0f, 1.0f);
            for (int i = 0; i < 18; ++i)
            {
                const float a = TwoPi * i / 18.0f + gameTime_ * 0.9f;
                const V2 p0 = boss_.pos + FromAngle(a) * (ArenaRadius * 0.88f);
                const V2 p1 = boss_.pos + FromAngle(a) * (boss_.radius * 2.25f);
                const V2 mid = (p0 + p1) * 0.5f;
                const float length = Len(p0 - p1);
                DrawSprite2D(L"effect_sword_line", mid, { 0.12f, length }, a + Pi * 0.5f, WithAlpha(Gold, 0.45f * fade), 0.155f);
            }
        }
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
        DrawSprite2D(L"2d_shot_player", p.pos + FromAngle(p.face) * (p.radius * 1.55f), { p.radius * 1.75f, p.radius * 0.18f }, p.face, WithAlpha(Cream, 0.58f), 0.12f);
        if (p.bombCharge > 0.0f && !p.downed)
        {
            // チョコ爆弾チャージのプレビュー（溜め段階で前方の弾が大きくなる）
            const int cs = p.bombCharge >= 1.0f ? 3 : (p.bombCharge >= 0.6f ? 2 : (p.bombCharge >= 0.3f ? 1 : 0));
            const Color cc = cs >= 3 ? Gold : (cs >= 2 ? Berry : Choco);
            const float cr = 0.26f + 0.28f * static_cast<float>(cs);
            const V2 at = p.pos + FromAngle(p.face) * (p.radius + 0.3f + cr);
            DrawSprite2D(L"2d_pickup_bomb", at, { cr * 3.2f, cr * 3.2f }, gameTime_ * 3.0f, cc, 0.13f);
            spriteCanvas_.DrawRing(at, cr * 1.3f, 0.05f, WithAlpha(Cream, 0.6f), 0.12f, 28);
        }
        if (p.charging && !p.downed)
        {
            // チャージ進行に応じて外側のリングが機体へ収束し、溜まり具合を可視化する。
            const float chargeProgress = ClampFloat(p.chargeT / 1.15f, 0.0f, 1.0f);
            const Color chargeColor = p.chargeFull ? Gold : (p.chargeReady ? Sky : playerColor);
            const float outer = p.radius * (2.6f - 1.3f * chargeProgress);
            spriteCanvas_.DrawRing(p.pos, outer, 0.05f + 0.07f * chargeProgress,
                WithAlpha(chargeColor, 0.30f + 0.45f * chargeProgress), 0.125f, 48);
            if (p.chargeReady)
            {
                // 発動可能になったら機体周りで脈動させ、撃てる合図を明確に出す。
                const float pulse = 0.45f + 0.35f * std::sin(gameTime_ * 18.0f);
                spriteCanvas_.DrawRing(p.pos, p.chargeFull ? p.radius * 1.85f : p.radius * 1.28f,
                    p.chargeFull ? 0.10f : 0.09f, WithAlpha(p.chargeFull ? Gold : Cream, pulse), 0.122f, 48);
            }
        }
        if ((p.focus || screen_ == Screen::HiddenBoss) && !p.downed)
        {
            spriteCanvas_.DrawCircle(p.pos, p.hitboxRadius, Red, 0.11f, 20);
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

// 発光系の追加表示です。
// 弾、反射リング、剣FX、必殺、隠しボスの金色オーラなどをここで重ねます。
void SweetsApp::DrawAdditiveScene()
{
    if (!additiveRtv_) return;
    const float clear[4] = { 0, 0, 0, 1 };
    context_->ClearRenderTargetView(additiveRtv_.Get(), clear);

    const bool showGameplayScene =
        screen_ == Screen::Playing ||
        screen_ == Screen::Paused ||
        screen_ == Screen::Clear ||
        screen_ == Screen::HiddenBossIntro ||
        screen_ == Screen::HiddenBoss ||
        screen_ == Screen::CompleteClear;
    if (!showGameplayScene)
    {
        return;
    }

    ID3D11RenderTargetView* rtv = additiveRtv_.Get();
    context_->OMSetRenderTargets(1, &rtv, dsv_.Get());
    spriteCanvas_.Begin(view_ * proj_, true);
#if defined(_DEBUG)
    const float enemyGlowFx = ClampFloat(debug_.enemyBulletGlow, 0.0f, 2.0f);
    const float swordFx = ClampFloat(debug_.swordFx, 0.0f, 2.0f);
    const float ultimateFx = ClampFloat(debug_.ultimateFx, 0.0f, 2.0f);
    const float auraFx = ClampFloat(debug_.hiddenBossAuraFx, 0.0f, 2.0f);
#else
    const float enemyGlowFx = 1.0f;
    const float swordFx = 1.0f;
    const float ultimateFx = 1.0f;
    const float auraFx = 1.0f;
#endif
    auto fxAlpha = [](float alpha) { return ClampFloat(alpha, 0.0f, 1.0f); };

    for (const auto& s : shots_)
    {
        if (s.visual == ShotVisualKind::Blade)
        {
            DrawSprite2D(L"effect_sword_line", s.pos, { s.radius * 5.0f, s.radius * 18.0f }, AngleOf(s.vel) - Pi * 0.5f, WithAlpha(Red, fxAlpha(0.52f * enemyGlowFx)), 0.055f);
            continue;
        }
        const float glow = s.hiddenBossAuraKey ? 3.0f : (s.enemy ? (s.visual == ShotVisualKind::Homing ? 2.55f : 2.1f) : (s.reflected ? 2.8f : 1.7f));
        const Color glowColor = s.hiddenBossAuraKey ? Gold : (s.enemy && s.visual == ShotVisualKind::Homing ? Grape : s.color);
        spriteCanvas_.DrawCircle(s.pos, s.radius * glow, WithAlpha(glowColor, fxAlpha(s.enemy ? (s.hiddenBossAuraKey ? 0.62f : 0.42f) * enemyGlowFx : 0.55f)), 0.06f, 24);
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
        spriteCanvas_.DrawRing(pulse.pos, radius, 0.15f + radius * 0.025f, WithAlpha(pulse.color, fxAlpha(0.88f * fade * ultimateFx)), 0.05f, 96);
        spriteCanvas_.DrawRing(pulse.pos, radius * 0.62f, 0.09f, WithAlpha(Cream, fxAlpha(0.34f * fade * ultimateFx)), 0.04f, 72);
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

        DrawSprite2D(L"effect_sword_thunder", center, { width * sweep, length * (1.0f + progress * 0.10f) }, rotation, WithAlpha(Cream, fxAlpha(0.96f * fade * swordFx)), 0.035f);
        DrawSprite2D(L"effect_sword_line", center - side * (0.16f * baseScale), { width * 0.72f, length * 0.92f }, rotation + 0.05f, WithAlpha(Sky, fxAlpha(0.58f * fade * swordFx)), 0.034f);
        DrawSprite2D(L"effect_sword_line", center + side * (0.18f * baseScale), { width * 0.52f, length * 0.76f }, rotation - 0.07f, WithAlpha(Choco, fxAlpha(0.46f * fade * swordFx)), 0.033f);
        if (visual.charged)
        {
            DrawSprite2D(L"effect_sword_ring", root + forward * (0.42f * baseScale), { 1.55f * baseScale, 1.55f * baseScale }, gameTime_ * 5.0f, WithAlpha(Sky, fxAlpha(0.72f * fade * swordFx)), 0.032f);
            DrawSprite2D(L"effect_sword_thunder", center + forward * (0.32f * baseScale), { width * 1.22f, length * 1.10f }, rotation, WithAlpha(Gold, fxAlpha(0.42f * fade * swordFx)), 0.031f);
        }

        const int sparkCount = visual.charged ? 9 : 5;
        for (int i = 0; i < sparkCount; ++i)
        {
            const float t = (static_cast<float>(i) + 0.35f) / static_cast<float>(sparkCount);
            const float wave = std::sin((t + progress) * TwoPi * 1.7f);
            const V2 sparkPos = root + forward * (length * t) + side * (wave * width * 0.42f);
            const float sparkSize = (visual.charged ? 0.34f : 0.24f) * baseScale * (1.0f - 0.35f * t);
            DrawSprite2D(L"effect_sword_particle", sparkPos, { sparkSize, sparkSize }, visual.angle + t * TwoPi, WithAlpha((i & 1) ? Gold : Cream, fxAlpha(0.82f * fade * swordFx)), 0.030f);
        }
    }
    for (const auto& p : particles_)
    {
        spriteCanvas_.DrawCircle(p.pos, 0.10f + p.y * 0.04f, WithAlpha(p.color, ClampFloat(p.ttl * 2.5f, 0.0f, 1.0f)), 0.04f, 16);
    }
    if (boss_.active && boss_.bossType == BossType::HiddenBoss && hiddenBossForm_ >= 2)
    {
        const float pulse = 0.15f * std::sin(gameTime_ * (hiddenBossForm_ >= 3 ? 8.0f : 5.4f));
        const float alpha = hiddenBossForm_ >= 3 ? 0.92f : 0.68f;
        const bool broken = hiddenBossForm_ == 2 && hiddenBossAuraBreakT_ > 0.0f;
        const float flameAlpha = broken ? 0.34f : alpha;
        spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (2.05f + pulse), 0.20f, WithAlpha(Gold, fxAlpha(flameAlpha * auraFx)), 0.035f, 120);
        spriteCanvas_.DrawRing(boss_.pos, boss_.radius * (2.72f - pulse), 0.10f, WithAlpha(Cream, fxAlpha(flameAlpha * 0.42f * auraFx)), 0.034f, 120);
        const int flameCount = hiddenBossForm_ >= 3 ? 22 : 16;
        for (int i = 0; i < flameCount; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(flameCount);
            const float a = TwoPi * t + gameTime_ * (hiddenBossForm_ >= 3 ? 0.95f : 0.68f);
            const float wave = std::sin(gameTime_ * 5.2f + t * 17.0f);
            const V2 base = boss_.pos + FromAngle(a) * (boss_.radius * (1.45f + 0.13f * wave));
            const float length = boss_.radius * (1.35f + 0.42f * std::fabs(wave)) * (hiddenBossForm_ >= 3 ? 1.18f : 1.0f);
            const float width = boss_.radius * (0.20f + 0.05f * std::fabs(wave));
            DrawSprite2D(L"effect_sword_line", base + V2{ 0.0f, length * 0.36f }, { width, length }, 0.0f, WithAlpha((i & 1) ? Gold : Cream, fxAlpha(0.58f * flameAlpha * auraFx)), 0.033f);
            if ((i % 3) == 0)
            {
                DrawSprite2D(L"effect_sword_particle", base + V2{ 0.0f, length * 0.78f }, { width * 2.0f, width * 2.0f }, a + gameTime_, WithAlpha(Gold, fxAlpha(0.65f * flameAlpha * auraFx)), 0.032f);
            }
        }
    }
    spriteCanvas_.End();
}

// ブルーム処理です。
// 明るい部分を抽出してぼかし、合成時に戻すことで発光感を出します。
void SweetsApp::RenderBloom()
{
    if (!bloomRtvA_ || !bloomRtvB_ || !bloomVs_ || !bloomPrefilterPs_ || !bloomBlurPs_ || !additiveSrv_ || !bloomCB_)
    {
        return;
    }

    float blendFactor[4]{ 0, 0, 0, 0 };
    context_->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(bloomVs_.Get(), nullptr, 0);
    context_->RSSetState(rasterState_.Get());
    ID3D11SamplerState* sampler = postSampler_.Get();
    context_->PSSetSamplers(0, 1, &sampler);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(bloomWidth_);
    vp.Height = static_cast<float>(bloomHeight_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    const float invW = bloomWidth_ ? 1.0f / static_cast<float>(bloomWidth_) : 0.0f;
    const float invH = bloomHeight_ ? 1.0f / static_cast<float>(bloomHeight_) : 0.0f;
    ID3D11Buffer* bcb = bloomCB_.Get();
    context_->PSSetConstantBuffers(0, 1, &bcb);
    ID3D11ShaderResourceView* nullSrv[1] = { nullptr };

    auto runPass = [&](ID3D11PixelShader* ps, ID3D11ShaderResourceView* src, ID3D11RenderTargetView* dst, const BloomCB& cbData)
    {
        context_->PSSetShaderResources(0, 1, nullSrv); // 入力に使う前に出力バインドを解除
        ID3D11RenderTargetView* rtv = dst;
        context_->OMSetRenderTargets(1, &rtv, nullptr);
        context_->UpdateSubresource(bloomCB_.Get(), 0, nullptr, &cbData, 0, 0);
        context_->PSSetShader(ps, nullptr, 0);
        context_->PSSetShaderResources(0, 1, &src);
        context_->Draw(3, 0);
        context_->PSSetShaderResources(0, 1, nullSrv);
    };

    // 1) 明るい部分を抽出して半解像度のAへ
    BloomCB pre{};
    pre.texel = XMFLOAT4(invW, invH, 0.0f, 0.0f);
    pre.params = XMFLOAT4(0.55f, 0.45f, 1.0f, 0.0f); // しきい値 / ソフトニー / 強度
    runPass(bloomPrefilterPs_.Get(), additiveSrv_.Get(), bloomRtvA_.Get(), pre);

    // 2) 分離ガウスブラー(横→縦)を2回繰り返して広く滑らかに
    for (int i = 0; i < 2; ++i)
    {
        BloomCB h{};
        h.texel = XMFLOAT4(invW, invH, 1.0f, 0.0f);
        h.params = pre.params;
        runPass(bloomBlurPs_.Get(), bloomSrvA_.Get(), bloomRtvB_.Get(), h);

        BloomCB v{};
        v.texel = XMFLOAT4(invW, invH, 0.0f, 1.0f);
        v.params = pre.params;
        runPass(bloomBlurPs_.Get(), bloomSrvB_.Get(), bloomRtvA_.Get(), v);
    }
    // 結果は bloomSrvA_ に残る
    context_->PSSetShaderResources(0, 1, nullSrv);
}

// シーン色、加算FX、ブルーム、履歴を最終画面へ合成します。
// Debug時はTAAや加算RT表示の切替もここで反映します。
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

    // ブルームで半解像度に変更したビューポートを全解像度へ戻す
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

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
    post.params = XMFLOAT4(useTaa ? 0.18f : 0.0f, ClampFloat(debug_.additiveFx, 0.0f, 2.0f), debug_.additiveView ? 1.0f : 0.0f, ClampFloat(debug_.brightness, 0.5f, 1.5f));
#else
    post.params = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
#endif
    // params2: x=ブルーム強度, y=ビネット, z=トーンマッピング有効, w=未使用
    post.params2 = XMFLOAT4(0.75f, 0.28f, 1.0f, 0.0f);
    context_->UpdateSubresource(postCB_.Get(), 0, nullptr, &post, 0, 0);
    ID3D11Buffer* pcb = postCB_.Get();
    context_->PSSetConstantBuffers(0, 1, &pcb);
    ID3D11ShaderResourceView* srvs[4] = { sceneColorSrv_.Get(), additiveSrv_.Get(), historySrv_.Get(), bloomSrvA_.Get() };
    context_->PSSetShaderResources(0, 4, srvs);
    ID3D11SamplerState* sampler = postSampler_.Get();
    context_->PSSetSamplers(0, 1, &sampler);
    context_->Draw(3, 0);
    ID3D11ShaderResourceView* nullSrvs[4] = { nullptr, nullptr, nullptr, nullptr };
    context_->PSSetShaderResources(0, 4, nullSrvs);

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

// 画面全体の一瞬フラッシュです。
// 剣、必殺、隠しボス演出などの衝撃を短く見せるために使います。
void SweetsApp::DrawScreenFlashOverlay()
{
    if (screenFlashT_ <= 0.0f || screenFlashLife_ <= 0.0f)
    {
        return;
    }

    const float t = ClampFloat(screenFlashT_ / std::max(0.01f, screenFlashLife_), 0.0f, 1.0f);
#if defined(_DEBUG)
    const float flashFx = ClampFloat(debug_.screenFlashFx, 0.0f, 2.0f);
#else
    const float flashFx = 1.0f;
#endif
    const float alpha = ClampFloat(0.34f * t * t * flashFx, 0.0f, 1.0f);
    const D2D1_RECT_F full = D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_));

    textBrush_->SetColor(D2D1::ColorF(screenFlashColor_.r, screenFlashColor_.g, screenFlashColor_.b, alpha));
    d2dContext_->FillRectangle(full, textBrush_.Get());
    textBrush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.28f));
    d2dContext_->FillRectangle(full, textBrush_.Get());
}

