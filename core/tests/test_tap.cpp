// core/tests/test_tap.cpp
// 双击判定的边界：时间窗口、位移容差、无效点击、时间倒流。

#include <catch2/catch_test_macros.hpp>

#include <ttt/tap.hpp>

using ttt::isDoubleTap;
using ttt::kDoubleTapMs;
using ttt::kDoubleTapSlop;
using ttt::Tap;

namespace {

Tap at(std::uint64_t t, int x, int y) { return Tap{t, x, y, true}; }

}  // namespace

TEST_CASE("首次点击不与空状态构成双击", "[tap]") {
    // lastTap_ 默认 invalid：若忘记检查，会把第一次点击误判成双击
    CHECK_FALSE(isDoubleTap(Tap{}, at(100, 10, 10)));
}

TEST_CASE("时间窗口内且位移小，构成双击", "[tap]") {
    CHECK(isDoubleTap(at(1000, 100, 100), at(1000 + 200, 110, 105)));
}

TEST_CASE("超出时间窗口不算双击", "[tap]") {
    CHECK(isDoubleTap(at(1000, 0, 0), at(1000 + kDoubleTapMs, 0, 0)));
    CHECK_FALSE(isDoubleTap(at(1000, 0, 0), at(1000 + kDoubleTapMs + 1, 0, 0)));
}

TEST_CASE("位移超过容差不算双击", "[tap]") {
    CHECK(isDoubleTap(at(1000, 0, 0), at(1100, kDoubleTapSlop, 0)));
    CHECK_FALSE(isDoubleTap(at(1000, 0, 0), at(1100, kDoubleTapSlop + 1, 0)));
    CHECK_FALSE(isDoubleTap(at(1000, 0, 0), at(1100, 0, -(kDoubleTapSlop + 1))));
}

TEST_CASE("时间倒流不做判定", "[tap]") {
    // 时钟回绕或事件乱序时，宁可漏判也不要误判
    CHECK_FALSE(isDoubleTap(at(2000, 0, 0), at(1000, 0, 0)));
}
