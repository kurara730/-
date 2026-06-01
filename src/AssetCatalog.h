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

// 画面上の役割ごとに、テクスチャ/スプライト/モデル差し替え口を持ちます。
// ファイルパスが空なら、コード側の図形フォールバックを使って必ず表示します。
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

// 後から画像やFBX/glTFモデルへ差し替えるための入口です。
// ゲームロジックはAssetCatalogを通じて素材を参照し、直接ファイル名へ依存しない方針です。
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
