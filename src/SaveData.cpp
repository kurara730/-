#include "SweetsApp.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
std::filesystem::path SaveFilePath()
{
    wchar_t localAppData[MAX_PATH]{};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    std::filesystem::path base = (len > 0 && len < MAX_PATH) ? std::filesystem::path(localAppData) : std::filesystem::current_path();
    return base / L"SweetsPanicDX11" / L"save.dat";
}

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
    aimMode_ = AimMode::MoveDirection;
    const std::filesystem::path path = SaveFilePath();
    std::ifstream in(path);
    if (!in) return;

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
        aimMode_ = static_cast<AimMode>(ParseSaveInt(line, "aimMode", static_cast<int>(aimMode_), 0, 2));
    }
    ApplyAudioVolume();
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
    out << "aimMode=" << static_cast<int>(aimMode_) << "\n";
}

void SweetsApp::SaveProgress()
{
    hiddenBossUnlocked_ = true;
    SaveSettings();
}

