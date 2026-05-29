#include "SweetsApp.h"

#include <filesystem>

namespace
{
std::wstring AssetPath(const wchar_t* relative)
{
    namespace fs = std::filesystem;
    const fs::path rel(relative);

    std::array<fs::path, 5> candidates{};
    candidates[0] = fs::current_path() / rel;

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    const fs::path exeDir = fs::path(modulePath).parent_path();
    candidates[1] = exeDir / rel;
    candidates[2] = exeDir.parent_path() / rel;
    candidates[3] = exeDir.parent_path().parent_path() / rel;
    candidates[4] = rel;

    for (const auto& path : candidates)
    {
        std::error_code ec;
        if (fs::exists(path, ec))
        {
            return path.wstring();
        }
    }
    return rel.wstring();
}
}

void SweetsApp::LoadAssets()
{
    LoadTitleAssets();
    LoadGameplayAssets();
    LoadEffectAssets();
}

void SweetsApp::LoadTitleAssets()
{
    if (titleAssetsLoaded_) return;

    textureLibrary_.Register(L"shortcake_icon", AssetPath(L"assets/textures/shortcake.png"));
    textureLibrary_.Register(L"chocolate_icon", AssetPath(L"assets/textures/chocolate.png"));
    textureLibrary_.Register(L"cheese_icon", AssetPath(L"assets/textures/cheese.png"));
    textureLibrary_.Register(L"roll_icon", AssetPath(L"assets/textures/roll.png"));
    textureLibrary_.Register(L"enemy_icon", AssetPath(L"assets/textures/enemy.png"));
    textureLibrary_.Register(L"pickup_icon", AssetPath(L"assets/textures/pickup.png"));
    textureLibrary_.LoadAll(device_.Get());

    spriteLibrary_.Register(L"shortcake_icon", L"shortcake_icon");
    spriteLibrary_.Register(L"chocolate_icon", L"chocolate_icon");
    spriteLibrary_.Register(L"cheese_icon", L"cheese_icon");
    spriteLibrary_.Register(L"roll_icon", L"roll_icon");
    spriteLibrary_.Register(L"enemy_icon", L"enemy_icon");
    spriteLibrary_.Register(L"pickup_icon", L"pickup_icon");

    LoadTitleImageBitmap();
    titleAssetsLoaded_ = true;
}

