#pragma once
#include <cstdint>
#include <cstddef>
// Exact-r29 read-only AVM variable reader. No native calls, allocations or writes.
namespace dialog_text {
using Read = bool (*)(void*, std::uint32_t, void*, std::size_t);
struct Memory { Read read; void* context; };
constexpr std::size_t TextCapacity = 4096;
enum class Kind : std::uint32_t { Missing, Integer, String, Number };
struct Value { Kind kind{}; union { std::uint32_t integer{}; float number; }; char text[TextCapacity]{}; };
static_assert(sizeof(Value)==4104&&offsetof(Value,text)==8,"Native copied value layout");
// Type1 AVM values are IEEE float32 (921730 MOVSS at+2C), not raw integers.
// Counts/flags require finite exact integral values within their own bound.
bool Unsigned(const Value& value,std::uint32_t maximum,std::uint32_t& output) noexcept;
// root is the value at *nativeFlashHandle. Reproduces 921430/921650 lookup,
// including scope in the hash and ASCII case folding. Fails on corrupt chains,
// unsupported types or truncation; the output is zero on every failure.
bool ReadVariable(Memory memory, std::uint32_t root, const char* name, Value& value);
// Successful lookup may return Kind::Missing for an empty bucket/end of chain.
// Corrupt memory, cycles, unsupported values and truncation still fail closed.
bool ReadOptionalVariable(Memory memory, std::uint32_t root, const char* name, Value& value);
struct NativeSnapshot {
    std::uint32_t root{}, handle{}, owner{}, userArgument{}, pendingResult{}, language{};
    Value body{}, ok{}, cancel{}, no{}, yes{}, validEnabled{}, cancelEnabled{};
};
// Must run on the game's UI thread while the caller has authenticated exact r29.
// Loading/message dialogs and incomplete text/label sets are rejected.
bool Capture(Memory memory, NativeSnapshot& snapshot);
}
