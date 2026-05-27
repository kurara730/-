#include "VideoSystem.h"

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace
{
constexpr DWORD FirstVideoStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

std::wstring HrText(const wchar_t* prefix, HRESULT hr)
{
    std::wostringstream ss;
    ss << prefix << L" failed. HRESULT=0x" << std::hex << static_cast<unsigned>(hr);
    return ss.str();
}
}

struct VideoSystem::Impl
{
    ~Impl()
    {
        Stop();
        if (mfStarted)
        {
            MFShutdown();
            mfStarted = false;
        }
    }

    bool EnsureMediaFoundation()
    {
        if (mfStarted) return true;
        const HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFStartup", hr));
            return false;
        }
        mfStarted = true;
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

        for (const auto& candidate : candidates)
        {
            std::error_code ec;
            if (fs::exists(candidate, ec))
            {
                return candidate.wstring();
            }
        }
        return {};
    }

    bool Open(const std::wstring& relativePath, bool loopVideo)
    {
        Stop();
        loop = loopVideo;
        relative = relativePath;
        resolved = ResolveAssetPath(relativePath);
        if (resolved.empty())
        {
            SetError(L"Video asset not found: " + relativePath);
            return false;
        }
        if (!EnsureMediaFoundation()) return false;
        return OpenReader();
    }

    bool OpenReader()
    {
        reader.Reset();
        HRESULT hr = MFCreateSourceReaderFromURL(resolved.c_str(), nullptr, &reader);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateSourceReaderFromURL", hr) + L": " + resolved);
            return false;
        }

        ComPtr<IMFMediaType> type;
        hr = MFCreateMediaType(&type);
        if (FAILED(hr))
        {
            SetError(HrText(L"MFCreateMediaType", hr));
            return false;
        }
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

        hr = reader->SetCurrentMediaType(FirstVideoStream, nullptr, type.Get());
        if (FAILED(hr))
        {
            SetError(HrText(L"SetCurrentMediaType", hr));
            return false;
        }
        reader->SetStreamSelection(FirstVideoStream, TRUE);

        ComPtr<IMFMediaType> current;
        hr = reader->GetCurrentMediaType(FirstVideoStream, &current);
        if (FAILED(hr))
        {
            SetError(HrText(L"GetCurrentMediaType", hr));
            return false;
        }

        UINT32 w = 0;
        UINT32 h = 0;
        hr = MFGetAttributeSize(current.Get(), MF_MT_FRAME_SIZE, &w, &h);
        if (FAILED(hr) || w == 0 || h == 0)
        {
            SetError(HrText(L"MF_MT_FRAME_SIZE", hr));
            return false;
        }

        width = w;
        height = h;
        pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);
        hasFrame = false;
        ended = false;
        open = true;
        frameAccum = 0.0f;
        frameInterval = 1.0f / 30.0f;
        ReadFrame();
        return true;
    }

    void Stop()
    {
        reader.Reset();
        pixels.clear();
        open = false;
        hasFrame = false;
        ended = false;
        width = 0;
        height = 0;
        frameAccum = 0.0f;
    }

    void Update(float dt)
    {
        if (!open || ended) return;
        frameAccum += dt;
        if (!hasFrame || frameAccum >= frameInterval)
        {
            frameAccum = 0.0f;
            ReadFrame();
        }
    }

    bool Rewind()
    {
        if (!reader) return false;
        PROPVARIANT pos{};
        pos.vt = VT_I8;
        pos.hVal.QuadPart = 0;
        const HRESULT hr = reader->SetCurrentPosition(GUID_NULL, pos);
        if (FAILED(hr))
        {
            SetError(HrText(L"SetCurrentPosition", hr));
            return false;
        }
        ended = false;
        return true;
    }

    bool ReadFrame()
    {
        if (!reader) return false;

        for (int tries = 0; tries < 8; ++tries)
        {
            DWORD streamIndex = 0;
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            ComPtr<IMFSample> sample;
            HRESULT hr = reader->ReadSample(FirstVideoStream, 0, &streamIndex, &flags, &timestamp, &sample);
            if (FAILED(hr))
            {
                SetError(HrText(L"ReadSample", hr));
                ended = true;
                return false;
            }

            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                if (loop && Rewind())
                {
                    continue;
                }
                ended = true;
                return false;
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

            const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
            const size_t copyLen = std::min(expected, static_cast<size_t>(currentLen));
            std::copy(data, data + copyLen, pixels.begin());
            if (copyLen < expected)
            {
                std::fill(pixels.begin() + copyLen, pixels.end(), static_cast<unsigned char>(0));
            }
            buffer->Unlock();

            hasFrame = true;
            ++serial;
            return true;
        }
        return false;
    }

    void SetError(const std::wstring& text)
    {
        lastError = text;
#if defined(_DEBUG)
        OutputDebugStringW((L"[SweetsActionDX11 Video] " + text + L"\n").c_str());
#endif
    }

    bool mfStarted = false;
    bool open = false;
    bool loop = false;
    bool hasFrame = false;
    bool ended = false;
    uint32_t width = 0;
    uint32_t height = 0;
    float frameAccum = 0.0f;
    float frameInterval = 1.0f / 30.0f;
    uint64_t serial = 0;
    std::wstring relative;
    std::wstring resolved;
    std::wstring lastError;
    std::vector<unsigned char> pixels;
    ComPtr<IMFSourceReader> reader;
};

VideoSystem::VideoSystem()
    : impl_(std::make_unique<Impl>())
{
}

VideoSystem::~VideoSystem() = default;

bool VideoSystem::Open(const std::wstring& relativePath, bool loop)
{
    return impl_->Open(relativePath, loop);
}

void VideoSystem::Stop()
{
    impl_->Stop();
}

void VideoSystem::Update(float dt)
{
    impl_->Update(dt);
}

bool VideoSystem::IsOpen() const
{
    return impl_->open;
}

bool VideoSystem::HasFrame() const
{
    return impl_->hasFrame;
}

bool VideoSystem::Ended() const
{
    return impl_->ended;
}

uint32_t VideoSystem::Width() const
{
    return impl_->width;
}

uint32_t VideoSystem::Height() const
{
    return impl_->height;
}

uint64_t VideoSystem::FrameSerial() const
{
    return impl_->serial;
}

const std::vector<unsigned char>& VideoSystem::Pixels() const
{
    return impl_->pixels;
}

const std::wstring& VideoSystem::LastError() const
{
    return impl_->lastError;
}
