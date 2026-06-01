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
            p = { Rand(-8.6f + margin, 8.6f - margin), Rand(-5.8f + margin, 5.8f - margin) };
        }
        else if (fieldShape_ == FieldShape::Corridor)
        {
            p = { Rand(-3.8f + margin, 3.8f - margin), Rand(-9.2f + margin, 9.2f - margin) };
        }
        else if (fieldShape_ == FieldShape::Ring)
        {
            const float r = Rand(3.6f + margin, ArenaRadius - margin);
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
        const float halfX = fieldShape_ == FieldShape::Corridor ? 4.1f : 8.8f;
        const float halfZ = fieldShape_ == FieldShape::Corridor ? 9.4f : 6.0f;
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
        const float inner = 3.05f + radius;
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
        const float side = 8.25f - radius;
        const float diagLimit = 10.85f - radius;
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

