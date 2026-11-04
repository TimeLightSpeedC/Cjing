// =============================================================================
//  core/src/layout.cpp —— 布局计算（把"逻辑画布"切成 3×3 格子）
//
//  这个文件的职责只有一个：**坐标换算**。
//
//    逻辑画布（短边 900，长边随窗口宽高比变化）
//        ├── 扣掉安全区（刘海/系统栏）
//        ├── 扣掉顶部预留（状态文字）
//        ├── 竖屏：取较短边、乘 88%、保证正方形
//        ├── 宽屏：棋盘靠左、HUD 占右侧一列（平板/电脑/自由窗口）
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
  // 字体度量是整数，实际渲染可能多出一两个像素；布局按此余量预留，
  // 避免提示行最后几个字被窗口边缘裁掉（桌面 1600x900 实测过）。
  constexpr int kHudSafety = 16;

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
Layout Layout::compute(int width, int height,
                       int insetTop, int insetRight,
                       int insetBottom, int insetLeft,
                       int reserveTop, int hudMinWidth) noexcept {
    Layout out;
    out.width_  = std::max(0, width);
    out.height_ = std::max(0, height);
    if (out.width_ <= 0 || out.height_ <= 0) return out;   // 退化输入

    const int left   = std::max(0, insetLeft);
    const int right  = std::max(0, insetRight);
    const int top    = std::max(0, insetTop);
    const int bottom = std::max(0, insetBottom);

    const int availX = left;
    const int availY = top;
    const int availW = out.width_ - left - right;
    const int availH = out.height_ - top - bottom;
    if (availW <= 0 || availH <= 0) return out;            // 空间被安全区挤没

    // 模式选择：可用区的高宽比 >= 1.15 视为竖屏，否则按宽屏处理。
    // 用 long long 相乘避免极端尺寸下溢出。
    // 宽 > 高 × 1.15 才算宽屏；正方形与略宽（如 1.11）仍按竖屏处理，
    // 避免平板上轻微横屏就把 HUD 甩到右侧。
    const bool wide = (static_cast<long long>(availW) * 100) >
                      (static_cast<long long>(availH) * 115);
    out.mode_ = wide ? Mode::Wide : Mode::Vertical;

    if (!wide) {
        // ---------------- 竖屏：HUD 在棋盘上方 ----------------
        const int reserve = std::max(0, reserveTop);
        const int usableH = availH - reserve;
        if (usableH <= 0) return out;

        const int side = std::min(availW, usableH) * kBoardRatioNum / kBoardRatioDen;
        out.cell_ = side / kSize;
        if (out.cell_ <= 0) return out;

        out.board_.w = out.cell_ * kSize;
        out.board_.h = out.board_.w;
        out.board_.x = availX + (availW - out.board_.w) / 2;
        out.board_.y = availY + reserve + (usableH - out.board_.h) / 2;

        // HUD 沿用既有公式（相对棋盘顶 12% / 50%），保持与历史截图一致
        out.hud_.statusX = out.board_.x;
        out.hud_.statusY = out.board_.y * 12 / 100;
        out.hud_.hintX   = out.board_.x;
        out.hud_.hintY   = out.board_.y * 50 / 100;
        out.hud_.hintBand = Recti{
            out.board_.x, out.hud_.hintY - 8,
            std::max(out.board_.w, out.width_ - out.board_.x), 64};
        return out;
    }

    // ---------------- 宽屏：棋盘靠左，HUD 占右侧一列 ----------------
    const int gap   = std::max(16, std::min(availW, availH) / 40);
    // HUD 列宽：至少 240，且必须容得下实测文字宽度（hudMinWidth）。
    // 上限取可用宽度的 55%，否则极长文案会把棋盘挤没。
    int hudW = std::max(240, availW * 32 / 100);
    if (hudMinWidth > 0) {
        hudW = std::max(hudW, hudMinWidth + gap + kHudSafety);
    }
    hudW = std::min(hudW, availW * 55 / 100);
    const int maxBoardW = availW - hudW - gap * 2;
    if (maxBoardW <= 0) return out;

    // 宽屏用 94%（竖屏是 88%）：宽屏时高度是瓶颈，留白可以更小，
    // 让棋盘在同样高度下比竖屏更大，真正利用横向空间。
    const int rawSide = std::min(availH * 94 / 100, maxBoardW);
    out.cell_ = rawSide / kSize;
    if (out.cell_ <= 0) return out;

    out.board_.w = out.cell_ * kSize;
    out.board_.h = out.board_.w;
    out.board_.x = availX + gap;
    out.board_.y = availY + (availH - out.board_.h) / 2;

    int hudX = out.board_.x + out.board_.w + gap * 2;

    // 硬约束：文字必须放得下。字体度量与实际渲染可能有几像素误差，
    // 这里按"超出多少就缩多少"再收一次棋盘，避免最后一两个字被窗口裁掉。
    if (hudMinWidth > 0) {
        const int overflow = (hudX + hudMinWidth + kHudSafety) - out.width_;
        if (overflow > 0) {
            const int shrunk = (out.board_.w - overflow) / kSize;
            if (shrunk > 0) {
                out.cell_ = shrunk;
                out.board_.w = out.board_.h = out.cell_ * kSize;
                out.board_.y = availY + (availH - out.board_.h) / 2;
                hudX = out.board_.x + out.board_.w + gap * 2;
            }
        }
    }

    if (hudX + 120 > out.width_) {
        // 太窄，右侧放不下 HUD：退回竖屏排布，避免文字被裁掉
        out.mode_ = Mode::Vertical;
        out.board_.x = availX + (availW - out.board_.w) / 2;
        out.hud_.statusX = out.board_.x;
        out.hud_.statusY = out.board_.y * 12 / 100;
        out.hud_.hintX   = out.board_.x;
        out.hud_.hintY   = out.board_.y * 50 / 100;
        out.hud_.hintBand = Recti{
            out.board_.x, out.hud_.hintY - 8,
            std::max(out.board_.w, out.width_ - out.board_.x), 64};
        return out;
    }

    // HUD 与棋盘垂直居中对齐：状态行在上，提示行在下
    out.hud_.statusX = hudX;
    out.hud_.statusY = out.board_.y + out.board_.h * 28 / 100;
    out.hud_.hintX   = hudX;
    out.hud_.hintY   = out.board_.y + out.board_.h * 46 / 100;
    out.hud_.hintBand = Recti{hudX, out.hud_.hintY - 8,
                              out.width_ - hudX, 64};
    return out;
}

// -----------------------------------------------------------------------------
// hudLines —— 状态区两行的 Y 坐标
//
// 用 board_.y（棋盘顶部）的百分比定位，而不是写死像素：
//   状态行在 12% 处，提示行在 50% 处。
// 这样窗口多高、棋盘多大，文字与棋盘之间的视觉间距都保持同一比例。
// -----------------------------------------------------------------------------
Layout::HudLines Layout::hudLines() const noexcept {
    // 位置在 compute() 里一次算好：竖屏/宽屏两套排布都写进 hud_，
    // 这里只做返回，避免"两处各算一套"再次发散。
    return hud_;
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
