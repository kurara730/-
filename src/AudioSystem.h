#pragma once

#include <memory>
#include <string>

enum class MusicTrack
{
    None,
    Title,
    Gameplay,
    GameOver,
    HiddenBossGauge1,
    HiddenBossGauge2,
    HiddenBossGauge3,
    HiddenBossClear
};

enum class SoundEffect
{
    ChocoSlash,
    UltimateSlash,
    Reflect
};

// BGM/SEを扱う薄いラッパーです。
// BGMはMedia Foundationで読み、XAudio2で再生します。失敗してもゲームは無音で続行します。
class AudioSystem
{
public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    // BGM再生入口。同じtrackが再要求された場合は無駄な開き直しを避けます。
    bool Play(MusicTrack track, const std::wstring& relativePath, bool loop);
    bool PlayLoop(MusicTrack track, const std::wstring& relativePath);
    bool PlayOnce(MusicTrack track, const std::wstring& relativePath);
    // 短いSEは事前にPCMとしてキャッシュします。ファイル欠落時はattemptedだけ残し無音にします。
    void LoadSoundEffect(SoundEffect effect, const std::wstring& relativePath);
    bool PlaySoundEffect(SoundEffect effect);
    void Update(float dt);
    void Stop();
    void SetVolume(float volume);
    void SetSoundVolume(float volume);
    float Volume() const;
    float SoundVolume() const;

    MusicTrack CurrentTrack() const;
    float CurrentDurationSeconds() const;
    std::wstring StreamStatus() const;
    const std::wstring& LastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
