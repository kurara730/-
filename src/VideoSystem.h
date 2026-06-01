#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// タイトル枠やイベント用動画を扱うクラスです。
// 動画ファイルが無い/読めない場合は、呼び出し側が静止画や通常背景へフォールバックします。
class VideoSystem
{
public:
    VideoSystem();
    ~VideoSystem();

    VideoSystem(const VideoSystem&) = delete;
    VideoSystem& operator=(const VideoSystem&) = delete;

    bool Open(const std::wstring& relativePath, bool loop);
    void Stop();
    void Update(float dt);

    bool IsOpen() const;
    bool HasFrame() const;
    bool Ended() const;
    uint32_t Width() const;
    uint32_t Height() const;
    uint64_t FrameSerial() const;
    const std::vector<unsigned char>& Pixels() const;
    const std::wstring& LastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
