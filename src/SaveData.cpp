#include "SweetsApp.h"

#include <filesystem>
#include <fstream>
#include <sstream>

// SaveData.cpp は、進行状況と設定をユーザー別のローカルフォルダへ保存します。
// 保存に失敗してもゲーム進行は止めず、次回起動時だけ既定値へ戻る設計です。

namespace
{
// %LOCALAPPDATA%\SweetsPanicDX11\save.dat を保存先にします。
// リポジトリ内へ書かないので、Git管理ファイルを汚しません。
std::filesystem::path SaveFilePath()
{
    wchar_t localAppData[MAX_PATH]{};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    std::filesystem::path base = (len > 0 && len < MAX_PATH) ? std::filesystem::path(localAppData) : std::filesystem::current_path();
    return base / L"SweetsPanicDX11" / L"save.dat";
}

// 壊れた値や範囲外の値を読んでも、fallbackへ戻して安全に起動します。
float ParseSaveFloat(const std::string& line, const char* key, float fallback)
{
    const std::string prefix = std::string(key) + "=";
    if (line.rfind(prefix, 0) != 0) return fallback;
    try
    {
        return ClampFloat(std::stof(line.substr(prefix.size())), 0.0f, 1.0f);
    }
    catch (...)
    {
        return fallback;
    }
}

int ParseSaveInt(const std::string& line, const char* key, int fallback, int minValue, int maxValue)
{
    const std::string prefix = std::string(key) + "=";
    if (line.rfind(prefix, 0) != 0) return fallback;
    try
    {
        return std::max(minValue, std::min(maxValue, std::stoi(line.substr(prefix.size()))));
    }
    catch (...)
    {
        return fallback;
    }
}
}

void SweetsApp::LoadProgress()
{
    hiddenBossUnlocked_ = false;
    masterVolume_ = 1.0f;
    bgmVolume_ = 1.0f;
    seVolume_ = 1.0f;
    uiVolume_ = 1.0f;
    shakeScale_ = 1.0f;
    showDamageNumbers_ = true;
    aimMode_ = AimMode::MoveDirection;
    bool savedFullscreen = false;
    const std::filesystem::path path = SaveFilePath();
    std::ifstream in(path);
    if (in)
    {
        std::string line;
        while (std::getline(in, line))
        {
            if (line == "hiddenBossUnlocked=1")
            {
                hiddenBossUnlocked_ = true;
            }
            masterVolume_ = ParseSaveFloat(line, "masterVolume", masterVolume_);
            bgmVolume_ = ParseSaveFloat(line, "bgmVolume", bgmVolume_);
            seVolume_ = ParseSaveFloat(line, "seVolume", seVolume_);
            uiVolume_ = ParseSaveFloat(line, "uiVolume", uiVolume_);
            shakeScale_ = ParseSaveFloat(line, "shakeScale", shakeScale_);
            showDamageNumbers_ = ParseSaveInt(line, "damageNumbers", showDamageNumbers_ ? 1 : 0, 0, 1) != 0;
            aimMode_ = static_cast<AimMode>(ParseSaveInt(line, "aimMode", static_cast<int>(aimMode_), 0, 2));
            savedFullscreen = ParseSaveInt(line, "fullscreen", savedFullscreen ? 1 : 0, 0, 1) != 0;
        }
    }
    ApplyAudioVolume();
    SetFullscreen(savedFullscreen, false);
    settingsLoaded_ = true;
}

void SweetsApp::SaveSettings()
{
    const std::filesystem::path path = SaveFilePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    out << "hiddenBossUnlocked=" << (hiddenBossUnlocked_ ? 1 : 0) << "\n";
    out << "masterVolume=" << ClampFloat(masterVolume_, 0.0f, 1.0f) << "\n";
    out << "bgmVolume=" << ClampFloat(bgmVolume_, 0.0f, 1.0f) << "\n";
    out << "seVolume=" << ClampFloat(seVolume_, 0.0f, 1.0f) << "\n";
    out << "uiVolume=" << ClampFloat(uiVolume_, 0.0f, 1.0f) << "\n";
    out << "shakeScale=" << ClampFloat(shakeScale_, 0.0f, 1.0f) << "\n";
    out << "damageNumbers=" << (showDamageNumbers_ ? 1 : 0) << "\n";
    out << "aimMode=" << static_cast<int>(aimMode_) << "\n";
    out << "fullscreen=" << (fullscreen_ ? 1 : 0) << "\n";
}

void SweetsApp::SaveProgress()
{
    hiddenBossUnlocked_ = true;
    SaveSettings();
}

