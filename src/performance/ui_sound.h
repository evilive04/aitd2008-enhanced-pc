#pragma once
#include <cstdint>
#include <atomic>
#include <initializer_list>
namespace ui_sound {
enum class Cue : std::uint32_t { Hover=1,Confirm=2,Back=3 };
using Sink=bool(*)(Cue) noexcept;
inline std::atomic<Sink> sink{nullptr};
inline void SetSink(Sink value)noexcept{sink.store(value,std::memory_order_release);}
inline bool Emit(Cue cue)noexcept{const auto target=sink.load(std::memory_order_acquire);return target&&target(cue);}
// Native AVM sound events, not gameplay, dialogue, phone calls or item foley.
inline Cue NativeCue(const char* name)noexcept{
 if(!name)return static_cast<Cue>(0);
 const auto equal=[](const char* a,const char* b){for(unsigned i=0;i<32;++i){if(a[i]!=b[i])return false;if(!a[i])return true;}return false;};
 for(const char* n:{"move","movechap","moveseq","movetel","move_ph","clic","clic_ph","switch"})if(equal(name,n))return Cue::Hover;
 for(const char* n:{"valid","val_chap","validtel","correct","valstart","openmenu","entphon","wooshin","mapin"})if(equal(name,n))return Cue::Confirm;
 for(const char* n:{"back","cancel","backtel","extphon","wooshout","mapoff","wrong","wrongtel","movelock","warning"})if(equal(name,n))return Cue::Back;
 return static_cast<Cue>(0);
}
}
