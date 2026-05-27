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

    assetCatalog_.ReplaceTexture(VisualRole::Player, AssetPath(L"assets/textures/shortcake.png"));
    assetCatalog_.ReplaceTexture(VisualRole::EnemyRunner, AssetPath(L"assets/textures/enemy.png"));
    assetCatalog_.ReplaceTexture(VisualRole::Pickup, AssetPath(L"assets/textures/pickup.png"));

    effekseer_.LoadEffect(L"sword_slash", L"assets/effects/sword_slash.efkefc", 0.55f);
    effekseer_.LoadEffect(L"ult_shortcake", L"assets/effects/ult_shortcake.efkefc", 1.0f);
    effekseer_.LoadEffect(L"ult_chocolate", L"assets/effects/ult_chocolate.efkefc", 1.1f);
    effekseer_.LoadEffect(L"ult_cheese", L"assets/effects/ult_cheese.efkefc", 1.1f);
    effekseer_.LoadEffect(L"ult_roll", L"assets/effects/ult_roll.efkefc", 1.0f);
}
