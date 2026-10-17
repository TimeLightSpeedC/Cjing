#include "sdl_view.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace ttt::sdl {
namespace {

// 配色
constexpr Uint8 kBgR = 24, kBgG = 24, kBgB = 28;
constexpr Uint8 kGridR = 96, kGridG = 100, kGridB = 112;
constexpr Uint8 kXR = 236, kXG = 108, kXB = 108;
constexpr Uint8 kOR = 118, kOG = 190, kOB = 250;
constexpr Uint8 kWinR = 250, kWinG = 214, kWinB = 96;
constexpr Uint8 kTextR = 226, kTextG = 228, kTextB = 236;

inline void setColor(SDL_Renderer* r, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
}

}  // namespace

SDLView::SDLView(SDL_Renderer* renderer, float scale, std::string assetBase)
    : renderer_(renderer),
      scale_(scale > 0.0F ? scale : 1.0F),
      assetBase_(std::move(assetBase)) {}

SDLView::~SDLView() {
#ifdef CJING_HAVE_TTF
    destroyTextCache();
    if (font_ != nullptr) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
    TTF_Quit();
#endif
}

#ifdef CJING_HAVE_TTF
namespace {
inline Uint32 packRgb(Uint8 r, Uint8 g, Uint8 b) {
    return (static_cast<Uint32>(r) << 16) | (static_cast<Uint32>(g) << 8) | b;
}
}  // namespace

void SDLView::destroyTextCache() {
    for (auto& [k, e] : textCache_) {
        if (e.tex != nullptr) SDL_DestroyTexture(e.tex);
    }
    textCache_.clear();
}
#endif

bool SDLView::loadFont(std::string_view assetName, int logicalPointSize) {
#ifdef CJING_HAVE_TTF
    // 同一字体只加载一次（缩放变化会重建 view，但字体重载是浪费）
    if (font_ != nullptr) return true;

    if (!TTF_Init()) {
        std::fprintf(stderr, "[Cjing] TTF_Init 失败: %s\n", SDL_GetError());
        return false;
    }

    // 资源定位：assets/<name>，并回退到多级父目录，
    // 以适配 CMake 构建目录（build/bin/）与打包后的目录结构差异。
    // 首次解析结果缓存在 fontPath_，后续复用。
    if (fontPath_.empty()) {
        // assetBase_ 以 '/' 结尾（见 App::init），prefix 负责向父目录回退：
        //   ""      -> <base>assets/...            打包后布局
        //   "../.." -> <repo>/assets/...          build/bin 构建目录布局
        static const char* kPrefixes[] = {"", "../..", "../../..", "../../../.."};
        for (const char* p : kPrefixes) {
            std::string cand = assetBase_ + p + "/assets/" + std::string(assetName);
            if (SDL_IOFromFile(cand.c_str(), "rb") != nullptr) { fontPath_ = cand; break; }
        }
    }
    if (fontPath_.empty()) {
        std::fprintf(stderr, "[Cjing] 找不到字体资源: assets/%s（已尝试多级回退）\n",
                     std::string(assetName).c_str());
        return false;
    }
    const std::string& found = fontPath_;

    // 字体栅格化尺寸随输出缩放，保证高分屏清晰
    const float ptsize = static_cast<float>(logicalPointSize) * scale_;
    font_ = TTF_OpenFont(found.c_str(), ptsize);
    if (font_ == nullptr) {
        std::fprintf(stderr, "[Cjing] 打开字体失败 (%s): %s\n", found.c_str(), SDL_GetError());
        return false;
    }
    SDL_Log("[Cjing] 字体已加载: %s @ %.0fpt", found.c_str(), static_cast<double>(ptsize));
    return true;
#else
    (void)assetName;
    (void)logicalPointSize;
    return false;
#endif
}

void SDLView::beginFrame() {
    setColor(renderer_, kBgR, kBgG, kBgB);
    SDL_RenderClear(renderer_);
    present_ = false;
}

void SDLView::present() {
    SDL_RenderPresent(renderer_);
    present_ = true;
}

void SDLView::drawBoard(const Layout& layout) {
    const Recti& b = layout.boardRect();
    if (b.w <= 0) return;

    setColor(renderer_, kGridR, kGridG, kGridB);
    for (int i = 1; i < kSize; ++i) {
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        if (layout.gridLine(i, true, x1, y1, x2, y2)) {
            SDL_RenderLine(renderer_, static_cast<float>(x1), static_cast<float>(y1),
                           static_cast<float>(x2), static_cast<float>(y2));
        }
        if (layout.gridLine(i, false, x1, y1, x2, y2)) {
            SDL_RenderLine(renderer_, static_cast<float>(x1), static_cast<float>(y1),
                           static_cast<float>(x2), static_cast<float>(y2));
        }
    }
    const SDL_FRect frame{static_cast<float>(b.x), static_cast<float>(b.y),
                          static_cast<float>(b.w), static_cast<float>(b.h)};
    SDL_RenderRect(renderer_, &frame);
}

