// Exact r29 native shadow size operands. Counts and renderer code remain native.
#pragma once
#include <cstdint>
namespace native_shadows {
struct Site { std::uint32_t address; unsigned char opcode; std::uint32_t original; };
static constexpr Site sites[]={
    {0x87e9c7,0xb8,1024},
    {0x87f43a,0xb8,1024},
    {0x87f560,0xb8,1024},
    {0x87f57d,0xb8,512},
    {0x87f59a,0xb8,256},
    {0x87f5b7,0xb8,128},
    {0x87f5e9,0x68,1024},
    {0x87f5ee,0x68,1024},
    {0x8f3d83,0xb8,1024},
    {0x8f3d9b,0xb8,512},
    {0x8f3dd3,0xb8,256},
    // Separate gPSShadowEyeBias producer. Missing these sent2048 into the
    // unrecognized-tier fallback, while the Z-bias producer above stayed valid.
    {0x8f3ee2,0xb8,1024},
    {0x8f3ef5,0xb8,512},
    {0x8f3f19,0xb8,256},
};
static constexpr unsigned siteCount=sizeof(sites)/sizeof(sites[0]);
static constexpr std::uint32_t tableAddress=0xc4c894;
static constexpr std::uint32_t sizes[]={1024,512,256,128};
}
