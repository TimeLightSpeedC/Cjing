#include "sdl_view.hpp"
#include <ttt/text.hpp>

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

inline void setColor(SDL_Renderer* r, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
}

}  // namespace

SDLView::SDLView(SDL_Renderer* renderer, float scale, const AssetIO& assets)
    : renderer_(renderer),
      scale_(scale > 0.0F ? scale : 1.0F),
      assets_(&assets) {}

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

void SDLView::releaseCachedResources() {
    const std::size_t before = cachedTextureCount();
    destroyTextCache();
    SDL_Log("[Cjing] 已释放文字纹理缓存: %zu 项", before);
}

void SDLView::onRenderDeviceReset() {
    const std::size_t before = cachedTextureCount();
    destroyTextCache();          // 旧纹理指针已失效，直接丢弃
    SDL_Log("[Cjing] 渲染设备重置，丢弃失效纹理 %zu 项（下次绘制时重建）", before);
}

std::size_t SDLView::cachedTextureCount() const noexcept {
#ifdef CJING_HAVE_TTF
    return textCache_.size();
#else
    return 0;
#endif
}

#endif

bool SDLView::loadFont(std::string_view assetName, int logicalPointSize) {
#ifdef CJING_HAVE_TTF
    if (font_ != nullptr) return true;      // 同一字体只加载一次

    if (!TTF_Init()) {
        std::fprintf(stderr, "[Cjing] TTF_Init 失败: %s\n", SDL_GetError());
        return false;
    }
    if (assets_ == nullptr) {
        std::fprintf(stderr, "[Cjing] 未绑定 AssetIO，无法加载字体\n");
        return false;
    }

    // ① 通过抽象层拿字节 —— 不拼路径、不假设文件系统结构。
    auto bytes = assets_->read(assetName);
    if (!bytes.has_value() || bytes->empty()) {
        std::fprintf(stderr, "[Cjing] 找不到字体资源: %s\n", std::string(assetName).c_str());
        return false;
    }
    fontData_ = std::move(*bytes);         // 必须由本对象持有（见头文件注释）

    // ② 把内存包装成 IOStream；closeio=true 表示流随字体一起关闭。
    //    注意：closeio 只管"流对象"，不管"内存"—— 内存归 fontData_ 管。
    SDL_IOStream* io = SDL_IOFromConstMem(fontData_.data(), fontData_.size());
    if (io == nullptr) {
        std::fprintf(stderr, "[Cjing] 创建内存流失败: %s\n", SDL_GetError());
        return false;
    }

    // ③ 字号随输出缩放，保证高分屏清晰
    const float ptsize = static_cast<float>(logicalPointSize) * scale_;
    font_ = TTF_OpenFontIO(io, /*closeio=*/true, ptsize);
    fontLogicalPt_ = logicalPointSize;         // 记录逻辑字号，供 setScale 重算
    if (font_ == nullptr) {
        std::fprintf(stderr, "[Cjing] 打开字体失败: %s\n", SDL_GetError());
        return false;
    }

    SDL_Log("[Cjing] 字体已加载: %s（%zu 字节 @ %.0fpt）",
            std::string(assetName).c_str(), fontData_.size(), static_cast<double>(ptsize));
    return true;
#else
    (void)assetName;
    (void)logicalPointSize;
    return false;
#endif
}

bool SDLView::setScale(float scale) {
    const float next = (scale > 0.0F) ? scale : 1.0F;

    // 用相对容差判断"是否真的变了"：
    // 窗口 resize 事件非常密集，哪怕像素级抖动也不该触发重建。
    // 0.01 的阈值足以区分 Retina(2.0) 与 1x(1.0)，也不会被浮点噪声扰动。
    const bool significant = std::fabs(next - scale_) >= 0.01F;

    // scale_ 必须【始终】更新：drawTextAt 用它把像素尺寸折回逻辑单位。
    // 若只在显著变化时更新，缓慢拖动窗口期间文字尺寸会有 1% 级漂移。
    scale_ = next;

    // 字体只在显著变化时重栅格化，避免密集 resize 反复重建字形。
    if (!significant) return false;

#ifdef CJING_HAVE_TTF
    if (font_ != nullptr && fontLogicalPt_ > 0) {
        // 只改字号：FreeType 内部只调整字符尺寸并重算度量，
        // 不需要重新读取或解析字体文件（那才是 resize 卡顿的根源）。
        const float ptsize = static_cast<float>(fontLogicalPt_) * scale_;
        if (!TTF_SetFontSize(font_, ptsize)) {
            SDL_Log("[Cjing] TTF_SetFontSize 失败: %s", SDL_GetError());
        }
        // 旧尺寸下栅格化的纹理不能复用
        destroyTextCache();
    }
#endif
    return true;
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
    // ⚠️ 这里是【双重缩放】的修复点。
    //
    // 字体按 "逻辑字号 × scale_" 栅格化，所以 e.w/e.h 的单位是【输出像素】；
    // 而 SDL_RenderTexture 的目标矩形用【逻辑单位】—— 渲染器会再按 letterbox
    // 比例放大一次。若直接把像素尺寸当逻辑尺寸传入，文字相对大小就变成
    // scale²（棋盘只有 scale），表现为：窗口放大时文字长得更快、右边被截断；
    // 窗口缩小时文字缩得更狠。
    //
    // 正确做法：像素尺寸除以 scale_ 折回逻辑单位，交给渲染器统一缩放。
    const SDL_FRect dst{static_cast<float>(x), static_cast<float>(y),
                        textLogicalExtent(e.w, scale_),
                        textLogicalExtent(e.h, scale_)};
    SDL_RenderTexture(renderer_, e.tex, nullptr, &dst);
#else
    (void)utf8; (void)x; (void)y; (void)r; (void)g; (void)b;
#endif
}

