// core/tests/test_layout.cpp —— 布局：正方形、居中、安全区注入、坐标映射
#include <catch2/catch_test_macros.hpp>

#include <ttt/layout.hpp>

using namespace ttt;

TEST_CASE("无安全区时棋盘为正方形且居中", "[layout]") {
    const Layout L = Layout::compute(900, 900, 0, 0, 0, 0);
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
    const Layout L = Layout::compute(900, 900, 100, 0, 0, 0);
    REQUIRE(L.boardRect().w == L.boardRect().h);
    REQUIRE(L.boardRect().y >= 100);   // 不侵入顶部安全区
}

TEST_CASE("安全区把棋盘压小且不越界", "[layout]") {
    const Layout none = Layout::compute(900, 900, 0, 0, 0, 0);
    const Layout safe = Layout::compute(900, 900, 60, 60, 60, 60);
    REQUIRE(safe.boardRect().w < none.boardRect().w);
    REQUIRE(safe.boardRect().x >= 60);
    REQUIRE(safe.boardRect().y >= 60);
    REQUIRE(safe.boardRect().x + safe.boardRect().w <= 900 - 60);
    REQUIRE(safe.boardRect().y + safe.boardRect().h <= 900 - 60);
}

TEST_CASE("四角与中心映射到正确格子", "[layout]") {
    const Layout L = Layout::compute(900, 900, 0, 0, 0, 0);
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
    const Layout L = Layout::compute(900, 900, 0, 0, 0, 0);
    REQUIRE(L.cellAt(-1, -1) == -1);
    REQUIRE(L.cellAt(2000, 2000) == -1);
    // 左上角留白区（棋盘之外）
    if (L.boardRect().x > 0) REQUIRE(L.cellAt(0, 0) == -1);
}

TEST_CASE("网格线落在棋盘范围内", "[layout]") {
    const Layout L = Layout::compute(900, 900, 0, 0, 0, 0);
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
    const Layout zero = Layout::compute(0, 0, 0, 0, 0, 0);
    REQUIRE(zero.cellSize() == 0);
    REQUIRE(zero.cellAt(0, 0) == -1);
    int a = 0, b = 0;
    REQUIRE_FALSE(zero.cellCenter(0, a, b));

    // 安全区大于逻辑尺寸
    const Layout bad = Layout::compute(900, 900, 500, 500, 500, 500);
    REQUIRE(bad.cellSize() >= 0);   // 不得为负
}

// -----------------------------------------------------------------------------
// HUD 定位
//
// 这组断言的价值：它锁住"状态区两行位置"的公式。
// 之前正式程序与自测截图各写一套（12%/50% vs 1/3、2/3），
// 导致 CI 截图与真机画面间距不一致 —— 人眼才能发现，自动测试抓不到。
// 现在两边都调用 Layout::hudLines()，这里是它的契约。
// -----------------------------------------------------------------------------
TEST_CASE("HUD 定位：两行都在棋盘上方的预留带内且彼此紧邻", "[layout]") {
    const ttt::Layout l = ttt::Layout::compute(900, 900, 0, 0, 0, 0, 120);
    REQUIRE(l.boardRect().y > 0);

    const ttt::Layout::HudLines h = l.hudLines();
    CHECK(h.hintY > h.statusY);
    CHECK(h.hintY < l.boardRect().y);          // 必须在棋盘上方，否则会压住棋格
    CHECK(h.statusY >= 0);

    // 关键约束：两行之间的间距应在一行文字的量级，
    // 而不是随画布高度被拉开（旧实现按 board.y 的百分比定位，手机上会拉开 200+）
    CHECK(h.hintY - h.statusY <= 80);
    // HUD 要贴着棋盘，不能飘到屏幕顶端
    CHECK(l.boardRect().y - h.hintY <= 140);

    // 命中带覆盖提示行
    CHECK(h.hintBand.y <= h.hintY);
    CHECK(h.hintBand.y + h.hintBand.h >= h.hintY);
}

