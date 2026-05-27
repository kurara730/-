#include "AssetCatalog.h"

#include <utility>

namespace
{
size_t IndexOf(VisualRole role)
{
    return static_cast<size_t>(role);
}
}

AssetCatalog::AssetCatalog()
{
    visuals_[IndexOf(VisualRole::Player)] = { VisualRole::Player, MeshKind::Sphere, Berry, L"Player" };
    visuals_[IndexOf(VisualRole::EnemyRunner)] = { VisualRole::EnemyRunner, MeshKind::Sphere, Rose, L"Runner Enemy" };
    visuals_[IndexOf(VisualRole::EnemyShooter)] = { VisualRole::EnemyShooter, MeshKind::Sphere, Gold, L"Shooter Enemy" };
    visuals_[IndexOf(VisualRole::EnemyHeavy)] = { VisualRole::EnemyHeavy, MeshKind::Sphere, Choco, L"Heavy Enemy" };
    visuals_[IndexOf(VisualRole::Boss)] = { VisualRole::Boss, MeshKind::Sphere, Grape, L"Boss" };
    visuals_[IndexOf(VisualRole::Shot)] = { VisualRole::Shot, MeshKind::Sphere, Cream, L"Projectile" };
    visuals_[IndexOf(VisualRole::Pickup)] = { VisualRole::Pickup, MeshKind::Sphere, Mint, L"Pickup" };
    visuals_[IndexOf(VisualRole::Obstacle)] = { VisualRole::Obstacle, MeshKind::Cylinder, Choco, L"Obstacle" };
}

const VisualAsset& AssetCatalog::Get(VisualRole role) const
{
    return visuals_[IndexOf(role)];
}

void AssetCatalog::ReplaceTexture(VisualRole role, std::wstring path)
{
    visuals_[IndexOf(role)].texturePath = std::move(path);
}

void AssetCatalog::ReplaceSprite(VisualRole role, std::wstring path)
{
    visuals_[IndexOf(role)].spritePath = std::move(path);
}

void AssetCatalog::ReplaceModel(VisualRole role, std::wstring path)
{
    visuals_[IndexOf(role)].modelPath = std::move(path);
}
