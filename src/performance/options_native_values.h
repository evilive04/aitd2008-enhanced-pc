#pragma once
#include "dialog_text_capture.h"
namespace options_native_values {
// Caller authenticates the movie and Resolve binding. Native91F5D0 uses the
// movie-root variable table and the resolved character's scope at instance+E0.
bool Scoped(dialog_text::Memory memory,std::uint32_t root,std::uint32_t instance,
            const char* name,dialog_text::Value& value,bool optional=false);
struct Gauge {float minimum{},maximum{},current{},steps{};};
// Reads the native54C3F0 widget domain. This does not change the widget/game.
bool ReadGauge(dialog_text::Memory memory,std::uint32_t widget,Gauge& value);
}
