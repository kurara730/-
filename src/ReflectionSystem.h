#pragma once

#include "GameTypes.h"

constexpr float ReflectDamageStep = 1.5f;
constexpr float ReflectDamageCap = 5.0f;

V2 ReflectVelocity(V2 velocity, V2 normal, float power = 1.0f);
void ApplyShotReflection(Shot& shot, V2 normal, float power = 1.0f);
float ReflectedDamage(const Shot& shot);

