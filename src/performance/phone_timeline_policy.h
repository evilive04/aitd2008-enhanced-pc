#pragma once
#include <cstdint>
namespace phone_timeline_policy {
// Exact original phone.vmf, one-based sprite166 and486 frames. These gates
// authorize fresh complete captures only, never a cached image/blanket hold.
// Alerts, block/off, another page and an unknown timeline remain native.
inline bool HomePaint(std::uint32_t frame) noexcept {
    return (frame>=7&&frame<=56)||(frame>=100&&frame<=124)||frame==177;
}
inline bool HomeInteractive(std::uint32_t frame) noexcept {
    return frame==56||frame==177;
}
inline bool ParametersPaint(std::uint32_t frame) noexcept {
    return (frame>=1&&frame<=23)||(frame>=35&&frame<=56);
}
inline bool ParametersInteractive(std::uint32_t frame) noexcept {
    return frame==23;
}
inline bool RosterPaint(std::uint32_t mode,std::uint32_t frame) noexcept {
    return mode==2?((frame>=7&&frame<=48)||(frame>=57&&frame<=88)):
           mode==5?((frame>=7&&frame<=49)||(frame>=60&&frame<=76)):false;
}
inline bool RosterInteractive(std::uint32_t mode,std::uint32_t frame) noexcept {
    return mode==2?frame==48:mode==5&&frame==49;
}
inline bool MessageParentPaint(std::uint32_t frame) noexcept {
    return frame>=89&&frame<=97;
}
inline bool MessageListPaint(std::uint32_t frame) noexcept {
    return (frame>=10&&frame<=43)||frame==62;
}
inline bool MessageListInteractive(std::uint32_t frame) noexcept {
    return frame==33||frame==43||frame==62;
}
inline bool MessageReaderPaint(std::uint32_t frame) noexcept {
    return frame>=75&&frame<=99;
}
inline bool DialerPaint(std::uint32_t frame,std::uint32_t digits) noexcept {
    return ((frame>=1&&frame<=29)||(frame>=51&&frame<=64))&&digits>=8&&digits<=25;
}
inline bool DialerInteractive(std::uint32_t frame,std::uint32_t digits) noexcept {
    return frame==29&&digits==25;
}
inline bool MapPaint(std::uint32_t frame) noexcept {
    //559 stops27;28..32 are not a native route. map_exit begins33.
    return (frame>=1&&frame<=27)||(frame>=33&&frame<=48);
}
inline bool MapInteractive(std::uint32_t frame) noexcept {
    return frame==27;
}
inline bool ContactCallPaint(std::uint32_t frame) noexcept {
    return frame>=78&&frame<=108;
}
}
