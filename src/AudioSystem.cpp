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
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace
{
struct DecodedAudio
{
    std::vector<BYTE> format;
    std::vector<BYTE> pcm;
    float durationSeconds = 0.0f;
};

constexpr DWORD FirstAudioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);

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

    bool DecodeFile(const std::wstring& path, DecodedAudio& out)
    {
        auto found = cache.find(path);
        if (found != cache.end())
        {
            out = found->second;
            return true;
        }

        ComPtr<IMFSourceReader> reader;
        HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateSourceReaderFromURL", hr) + L": " + path);
            return false;
        }

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

        out.format.resize(waveSize);
        std::memcpy(out.format.data(), wave, waveSize);
        CoTaskMemFree(wave);

        for (;;)
        {
            DWORD flags = 0;
            ComPtr<IMFSample> sample;
            hr = reader->ReadSample(FirstAudioStream, 0, nullptr, &flags, nullptr, &sample);
            if (FAILED(hr))
            {
                SetError(HrText(L"ReadSample", hr));
                return false;
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
            if (!sample) continue;

            ComPtr<IMFMediaBuffer> buffer;
            hr = sample->ConvertToContiguousBuffer(&buffer);
            if (FAILED(hr))
            {
                SetError(HrText(L"ConvertToContiguousBuffer", hr));
                return false;
            }

            BYTE* data = nullptr;
            DWORD maxLen = 0;
            DWORD currentLen = 0;
            hr = buffer->Lock(&data, &maxLen, &currentLen);
            if (FAILED(hr))
            {
                SetError(HrText(L"IMFMediaBuffer::Lock", hr));
                return false;
            }
            const size_t oldSize = out.pcm.size();
            out.pcm.resize(oldSize + currentLen);
            std::memcpy(out.pcm.data() + oldSize, data, currentLen);
            buffer->Unlock();
        }

        if (out.format.empty() || out.pcm.empty())
        {
            SetError(L"Decoded audio was empty: " + path);
            return false;
        }

        const auto* wf = reinterpret_cast<const WAVEFORMATEX*>(out.format.data());
        if (wf->nAvgBytesPerSec > 0)
        {
            out.durationSeconds = static_cast<float>(out.pcm.size()) / static_cast<float>(wf->nAvgBytesPerSec);
        }

        cache[path] = out;
        return true;
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

        Stop();

        const std::wstring path = ResolveAssetPath(relativePath);
        if (path.empty())
        {
            SetError(L"Audio asset not found: " + relativePath);
            currentTrack = track;
            currentPath = relativePath;
            currentLoop = loop;
            return false;
        }

        DecodedAudio decoded;
        if (!DecodeFile(path, decoded))
        {
            currentTrack = track;
            currentPath = relativePath;
            currentLoop = loop;
            return false;
        }

        activeAudio = std::move(decoded.pcm);
        activeFormat = std::move(decoded.format);
        activeDuration = decoded.durationSeconds;

        auto* wf = reinterpret_cast<WAVEFORMATEX*>(activeFormat.data());
        HRESULT hr = engine->CreateSourceVoice(&sourceVoice, wf);
        if (FAILED(hr))
        {
            SetError(HrText(L"CreateSourceVoice", hr));
            activeAudio.clear();
            activeFormat.clear();
            currentTrack = track;
            currentPath = relativePath;
            currentLoop = loop;
            return false;
        }
        sourceVoice->SetVolume(volume);

        XAUDIO2_BUFFER buffer{};
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.AudioBytes = static_cast<UINT32>(activeAudio.size());
        buffer.pAudioData = activeAudio.data();
        if (loop)
        {
            buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
        }

        hr = sourceVoice->SubmitSourceBuffer(&buffer);
        if (FAILED(hr))
        {
            SetError(HrText(L"SubmitSourceBuffer", hr));
            Stop();
            return false;
        }
        hr = sourceVoice->Start(0);
        if (FAILED(hr))
        {
            SetError(HrText(L"IXAudio2SourceVoice::Start", hr));
            Stop();
            return false;
        }

        currentTrack = track;
        currentPath = relativePath;
        currentLoop = loop;
        startedAt = std::chrono::steady_clock::now();
        lastError.clear();
        return true;
    }

    void SetVolume(float value)
    {
        volume = std::max(0.0f, std::min(value, 1.0f));
        if (sourceVoice)
        {
            sourceVoice->SetVolume(volume);
        }
    }

    void Stop()
    {
        if (sourceVoice)
        {
            sourceVoice->Stop(0);
            sourceVoice->FlushSourceBuffers();
            sourceVoice->DestroyVoice();
            sourceVoice = nullptr;
        }
        activeAudio.clear();
        activeFormat.clear();
        activeDuration = 0.0f;
        currentTrack = MusicTrack::None;
        currentPath.clear();
        currentLoop = false;
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
    ComPtr<IXAudio2> engine;
    IXAudio2MasteringVoice* masteringVoice = nullptr;
    IXAudio2SourceVoice* sourceVoice = nullptr;
    std::unordered_map<std::wstring, DecodedAudio> cache;
    std::vector<BYTE> activeAudio;
    std::vector<BYTE> activeFormat;
    MusicTrack currentTrack = MusicTrack::None;
    std::wstring currentPath;
    bool currentLoop = false;
    float activeDuration = 0.0f;
    float volume = 1.0f;
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

void AudioSystem::Stop()
{
    impl_->Stop();
}

void AudioSystem::SetVolume(float volume)
{
    impl_->SetVolume(volume);
}

float AudioSystem::Volume() const
{
    return impl_->volume;
}

MusicTrack AudioSystem::CurrentTrack() const
{
    return impl_->currentTrack;
}

float AudioSystem::CurrentDurationSeconds() const
{
    return impl_->activeDuration;
}

const std::wstring& AudioSystem::LastError() const
{
    return impl_->lastError;
}
