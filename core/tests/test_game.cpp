// core/tests/test_game.cpp —— 胜负线全覆盖（路线图 P1-T13..T21）
#include <catch2/catch_test_macros.hpp>

#include <ttt/game.hpp>

using namespace ttt;

namespace {
// 依次落子到指定格子，返回最后一次的 MoveResult
MoveResult playAll(Game& g, std::initializer_list<int> cells) {
    MoveResult last = MoveResult::Rejected;
    for (int c : cells) last = g.applyMove(c);
    return last;
}
}  // namespace

TEST_CASE("空棋盘状态为 Playing", "[game]") {
    Game g;
    REQUIRE(g.status() == Status::Playing);
    REQUIRE(g.current() == Player::X);
    REQUIRE(g.moveCount() == 0);
    REQUIRE_FALSE(g.winningLine().has_value());
    for (int i = 0; i < kCellCount; ++i) REQUIRE(g.state().at(i) == Cell::Empty);
}

// ---- 8 条获胜线逐条覆盖（路线图 P1-T14..T20）----

TEST_CASE("第一行三连 X 获胜", "[game][win]") {
    Game g;
    REQUIRE(playAll(g, {0, 3, 1, 4, 2}) == MoveResult::XWon);
    REQUIRE(g.status() == Status::XWins);
    REQUIRE(g.winningLine().has_value());
    REQUIRE(*g.winningLine() == Line{0, 1, 2});
}

TEST_CASE("第二行三连 O 获胜", "[game][win]") {
    Game g;
    // X 落 0,1(被占后改) —— 构造 O 在 3,4,5 连胜
    REQUIRE(playAll(g, {0, 3, 1, 4, 8, 5}) == MoveResult::OWon);
    REQUIRE(g.status() == Status::OWins);
    REQUIRE(*g.winningLine() == Line{3, 4, 5});
}

TEST_CASE("第三行三连 X 获胜", "[game][win]") {
    Game g;
    REQUIRE(playAll(g, {6, 0, 7, 1, 8}) == MoveResult::XWon);
    REQUIRE(*g.winningLine() == Line{6, 7, 8});
}

TEST_CASE("第一列三连 O 获胜", "[game][win]") {
    Game g;
    REQUIRE(playAll(g, {1, 0, 2, 3, 8, 6}) == MoveResult::OWon);
    REQUIRE(*g.winningLine() == Line{0, 3, 6});
}

TEST_CASE("第二列三连 X 获胜", "[game][win]") {
    Game g;
    REQUIRE(playAll(g, {1, 0, 4, 3, 7}) == MoveResult::XWon);
    REQUIRE(*g.winningLine() == Line{1, 4, 7});
}

TEST_CASE("第三列三连 O 获胜", "[game][win]") {
    Game g;
    REQUIRE(playAll(g, {0, 2, 1, 5, 3, 8}) == MoveResult::OWon);
    REQUIRE(*g.winningLine() == Line{2, 5, 8});
}

TEST_CASE("主对角线三连获胜", "[game][win]") {
    Game g;
    REQUIRE(playAll(g, {0, 1, 4, 2, 8}) == MoveResult::XWon);
    REQUIRE(*g.winningLine() == Line{0, 4, 8});
}

TEST_CASE("副对角线三连获胜", "[game][win]") {
    Game g;
    REQUIRE(playAll(g, {2, 0, 4, 1, 6}) == MoveResult::XWon);
    REQUIRE(*g.winningLine() == Line{2, 4, 6});
}

// ---- 平局 ----
TEST_CASE("满盘无胜者判平局", "[game][draw]") {
    Game g;
    // X O X / X O O / O X X
    REQUIRE(playAll(g, {0, 1, 2, 4, 3, 5, 7, 6, 8}) == MoveResult::Drawn);
    REQUIRE(g.status() == Status::Draw);
    REQUIRE(g.moveCount() == kCellCount);
    REQUIRE_FALSE(g.winningLine().has_value());
}

// ---- 非法落子（路线图 P1-T21）----
TEST_CASE("越界落子被拒绝且不改变状态", "[game][invalid]") {
    Game g;
    REQUIRE(g.applyMove(-1) == MoveResult::Rejected);
    REQUIRE(g.applyMove(kCellCount) == MoveResult::Rejected);
    REQUIRE(g.applyMove(999) == MoveResult::Rejected);
    REQUIRE(g.moveCount() == 0);
    REQUIRE(g.current() == Player::X);   // 回合不得被消耗
}

TEST_CASE("重复落子被拒绝且不切换回合", "[game][invalid]") {
    Game g;
    REQUIRE(g.applyMove(4) == MoveResult::Accepted);
    REQUIRE(g.current() == Player::O);
    REQUIRE(g.applyMove(4) == MoveResult::Rejected);   // 同一格
    REQUIRE(g.moveCount() == 1);
    REQUIRE(g.current() == Player::O);                 // 回合未被错误切换
}

TEST_CASE("终局后继续落子被拒绝", "[game][invalid]") {
    Game g;
    REQUIRE(playAll(g, {0, 3, 1, 4, 2}) == MoveResult::XWon);
    const auto before = g.snapshot();
    REQUIRE(g.applyMove(8) == MoveResult::Rejected);
    REQUIRE(g.moveCount() == before.moves);
    REQUIRE(g.state().at(8) == Cell::Empty);   // 棋盘不得被污染
}

TEST_CASE("reset 清空全部状态", "[game][reset]") {
    Game g;
    playAll(g, {0, 3, 1, 4, 2});
    REQUIRE(g.status() == Status::XWins);
    g.reset();
    REQUIRE(g.status() == Status::Playing);
    REQUIRE(g.current() == Player::X);
    REQUIRE(g.moveCount() == 0);
    REQUIRE_FALSE(g.winningLine().has_value());
    for (int i = 0; i < kCellCount; ++i) REQUIRE(g.state().at(i) == Cell::Empty);
}

TEST_CASE("回合在 X 与 O 之间交替", "[game]") {
    Game g;
    REQUIRE(g.current() == Player::X);
    g.applyMove(0); REQUIRE(g.current() == Player::O);
    g.applyMove(1); REQUIRE(g.current() == Player::X);
    g.applyMove(2); REQUIRE(g.current() == Player::O);
}
