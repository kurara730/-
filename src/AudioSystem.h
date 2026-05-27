#pragma once

#include <string>

enum class MusicTrack
{
    None,
    Gameplay,
    GameOver
};

class AudioSystem
{
public:
    ~AudioSystem();

    void PlayLoop(MusicTrack track, const std::wstring& relativePath);
    void Stop();

private:
    std::wstring ResolveAssetPath(const std::wstring& relativePath) const;
    bool Send(const std::wstring& command) const;
    void CloseDevice();

    MusicTrack currentTrack_ = MusicTrack::None;
    std::wstring alias_;
    unsigned int serial_ = 0;
};
