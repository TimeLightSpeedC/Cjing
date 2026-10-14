#include "ttt/io.hpp"

#include <charconv>

namespace ttt {
namespace {

constexpr std::string_view kMagic = "TTT1";
constexpr char kSep = ',';

char cellChar(Cell c) noexcept {
    switch (c) {
        case Cell::X: return 'X';
        case Cell::O: return 'O';
        case Cell::Empty: break;
    }
    return '.';
}

Cell charCell(char ch) noexcept {
    if (ch == 'X') return Cell::X;
    if (ch == 'O') return Cell::O;
    return Cell::Empty;
}

// 状态由棋盘推导，不落盘（避免存档与棋盘不一致）
Status deriveStatus(const Board& b, std::optional<Line>& outLine) noexcept {
    if (const auto line = Game::findWin(b)) {
        outLine = line;
        return b[static_cast<std::size_t>((*line)[0])] == Cell::X ? Status::XWins : Status::OWins;
    }
    return Game::isFull(b) ? Status::Draw : Status::Playing;
}

}  // namespace

std::string serialize(const GameState& state) {
    std::string out;
    out.reserve(32);
    out.append(kMagic);
    out.push_back(kSep);
    for (const Cell c : state.board) out.push_back(cellChar(c));
    out.push_back(kSep);
    out.push_back(state.current == Player::X ? 'X' : 'O');
    out.push_back(kSep);
    out.append(std::to_string(state.moves));
    return out;
}

std::optional<GameState> deserialize(std::string_view data) {
    // 期望格式: TTT1,<9 格>,<当前玩家>,<走子数>
    if (data.size() < kMagic.size() + 1) return std::nullopt;
    if (data.substr(0, kMagic.size()) != kMagic) return std::nullopt;

    std::size_t pos = kMagic.size();
    if (data[pos] != kSep) return std::nullopt;
    ++pos;

    if (pos + kCellCount > data.size()) return std::nullopt;
    GameState st;
    for (int i = 0; i < kCellCount; ++i) {
        st.board[static_cast<std::size_t>(i)] = charCell(data[pos + static_cast<std::size_t>(i)]);
    }
    pos += kCellCount;

    if (pos + 1 >= data.size() || data[pos] != kSep) return std::nullopt;
    ++pos;
    const char who = data[pos];
    if (who != 'X' && who != 'O') return std::nullopt;
    st.current = (who == 'X') ? Player::X : Player::O;
    ++pos;

    if (pos >= data.size() || data[pos] != kSep) return std::nullopt;
    ++pos;
    int moves = 0;
    const char* first = data.data() + pos;
    const char* last  = data.data() + data.size();
    if (auto [ptr, ec] = std::from_chars(first, last, moves); ec != std::errc{}) return std::nullopt;
    if (moves < 0 || moves > kCellCount) return std::nullopt;

    st.moves  = moves;
    st.status = deriveStatus(st.board, st.winLine);
    return st;
}

}  // namespace ttt
