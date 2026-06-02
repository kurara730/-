#include "AudioSystem.h"

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <xaudio2.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <filesystem>
#include <sstream>
#include <vector>

using Microsoft::WRL::ComPtr;

// AudioSystem.cpp はBGMのストリーミング再生と、短いSEのキャッシュ再生を担当します。
// MP3/WAVの読み込みに失敗しても例外で止めず、LastErrorへ理由を残して無音継続します。

namespace
{
constexpr DWORD FirstAudioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
constexpr size_t TargetPacketBytes = 64u * 1024u;
constexpr size_t MaxQueuedPackets = 4u;
constexpr LONGLONG MediaFoundationTicksPerSecond = 10000000;

struct StreamPacket
{
    std::vector<BYTE> bytes;
    bool endOfStream = false;
};

// SEは短い想定なので、初回読み込み時にPCMへ展開して保持します。
// BGMは長いため、全展開せず小さなチャンクを順番に送ります。
struct CachedSound
{
    std::vector<BYTE> format;
    std::vector<BYTE> pcm;
    std::wstring path;
    bool attempted = false;
};

struct ActiveSound
{
    IXAudio2SourceVoice* voice = nullptr;
};

std::wstring HrText(const wchar_t* prefix, HRESULT hr)
{
    std::wostringstream ss;
    ss << prefix << L" failed. HRESULT=0x" << std::hex << static_cast<unsigned>(hr);
    return ss.str();
}
}

struct AudioSystem::Impl
{
    ~Impl()
    {
        Stop();
        if (masteringVoice)
        {
            masteringVoice->DestroyVoice();
            masteringVoice = nullptr;
        }
        engine.Reset();
        if (mfStarted)
        {
            MFShutdown();
            mfStarted = false;
        }
    }

    // Media FoundationとXAudio2を初期化します。
    // 初回再生時に遅延初期化することで、起動時の処理を軽くしています。
    bool Initialize()
    {
        if (initialized) return ready;
        initialized = true;

        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFStartup", hr));
            return false;
        }
        mfStarted = true;

        hr = XAudio2Create(&engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(hr))
        {
            SetError(HrText(L"XAudio2Create", hr));
            return false;
        }

        hr = engine->CreateMasteringVoice(&masteringVoice);
        if (FAILED(hr))
        {
            SetError(HrText(L"CreateMasteringVoice", hr));
            return false;
        }

