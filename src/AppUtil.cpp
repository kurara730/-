#include "SweetsApp.h"

bool SweetsApp::KeyDown(int key) const
{
    if (key < 0 || key >= MaxKeys) return false;
    return keys_[key];
}

float SweetsApp::Rand(float a, float b)
{
    std::uniform_real_distribution<float> dist(a, b);
    return dist(rng_);
}

int SweetsApp::RandInt(int a, int b)
{
    std::uniform_int_distribution<int> dist(a, b);
    return dist(rng_);
}

V2 SweetsApp::RandInArena(float margin)
{
    for (int i = 0; i < 24; ++i)
    {
        V2 p{};
        if (fieldShape_ == FieldShape::Rectangle)
        {
            p = { Rand(-12.2f + margin, 12.2f - margin), Rand(-8.4f + margin, 8.4f - margin) };
        }
        else if (fieldShape_ == FieldShape::Corridor)
        {
            p = { Rand(-5.4f + margin, 5.4f - margin), Rand(-13.0f + margin, 13.0f - margin) };
        }
        else if (fieldShape_ == FieldShape::Ring)
        {
            const float r = Rand(4.5f + margin, ArenaRadius - margin);
            const float a = Rand(0.0f, TwoPi);
            p = FromAngle(a) * r;
        }
        else
        {
            const float r = std::sqrt(Rand(0.0f, 1.0f)) * (ArenaRadius - margin);
            const float a = Rand(0.0f, TwoPi);
            p = FromAngle(a) * r;
        }

        V2 check = p;
        V2 normal{};
        if (!ResolveFieldBoundary(check, margin, normal))
        {
            return p;
        }
    }

    const float r = std::sqrt(Rand(0.0f, 1.0f)) * (ArenaRadius - margin);
    const float a = Rand(0.0f, TwoPi);
    return FromAngle(a) * r;
}

void SweetsApp::ClampInside(V2& p, float radius) const
{
    V2 normal{};
    ResolveFieldBoundary(p, radius, normal);
}

void SweetsApp::ClampInside(V3& p, float radius) const
{
    V2 xz{ p.x, p.z };
    ClampInside(xz, radius);
    p.x = xz.x;
    p.z = xz.z;
    p.y = ClampFloat(p.y, GameplayYMin, GameplayYMax);
}

bool SweetsApp::ResolveFieldBoundary(V2& p, float radius, V2& normal) const
{
    auto resolveCircle = [&](float limit)
    {
        const float maxR = std::max(0.1f, limit - radius);
        const float d = Len(p);
        if (d > maxR && d > 0.0001f)
        {
            normal = Normalize(p);
            p = normal * maxR;
            return true;
        }
        return false;
    };

    if (fieldShape_ == FieldShape::Rectangle || fieldShape_ == FieldShape::Corridor)
    {
        const float halfX = fieldShape_ == FieldShape::Corridor ? 5.8f : 12.6f;
        const float halfZ = fieldShape_ == FieldShape::Corridor ? 13.4f : 8.8f;
        bool hit = false;
        float bestPen = 0.0f;
        V2 bestNormal{};
        if (p.x > halfX - radius)
        {
            const float pen = p.x - (halfX - radius);
            p.x = halfX - radius;
            if (pen >= bestPen) { bestPen = pen; bestNormal = { 1.0f, 0.0f }; }
            hit = true;
        }
        if (p.x < -halfX + radius)
        {
            const float pen = (-halfX + radius) - p.x;
            p.x = -halfX + radius;
            if (pen >= bestPen) { bestPen = pen; bestNormal = { -1.0f, 0.0f }; }
            hit = true;
        }
        if (p.z > halfZ - radius)
        {
            const float pen = p.z - (halfZ - radius);
            p.z = halfZ - radius;
            if (pen >= bestPen) { bestPen = pen; bestNormal = { 0.0f, 1.0f }; }
            hit = true;
        }
        if (p.z < -halfZ + radius)
        {
            const float pen = (-halfZ + radius) - p.z;
            p.z = -halfZ + radius;
            if (pen >= bestPen) { bestPen = pen; bestNormal = { 0.0f, -1.0f }; }
            hit = true;
        }
        if (hit) normal = bestNormal;
        return hit;
    }

    if (fieldShape_ == FieldShape::Ring)
    {
        const float d = Len(p);
        const float inner = 4.15f + radius;
        if (d < inner && d > 0.0001f)
        {
            normal = Normalize(p) * -1.0f;
            p = Normalize(p) * inner;
            return true;
        }
        if (d <= 0.0001f)
        {
            normal = { -1.0f, 0.0f };
            p = { inner, 0.0f };
            return true;
        }
        return resolveCircle(ArenaRadius);
    }

    if (fieldShape_ == FieldShape::Octagon)
    {
        bool hit = resolveCircle(ArenaRadius * 0.96f);
        const float side = 11.7f - radius;
        const float diagLimit = 15.15f - radius;
        auto clampPlane = [&](V2 n)
        {
            const float d = Dot(p, n);
            if (d > diagLimit)
            {
                p -= n * (d - diagLimit);
                normal = n;
                hit = true;
            }
        };
        if (p.x > side) { p.x = side; normal = { 1.0f, 0.0f }; hit = true; }
        if (p.x < -side) { p.x = -side; normal = { -1.0f, 0.0f }; hit = true; }
        if (p.z > side) { p.z = side; normal = { 0.0f, 1.0f }; hit = true; }
        if (p.z < -side) { p.z = -side; normal = { 0.0f, -1.0f }; hit = true; }
        const float inv = 0.70710678f;
        clampPlane({ inv, inv });
        clampPlane({ -inv, inv });
        clampPlane({ inv, -inv });
        clampPlane({ -inv, -inv });
        return hit;
    }

    const float radiusLimit = fieldShape_ == FieldShape::ShrinkCircle ? shrinkRadius_ : ArenaRadius;
    return resolveCircle(radiusLimit);
}

