#include "ttt/text.hpp"

#include <array>

namespace ttt {
namespace {

// 文案表：下标必须与 TextId 的枚举值一一对应。
//
// ⚠️ 顺序即契约：新增 TextId 时【必须】在相同位置插入文案，
//    否则所有后续条目都会错位（且编译器不会报错）。
//    core/tests/test_text.cpp 会检查每个条目非空，
//    但"语义错位"无法自动发现 —— 所以这里用带注释的显式顺序。
constexpr std::array<std::string_view, kTextCount> kTexts = {
    /* AppTitle        */ "井字棋 Tic-Tac-Toe",
    /* TurnX           */ "轮到 X",
    /* TurnO           */ "轮到 O",
    /* XWins           */ "X 获胜！",
    /* OWins           */ "O 获胜！",
    /* Draw            */ "平局",
    /* RestartHint     */ "按 R 再来一局",
    /* QuitHint        */ "按 ESC 退出",
    /* MenuStart       */ "开始对局",
    /* MenuDifficulty  */ "难度",
    /* MenuQuit        */ "退出",
    /* DifficultyEasy  */ "简单",
    /* DifficultyNormal*/ "普通",
    /* DifficultyHard  */ "困难",
    /* HintPlaying     */ "点击棋盘落子 · R 重开 · ESC 退出",
    /* HintOver        */ "按 R 再来一局 · ESC 退出",
};

}  // namespace

std::string_view textOf(TextId id) noexcept {
    const auto idx = static_cast<std::size_t>(id);
    if (idx >= kTextCount) return {};     // 非法 id -> 空串，调用方按无文案处理
    return kTexts[idx];
}

}  // namespace ttt
