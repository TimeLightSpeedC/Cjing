// platform_sdl/sdl_view.hpp —— IView 的 SDL3 实现
// 约定（路线图 1.1 架构原则）：
//   - 只消费渲染坐标 / 逻辑单位，不读窗口像素尺寸
//   - 棋盘与 X/O 用矢量绘制（不引入 SDL3_image）
//   - 文本经 SDL3_ttf 渲染；字体路径经 AssetIO 语义解析（不写死绝对路径）
#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>

#include <ttt/game.hpp>
#include <ttt/layout.hpp>
#include <ttt/text_id.hpp>
#include <ttt/view.hpp>

#ifdef CJING_HAVE_TTF
#include <SDL3_ttf/SDL_ttf.h>
#endif

namespace ttt::sdl {

class SDLView final : public IView {
public:
    // scale：逻辑单位 -> 渲染输出像素（用于字体栅格化尺寸）
    // assetBase：资源根目录（桌面为可执行文件所在目录，移动端由平台提供）
    SDLView(SDL_Renderer* renderer, float scale, std::string assetBase);
    ~SDLView() override;

    SDLView(const SDLView&)            = delete;
    SDLView& operator=(const SDLView&) = delete;

    bool loadFont(std::string_view assetName, int logicalPointSize) override;

    void beginFrame() override;
    void present() override;

    void drawBoard(const Layout& layout) override;
    void drawPieces(const GameState& state, const Layout& layout) override;
    void drawWinningLine(const GameState& state, const Layout& layout) override;
    void drawText(TextId id, const Layout& layout) override;

    // 以渲染坐标在指定位置绘制一行文字（供状态区使用）
    void drawTextAt(std::string_view utf8, int x, int y, Uint8 r, Uint8 g, Uint8 b);

#ifdef CJING_HAVE_TTF
    [[nodiscard]] bool hasFont() const noexcept { return font_ != nullptr; }
#else
    [[nodiscard]] bool hasFont() const noexcept { return false; }
#endif

private:
    void drawX(int cx, int cy, int half);
    void drawO(int cx, int cy, int radius);
    void destroyTextCache();

    SDL_Renderer* renderer_ = nullptr;
    float         scale_    = 1.0F;
    std::string   assetBase_;
    std::string   fontPath_;      // 已解析的字体路径（避免重复扫描与重复加载）
    bool          present_  = false;

#ifdef CJING_HAVE_TTF
    TTF_Font* font_ = nullptr;
    // 文本纹理缓存：同一 (文案,颜色) 只栅格化一次
    struct TextKey {
        std::string text;
        Uint32      color;
        bool operator==(const TextKey& o) const noexcept {
            return color == o.color && text == o.text;
        }
    };
    struct TextKeyHash {
        std::size_t operator()(const TextKey& k) const noexcept {
            return std::hash<std::string>{}(k.text) ^ (static_cast<std::size_t>(k.color) << 1);
        }
    };
    struct TextEntry {
        SDL_Texture* tex = nullptr;
        int          w   = 0;
        int          h   = 0;
    };
    std::unordered_map<TextKey, TextEntry, TextKeyHash> textCache_;
#endif
};

}  // namespace ttt::sdl