float SweetsApp::GameplayViewHalfHeight() const
{
    return camera_.halfHeight;
}

float SweetsApp::GameplayViewHalfWidth() const
{
    const float aspect = static_cast<float>(std::max(1u, width_)) / std::max(1.0f, static_cast<float>(height_));
    return GameplayViewHalfHeight() * aspect;
}

// ジャスト回避ズームの倍率。0→ピーク→0 と滑らかに変化させます。
float SweetsApp::CameraZoom() const
{
    if (justZoomLife_ <= 0.0f || justZoomT_ <= 0.0f) return 1.0f;
    const float p = ClampFloat(1.0f - justZoomT_ / justZoomLife_, 0.0f, 1.0f); // 0→1
    return 1.0f + JustZoomPeak * std::sin(p * Pi);
}

void SweetsApp::UpdateCamera(float dt)
{
    V2 target{};
    int count = 0;
    for (const auto& p : players_)
    {
        if (!p.active || p.downed) continue;
        target += p.pos;
        ++count;
    }
    if (count > 0)
    {
        target = target / static_cast<float>(count);
    }
    else if (boss_.active)
    {
        target = boss_.pos;
    }
    camera_.target = target;
    const float follow = 1.0f - std::exp(-camera_.follow * std::max(0.0f, dt));
    camera_.center += (camera_.target - camera_.center) * follow;
    ClampInside(camera_.center, 0.0f);
}

V2 SweetsApp::WorldToScreen(V2 world) const
{
    const float halfW = GameplayViewHalfWidth();
    const float halfH = GameplayViewHalfHeight();
    const V2 local = world - camera_.center;
    return {
        static_cast<float>(width_) * 0.5f + local.x / std::max(0.01f, halfW) * static_cast<float>(width_) * 0.5f,
        static_cast<float>(height_) * 0.5f - local.z / std::max(0.01f, halfH) * static_cast<float>(height_) * 0.5f
    };
}

SettingsLayout SweetsApp::BuildSettingsLayout() const
{
    SettingsLayout layout{};
    const float panelW = 480.0f;
    const float panelH = 452.0f;
    const float left = (static_cast<float>(width_) - panelW) * 0.5f;
    const float top = (static_cast<float>(height_) - panelH) * 0.5f;
    layout.panel = { left, top, left + panelW, top + panelH };
    layout.sliderLeft = left + 170.0f;
    layout.sliderRight = left + panelW - 48.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float y = top + 110.0f + i * 44.0f;
        layout.volumeSliders[i] = { layout.sliderLeft - 12.0f, y - 14.0f, layout.sliderRight + 12.0f, y + 22.0f };
    }
    const float aimTop = top + 110.0f + 4 * 44.0f + 8.0f;
    const float aimButtonW = 104.0f;
    const float aimButtonH = 32.0f;
    const float aimStartX = left + 138.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float x = aimStartX + i * (aimButtonW + 10.0f);
        layout.aimButtons[i] = { x, aimTop, x + aimButtonW, aimTop + aimButtonH };
    }
    const float fullscreenTop = aimTop + 54.0f;
    layout.fullscreenToggle = { left + panelW - 148.0f, fullscreenTop, left + panelW - 48.0f, fullscreenTop + 32.0f };
    return layout;
}
