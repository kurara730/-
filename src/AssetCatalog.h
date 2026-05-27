#pragma once

#include "GameTypes.h"

#include <array>
#include <string>

enum class VisualRole
{
    Player,
    EnemyRunner,
    EnemyShooter,
    EnemyHeavy,
    Boss,
    Shot,
    Pickup,
    Obstacle,
    Count
};

enum class MeshKind
{
    Sphere,
    Cylinder,
    Floor,
    Ring,
    Cube,
    Wedge
};

struct VisualAsset
{
    VisualRole role = VisualRole::Player;
    MeshKind fallbackMesh = MeshKind::Sphere;
    Color fallbackColor = Cream;
    std::wstring displayName;

    // Empty paths mean procedural fallback rendering is used.
    // Fill these later when replacing primitives with art assets.
    std::wstring texturePath;
    std::wstring spritePath;
    std::wstring modelPath; // Intended for FBX/glTF integration.
};

class AssetCatalog
{
public:
    AssetCatalog();

    const VisualAsset& Get(VisualRole role) const;
    void ReplaceTexture(VisualRole role, std::wstring path);
    void ReplaceSprite(VisualRole role, std::wstring path);
    void ReplaceModel(VisualRole role, std::wstring path);

private:
    std::array<VisualAsset, static_cast<size_t>(VisualRole::Count)> visuals_{};
};
