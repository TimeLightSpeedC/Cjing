// core/include/ttt/text_id.hpp
// 文案间接层：代码中不得硬编码用户可见字符串。
// 第一阶段只需中文表（见 platform/ 的 TextTable 实现）。
#pragma once

#include <cstdint>

namespace ttt {

enum class TextId : std::uint8_t {
    AppTitle = 0,
    TurnX,
    TurnO,
    XWins,
    OWins,
    Draw,
    RestartHint,
    QuitHint,
    MenuStart,
    MenuDifficulty,
    MenuQuit,
    DifficultyEasy,
    DifficultyNormal,
    DifficultyHard,
    HintPlaying,      // 对局中的提示行（落子 / 重开 / 退出）
    HintOver,         // 终局后的提示行（重开 / 退出）

    // 触屏平台专用（没有实体键盘，提示里不能出现 R / ESC）
    HintPlayingTouch, // 对局中：说明如何重开
    HintOverTouch,    // 终局后：说明如何再来一局
    _Count  // 计数哨兵，勿作 UI 使用
};

}  // namespace ttt
