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
    const float r = std::sqrt(Rand(0.0f, 1.0f)) * (ArenaRadius - margin);
    const float a = Rand(0.0f, TwoPi);
    return FromAngle(a) * r;
}

void SweetsApp::ClampInside(V2& p, float radius) const
{
    const float maxR = ArenaRadius - radius;
    const float d = Len(p);
    if (d > maxR && d > 0.0001f)
    {
        p = p / d * maxR;
    }
}

void SweetsApp::ClampInside(V3& p, float radius) const
{
    V2 xz{ p.x, p.z };
    ClampInside(xz, radius);
    p.x = xz.x;
    p.z = xz.z;
    p.y = ClampFloat(p.y, GameplayYMin, GameplayYMax);
}

