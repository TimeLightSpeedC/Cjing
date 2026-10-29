// =============================================================================
//  core/include/ttt/text.hpp —— 文案表查询（TextId -> 用户可见字符串）
//
//  ▍为什么文案表放在 core，而不是渲染层
//    1. 它是【纯数据】，与平台无关：桌面、Android、鸿蒙显示的是同一句话
//    2. 它可以被单元测试覆盖（见 core/tests/test_text.cpp）：
//       能断言"每个 TextId 都有非空文案"，这在渲染层很难测
//    3. 将来做多语言时，只需在这里按语言选表，所有平台自动生效
//
//  ▍为什么返回 std::string_view 而不是 std::string
//    文案是编译期常量，不需要拷贝。string_view 零分配，适合每帧查询。
// =============================================================================
#pragma once

#include <cstddef>
#include <string_view>

#include "ttt/text_id.hpp"

namespace ttt {

// 文案条目总数（不含 _Count 哨兵）。用于遍历与断言。
inline constexpr std::size_t kTextCount = static_cast<std::size_t>(TextId::_Count);

// 查询文案。id 非法（>= _Count）时返回空字符串，绝不越界。
[[nodiscard]] std::string_view textOf(TextId id) noexcept;

}  // namespace ttt
