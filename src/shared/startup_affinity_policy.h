#pragma once
#include <cstddef>
#include <cstdint>

// Keep the original game's two-processor startup limit while preferring
// separate physical cores over two SMT siblings on one physical core.
namespace startup_affinity {
struct Core {
    std::uintptr_t mask{};
    unsigned efficiency{};
};
inline std::uintptr_t First(std::uintptr_t mask) noexcept {
    return mask & (~mask + 1);
}
inline std::uintptr_t Select(std::uintptr_t allowed,const Core* cores,std::size_t count) noexcept {
    std::uintptr_t seen=0;
    if(cores)for(std::size_t i=0;i<count;++i){
        // Physical-core masks must be disjoint. Truncated/inconsistent OS
        // topology (including WOW64 edge cases) uses the old safe fallback.
        if(seen&cores[i].mask){cores=nullptr;count=0;break;}
        seen|=cores[i].mask;
    }
    std::uintptr_t selected=0;
    for(unsigned n=0;n<2;++n){
        std::uintptr_t candidate=0;unsigned best=0;
        if(cores)for(std::size_t i=0;i<count;++i){
            // A mask overlapping the selection belongs to an already used core.
            const auto available=cores[i].mask&allowed;
            if(!available||(cores[i].mask&selected))continue;
            const auto first=First(available);
            if(!candidate||cores[i].efficiency>best||
               (cores[i].efficiency==best&&first<candidate)){
                candidate=first;best=cores[i].efficiency;
            }
        }
        // Missing topology or one available physical core: retain the original
        // bounded logical-processor fallback, never widen the allowed mask.
        if(!candidate)candidate=First(allowed&~selected);
        if(!candidate)break;
        selected|=candidate;
    }
    return selected;
}
}
