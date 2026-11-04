#pragma once
#include <cstdint>
#include <atomic>
namespace ui_paint_owner {
// Both observations must be fresh and refer to the same exact root/generation.
// No closing-input fence is a visual ownership lease.
inline bool Fresh(std::uintptr_t requested,std::uintptr_t root,bool featureEnabled,bool visible,
                  bool ready,std::uint32_t generation,std::uint32_t readyGeneration,
                  std::uint32_t stateTick,std::uint32_t readyTick,std::uint32_t now) noexcept {
    return requested&&requested==root&&featureEnabled&&visible&&ready&&generation&&
           generation==readyGeneration&&now-stateTick<=250u&&now-readyTick<=250u;
}
// Visual ownership survives a presenter-generation handoff. Input leases still
// use Fresh unchanged. Set only by the matching renderer protocol handshake.
inline std::atomic<bool> rendererAvailable{false};
inline bool Visual(std::uintptr_t requested,std::uintptr_t root,bool featureAvailable,bool visible,
                   bool ready,std::uint32_t generation,std::uint32_t readyGeneration,
                   std::uint32_t stateTick,std::uint32_t readyTick,std::uint32_t now) noexcept {
    if(rendererAvailable.load(std::memory_order_relaxed))
        return requested&&requested==root&&featureAvailable&&visible&&generation&&now-stateTick<=250u;
    return Fresh(requested,root,featureAvailable,visible,ready,generation,readyGeneration,stateTick,readyTick,now);
}
}
