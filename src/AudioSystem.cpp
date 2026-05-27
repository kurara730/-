#include "AudioSystem.h"

#include <windows.h>
#include <mmsystem.h>

#include <array>
#include <filesystem>

AudioSystem::~AudioSystem()
{
    Stop();
}

void AudioSystem::PlayLoop(MusicTrack track, const std::wstring& relativePath)
{
    if (track == MusicTrack::None)
    {
        Stop();
        return;
    }
    if (currentTrack_ == track)
    {
        return;
    }

    CloseDevice();
    currentTrack_ = track;

    const std::wstring path = ResolveAssetPath(relativePath);
    if (path.empty())
    {
        return;
    }

    alias_ = L"sweets_bgm_" + std::to_wstring(++serial_);
    if (!Send(L"open \"" + path + L"\" type mpegvideo alias " + alias_))
    {
        alias_.clear();
        return;
    }
    if (!Send(L"play " + alias_ + L" repeat"))
    {
        CloseDevice();
    }
}

void AudioSystem::Stop()
{
    CloseDevice();
    currentTrack_ = MusicTrack::None;
}

std::wstring AudioSystem::ResolveAssetPath(const std::wstring& relativePath) const
{
    namespace fs = std::filesystem;
    const fs::path rel(relativePath);

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    const fs::path exeDir = fs::path(modulePath).parent_path();

    const std::array<fs::path, 5> candidates{
        fs::current_path() / rel,
        exeDir / rel,
        exeDir.parent_path() / rel,
        exeDir.parent_path().parent_path() / rel,
        rel
    };

    for (const auto& path : candidates)
    {
        std::error_code ec;
        if (fs::exists(path, ec))
        {
            return path.wstring();
        }
    }
    return {};
}

bool AudioSystem::Send(const std::wstring& command) const
{
    return mciSendStringW(command.c_str(), nullptr, 0, nullptr) == 0;
}

void AudioSystem::CloseDevice()
{
    if (!alias_.empty())
    {
        Send(L"stop " + alias_);
        Send(L"close " + alias_);
        alias_.clear();
    }
}
