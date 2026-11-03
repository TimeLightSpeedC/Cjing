#include "ttt/tap.hpp"

#include <cstdlib>

namespace ttt {

bool isDoubleTap(const Tap& first, const Tap& second) noexcept {
    if (!first.valid || !second.valid) return false;

    // 时间倒流（时钟回绕/异常）时不做判定，宁可漏判也不误判
    if (second.timeMs < first.timeMs) return false;
    if ((second.timeMs - first.timeMs) > kDoubleTapMs) return false;

    return std::abs(second.x - first.x) <= kDoubleTapSlop &&
           std::abs(second.y - first.y) <= kDoubleTapSlop;
}

}  // namespace ttt
