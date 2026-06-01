#include "ReflectionSystem.h"

// 速度ベクトルを壁の法線normalで反射させます。
// 物理でいう「入射角=反射角」の計算を、2Dベクトルで行っています。
V2 ReflectVelocity(V2 velocity, V2 normal, float power)
{
    const V2 n = Normalize(normal);
    V2 out = velocity - n * (2.0f * Dot(velocity, n));
    return out * std::max(0.2f, power);
}

// 弾を反射済み状態にし、反射回数を増やします。
// reflectedCount は最終ダメージ計算とUI演出の両方で使います。
void ApplyShotReflection(Shot& shot, V2 normal, float power)
{
    shot.vel = ReflectVelocity(shot.vel, normal, power);
    shot.reflected = true;
    shot.reflectedCount = std::min(6, shot.reflectedCount + 1);
}

// 反射回数に応じた最終ダメージです。
// 上限を設けることで、無限反射でボスHPが壊れないようにしています。
float ReflectedDamage(const Shot& shot)
{
    if (!shot.reflected) return shot.damage;
    const float multiplier = std::min(ReflectDamageCap, std::pow(ReflectDamageStep, static_cast<float>(shot.reflectedCount)));
    return shot.damage * multiplier;
}
