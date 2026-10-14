#include "sdl_view.hpp"

#include <cmath>
#include <cstdio>

namespace ttt::sdl {
namespace {

// 配色
constexpr Uint8 kBgR = 24, kBgG = 24, kBgB = 28;
constexpr Uint8 kGridR = 96, kGridG = 100, kGridB = 112;
constexpr Uint8 kXR = 236, kXG = 108, kXB = 108;
constexpr Uint8 kOR = 118, kOG = 190, kOB = 250;
constexpr Uint8 kWinR = 250, kWinG = 214, kWinB = 96;

inline void setColor(SDL_Renderer* r, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
}

}  // namespace

SDLView::SDLView(SDL_Renderer* renderer, float scale)
    : renderer_(renderer) {
    (void)scale;   // 待 SDL3_ttf 接入后使用（字体栅格化尺寸）
}

bool SDLView::loadFont(std::string_view /*assetName*/, int /*logicalPointSize*/) {
    // TODO(P2-T12/T13): 接入 SDL3_ttf。当前阶段文本用几何提示替代，
    // 待 SDL3_ttf 引入后实现真实文字渲染。
    return false;
}

void SDLView::beginFrame() {
    setColor(renderer_, kBgR, kBgG, kBgB);
    SDL_RenderClear(renderer_);
    present_ = false;
}

void SDLView::present() {
    // 由调用方保证仅在状态变化时调用（P2-T20）
    SDL_RenderPresent(renderer_);
    present_ = true;
}

void SDLView::drawBoard(const Layout& layout) {
    const Recti& b = layout.boardRect();
    if (b.w <= 0) return;

    setColor(renderer_, kGridR, kGridG, kGridB);
    const float t = 2.0F;   // 线宽（逻辑单位）

    // 2 条竖线 + 2 条横线（内部 4 条线，共 4 次绘制）
    for (int i = 1; i < kSize; ++i) {
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        if (layout.gridLine(i, /*vertical=*/true, x1, y1, x2, y2)) {
            SDL_RenderLine(renderer_, static_cast<float>(x1), static_cast<float>(y1),
                           static_cast<float>(x2), static_cast<float>(y2));
        }
        if (layout.gridLine(i, /*vertical=*/false, x1, y1, x2, y2)) {
            SDL_RenderLine(renderer_, static_cast<float>(x1), static_cast<float>(y1),
                           static_cast<float>(x2), static_cast<float>(y2));
        }
    }

    // 外框
    const SDL_FRect frame{static_cast<float>(b.x), static_cast<float>(b.y),
                          static_cast<float>(b.w), static_cast<float>(b.h)};
    SDL_RenderRect(renderer_, &frame);
    (void)t;
}

void SDLView::drawX(int cx, int cy, int half) {
    setColor(renderer_, kXR, kXG, kXB);
    SDL_RenderLine(renderer_, static_cast<float>(cx - half), static_cast<float>(cy - half),
                   static_cast<float>(cx + half), static_cast<float>(cy + half));
    SDL_RenderLine(renderer_, static_cast<float>(cx - half), static_cast<float>(cy + half),
                   static_cast<float>(cx + half), static_cast<float>(cy - half));
}

void SDLView::drawO(int cx, int cy, int radius) {
    setColor(renderer_, kOR, kOG, kOB);
    // 用 N 段折线近似圆（矢量绘制，无贴图）
    constexpr int kSegments = 48;
    float px = 0.0F, py = 0.0F;
    for (int i = 0; i <= kSegments; ++i) {
        const float a = (static_cast<float>(i) / static_cast<float>(kSegments)) *
                        6.2831853F;
        const float x = static_cast<float>(cx) + static_cast<float>(radius) * std::cos(a);
        const float y = static_cast<float>(cy) + static_cast<float>(radius) * std::sin(a);
        if (i > 0) SDL_RenderLine(renderer_, px, py, x, y);
        px = x;
        py = y;
    }
}

void SDLView::drawPieces(const GameState& state, const Layout& layout) {
    const int cell = layout.cellSize();
    if (cell <= 0) return;
    const int half   = cell / 3;
    const int radius = cell / 3;

    for (int i = 0; i < kCellCount; ++i) {
        const Cell c = state.at(i);
        if (c == Cell::Empty) continue;
        int cx = 0, cy = 0;
        if (!layout.cellCenter(i, cx, cy)) continue;
        if (c == Cell::X) drawX(cx, cy, half);
        else              drawO(cx, cy, radius);
    }
}

void SDLView::drawWinningLine(const GameState& state, const Layout& layout) {
    const auto line = state.winLine;
    if (!line.has_value()) return;

    const int cell = layout.cellSize();
    if (cell <= 0) return;

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    if (!layout.cellCenter((*line)[0], x1, y1)) return;
    if (!layout.cellCenter((*line)[2], x2, y2)) return;

    setColor(renderer_, kWinR, kWinG, kWinB);
    // 加粗绘制 3 条平行线，增强可见性
    for (int d = -2; d <= 2; ++d) {
        SDL_RenderLine(renderer_,
                       static_cast<float>(x1), static_cast<float>(y1 + d),
                       static_cast<float>(x2), static_cast<float>(y2 + d));
    }
}

void SDLView::drawText(TextId /*id*/, const Layout& /*layout*/) {
    // TODO(P2-T13): 需要 SDL3_ttf。
    // 当前阶段以"获胜线高亮"表达胜负结果（已实现），文字渲染留待 ttf 接入。
}

}  // namespace ttt::sdl
