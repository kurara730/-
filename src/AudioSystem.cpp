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
    MusicTrack currentTrack = MusicTrack::None;
    std::wstring currentPath;
    std::wstring currentResolvedPath;
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

std::wstring AudioSystem::StreamStatus() const
{
    return impl_->StreamStatus();
}

const std::wstring& AudioSystem::LastError() const
{
    return impl_->lastError;
}
