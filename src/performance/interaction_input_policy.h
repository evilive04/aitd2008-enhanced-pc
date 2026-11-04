#pragma once
namespace interaction_input {
enum class Mode : unsigned { Ordinary, Cabinet, Other, HeldObject, Vehicle, VehicleInteractive, VehicleHeld, VehicleNative };
// Only called after authenticating the real HVCL handler and vehicle owner.
inline Mode VehicleMode(unsigned state,bool keyFocus){
    if(state==5||state==15)return keyFocus?Mode::VehicleInteractive:Mode::Vehicle;
    if(state==0x1d||state==0x1e)return Mode::VehicleHeld;
    return Mode::VehicleNative;
}
struct Frame {unsigned tick{},owner{};bool eligible{},held{};Mode mode{Mode::Ordinary};};
struct Output {bool enter{},leave{};};
struct State {
    static constexpr unsigned HoldMilliseconds=500,MaximumGap=250;
    unsigned owner{},last{},started{};Mode mode{Mode::Ordinary};
    bool active{},latch{},previous{},armed{},longPress{};
    Output Step(const Frame& f){
        if(!f.eligible||!f.owner){*this={};latch=f.held;return {};}
        const bool fresh=!active||owner!=f.owner||f.tick-last>MaximumGap;
        if(fresh){const bool held=latch||f.held;*this={};active=true;owner=f.owner;mode=f.mode;latch=held;}
        if(mode!=f.mode){
            const bool oldVehicle=mode==Mode::Vehicle||mode==Mode::VehicleInteractive||mode==Mode::VehicleHeld;
            const bool stream=f.mode==Mode::VehicleInteractive||f.mode==Mode::VehicleHeld;
            // A cabin interaction may enter its native held-input animation
            // while the same physical F press is still down. Keep that hold;
            // returning to the cabin requires a new press before hold-to-exit.
            const bool carry=oldVehicle&&stream&&previous&&!latch;
            mode=f.mode;if(!carry){latch=f.held;previous=false;}armed=longPress=false;
        }
        last=f.tick;if(!f.held)latch=false;
        const bool held=f.held&&!latch;Output result{};
        if(f.mode!=Mode::Cabinet&&f.mode!=Mode::Vehicle){result.enter=held&&(f.mode==Mode::Ordinary||f.mode==Mode::VehicleInteractive||f.mode==Mode::VehicleHeld);result.leave=held&&(f.mode==Mode::Other||f.mode==Mode::HeldObject);}
        else {
            if(held&&!previous){started=f.tick;armed=true;longPress=false;}
            if(held&&armed&&f.tick-started>=HoldMilliseconds)longPress=true;
            // The threshold may fall between the final held sample and the
            // release. Classify elapsed physical hold time, not frame timing.
            const bool released=!held&&previous&&armed;
            const bool crossedOnRelease=released&&!longPress&&f.tick-started>=HoldMilliseconds;
            result.leave=(held&&longPress)||crossedOnRelease;
            if(released&&!longPress&&!crossedOnRelease)result.enter=true;
            if(!held){armed=false;longPress=false;}
        }
        previous=held;return result;
    }
};
}
