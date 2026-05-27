#include "ReflectionSystem.h"

V2 ReflectVelocity(V2 velocity, V2 normal, float power)
{
    const V2 n = Normalize(normal);
    V2 out = velocity - n * (2.0f * Dot(velocity, n));
    return out * std::max(0.2f, power);
}

void ApplyShotReflection(Shot& shot, V2 normal, float power)
{
    shot.vel = ReflectVelocity(shot.vel, normal, power);
    shot.reflected = true;
    shot.reflectedCount = std::min(6, shot.reflectedCount + 1);
}

float ReflectedDamage(const Shot& shot)
{
    if (!shot.reflected) return shot.damage;
    const float multiplier = std::min(ReflectDamageCap, std::pow(ReflectDamageStep, static_cast<float>(shot.reflectedCount)));
    return shot.damage * multiplier;
}

