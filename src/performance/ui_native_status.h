#pragma once
#include <cstdint>
namespace ui_native_status {
//54B3D0 latches5 while closing the loading film; the value can remain5 during
// actual gameplay (v17e: Episode4 clinic rendered, save written). It is not a
// reliable "loading is visible" boolean. Concrete UI owner/routes/mode still
// have to authenticate independently. Unknown states remain blocking.
inline bool BlockingLoad(std::uint32_t value)noexcept{return value!=0&&value!=5;}
}
