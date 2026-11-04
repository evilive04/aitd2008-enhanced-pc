#pragma once
#include <cstdint>
// Original bitmap proxy321 and source-bound authored marker pack. Runtime replaces
// only words0/4 with vtable/next. Proxy bitmap fills/UVs/geometry remain original.
// A mismatched/absent pack must keep the original GPS renderer authoritative.
namespace phone_map_content {
struct Descriptor{std::uint32_t id,bytes,tailFnv;};
static constexpr Descriptor descriptors[]={
 {321,140,0xc4f64fa2u},
 {531,140,0x804767b4u},
 {535,104,0xcacecc3au},
 {537,104,0x5c641d7au},
 {539,104,0xa49a2a52u},
 {542,104,0x237664acu},
 {546,140,0xf686be32u},
 {549,140,0x02d8c674u},
};
}