void SDLView::drawX(int cx, int cy, int half) {
    setColor(renderer_, kXR, kXG, kXB);
    const float x = static_cast<float>(cx);
    const float y = static_cast<float>(cy);
    const float h = static_cast<float>(half);

    // 加粗方式：沿笔画的【法线】方向平移多条平行线。
    // 若沿对角线方向平移（x+d, y+d），得到的是"平行斜线偏移"，会让两条笔画粗细不均、
    // 交叉点错位 —— 之前就是这样画错了。
    const float thickness = 5.0F;   // 总线宽（逻辑单位）
    const float k         = 0.70710678F;  // 1/√2

    // 笔画 A：左上 → 右下，法线 (k, -k)
    // 笔画 B：右上 → 左下，法线 (k,  k)
    const int layers = static_cast<int>(thickness);
    for (int i = 0; i < layers; ++i) {
        const float d = (static_cast<float>(i) - (static_cast<float>(layers) - 1.0F) / 2.0F);
        const float ax = k * d, ay = -k * d;   // A 的法线偏移
        const float bx = k * d, by =  k * d;   // B 的法线偏移

        SDL_RenderLine(renderer_, x - h + ax, y - h + ay, x + h + ax, y + h + ay);
        SDL_RenderLine(renderer_, x - h + bx, y + h + by, x + h + bx, y - h + by);
    }
}

void SDLView::drawO(int cx, int cy, int radius) {
    setColor(renderer_, kOR, kOG, kOB);
    constexpr int   kSegments = 96;        // 折线段数（越大越圆）
    constexpr float kTwoPi     = 6.2831853F;
    constexpr float kThickness = 5.0F;     // 与 X 的线宽保持一致

    // 用多条同心折线模拟圆环笔宽；每个半径都画满一整圈（含首尾闭合）
    const int rings = static_cast<int>(kThickness);
    for (int ring = 0; ring < rings; ++ring) {
        const float d  = (static_cast<float>(ring) - (static_cast<float>(rings) - 1.0F) / 2.0F);
        const float rr = static_cast<float>(radius) + d;
        if (rr <= 0.5F) continue;

        float px = static_cast<float>(cx) + rr;
        float py = static_cast<float>(cy);
        for (int i = 1; i <= kSegments; ++i) {
            const float a = (static_cast<float>(i) / static_cast<float>(kSegments)) * kTwoPi;
            const float x = static_cast<float>(cx) + rr * std::cos(a);
            const float y = static_cast<float>(cy) + rr * std::sin(a);
            SDL_RenderLine(renderer_, px, py, x, y);
            px = x; py = y;
        }
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
    if (!state.winLine.has_value()) return;
    const int cell = layout.cellSize();
    if (cell <= 0) return;

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    if (!layout.cellCenter((*state.winLine)[0], x1, y1)) return;
    if (!layout.cellCenter((*state.winLine)[2], x2, y2)) return;

    setColor(renderer_, kWinR, kWinG, kWinB);
    for (int d = -3; d <= 3; ++d) {
        SDL_RenderLine(renderer_, static_cast<float>(x1), static_cast<float>(y1 + d),
                       static_cast<float>(x2), static_cast<float>(y2 + d));
    }
}

void SDLView::drawTextAt(std::string_view utf8, int x, int y, Uint8 r, Uint8 g, Uint8 b) {
#ifdef CJING_HAVE_TTF
    if (font_ == nullptr || utf8.empty()) return;

    const TextKey key{std::string(utf8), packRgb(r, g, b)};
    auto it = textCache_.find(key);
    if (it == textCache_.end()) {
        const SDL_Color fg{r, g, b, 255};
        SDL_Surface* surf = TTF_RenderText_Blended(font_, key.text.c_str(),
                                                   key.text.size(), fg);
        if (surf == nullptr) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        TextEntry e;
        e.w = surf->w;
        e.h = surf->h;
        SDL_DestroySurface(surf);
        if (tex == nullptr) return;
        e.tex = tex;
        it = textCache_.emplace(key, e).first;
    }

    const TextEntry& e = it->second;
    const SDL_FRect dst{static_cast<float>(x), static_cast<float>(y),
                        static_cast<float>(e.w), static_cast<float>(e.h)};
    SDL_RenderTexture(renderer_, e.tex, nullptr, &dst);
#else
    (void)utf8; (void)x; (void)y; (void)r; (void)g; (void)b;
#endif
}

void SDLView::drawText(TextId id, const Layout& layout) {
    // 状态文字：位于棋盘上方预留区（kReserveTop）
    const Recti& b = layout.boardRect();
    if (b.w <= 0) return;

    const int lineH = b.y / 2;          // 预留区中线
    const int x     = b.x;

    switch (id) {
        case TextId::AppTitle:
            drawTextAt("井字棋 Tic-Tac-Toe", x, lineH - 40, kTextR, kTextG, kTextB);
            break;
        default:
            break;
    }
}

}  // namespace ttt::sdl
