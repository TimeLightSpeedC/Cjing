#pragma once

// =============================================================================
//  点击/双击判定（纯逻辑，无 SDL 依赖，可单元测试）
//
//  为什么放在 core：
//    移动端没有键盘，"双击提示行重开"是移动端唯一的重开入口。
//    这类判定规则应当与渲染/窗口系统无关，放进 core 才能被单测覆盖，
//    而不是埋在 app.cpp 里靠手点验证。
// =============================================================================

#include <cstdint>

namespace ttt {

// 双击的两个阈值（逻辑单位 / 毫秒）。
// 取值说明：
//   · 450ms  —— 比系统常见的 300ms 宽松一些，触摸屏上的第二次点击往往更慢；
//   · 48 逻辑单位 —— 约等于一行的字号量级，容得下手指抖动，
//     又不会把"点了两个不同位置"误判成双击。
inline constexpr std::uint64_t kDoubleTapMs = 450;
inline constexpr int kDoubleTapSlop = 48;

struct Tap {
    std::uint64_t timeMs{0};
    int x{0};
    int y{0};
    bool valid{false};   // 首次点击前为 false，避免与 (0,0) 误配
};

// 判断 first -> second 是否构成一次双击。
// 要求：两次都有效、时间不倒流、间隔不超过 kDoubleTapMs、位移不超过 kDoubleTapSlop。
[[nodiscard]] bool isDoubleTap(const Tap& first, const Tap& second) noexcept;

}  // namespace ttt
