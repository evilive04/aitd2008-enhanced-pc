#pragma once
// DirectInput flags only; foreground/background ownership remains unchanged.
static constexpr unsigned SharedKeyboardFlags(bool sharing,bool verifiedKeyboard,unsigned flags){
    if(!sharing||!verifiedKeyboard)return flags;
    constexpr unsigned exclusive=1,nonexclusive=2,noWindowsKey=16;
    unsigned adjusted=flags&~noWindowsKey;
    if((flags&exclusive)&&!(flags&nonexclusive))adjusted=(adjusted&~exclusive)|nonexclusive;
    return adjusted;
}
