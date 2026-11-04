// Native Flash rich-text labels. No game assets or font glyphs are embedded.
#pragma once
#include <cstddef>
#include <cstring>
namespace pc_prompts {
inline bool Equal(const char* a,const char* b) {
    return a&&b&&std::strcmp(a,b)==0;
}
inline const char* TutorialLabel(const char* command,unsigned long long message,bool modernControls){
    // GAMETIPS.LNG: TXTID_TIP_RECEPTION_HALL_FLASHLIGHT_ON. Its original
    // template uses ACTION_ENTER; ordinary interactions must still show F.
    return modernControls&&message==0xeeebe447c0ae3c4eULL&&Equal(command,"ACTION_ENTER")?"Q":nullptr;
}
inline const char* FixedLabel(const char* command,bool modernControls) {
    if(modernControls&&(Equal(command,"ACTION_ENTER")||Equal(command,"ACTION_LEAVE")))return "F";
    if(modernControls&&(Equal(command,"VCL_LEAVE")||Equal(command,"VCL_EJECT")))return "Hold F";
    return nullptr;
}
inline char* AppendLabel(char* output,std::size_t remaining,const char* label) {
    if(!output||!remaining)return output;
    constexpr char prefix[]="#color:F2E6C9FF#[";
    constexpr char suffix[]="]#nocolor#";
    const std::size_t labelBytes=label?std::strlen(label):0;
    if(!labelBytes||labelBytes>24||remaining<sizeof(prefix)-1+labelBytes+sizeof(suffix)) {
        output[0]=0;return output;
    }
    for(std::size_t n=0;n<labelBytes;++n)if(label[n]<' '||label[n]>'~'||label[n]=='#'||label[n]=='%') {
        output[0]=0;return output;
    }
    std::memcpy(output,prefix,sizeof(prefix)-1);
    std::memcpy(output+sizeof(prefix)-1,label,labelBytes);
    std::memcpy(output+sizeof(prefix)-1+labelBytes,suffix,sizeof(suffix));
    return output+sizeof(prefix)-1+labelBytes+sizeof(suffix)-1;
}
inline void RecolorNativeLabel(char* output,std::size_t bytes) {
    constexpr char original[]="#color:A6440DFF#[";
    constexpr char updated[]="#color:F2E6C9FF#[";
    if(output&&bytes>=sizeof(original)-1&&std::memcmp(output,original,sizeof(original)-1)==0)
        std::memcpy(output,updated,sizeof(updated)-1);
}
}
