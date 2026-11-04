#pragma once
#include <cmath>

namespace aim_range {
// Exact r29 camera query 46D970 casts (requested - 1) metres, but its
// successful return at 46DB6E multiplies the hit fraction by requested.
// Camera collision callers deliberately retain that padding. A weapon target
// needs the actual scene distance, rather than the padded camera distance.
constexpr float nativePadding=1.0f;
constexpr float maximum=100.0f;
constexpr float query=maximum+nativePadding;
inline float SceneDistance(float paddedResult) {
    if(!std::isfinite(paddedResult)||paddedResult<0||paddedResult>query)return -1.0f;
    return paddedResult*(maximum/query);
}
}
