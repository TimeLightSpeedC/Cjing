// core/include/ttt/types.hpp
// 基础值类型：无窗口、无像素、无 SDL 依赖。
#pragma once

#include <array>
#include <cstdint>

namespace ttt {

inline constexpr int kSize     = 3;                    // 棋盘 3x3
inline constexpr int kCellCount = kSize * kSize;       // 9

enum class Cell : std::uint8_t { Empty = 0, X = 1, O = 2 };

enum class Player : std::uint8_t { X = 0, O = 1 };

enum class Status : std::uint8_t { Playing = 0, XWins, OWins, Draw };

// 落子结果（P0-T9 契约：UI 只做查询与渲染，不得直接读写棋盘）
enum class MoveResult : std::uint8_t {
    Accepted = 0,   // 合法落子，对局继续
    Rejected,       // 非法：越界 / 已占用 / 终局后落子
    XWon,           // 该步导致 X 获胜
    OWon,           // 该步导致 O 获胜
    Drawn           // 该步导致平局
};

inline constexpr Player opponent(Player p) noexcept {
    return p == Player::X ? Player::O : Player::X;
}

inline constexpr Cell cellOf(Player p) noexcept {
    return p == Player::X ? Cell::X : Cell::O;
}

inline constexpr Status statusOf(Player p) noexcept {
    return p == Player::X ? Status::XWins : Status::OWins;
}

// 一条获胜连线：3 个格子索引
using Line = std::array<int, kSize>;
using Board = std::array<Cell, kCellCount>;

// 内部辅助：行列 <-> 索引
inline constexpr int indexOf(int row, int col) noexcept { return row * kSize + col; }
inline constexpr int rowOf(int index) noexcept { return index / kSize; }
inline constexpr int colOf(int index) noexcept { return index % kSize; }

inline constexpr bool isOnBoard(int index) noexcept { return index >= 0 && index < kCellCount; }

}  // namespace ttt
