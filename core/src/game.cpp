#include "ttt/game.hpp"

namespace ttt {
namespace {

// 8 条获胜线：3 行 + 3 列 + 2 对角线
constexpr Line kLines[8] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},   // 行
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},   // 列
    {0, 4, 8}, {2, 4, 6},              // 对角线
};

}  // namespace

std::optional<Line> Game::findWin(const Board& b) noexcept {
    for (const Line& line : kLines) {
        const Cell first = b[static_cast<std::size_t>(line[0])];
        if (first == Cell::Empty) continue;
        if (b[static_cast<std::size_t>(line[1])] == first &&
            b[static_cast<std::size_t>(line[2])] == first) {
            return line;
        }
    }
    return std::nullopt;
}

bool Game::isFull(const Board& b) noexcept {
    for (const Cell c : b) {
        if (c == Cell::Empty) return false;
    }
    return true;
}

void Game::reset() noexcept { state_ = GameState{}; }

MoveResult Game::applyMove(int index) noexcept {
    // 终局后不得继续落子
    if (state_.isOver())            return MoveResult::Rejected;
    // 越界
    if (!isOnBoard(index))          return MoveResult::Rejected;

    const auto pos = static_cast<std::size_t>(index);
    // 已占用
    if (state_.board[pos] != Cell::Empty) return MoveResult::Rejected;

    state_.board[pos] = cellOf(state_.current);
    ++state_.moves;

    if (const auto line = findWin(state_.board)) {
        state_.winLine = line;
        state_.status  = statusOf(state_.current);
        return state_.status == Status::XWins ? MoveResult::XWon : MoveResult::OWon;
    }

    if (isFull(state_.board)) {
        state_.status = Status::Draw;
        return MoveResult::Drawn;
    }

    state_.current = opponent(state_.current);
    return MoveResult::Accepted;
}

}  // namespace ttt