        ready = true;
        return true;
    }

    // assets/ の場所は実行方法で変わるため、複数候補から実在するファイルを探します。
    std::wstring ResolveAssetPath(const std::wstring& relativePath) const
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

    // Media Foundation Source Readerを開き、音声ストリームをPCM形式へ変換できる状態にします。
    bool OpenReader(const std::wstring& path)
    {
        reader.Reset();
        activeFormat.clear();
        activeDuration = 0.0f;
        readerEnded = false;

        HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateSourceReaderFromURL", hr) + L": " + path);
            return false;
        }

        reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
        reader->SetStreamSelection(FirstAudioStream, TRUE);

        PROPVARIANT duration{};
        PropVariantInit(&duration);
        if (SUCCEEDED(reader->GetPresentationAttribute(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &duration)))
        {
            if (duration.vt == VT_UI8 || duration.vt == VT_I8)
            {
                activeDuration = static_cast<float>(duration.uhVal.QuadPart) / static_cast<float>(MediaFoundationTicksPerSecond);
            }
        }
        PropVariantClear(&duration);

        ComPtr<IMFMediaType> pcmType;
        hr = MFCreateMediaType(&pcmType);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateMediaType", hr));
            return false;
        }
        pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pcmType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);

        hr = reader->SetCurrentMediaType(FirstAudioStream, nullptr, pcmType.Get());
        if (FAILED(hr))
        {
            SetError(HrText(L"SetCurrentMediaType PCM", hr) + L": " + path);
            return false;
        }

        ComPtr<IMFMediaType> actualType;
        hr = reader->GetCurrentMediaType(FirstAudioStream, &actualType);
        if (FAILED(hr))
        {
            SetError(HrText(L"GetCurrentMediaType", hr));
            return false;
        }

        WAVEFORMATEX* wave = nullptr;
        UINT32 waveSize = 0;
        hr = MFCreateWaveFormatExFromMFMediaType(actualType.Get(), &wave, &waveSize);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateWaveFormatExFromMFMediaType", hr));
            return false;
        }

        activeFormat.resize(waveSize);
        std::memcpy(activeFormat.data(), wave, waveSize);
        CoTaskMemFree(wave);
        return true;
    }

    bool DecodeFile(const std::wstring& relativePath, CachedSound& out)
    {
        out.attempted = true;
        out.path = relativePath;
        out.format.clear();
        out.pcm.clear();

        if (!Initialize())
        {
            return false;
        }

        const std::wstring path = ResolveAssetPath(relativePath);
        if (path.empty())
        {
            SetError(L"SE asset not found: " + relativePath);
            return false;
        }

        ComPtr<IMFSourceReader> soundReader;
        HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &soundReader);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateSourceReaderFromURL SE", hr) + L": " + path);
            return false;
        }

        soundReader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
        soundReader->SetStreamSelection(FirstAudioStream, TRUE);

        ComPtr<IMFMediaType> pcmType;
        hr = MFCreateMediaType(&pcmType);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateMediaType SE", hr));
            return false;
        }
        pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pcmType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);

        hr = soundReader->SetCurrentMediaType(FirstAudioStream, nullptr, pcmType.Get());
        if (FAILED(hr))
        {
            SetError(HrText(L"SetCurrentMediaType PCM SE", hr) + L": " + path);
            return false;
        }

        ComPtr<IMFMediaType> actualType;
        hr = soundReader->GetCurrentMediaType(FirstAudioStream, &actualType);
        if (FAILED(hr))
        {
            SetError(HrText(L"GetCurrentMediaType SE", hr));
            return false;
        }

        WAVEFORMATEX* wave = nullptr;
        UINT32 waveSize = 0;
        hr = MFCreateWaveFormatExFromMFMediaType(actualType.Get(), &wave, &waveSize);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateWaveFormatExFromMFMediaType SE", hr));
            return false;
        }
        out.format.resize(waveSize);
        std::memcpy(out.format.data(), wave, waveSize);
        CoTaskMemFree(wave);

        for (int guard = 0; guard < 8192; ++guard)
        {
            DWORD flags = 0;
            ComPtr<IMFSample> sample;
            hr = soundReader->ReadSample(FirstAudioStream, 0, nullptr, &flags, nullptr, &sample);
            if (FAILED(hr))
            {
                SetError(HrText(L"ReadSample SE", hr));
                return false;
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                break;
            }
            if (!sample)
            {
                continue;
            }

            ComPtr<IMFMediaBuffer> buffer;
            hr = sample->ConvertToContiguousBuffer(&buffer);
            if (FAILED(hr))
            {
                SetError(HrText(L"ConvertToContiguousBuffer SE", hr));
                return false;
            }

            BYTE* data = nullptr;
            DWORD maxLen = 0;
            DWORD currentLen = 0;
            hr = buffer->Lock(&data, &maxLen, &currentLen);
            if (FAILED(hr))
            {
                SetError(HrText(L"IMFMediaBuffer::Lock SE", hr));
                return false;
            }
            const size_t oldSize = out.pcm.size();
            out.pcm.resize(oldSize + currentLen);
            std::memcpy(out.pcm.data() + oldSize, data, currentLen);
            buffer->Unlock();
        }

        if (out.pcm.empty())
        {
            SetError(L"SE produced no data: " + path);
            return false;
        }
        return true;
    }

    bool Rewind()
    {
        if (!reader) return false;
        PROPVARIANT pos{};
        PropVariantInit(&pos);
        pos.vt = VT_I8;
        pos.hVal.QuadPart = 0;
        const HRESULT hr = reader->SetCurrentPosition(GUID_NULL, pos);
        PropVariantClear(&pos);
        if (FAILED(hr))
        {
            SetError(HrText(L"SetCurrentPosition", hr));
            return false;
        }
        readerEnded = false;
        return true;
    }

    bool ReadPacket(StreamPacket& packet)
    {
        if (!reader || readerEnded) return false;

        packet.bytes.clear();
        packet.endOfStream = false;

        for (int guard = 0; packet.bytes.size() < TargetPacketBytes && guard < 128; ++guard)
        {
            DWORD flags = 0;
            ComPtr<IMFSample> sample;
            HRESULT hr = reader->ReadSample(FirstAudioStream, 0, nullptr, &flags, nullptr, &sample);
            if (FAILED(hr))
            {
                SetError(HrText(L"ReadSample", hr));
                readerEnded = true;
                packet.endOfStream = true;
                return !packet.bytes.empty();
            }

            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                if (currentLoop && Rewind())
                {
                    continue;
                }
                readerEnded = true;
                packet.endOfStream = true;
                break;
            }

            if (!sample)
            {
                continue;
            }

            ComPtr<IMFMediaBuffer> buffer;
            hr = sample->ConvertToContiguousBuffer(&buffer);
            if (FAILED(hr))
            {
                SetError(HrText(L"ConvertToContiguousBuffer", hr));
                readerEnded = true;
                packet.endOfStream = true;
                return !packet.bytes.empty();
            }

            BYTE* data = nullptr;
            DWORD maxLen = 0;
            DWORD currentLen = 0;
            hr = buffer->Lock(&data, &maxLen, &currentLen);
            if (FAILED(hr))
            {
                SetError(HrText(L"IMFMediaBuffer::Lock", hr));
                readerEnded = true;
                packet.endOfStream = true;
                return !packet.bytes.empty();
            }
            const size_t oldSize = packet.bytes.size();
            packet.bytes.resize(oldSize + currentLen);
            std::memcpy(packet.bytes.data() + oldSize, data, currentLen);
            buffer->Unlock();
        }

        return !packet.bytes.empty();
    }

    void ReclaimConsumedPackets()
    {
        if (!sourceVoice)
        {
            queuedPackets.clear();
            return;
        }

        XAUDIO2_VOICE_STATE state{};
        sourceVoice->GetState(&state);
        while (queuedPackets.size() > state.BuffersQueued)
        {
            queuedPackets.pop_front();
        }
    }

    bool SubmitPacket(StreamPacket&& packet)
    {
        if (!sourceVoice || packet.bytes.empty()) return false;

        queuedPackets.push_back(std::move(packet));
        const StreamPacket& stored = queuedPackets.back();

        XAUDIO2_BUFFER buffer{};
        buffer.Flags = stored.endOfStream ? XAUDIO2_END_OF_STREAM : 0;
        buffer.AudioBytes = static_cast<UINT32>(stored.bytes.size());
        buffer.pAudioData = stored.bytes.data();

        const HRESULT hr = sourceVoice->SubmitSourceBuffer(&buffer);
        if (FAILED(hr))
        {
            queuedPackets.pop_back();
            SetError(HrText(L"SubmitSourceBuffer", hr));
            return false;
        }
        return true;
    }

    void QueuePackets()
    {
        if (!sourceVoice || !reader) return;
        ReclaimConsumedPackets();

        while (queuedPackets.size() < MaxQueuedPackets && !readerEnded)
        {
            StreamPacket packet{};
            if (!ReadPacket(packet))
            {
                break;
            }
            if (!SubmitPacket(std::move(packet)))
            {
                break;
            }
        }
    }

    bool Play(MusicTrack track, const std::wstring& relativePath, bool loop)
    {
        if (track == MusicTrack::None)
        {
            Stop();
            return true;
        }
        if (currentTrack == track && currentPath == relativePath && currentLoop == loop)
        {
            return true;
        }
        if (!Initialize())
        {
            currentTrack = track;
            currentPath = relativePath;
            currentLoop = loop;
            return false;
        }

        StopMusic();

        const std::wstring path = ResolveAssetPath(relativePath);
        if (path.empty())
        {
            SetError(L"Audio asset not found: " + relativePath);
            currentTrack = track;
            currentPath = relativePath;
            currentLoop = loop;
            return false;
        }

        currentTrack = track;
        currentPath = relativePath;
        currentResolvedPath = path;
        currentLoop = loop;

        if (!OpenReader(path))
        {
            return false;
        }

        auto* wf = reinterpret_cast<WAVEFORMATEX*>(activeFormat.data());
        HRESULT hr = engine->CreateSourceVoice(&sourceVoice, wf);
        if (FAILED(hr))
        {
            SetError(HrText(L"CreateSourceVoice", hr));
            Stop();
            currentTrack = track;
            currentPath = relativePath;
            currentLoop = loop;
            return false;
        }
        sourceVoice->SetVolume(volume);

        QueuePackets();
        if (queuedPackets.empty())
        {
            SetError(L"Audio stream produced no data: " + path);
            Stop();
            currentTrack = track;
            currentPath = relativePath;
            currentLoop = loop;
            return false;
        }

        hr = sourceVoice->Start(0);
        if (FAILED(hr))
        {
            SetError(HrText(L"IXAudio2SourceVoice::Start", hr));
            Stop();
            currentTrack = track;
            currentPath = relativePath;
            currentLoop = loop;
            return false;
        }

        startedAt = std::chrono::steady_clock::now();
        lastError.clear();
        return true;
    }

    void Update(float)
    {
        QueuePackets();
        for (auto it = activeSounds.begin(); it != activeSounds.end();)
        {
            XAUDIO2_VOICE_STATE state{};
            if (it->voice)
            {
                it->voice->GetState(&state);
            }
            if (!it->voice || state.BuffersQueued == 0)
            {
                if (it->voice)
                {
                    it->voice->DestroyVoice();
                }
                it = activeSounds.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void SetVolume(float value)
    {
        volume = std::max(0.0f, std::min(value, 1.0f));
        if (sourceVoice)
        {
            sourceVoice->SetVolume(volume);
        }
    }

    void SetSoundVolume(float value)
    {
        soundVolume = std::max(0.0f, std::min(value, 1.0f));
        for (auto& sound : activeSounds)
        {
            if (sound.voice)
            {
                sound.voice->SetVolume(soundVolume);
            }
        }
    }

    void LoadSoundEffect(SoundEffect effect, const std::wstring& relativePath)
    {
        const size_t index = static_cast<size_t>(effect);
        if (index >= soundCache.size()) return;
        DecodeFile(relativePath, soundCache[index]);
    }

    bool PlaySoundEffect(SoundEffect effect)
    {
        const size_t index = static_cast<size_t>(effect);
        if (index >= soundCache.size()) return false;
        CachedSound& sound = soundCache[index];
        if (!sound.attempted || sound.pcm.empty())
        {
            return false;
        }
        if (!Initialize())
        {
            return false;
        }

        IXAudio2SourceVoice* voice = nullptr;
        auto* wf = reinterpret_cast<WAVEFORMATEX*>(sound.format.data());
        HRESULT hr = engine->CreateSourceVoice(&voice, wf);
        if (FAILED(hr))
        {
            SetError(HrText(L"CreateSourceVoice SE", hr));
            return false;
        }

        XAUDIO2_BUFFER buffer{};
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.AudioBytes = static_cast<UINT32>(sound.pcm.size());
        buffer.pAudioData = sound.pcm.data();
        hr = voice->SubmitSourceBuffer(&buffer);
        if (FAILED(hr))
        {
            voice->DestroyVoice();
            SetError(HrText(L"SubmitSourceBuffer SE", hr));
            return false;
        }
        voice->SetVolume(soundVolume);
        hr = voice->Start(0);
        if (FAILED(hr))
        {
            voice->DestroyVoice();
            SetError(HrText(L"IXAudio2SourceVoice::Start SE", hr));
            return false;
        }
        activeSounds.push_back({ voice });
        return true;
    }

    void StopMusic()
    {
        if (sourceVoice)
        {
            sourceVoice->Stop(0);
            sourceVoice->FlushSourceBuffers();
            sourceVoice->DestroyVoice();
            sourceVoice = nullptr;
        }
        reader.Reset();
        queuedPackets.clear();
        activeFormat.clear();
        activeDuration = 0.0f;
        currentTrack = MusicTrack::None;
        currentPath.clear();
        currentResolvedPath.clear();
        currentLoop = false;
        readerEnded = false;
    }

    void Stop()
    {
        StopMusic();
        for (auto& sound : activeSounds)
        {
            if (sound.voice)
            {
                sound.voice->Stop(0);
                sound.voice->DestroyVoice();
            }
        }
        activeSounds.clear();
    }

    std::wstring StreamStatus() const
    {
        std::wostringstream ss;
        ss << (ready ? L"stream" : L"not-ready")
            << L" q=" << queuedPackets.size()
            << (currentLoop ? L" loop" : L" once");
        if (readerEnded) ss << L" end";
        return ss.str();
    }

    void SetError(const std::wstring& text)
    {
        lastError = text;
#if defined(_DEBUG)
        OutputDebugStringW((L"[SweetsActionDX11 Audio] " + text + L"\n").c_str());
#endif
    }

    bool initialized = false;
    bool ready = false;
    bool mfStarted = false;
    bool readerEnded = false;
    ComPtr<IXAudio2> engine;
    IXAudio2MasteringVoice* masteringVoice = nullptr;
    IXAudio2SourceVoice* sourceVoice = nullptr;
    ComPtr<IMFSourceReader> reader;
    std::deque<StreamPacket> queuedPackets;
    std::vector<BYTE> activeFormat;
    std::array<CachedSound, 3> soundCache;
    std::vector<ActiveSound> activeSounds;
    MusicTrack currentTrack = MusicTrack::None;
    std::wstring currentPath;
    std::wstring currentResolvedPath;
    bool currentLoop = false;
    float activeDuration = 0.0f;
    float volume = 1.0f;
    float soundVolume = 1.0f;
    std::chrono::steady_clock::time_point startedAt{};
    std::wstring lastError;
};

AudioSystem::AudioSystem()
    : impl_(std::make_unique<Impl>())
{
}

AudioSystem::~AudioSystem() = default;

bool AudioSystem::Play(MusicTrack track, const std::wstring& relativePath, bool loop)
{
    return impl_->Play(track, relativePath, loop);
}

bool AudioSystem::PlayLoop(MusicTrack track, const std::wstring& relativePath)
{
    return impl_->Play(track, relativePath, true);
}

bool AudioSystem::PlayOnce(MusicTrack track, const std::wstring& relativePath)
{
    return impl_->Play(track, relativePath, false);
}

void AudioSystem::LoadSoundEffect(SoundEffect effect, const std::wstring& relativePath)
{
    impl_->LoadSoundEffect(effect, relativePath);
}

bool AudioSystem::PlaySoundEffect(SoundEffect effect)
{
    return impl_->PlaySoundEffect(effect);
}

void AudioSystem::Update(float dt)
{
    impl_->Update(dt);
}

void AudioSystem::Stop()
{
    impl_->Stop();
}

void AudioSystem::SetVolume(float volume)
{
    impl_->SetVolume(volume);
}

void AudioSystem::SetSoundVolume(float volume)
{
    impl_->SetSoundVolume(volume);
}

float AudioSystem::Volume() const
{
    return impl_->volume;
}

float AudioSystem::SoundVolume() const
{
    return impl_->soundVolume;
}

MusicTrack AudioSystem::CurrentTrack() const
{
    return impl_->currentTrack;
}

float AudioSystem::CurrentDurationSeconds() const
{
    return impl_->activeDuration;
}

std::wstring AudioSystem::StreamStatus() const
{
    return impl_->StreamStatus();
}

const std::wstring& AudioSystem::LastError() const
{
    return impl_->lastError;
}
