#pragma once

#include "GameTypes.h"

// 反射のダメージ倍率です。
// 1回反射するごとに1.5倍、上限5倍まで上がり、反射キルの気持ちよさを作ります。
constexpr float ReflectDamageStep = 1.5f;
constexpr float ReflectDamageCap = 5.0f;

// 反射処理は複数キャラ/壁で使うため、独立した小さな関数に分けています。
V2 ReflectVelocity(V2 velocity, V2 normal, float power = 1.0f);
void ApplyShotReflection(Shot& shot, V2 normal, float power = 1.0f);
float ReflectedDamage(const Shot& shot);
