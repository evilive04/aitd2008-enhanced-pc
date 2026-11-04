// Original mainmenu.vmf release-handler fingerprints; no game action bytes.
#pragma once
namespace recap_native_actions {
// Native91fdd0 parser replaces on-disk tag7 with this DefineButton vtable.
static constexpr unsigned ButtonVtable=0x00ccfd04;
struct Spec {unsigned id, bytes, releaseBytes, fnv;};
static constexpr Spec specs[]={
 {621,1008,820,0x447bbe6bu},
 {622,1452,1264,0x206f3f01u},
 {623,688,508,0x43145b83u},
 {624,1308,1120,0x6afd47f9u},
 {625,996,816,0xc3836c05u},
};
}
