// core/tests/test_layout.cpp —— 布局：正方形、居中、安全区注入、坐标映射
#include <catch2/catch_test_macros.hpp>

#include <ttt/layout.hpp>

using namespace ttt;

TEST_CASE("无安全区时棋盘为正方形且居中", "[layout]") {
    const Layout L = Layout::compute(900, 0, 0, 0, 0);
    REQUIRE(L.boardRect().w > 0);
    REQUIRE(L.boardRect().w == L.boardRect().h);          // 正方形
    REQUIRE(L.cellSize() == L.boardRect().w / kSize);

    // 居中：左右留白相等
    const int leftGap  = L.boardRect().x;
    const int rightGap = 900 - (L.boardRect().x + L.boardRect().w);
    REQUIRE(leftGap == rightGap);
    REQUIRE(L.boardRect().y == 900 - (L.boardRect().y + L.boardRect().h));
}

TEST_CASE("非正方形窗口下棋盘仍是正方形", "[layout]") {
    // 逻辑空间恒为正方形；此处用安全区制造不对称可用区域
    const Layout L = Layout::compute(900, 100, 0, 0, 0);
    REQUIRE(L.boardRect().w == L.boardRect().h);
    REQUIRE(L.boardRect().y >= 100);   // 不侵入顶部安全区
}

TEST_CASE("安全区把棋盘压小且不越界", "[layout]") {
    const Layout none = Layout::compute(900, 0, 0, 0, 0);
    const Layout safe = Layout::compute(900, 60, 60, 60, 60);
    REQUIRE(safe.boardRect().w < none.boardRect().w);
    REQUIRE(safe.boardRect().x >= 60);
    REQUIRE(safe.boardRect().y >= 60);
    REQUIRE(safe.boardRect().x + safe.boardRect().w <= 900 - 60);
    REQUIRE(safe.boardRect().y + safe.boardRect().h <= 900 - 60);
}

TEST_CASE("四角与中心映射到正确格子", "[layout]") {
    const Layout L = Layout::compute(900, 0, 0, 0, 0);
    const Recti& r = L.boardRect();

    // 每格中心必须映射回自身索引
    for (int i = 0; i < kCellCount; ++i) {
        int cx = 0, cy = 0;
        REQUIRE(L.cellCenter(i, cx, cy));
        REQUIRE(cx >= r.x); REQUIRE(cx < r.x + r.w);
        REQUIRE(cy >= r.y); REQUIRE(cy < r.y + r.h);
        REQUIRE(L.cellAt(cx, cy) == i);
    }
}

TEST_CASE("棋盘外与非法输入返回 -1", "[layout]") {
    const Layout L = Layout::compute(900, 0, 0, 0, 0);
    REQUIRE(L.cellAt(-1, -1) == -1);
    REQUIRE(L.cellAt(2000, 2000) == -1);
    // 左上角留白区（棋盘之外）
    if (L.boardRect().x > 0) REQUIRE(L.cellAt(0, 0) == -1);
}

TEST_CASE("网格线落在棋盘范围内", "[layout]") {
    const Layout L = Layout::compute(900, 0, 0, 0, 0);
    for (int i = 1; i < kSize; ++i) {
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        REQUIRE(L.gridLine(i, /*vertical=*/true, x1, y1, x2, y2));
        REQUIRE(x1 == x2);                                   // 竖线
        REQUIRE(y1 == L.boardRect().y);
        REQUIRE(y2 == L.boardRect().y + L.boardRect().h);

        REQUIRE(L.gridLine(i, /*vertical=*/false, x1, y1, x2, y2));
        REQUIRE(y1 == y2);                                   // 横线
        REQUIRE(x1 == L.boardRect().x);
        REQUIRE(x2 == L.boardRect().x + L.boardRect().w);
    }
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    REQUIRE_FALSE(L.gridLine(0, true, x1, y1, x2, y2));      // i 非法
    REQUIRE_FALSE(L.gridLine(kSize, true, x1, y1, x2, y2));
}

TEST_CASE("退化输入不崩溃", "[layout][robust]") {
    const Layout zero = Layout::compute(0, 0, 0, 0, 0);
    REQUIRE(zero.cellSize() == 0);
    REQUIRE(zero.cellAt(0, 0) == -1);
    int a = 0, b = 0;
    REQUIRE_FALSE(zero.cellCenter(0, a, b));

    // 安全区大于逻辑尺寸
    const Layout bad = Layout::compute(900, 500, 500, 500, 500);
    REQUIRE(bad.cellSize() >= 0);   // 不得为负
}
