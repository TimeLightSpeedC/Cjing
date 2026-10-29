// platform_sdl/sdl_view.hpp —— IView 的 SDL3 实现
// 约定（路线图 1.1 架构原则）：
//   - 只消费渲染坐标 / 逻辑单位，不读窗口像素尺寸
//   - 棋盘与 X/O 用矢量绘制（不引入 SDL3_image）
//   - 文本经 SDL3_ttf 渲染；字体路径经 AssetIO 语义解析（不写死绝对路径）
#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>

#include <cstddef>
#include <vector>

#include <ttt/game.hpp>
#include <ttt/io.hpp>
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
    // assets：资源访问抽象。渲染层不关心资源在磁盘、APK assets 还是 HAP raw-file 里，
    //         只要求"给名字，拿到字节"——这正是 core 面向接口设计的价值。
    SDLView(SDL_Renderer* renderer, float scale, const AssetIO& assets);
    ~SDLView() override;

    SDLView(const SDLView&)            = delete;
    SDLView& operator=(const SDLView&) = delete;

    bool loadFont(std::string_view assetName, int logicalPointSize) override;

    void beginFrame() override;
    void present() override;

    void drawBoard(const Layout& layout) override;
    void drawPieces(const GameState& state, const Layout& layout) override;
    void drawWinningLine(const GameState& state, const Layout& layout) override;
    void drawText(TextId id, int x, int y,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b) override;

    // ---- 可重建资源的生命周期 ----
    //
    // 低内存 / 渲染设备重置时，GPU 侧资源需要主动释放或重建。
    // 这两个操作只影响"缓存"，不影响对局状态 —— 状态在 Game 里，与此无关。

    // 释放所有可重建的 GPU 资源（当前是文字纹理缓存）。
    // 字体对象【保留】：它在 CPU 侧，重建成本高（要重新解析 10MB 字体），
    // 而且不占显存。
    void releaseCachedResources();

    // 渲染设备重置后调用。
    // 语义差别：
    //   低内存  = 我们主动释放，之后按需重建（可以继续画）
    //   设备重置 = 纹理已经失效，必须丢弃，重建后才能画
    void onRenderDeviceReset();

    // 当前缓存的纹理条目数（用于日志和自测断言）
    [[nodiscard]] std::size_t cachedTextureCount() const noexcept;

    // 以渲染坐标在指定位置绘制一行文字（供状态区使用）
    void drawTextAt(std::string_view utf8, int x, int y, Uint8 r, Uint8 g, Uint8 b);

    // 更新"逻辑单位 -> 输出像素"的缩放，并按新比例重设字号。
    //
    // 返回值：true 表示缩放确实变了（已重建字号与缓存）；false 表示无需处理。
    //
    // ⚠️ 为什么需要这个接口（一个真实的性能 bug）：
    //   原实现每次窗口 resize 都重建整个 SDLView 并重新 loadFont()，
    //   等于重新读 10.5MB 字体文件 + 重新解析。
    //   拖动窗口时一秒会产生几十次 resize 事件 → 严重卡顿、日志刷屏。
    //   现在改为：只调 TTF_SetFontSize（FreeType 只改字符尺寸，不重新解析字体），
    //   并清空文字纹理缓存（旧尺寸的纹理不能复用）。
    bool setScale(float scale);

#ifdef CJING_HAVE_TTF
    [[nodiscard]] bool hasFont() const noexcept { return font_ != nullptr; }
    // 仅供自测使用：字形覆盖检查需要直接问 TTF 字体是否有某个码位的字形。
    // 生产代码不应通过它直接绘制（绘制请走 drawText/drawTextAt）。
    [[nodiscard]] TTF_Font* rawFontForTest() const noexcept { return font_; }
#else
    [[nodiscard]] bool hasFont() const noexcept { return false; }
#endif

private:
    void drawX(int cx, int cy, int half);
    void drawO(int cx, int cy, int radius);
    void destroyTextCache();

    SDL_Renderer*  renderer_ = nullptr;
    float          scale_    = 1.0F;
    const AssetIO* assets_   = nullptr;   // 资源访问抽象（不持有所有权）
    bool           present_  = false;

#ifdef CJING_HAVE_TTF
    // 字体文件字节必须【活得比 TTF_Font 更久】：
    // 我们用 SDL_IOFromConstMem 把这块内存包装成流交给 SDL_ttf，
    // FreeType 会在字体生命周期内持续从它读取字形数据。
    // 提前释放的后果是随机崩溃或渲染出乱码 —— 这类 bug 极难排查。
    std::vector<std::byte> fontData_;
    TTF_Font* font_ = nullptr;
    int       fontLogicalPt_ = 0;   // 字体的【逻辑】字号（重建时按 scale 换算）
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
