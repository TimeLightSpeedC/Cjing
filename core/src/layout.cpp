// =============================================================================
//  core/src/layout.cpp —— 布局计算（把"逻辑画布"切成 3×3 格子）
//
//  这个文件的职责只有一个：**坐标换算**。
//
//    逻辑画布（如 900×900）
//        ├── 扣掉安全区（刘海/系统栏）
//        ├── 扣掉顶部预留（状态文字）
//        ├── 取较短边、乘 88%、保证正方形
//        └── 居中 → 得到棋盘矩形 + 格子边长
//
//  它不碰窗口、不碰 SDL、不读像素尺寸 —— 输入全是"逻辑单位"。
//  好处：可以在没有窗口的情况下被单元测试覆盖（见 tests/test_layout.cpp）。
//
//  ⚠️ 全程使用整数运算，有意的：
//    · 浮点在 32/64 位、不同平台上可能有细微差异，导致"同一格算成两格"
//    · 整数除法天然向下取整，格子边缘与像素严格对齐，不会出现 0.5 像素模糊
//    · 代价是可能损失几个像素的利用率，对布局来说无所谓
// =============================================================================

#include "ttt/layout.hpp"

#include <algorithm>   // std::min / std::max

namespace ttt {
namespace {

// 棋盘占"可用区域"的比例：88%。
// 为什么不占满 100%：需要留出视觉边距，让棋盘不要贴边；
// 这个数也决定了棋盘周围的留白宽度，改动它会直接改变观感。
constexpr int kBoardRatioNum = 88;
constexpr int kBoardRatioDen = 100;

}  // namespace

// -----------------------------------------------------------------------------
// compute —— 计算棋盘矩形与格子尺寸
//
// 参数（全部是逻辑单位）：
//   logical     逻辑画布边长，例如 900
//   insetTop/Right/Bottom/Left   安全区内缩（刘海、系统栏、窗口装饰）
//   reserveTop  顶部额外预留（给状态文字留空间）
//
// 返回的 Layout 可能"无效"（cell_ == 0），表示空间不足以放下棋盘。
// 调用方应检查 boardRect().w > 0 再用。
// -----------------------------------------------------------------------------
Layout Layout::compute(int logical, int insetTop, int insetRight,
                       int insetBottom, int insetLeft, int reserveTop) noexcept {
    Layout out;
    if (logical <= 0) return out;      // 退化输入，直接返回空布局

    // ① 先算出"可用区域"
    //    用 std::max(0, x) 夹紧：万一调用方传了负的安全区（某些平台会），
    //    负值会让可用区域反而变大，后面的居中和裁剪就全乱了。
    const int top    = std::max(0, insetTop) + std::max(0, reserveTop);
    const int availW = logical - std::max(0, insetLeft) - std::max(0, insetRight);
    const int availH = logical - top - std::max(0, insetBottom);
    if (availW <= 0 || availH <= 0) return out;   // 空间被挤没了

    // ② 棋盘必须是正方形 → 取较短边
    //    如果按宽度算，窗口很矮时棋盘会超出屏幕；
    //    取 min 保证两个方向都放得下。
    const int side = std::min(availW, availH) * kBoardRatioNum / kBoardRatioDen;

    // ③ 先定格子边长，再反推棋盘尺寸
    //    顺序很重要：直接 side/3 会丢余数，
    //    所以先算 cell_ 再令 board_ = cell_ * 3，
    //    保证三条边严格对齐、总宽度正好等于 3 个格子（不会有 1~2 像素的缝）。
    out.cell_ = side / kSize;
    if (out.cell_ <= 0) return out;    // 太小，连一格都放不下

    out.board_.w = out.cell_ * kSize;
    out.board_.h = out.board_.w;       // 正方形

    // ④ 在可用区域内居中
    //    originX/Y 是可用区域的起点（安全区 + 顶部预留），
    //    再加 (可用尺寸 - 棋盘尺寸)/2 即得到左上角。
    //    整数除法向下取整，所以多出来的 1 像素会落在右侧/下侧，视觉上无所谓。
    const int originX = std::max(0, insetLeft);
    const int originY = top;
    out.board_.x = originX + (availW - out.board_.w) / 2;
    out.board_.y = originY + (availH - out.board_.h) / 2;
    return out;
}

// -----------------------------------------------------------------------------
// cellAt —— 渲染坐标 → 格子索引（命中判定）
//
// 这是"点击落在哪一格"的唯一实现。返回 -1 表示"没点在任何格子上"。
//
// ⚠️ 边界检查必须放在除法之前：
//    传入负数时，C++ 的整数除法是向零取整（-3/228 == 0），
//    如果不先判界，点在棋盘左侧空白处会被错误地算成第 0 列。
// -----------------------------------------------------------------------------
int Layout::cellAt(int x, int y) const noexcept {
    if (cell_ <= 0) return -1;         // 布局无效

    // 棋盘矩形之外（含四个方向）一律不命中
    if (x < board_.x || y < board_.y ||
        x >= board_.x + board_.w || y >= board_.y + board_.h) {
        return -1;
    }

    // 减掉棋盘原点后整除格子边长 → 行列号
    // 因为上面已保证 x/y 在 [origin, origin+size) 内，结果必然落在 0..2
    const int col = (x - board_.x) / cell_;
    const int row = (y - board_.y) / cell_;

    // 这里再查一次是防御性的：万一将来 board_ 与 cell_ 不一致（比如动态缩放时
    // 半途修改），也不会返回越界索引导致调用方越界读棋盘。
    if (row < 0 || row >= kSize || col < 0 || col >= kSize) return -1;

    return indexOf(row, col);          // 行列 → 0..8
}

// -----------------------------------------------------------------------------
// cellCenter —— 格子索引 → 该格中心点（渲染 X/O 用）
//
// 为什么需要中心点：绘制 X/O 时需要以格子中心为原点，
// 否则图标会偏到格子左上角。
//
// 计算公式：
//   左上角 + 列号 × 格子宽 + 半格
// -----------------------------------------------------------------------------
bool Layout::cellCenter(int index, int& outX, int& outY) const noexcept {
    if (cell_ <= 0 || !isOnBoard(index)) return false;

    outX = board_.x + colOf(index) * cell_ + cell_ / 2;
    outY = board_.y + rowOf(index) * cell_ + cell_ / 2;
    return true;
}

// -----------------------------------------------------------------------------
// gridLine —— 第 i 条【内部】网格线的两个端点
//
// 参数 i 取 1..kSize-1（也就是 1、2）：
//   · 3×3 棋盘内部只有 2 条竖线 + 2 条横线
//   · 最外圈由调用方单独画边框（drawBoard 里用 SDL_RenderRect）
//   这样职责清晰：本函数只负责"内部线"，外框是另一个概念。
//
// 为什么用出参而不是返回结构体：
//   调用方（渲染层）直接拿到四个 float 参数就能调 SDL_RenderLine，
//   少一次构造。这是 C 风格的接口，在本项目里保持这个风格以便和 SDL 对齐。
// -----------------------------------------------------------------------------
bool Layout::gridLine(int i, bool vertical, int& x1, int& y1,
                      int& x2, int& y2) const noexcept {
    if (cell_ <= 0 || i < 1 || i >= kSize) return false;   // 只有 1、2 合法

    if (vertical) {
        // 竖线：x 固定在第 i 格边界，y 从棋盘顶到底
        const int x = board_.x + i * cell_;
        x1 = x;              y1 = board_.y;
        x2 = x;              y2 = board_.y + board_.h;
    } else {
        // 横线：y 固定在第 i 格边界，x 从棋盘左到右
        const int y = board_.y + i * cell_;
        x1 = board_.x;       y1 = y;
        x2 = board_.x + board_.w; y2 = y;
    }
    return true;
}

}  // namespace ttt