TEST_CASE("超长画布（手机 900x1950）HUD 不被拉散", "[layout]") {
    // 复现真机问题：逻辑画布 900x1951，棋盘 792，旧实现 board.y=621，
    // HUD 按百分比定位后两行相距 236 单位，肉眼可见"间距过大"。
    const ttt::Layout l = ttt::Layout::compute(900, 1951, 0, 0, 0, 0, 120);
    REQUIRE(l.cellSize() > 0);

    const ttt::Layout::HudLines h = l.hudLines();
    CHECK(h.hintY - h.statusY <= 80);                 // 两行紧邻
    CHECK(l.boardRect().y - h.hintY <= 140);          // HUD 贴着棋盘
    CHECK(l.boardRect().y + l.boardRect().h < 1951);  // 棋盘不出界
}

TEST_CASE("HUD 定位：无效布局返回 0，不产生负坐标", "[layout]") {
    const ttt::Layout bad;                     // 默认构造 = 无效布局
    const ttt::Layout::HudLines h = bad.hudLines();
    CHECK(h.statusY == 0);
    CHECK(h.hintY   == 0);
}

// -----------------------------------------------------------------------------
// 宽屏/竖屏排布（平板、电脑、自由窗口）
//
// 背景：逻辑画布不再固定 900×900 —— 短边固定 900，长边跟随窗口宽高比。
// 宽屏时棋盘靠左、HUD 占右侧一列；竖屏保持 HUD 在棋盘上方。
// -----------------------------------------------------------------------------
TEST_CASE("排布模式：按可用区高宽比选择", "[layout]") {
    CHECK(ttt::Layout::compute(900, 1600, 0, 0, 0, 0).mode() == ttt::Layout::Mode::Vertical);
    CHECK(ttt::Layout::compute(900, 900, 0, 0, 0, 0).mode() == ttt::Layout::Mode::Vertical);
    // 1000x900 只有 1.11，仍算竖屏（阈值 1.15）
    CHECK(ttt::Layout::compute(1000, 900, 0, 0, 0, 0).mode() == ttt::Layout::Mode::Vertical);
    CHECK(ttt::Layout::compute(1600, 900, 0, 0, 0, 0).mode() == ttt::Layout::Mode::Wide);
}

TEST_CASE("宽屏模式：棋盘仍是正方形且靠左，HUD 在其右侧", "[layout]") {
    const ttt::Layout L = ttt::Layout::compute(1600, 900, 0, 0, 0, 0, 120);
    REQUIRE(L.cellSize() > 0);
    const ttt::Recti& b = L.boardRect();

    CHECK(b.w == b.h);                      // 正方形约束不能破
    CHECK(b.x >= 0);
    CHECK(b.x + b.w <= 1600);               // 不超出画布
    CHECK(b.y >= 0);
    CHECK(b.y + b.h <= 900);

    const ttt::Layout::HudLines h = L.hudLines();
    CHECK(h.statusX > b.x + b.w);           // HUD 在棋盘右侧
    CHECK(h.hintX   == h.statusX);
    CHECK(h.hintY   > h.statusY);           // 提示行在状态行下方
    CHECK(h.hintBand.w > 0);
    CHECK(h.hintBand.x >= h.hintX);
    CHECK(h.hintBand.x + h.hintBand.w <= 1600);
}

TEST_CASE("宽屏模式：棋盘比竖屏同高度时更大", "[layout]") {
    const ttt::Layout tall = ttt::Layout::compute(900, 1600, 0, 0, 0, 0, 120);
    const ttt::Layout wide = ttt::Layout::compute(1600, 900, 0, 0, 0, 0, 120);
    REQUIRE(tall.cellSize() > 0);
    REQUIRE(wide.cellSize() > 0);
    // 宽屏可用高度虽只有 900，但宽度充裕；棋盘应比 900x1600 竖屏更大
    CHECK(wide.cellSize() > tall.cellSize());
}

TEST_CASE("宽屏模式：安全区仍被尊重", "[layout]") {
    const ttt::Layout L = ttt::Layout::compute(1600, 900, 40, 30, 40, 30);
    REQUIRE(L.cellSize() > 0);
    const ttt::Recti& b = L.boardRect();
    CHECK(b.x >= 30);
    CHECK(b.y >= 40);
    CHECK(b.x + b.w <= 1600 - 30);
    CHECK(b.y + b.h <= 900 - 40);
}