namespace {

// 把 UTF-8 文案按 " · " 切成若干段（分隔符保留在段之间）
std::vector<std::string> splitBySeparator(std::string_view text) {
    // 分隔符：空格 + U+00B7（中点）+ 空格
    static constexpr std::string_view kSep = " \u00B7 ";
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t hit = text.find(kSep, pos);
        if (hit == std::string_view::npos) {
            out.emplace_back(text.substr(pos));
            break;
        }
        out.emplace_back(text.substr(pos, hit - pos));
        pos = hit + kSep.size();
    }
    return out;
}

// 折行结果：逐行文本 + 度量
struct WrappedText {
    std::vector<std::string> lines;
    int width = 0;          // 最宽一行（逻辑单位）
    int lineHeight = 0;     // 单行高度
    int totalHeight = 0;    // 含行间距的总高度
};

// 按 " · " 分段，逐段贪心装箱。
// 为什么按分隔符而不是按字符：中文没有空格，任意字符断行会切出半个词；
// 文案本身就用 " · " 分隔语义单元，按它断行既自然又稳定。
// 按 UTF-8 码位切成单个字符（用于超长分句的逐字折行）
std::vector<std::string> splitChars(std::string_view s) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        std::size_t len = 1;
        if ((c & 0x80) == 0x00)      len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > s.size()) len = 1;
        out.emplace_back(s.substr(i, len));
        i += len;
    }
    return out;
}

WrappedText wrapText(TTF_Font* font, float scale, std::string_view text,
                     int maxWidth, int lineGap) {
    WrappedText out;
#ifdef CJING_HAVE_TTF
    if (font == nullptr || text.empty()) return out;

    out.lineHeight = static_cast<int>(textLogicalExtent(TTF_GetFontHeight(font), scale));
    if (out.lineHeight <= 0) out.lineHeight = 44;

    auto measure = [&](std::string_view sv) -> int {
        int tw = 0, th = 0;
        if (!TTF_GetStringSize(font, sv.data(), sv.size(), &tw, &th)) return 0;
        return static_cast<int>(textLogicalExtent(tw, scale));
    };

    const std::vector<std::string> segs = splitBySeparator(text);
    const int limit = (maxWidth > 0) ? maxWidth : (1 << 20);

    // 先把文案切成"词"：正常分句整段作为一个词；
    // 若单个分句本身就超过可用宽度（如结束语「双击这行字再来一局」），
    // 则退化为逐字切分，否则整段无法折行、会直接冲出屏幕（iPad 实测）。
    struct Token { std::string text; bool sepBefore; };
    std::vector<Token> tokens;
    for (std::size_t si = 0; si < segs.size(); ++si) {
        const std::string& seg = segs[si];
        if (measure(seg) <= limit) {
            tokens.push_back({seg, si > 0});
        } else {
            bool first = true;
            for (const std::string& ch : splitChars(seg)) {
                tokens.push_back({ch, first && si > 0});
                first = false;
            }
        }
    }

    std::string current;
    auto flush = [&]() {
        if (!current.empty()) out.lines.push_back(current);
        current.clear();
    };

    for (const Token& t : tokens) {
        std::string candidate = current;
        if (!candidate.empty() && t.sepBefore) candidate += " \u00B7 ";
        candidate += t.text;

        if (current.empty() || measure(candidate) <= limit) {
            current = candidate;      // 首词无论多宽都接受，避免死循环
        } else {
            flush();
            current = t.text;
        }
    }
    flush();

    for (const std::string& line : out.lines) {
        out.width = std::max(out.width, measure(line));
    }
    const int n = static_cast<int>(out.lines.size());
    out.totalHeight = (n > 0) ? (n * out.lineHeight + (n - 1) * lineGap) : 0;
#endif
    return out;
}

}  // namespace

int SDLView::measureTextWrapped(TextId id, int maxWidth, int lineGap, int& outWidth) {
    const WrappedText w = wrapText(font_, scale_, textOf(id), maxWidth, lineGap);
    outWidth = w.width;
    return w.totalHeight;
}

int SDLView::drawTextWrapped(TextId id, int x, int y, int maxWidth, int lineGap,
                             std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const WrappedText w = wrapText(font_, scale_, textOf(id), maxWidth, lineGap);
    int yy = y;
    for (const std::string& line : w.lines) {
        drawTextAt(line, x, yy, r, g, b);
        yy += w.lineHeight + lineGap;
    }
    return w.totalHeight;
}

bool SDLView::textLogicalSize(TextId id, int& outW, int& outH) {
#ifdef CJING_HAVE_TTF
    if (font_ == nullptr) return false;
    const std::string_view sv = textOf(id);
    if (sv.empty()) return false;

    int tw = 0, th = 0;
    if (!TTF_GetStringSize(font_, sv.data(), sv.size(), &tw, &th)) return false;

    // 字体按 逻辑字号 × scale_ 栅格化，所以像素尺寸要除回逻辑单位
    outW = static_cast<int>(textLogicalExtent(tw, scale_));
    outH = static_cast<int>(textLogicalExtent(th, scale_));
    return outW > 0;
#else
    (void)id; (void)outW; (void)outH;
    return false;
#endif
}

void SDLView::drawText(TextId id, int x, int y,
                       std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    // 视图不再持有文案表：统一向 core 的文案表查询。
    // 这样"改一句文案"只需改一处，所有平台同时生效。
    drawTextAt(textOf(id), x, y, r, g, b);
}


}  // namespace ttt::sdl
