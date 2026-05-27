#pragma once

#include <memory>
#include <string>

enum class MusicTrack
{
    None,
    Title,
    Gameplay,
    GameOver,
    HiddenBoss
};

class AudioSystem
{
public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool Play(MusicTrack track, const std::wstring& relativePath, bool loop);
    bool PlayLoop(MusicTrack track, const std::wstring& relativePath);
    bool PlayOnce(MusicTrack track, const std::wstring& relativePath);
    void Stop();

    MusicTrack CurrentTrack() const;
    float CurrentDurationSeconds() const;
    const std::wstring& LastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
