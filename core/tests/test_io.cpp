// core/tests/test_io.cpp —— 存档序列化往返（路线图 P1 状态往返；P3 切后台落盘前置验证）
#include <catch2/catch_test_macros.hpp>

#include <ttt/io.hpp>

using namespace ttt;

TEST_CASE("空对局序列化往返一致", "[io]") {
    const Game g;
    const std::string data = serialize(g.state());
    const auto back = deserialize(data);
    REQUIRE(back.has_value());
    REQUIRE(back->board == g.state().board);
    REQUIRE(back->current == g.state().current);
    REQUIRE(back->moves == g.state().moves);
    REQUIRE(back->status == Status::Playing);
    REQUIRE_FALSE(back->winLine.has_value());
}

TEST_CASE("对局中途序列化往返一致", "[io]") {
    Game g;
    g.applyMove(0);   // X
    g.applyMove(4);   // O
    g.applyMove(8);   // X

    const auto back = deserialize(serialize(g.state()));
    REQUIRE(back.has_value());
    REQUIRE(back->board == g.state().board);
    REQUIRE(back->current == g.state().current);   // 轮到 O
    REQUIRE(back->current == Player::O);
    REQUIRE(back->moves == 3);
}

TEST_CASE("获胜局面的状态由棋盘推导而非存盘", "[io]") {
    Game g;
    for (int c : {0, 3, 1, 4, 2}) g.applyMove(c);   // X 第一行获胜
    REQUIRE(g.status() == Status::XWins);

    const auto back = deserialize(serialize(g.state()));
    REQUIRE(back.has_value());
    REQUIRE(back->status == Status::XWins);          // 推导正确
    REQUIRE(back->winLine.has_value());
    REQUIRE(*back->winLine == Line{0, 1, 2});
}

TEST_CASE("平局局面往返后仍为平局", "[io]") {
    Game g;
    for (int c : {0, 1, 2, 4, 3, 5, 7, 6, 8}) g.applyMove(c);
    REQUIRE(g.status() == Status::Draw);

    const auto back = deserialize(serialize(g.state()));
    REQUIRE(back.has_value());
    REQUIRE(back->status == Status::Draw);
    REQUIRE_FALSE(back->winLine.has_value());
}

TEST_CASE("恢复后的状态可继续对局", "[io]") {
    Game g;
    g.applyMove(0);
    const auto back = deserialize(serialize(g.state()));
    REQUIRE(back.has_value());

    Game g2;
    g2.restore(*back);
    REQUIRE(g2.current() == Player::O);
    REQUIRE(g2.applyMove(4) == MoveResult::Accepted);   // 能继续走
    REQUIRE(g2.moveCount() == 2);
}

TEST_CASE("损坏或非法存档被安全拒绝", "[io][robust]") {
    REQUIRE_FALSE(deserialize("").has_value());
    REQUIRE_FALSE(deserialize("XXXX").has_value());                  // 魔数不符
    REQUIRE_FALSE(deserialize("TTT1").has_value());                  // 截断
    REQUIRE_FALSE(deserialize("TTT1,.........").has_value());        // 缺少玩家字段
    REQUIRE_FALSE(deserialize("TTT1,.........ZZ,5").has_value());    // 非法玩家
    REQUIRE_FALSE(deserialize("TTT1,.........,X,abc").has_value());  // 非数字走子数
    REQUIRE_FALSE(deserialize("TTT1,.........,X,99").has_value());   // 走子数越界
    REQUIRE_FALSE(deserialize("TTT1,.........,X,-1").has_value());   // 负数
}

TEST_CASE("未知字符按空格子处理", "[io][robust]") {
    const auto st = deserialize("TTT1,X........,O,1");
    REQUIRE(st.has_value());
    REQUIRE(st->board[0] == Cell::X);
    REQUIRE(st->board[1] == Cell::Empty);
    REQUIRE(st->moves == 1);
}
