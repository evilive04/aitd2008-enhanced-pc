#pragma once
#include <cstdint>
namespace target_lock_policy {
// The caller samples the ORIGINAL bound native action62, before substituting
// the effective action record. No physical Space key is assumed here.
struct State {
    std::uintptr_t owner{};
    std::uint32_t tick{};
    bool latched{},previousDown{},armed{};
    bool Step(std::uintptr_t nextOwner,std::uint32_t now,bool eligible,bool physicalDown) noexcept {
        const bool fresh=eligible&&nextOwner&&owner==nextOwner&&now-tick<=250;
        if(!fresh){latched=false;armed=!physicalDown;previousDown=physicalDown;}
        owner=eligible?nextOwner:0;tick=now;
        if(!eligible){latched=false;armed=false;previousDown=physicalDown;return false;}
        if(!physicalDown)armed=true;
        if(fresh&&armed&&physicalDown&&!previousDown){latched=!latched;armed=false;}
        previousDown=physicalDown;return latched;
    }
    void Reset() noexcept {*this={};}
};
}
