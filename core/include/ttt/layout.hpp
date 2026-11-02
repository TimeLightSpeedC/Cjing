// core/include/ttt/layout.hpp
// 布局模型：只消费"逻辑尺寸"与"渲染坐标"，内部按逻辑单位算格子。
// 关键约束：本文件与实现都不得读取窗口像素尺寸 —— 那是平台层的事。
#pragma once

#include "ttt/types.hpp"

namespace ttt {

// 渲染坐标系下的矩形（原点在渲染空间左上角，单位 = 逻辑单位）
struct Recti {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

class Layout {
public:
    // logical: 逻辑分辨率边长（如 900）；insets: 安全区在各边占用的逻辑单位
    // reserveTop: 顶部额外预留（如状态文字区），棋盘在这个范围内居中
    static Layout compute(int logical, int insetTop, int insetRight,
                          int insetBottom, int insetLeft,
                          int reserveTop = 0) noexcept;

    // 棋盘整体区域（正方形、居中、已扣安全区）
    [[nodiscard]] const Recti& boardRect() const noexcept { return board_; }
    [[nodiscard]] int cellSize() const noexcept { return cell_; }

    // 逻辑坐标 -> 格子索引；空白间隔或棋盘外返回 -1
    [[nodiscard]] int cellAt(int x, int y) const noexcept;

    // 格子索引 -> 该格中心点（供渲染 X/O）
    [[nodiscard]] bool cellCenter(int index, int& outX, int& outY) const noexcept;

    // 状态文字两行的 Y 坐标（渲染坐标，逻辑单位）
    //
    // 为什么放在 Layout 里而不是各画各的：
    //   正式程序与自测截图曾各写一套公式（12%/50% vs 1/3、2/3），
    //   导致 CI 截图与真机画面的间距不一致 —— 这类偏差只有肉眼能发现。
    //   统一到这里后，两边调用同一个函数，结构上不可能再分叉。
    struct HudLines {
        int statusY = 0;   // 状态行（"轮到 X" / "X 获胜！"）
        int hintY   = 0;   // 提示行（"点击棋盘落子 …"）
    };
    [[nodiscard]] HudLines hudLines() const noexcept;

    // 单条网格线的起止点：i 取 1..2（第 1、2 条内部分隔线）
    [[nodiscard]] bool gridLine(int i, bool vertical, int& x1, int& y1,
                                int& x2, int& y2) const noexcept;

private:
    Recti board_{};
    int   cell_ = 0;
};

}  // namespace ttt
