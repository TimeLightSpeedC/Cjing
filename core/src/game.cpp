// =============================================================================
//  core/src/game.cpp —— 井字棋规则引擎（纯逻辑，零 SDL 依赖）
//
//  这个文件只有 60 多行，但它承载了整个游戏的"正确性"：
//    · 什么算合法落子
//    · 什么时候算赢 / 平局
//    · 落子后轮到谁
//  渲染、输入、存档全都不在这里 —— 它们只读取这里产生的 GameState 快照。
//
//  设计要点（读代码时对照着看）：
//    1. 所有函数都是 noexcept：这里不该有异常，失败用返回值表达
//    2. 棋盘用 std::array<Cell,9>：值语义，拷贝即快照
//    3. 判定函数是 static 纯函数：给定棋盘就得到结果，不依赖内部状态
//       —— 这样存档恢复时可以直接用棋盘重算出胜负（见 io.cpp）
// =============================================================================

#include "ttt/game.hpp"

namespace ttt {
namespace {

// -----------------------------------------------------------------------------
// 8 条获胜线：3 行 + 3 列 + 2 条对角线
//
// 为什么用"索引表"而不是写 8 个 if：
//   · 一遍循环就能查完，代码短且不容易漏（漏掉一条对角线是很经典的 bug）
//   · 返回的是 Line（三个索引），渲染层可以直接拿去做"获胜连线高亮"
//   · constexpr：编译期就放进只读数据段，运行期零开销
//
// 索引与棋盘的对应关系（行优先）：
//        0 | 1 | 2
//       ---+---+---
//        3 | 4 | 5
//       ---+---+---
//        6 | 7 | 8
// -----------------------------------------------------------------------------
constexpr Line kLines[8] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},   // 三行
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},   // 三列
    {0, 4, 8}, {2, 4, 6},              // 两条对角线
};

}  // namespace

// -----------------------------------------------------------------------------
// findWin —— 给定棋盘，返回第一条被同一方占满的线
//
// 返回 std::optional<Line>：
//   · 有胜者 → 返回那三个格子
//   · 无胜者 → std::nullopt（而不是返回 {-1,-1,-1} 这种"魔法值"）
//   这比"返回 bool + 出参"更清晰：调用处 `if (auto line = findWin(b))` 直接可用。
//
// 这是 static 纯函数（不依赖 state_）：存档恢复时要用它重建胜负状态。
// -----------------------------------------------------------------------------
std::optional<Line> Game::findWin(const Board& b) noexcept {
    for (const Line& line : kLines) {
        const Cell first = b[static_cast<std::size_t>(line[0])];

        // 第一个格子是空的，这条线就不可能被占满 —— 直接跳过。
        // 没有这行也能工作（空 == 空 会误判），所以它是【正确性必需】的：
        // 否则三个空格会被认为"三方占满"而判胜。
        if (first == Cell::Empty) continue;

        // 三个格子都是同一方 → 获胜
        if (b[static_cast<std::size_t>(line[1])] == first &&
            b[static_cast<std::size_t>(line[2])] == first) {
            return line;
        }
    }
    return std::nullopt;
}

// -----------------------------------------------------------------------------
// isFull —— 棋盘是否已无空位（平局判定的前提）
//
// 只要发现一个空格就立刻返回 false（短路），
// 最坏情况 9 次比较，实际通常更少。
// -----------------------------------------------------------------------------
bool Game::isFull(const Board& b) noexcept {
    for (const Cell c : b) {
        if (c == Cell::Empty) return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// reset —— 重开一局
//
// `GameState{}` 是【值初始化】：所有成员被清零/置默认值。
//   · board 全是 Cell::Empty（枚举值 0）
//   · current = Player::X
//   · status = Status::Playing
//   · winLine = std::nullopt
//   · moves = 0
// 一行代码胜过手写 5 行赋值，而且加了新字段也不会忘记初始化。
// -----------------------------------------------------------------------------
void Game::reset() noexcept { state_ = GameState{}; }

// -----------------------------------------------------------------------------
// applyMove —— 核心：推进一次落子
//
// ⚠️ 下面这段检查的【顺序】不是随意的，每一行都有理由：
//
//   ① 终局检查必须最先
//      如果先落子再检查终局，就会出现"赢了还能继续下"的局面。
//      测试用例 "终局后继续落子被拒绝" 锁住了这条。
//
//   ② 越界检查必须在访问 board[index] 之前
//      否则 -1 或 9 会造成越界读写（未定义行为，可能崩溃也可能"看起来正常"）。
//
//   ③ 占用检查必须在写入之前
//      同格重复落子会覆盖对方的棋子 —— 这是最常见的作弊路径。
//
//   ④ 写入棋盘后立刻判定胜负，再判定平局，最后才换手
//      如果先换手再判胜负，"获胜的是谁"就会算错（会算成下一位玩家）。
//
// 返回值 MoveResult 同时表达"是否成功"和"结果是什么"，
// 调用方（UI）据此决定要不要播放音效 / 显示胜利文字。
// -----------------------------------------------------------------------------
MoveResult Game::applyMove(int index) noexcept {
    // ① 终局后不得继续落子
    if (state_.isOver())            return MoveResult::Rejected;

    // ② 越界（合法范围 0..8）
    if (!isOnBoard(index))          return MoveResult::Rejected;

    const auto pos = static_cast<std::size_t>(index);

    // ③ 该格已被占用
    if (state_.board[pos] != Cell::Empty) return MoveResult::Rejected;

    // ④ 落子：把当前玩家的符号写进棋盘，并累加手数
    state_.board[pos] = cellOf(state_.current);
    ++state_.moves;

    // ⑤ 判定胜负（优先级高于平局：第 9 手可能既是最后一格也是获胜）
    if (const auto line = findWin(state_.board)) {
        state_.winLine = line;                        // 供渲染层画高亮连线
        state_.status  = statusOf(state_.current);    // XWins / OWins
        return state_.status == Status::XWins ? MoveResult::XWon : MoveResult::OWon;
    }

    // ⑥ 没赢但棋盘满了 → 平局
    if (isFull(state_.board)) {
        state_.status = Status::Draw;
        return MoveResult::Drawn;
    }

    // ⑦ 都不是 → 换手，对局继续
    state_.current = opponent(state_.current);
    return MoveResult::Accepted;
}

}  // namespace ttt
