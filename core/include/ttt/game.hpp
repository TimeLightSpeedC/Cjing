// core/include/ttt/game.hpp
// 对局状态机：纯逻辑，零 SDL 依赖。UI 只通过本接口查询与推进。
#pragma once

#include <optional>

#include "ttt/types.hpp"

namespace ttt {

// 对外暴露的对局快照（值语义，可安全跨线程传递与序列化）
struct GameState {
    Board       board{};
    Player      current = Player::X;
    Status      status  = Status::Playing;
    std::optional<Line> winLine{};
    int         moves   = 0;

    [[nodiscard]] bool isOver() const noexcept { return status != Status::Playing; }
    [[nodiscard]] Cell at(int index) const noexcept {
        return isOnBoard(index) ? board[static_cast<std::size_t>(index)] : Cell::Empty;
    }
};

class Game {
public:
    Game() = default;
    Game(const Game&) = default;
    Game& operator=(const Game&) = default;

    // 推进一次落子。非法输入不改变状态，返回 MoveResult::Rejected。
    MoveResult applyMove(int index) noexcept;

    // 重开一局
    void reset() noexcept;

    [[nodiscard]] const GameState& state() const noexcept { return state_; }
    [[nodiscard]] Status status() const noexcept { return state_.status; }
    [[nodiscard]] Player current() const noexcept { return state_.current; }
    [[nodiscard]] int moveCount() const noexcept { return state_.moves; }

    // 供 UI 高亮获胜线；无胜者时返回 nullopt
    [[nodiscard]] std::optional<Line> winningLine() const noexcept { return state_.winLine; }

    // 存档往返（P0-T9 契约；P1 由单元测试覆盖）
    [[nodiscard]] GameState snapshot() const noexcept { return state_; }
    void restore(const GameState& s) noexcept { state_ = s; }

    // 查询判定（供 AI 与测试使用，纯函数）
    [[nodiscard]] static std::optional<Line> findWin(const Board& b) noexcept;
    [[nodiscard]] static bool isFull(const Board& b) noexcept;

private:
    GameState state_{};
};

}  // namespace ttt
