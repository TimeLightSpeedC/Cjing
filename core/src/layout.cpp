#include "ttt/layout.hpp"

#include <algorithm>

namespace ttt {
namespace {
// 棋盘占可用区域的比例（留出边距与状态文字空间）
constexpr int kBoardRatioNum = 88;
constexpr int kBoardRatioDen = 100;
}  // namespace

Layout Layout::compute(int logical, int insetTop, int insetRight,
                       int insetBottom, int insetLeft, int reserveTop) noexcept {
    Layout out;
    if (logical <= 0) return out;

    // 可用区域 = 逻辑尺寸扣掉安全区与顶部预留
    const int top = std::max(0, insetTop) + std::max(0, reserveTop);
    const int availW = logical - std::max(0, insetLeft) - std::max(0, insetRight);
    const int availH = logical - top - std::max(0, insetBottom);
    if (availW <= 0 || availH <= 0) return out;

    // 棋盘保持正方形：取可用区域较短边
    const int side = std::min(availW, availH) * kBoardRatioNum / kBoardRatioDen;

    out.cell_ = side / kSize;
    if (out.cell_ <= 0) return out;

    out.board_.w = out.cell_ * kSize;
    out.board_.h = out.board_.w;

    // 居中于可用区域
    const int originX = std::max(0, insetLeft);
    const int originY = top;
    out.board_.x = originX + (availW - out.board_.w) / 2;
    out.board_.y = originY + (availH - out.board_.h) / 2;
    return out;
}

int Layout::cellAt(int x, int y) const noexcept {
    if (cell_ <= 0) return -1;
    if (x < board_.x || y < board_.y || x >= board_.x + board_.w || y >= board_.y + board_.h) {
        return -1;
    }
    const int col = (x - board_.x) / cell_;
    const int row = (y - board_.y) / cell_;
    if (row < 0 || row >= kSize || col < 0 || col >= kSize) return -1;
    return indexOf(row, col);
}

bool Layout::cellCenter(int index, int& outX, int& outY) const noexcept {
    if (cell_ <= 0 || !isOnBoard(index)) return false;
    outX = board_.x + colOf(index) * cell_ + cell_ / 2;
    outY = board_.y + rowOf(index) * cell_ + cell_ / 2;
    return true;
}

bool Layout::gridLine(int i, bool vertical, int& x1, int& y1,
                      int& x2, int& y2) const noexcept {
    if (cell_ <= 0 || i < 1 || i >= kSize) return false;
    if (vertical) {
        const int x = board_.x + i * cell_;
        x1 = x; y1 = board_.y;
        x2 = x; y2 = board_.y + board_.h;
    } else {
        const int y = board_.y + i * cell_;
        x1 = board_.x; y1 = y;
        x2 = board_.x + board_.w; y2 = y;
    }
    return true;
}

}  // namespace ttt
