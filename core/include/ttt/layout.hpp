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
    static Layout compute(int logical, int insetTop, int insetRight,
                          int insetBottom, int insetLeft) noexcept;

    // 棋盘整体区域（正方形、居中、已扣安全区）
    [[nodiscard]] const Recti& boardRect() const noexcept { return board_; }
    [[nodiscard]] int cellSize() const noexcept { return cell_; }

    // 逻辑坐标 -> 格子索引；空白间隔或棋盘外返回 -1
    [[nodiscard]] int cellAt(int x, int y) const noexcept;

    // 格子索引 -> 该格中心点（供渲染 X/O）
    [[nodiscard]] bool cellCenter(int index, int& outX, int& outY) const noexcept;

    // 单条网格线的起止点：i 取 1..2（第 1、2 条内部分隔线）
    [[nodiscard]] bool gridLine(int i, bool vertical, int& x1, int& y1,
                                int& x2, int& y2) const noexcept;

private:
    Recti board_{};
    int   cell_ = 0;
};

}  // namespace ttt