void SweetsApp::LoadGameplayAssets()
{
    if (gameplayAssetsLoaded_) return;

    assetCatalog_.ReplaceTexture(VisualRole::Player, AssetPath(L"assets/textures/shortcake.png"));
    assetCatalog_.ReplaceTexture(VisualRole::EnemyRunner, AssetPath(L"assets/textures/enemy.png"));
    assetCatalog_.ReplaceTexture(VisualRole::Pickup, AssetPath(L"assets/textures/pickup.png"));
    assetCatalog_.ReplaceSprite(VisualRole::Player, AssetPath(L"assets/textures/2d/player_shortcake.png"));
    assetCatalog_.ReplaceSprite(VisualRole::EnemyRunner, AssetPath(L"assets/textures/2d/enemy_normal.png"));
    assetCatalog_.ReplaceSprite(VisualRole::EnemyShooter, AssetPath(L"assets/textures/2d/enemy_teleport.png"));
    assetCatalog_.ReplaceSprite(VisualRole::EnemyHeavy, AssetPath(L"assets/textures/2d/enemy_mine.png"));
    assetCatalog_.ReplaceSprite(VisualRole::Boss, AssetPath(L"assets/textures/2d/boss_normal.png"));
    assetCatalog_.ReplaceSprite(VisualRole::Shot, AssetPath(L"assets/textures/2d/shot_player.png"));
    assetCatalog_.ReplaceSprite(VisualRole::Pickup, AssetPath(L"assets/textures/2d/pickup_attack.png"));
    assetCatalog_.ReplaceSprite(VisualRole::Obstacle, AssetPath(L"assets/textures/2d/obstacle_wall.png"));

    auto addSprite = [&](const wchar_t* id, const wchar_t* relativePath)
    {
        textureLibrary_.Register(id, AssetPath(relativePath));
        spriteLibrary_.Register(id, id);
    };

    addSprite(L"2d_player_shortcake", L"assets/textures/2d/player_shortcake.png");
    addSprite(L"2d_player_chocolate", L"assets/textures/2d/player_chocolate.png");
    addSprite(L"2d_player_cheese", L"assets/textures/2d/player_cheese.png");
    addSprite(L"2d_player_roll", L"assets/textures/2d/player_roll.png");
    addSprite(L"2d_enemy_normal", L"assets/textures/2d/enemy_normal.png");
    addSprite(L"2d_enemy_shield", L"assets/textures/2d/enemy_shield.png");
    addSprite(L"2d_enemy_split", L"assets/textures/2d/enemy_split.png");
    addSprite(L"2d_enemy_healer", L"assets/textures/2d/enemy_healer.png");
    addSprite(L"2d_enemy_barrier", L"assets/textures/2d/enemy_barrier.png");
    addSprite(L"2d_enemy_mirror", L"assets/textures/2d/enemy_mirror.png");
    addSprite(L"2d_enemy_mine", L"assets/textures/2d/enemy_mine.png");
    addSprite(L"2d_enemy_teleport", L"assets/textures/2d/enemy_teleport.png");
    addSprite(L"2d_boss_normal", L"assets/textures/2d/boss_normal.png");
    addSprite(L"2d_boss_hidden", L"assets/textures/2d/boss_hidden.png");
    addSprite(L"2d_shot_player", L"assets/textures/2d/shot_player.png");
    addSprite(L"2d_shot_enemy", L"assets/textures/2d/shot_enemy.png");
    addSprite(L"2d_slash", L"assets/textures/2d/slash.png");
    addSprite(L"2d_obstacle_wall", L"assets/textures/2d/obstacle_wall.png");
    addSprite(L"2d_pickup_attack", L"assets/textures/2d/pickup_attack.png");
    addSprite(L"2d_pickup_slow", L"assets/textures/2d/pickup_slow.png");
    addSprite(L"2d_pickup_invincible", L"assets/textures/2d/pickup_invincible.png");
    addSprite(L"2d_pickup_magnet", L"assets/textures/2d/pickup_magnet.png");
    addSprite(L"2d_pickup_bomb", L"assets/textures/2d/pickup_bomb.png");
    addSprite(L"2d_pickup_heal", L"assets/textures/2d/pickup_heal.png");
    addSprite(L"2d_pickup_ult", L"assets/textures/2d/pickup_ult.png");
    addSprite(L"2d_pickup_spread", L"assets/textures/2d/pickup_spread.png");
    addSprite(L"2d_pickup_speed", L"assets/textures/2d/pickup_speed.png");
    addSprite(L"2d_pickup_score", L"assets/textures/2d/pickup_score.png");
    textureLibrary_.LoadAll(device_.Get());

    gameplayAssetsLoaded_ = true;
}

void SweetsApp::LoadEffectAssets()
{
    if (effectAssetsLoaded_) return;

    if (!effekseer_.Available())
    {
        effekseer_.Initialize(device_.Get(), context_.Get());
    }

    effekseer_.LoadEffect(L"sword_slash", L"assets/effects/sword_slash.efkefc", 3.0f);
    effekseer_.LoadEffect(L"ult_shortcake", L"assets/effects/ult_shortcake.efkefc", 4.0f);
    effekseer_.LoadEffect(L"ult_chocolate", L"assets/effects/ult_chocolate.efkefc", 4.0f);
    effekseer_.LoadEffect(L"ult_cheese", L"assets/effects/ult_cheese.efkefc", 4.0f);
    effekseer_.LoadEffect(L"ult_roll", L"assets/effects/ult_roll.efkefc", 4.0f);
    effectAssetsLoaded_ = true;
}

void SweetsApp::EnsureGameplayAssetsReady()
{
    LoadGameplayAssets();
    LoadEffectAssets();
}
