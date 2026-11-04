#pragma once
// Local-file-only, silent Media Foundation decoding. Never call Start/Stop in DllMain.
// Rendering/audio/context ownership deliberately stay with the caller.
#include <windows.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace menu_video {
struct Diagnostic {
    DWORD size{sizeof(Diagnostic)},stage{},exceptionKind{},exceptionCode{},thread{};
    HRESULT error{S_OK};
    std::uint64_t decoded{},published{},copied{},requestedBytes{};
    std::uint64_t freeAtFailure{},largestFreeAtFailure{};
    char detail[160]{};
};
struct Frame {
    unsigned width{}, height{};
    // Media Foundation enumeration values, retained from the decoded stream.
    unsigned matrix{},nominalRange{},chromaSiting{},primaries{},transfer{};
    std::int64_t timestamp100ns{}, duration100ns{};
    std::uint64_t serial{}, loop{};
    // Native, tightly packed NV12: width*height Y bytes followed by width*height/2
    // interleaved U,V bytes. No 4K RGB conversion/copies in the 32-bit host.
    std::vector<unsigned char> pixels;
};
class Decoder {
public:
    Decoder(); ~Decoder();
    Decoder(const Decoder&) = delete; Decoder& operator=(const Decoder&) = delete;
    // Open/Read/Rewind/Close must run on the same COM-initialized thread.
    HRESULT Open(const wchar_t* path);
    HRESULT Read(Frame& frame, bool& end);
    HRESULT Rewind();
    void Close();
    DWORD Stage()const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
class Player {
public:
    Player(); ~Player();
    Player(const Player&) = delete; Player& operator=(const Player&) = delete;
    // Nonblocking decode startup; Error() reports asynchronous failures.
    bool Start(const wchar_t* localPath);
    // Nonblocking; suspends decode work without discarding the last frame.
    void SetActive(bool active)noexcept;
    void Stop();
    // No allocation or copy when caller already owns the latest frame.
    bool CopyLatest(Frame& output);
    HRESULT Error() const;
    Diagnostic Diagnostics()const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
