// =============================================================================
//  core/tests/test_text.cpp —— 文案表完整性
//
//  这个测试很小，但能挡住一类很隐蔽的问题：
//  新增 TextId 却忘了在文案表里加条目 —— 运行时会显示空白，
//  而且在 UI 上很难一眼看出是"没文案"还是"渲染失败"。
// =============================================================================
#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

#include "ttt/text.hpp"

using namespace ttt;

TEST_CASE("文案表：每个 TextId 都有非空文案", "[text]") {
    for (std::size_t i = 0; i < kTextCount; ++i) {
        const auto id = static_cast<TextId>(i);
        const std::string_view t = textOf(id);
        INFO("TextId 下标 = " << i);
        CHECK_FALSE(t.empty());
    }
}

TEST_CASE("文案表：条目内容不重复（避免复制粘贴错位）", "[text]") {
    std::set<std::string> seen;
    for (std::size_t i = 0; i < kTextCount; ++i) {
        const std::string t{textOf(static_cast<TextId>(i))};
        INFO("重复文案: " << t);
        CHECK(seen.insert(t).second);      // 第一次插入成功
    }
}

TEST_CASE("文案表：越界 id 返回空串而不是崩溃", "[text]") {
    CHECK(textOf(TextId::_Count).empty());
    CHECK(textOf(static_cast<TextId>(200)).empty());
}
