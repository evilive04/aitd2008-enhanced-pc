#pragma once
// Retain a physical E release until the authentic HLBO/HLLU throw-ready state.
// No engine call or state write. Native bridge must supply revalidated ownership/identity.
namespace bottle_throw {
struct Identity {
    unsigned hero{},item{},low{},high{},handler{};
    bool operator==(const Identity& b)const{return hero==b.hero&&item==b.item&&low==b.low&&high==b.high&&handler==b.handler;}
};
struct Frame { Identity identity;unsigned tick{};bool eligible{},e{},ready{},cancel{}; };
struct Output {bool overrideActions{},prepare{},launch{};};
struct Queue {
    static constexpr unsigned Deadline=1500,MaximumGap=250;
    Identity identity{};unsigned last{},releasedAt{};
    bool active{},latched{},previous{},armed{},pending{};
    void Clear(){*this={};}
    Output Step(const Frame& f){
        if(!f.eligible||!f.identity.hero||!f.identity.item||!f.identity.handler){Clear();latched=f.e;return {};}
        const bool fresh=!active||!(identity==f.identity)||f.tick-last>MaximumGap;
        if(fresh){const bool hold=latched||f.e;Clear();active=true;identity=f.identity;latched=hold;}
        last=f.tick;
        if(f.cancel){latched=f.e;previous=armed=pending=false;releasedAt=0;return {true,false,false};}
        if(!f.e)latched=false;
        const bool e=f.e&&!latched;
        if(e&&!previous){armed=true;pending=false;}
        if(!e&&previous&&armed){releasedAt=f.tick;pending=true;armed=false;}
        if(pending&&f.tick-releasedAt>Deadline)pending=false;
        Output out{true,e||pending,false};
        if(pending&&f.ready){out.launch=true;out.prepare=true;pending=false;}
        previous=e;return out;
    }
};
}
