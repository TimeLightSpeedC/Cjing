// platform_sdl/sdl_view.hpp —— IView 的 SDL3 实现
// 约定（路线图 1.1 架构原则）：
//   - 只消费渲染坐标 / 逻辑单位，不读窗口像素尺寸
//   - 棋盘与 X/O 用矢量绘制（不引入 SDL3_image）
#pragma once

#include <SDL3/SDL.h>

#include <ttt/game.hpp>
#include <ttt/layout.hpp>
#include <ttt/view.hpp>

namespace ttt::sdl {

class SDLView final : public IView {
public:
    // scale：逻辑单位 -> 渲染输出像素。SDL3_ttf 接入后用于字体栅格化尺寸，
    // 当前矢量绘制不依赖它，故不存储（避免未使用字段告警）。
    SDLView(SDL_Renderer* renderer, float scale);

    bool loadFont(std::string_view assetName, int logicalPointSize) override;

    void beginFrame() override;
    void present() override;

    void drawBoard(const Layout& layout) override;
    void drawPieces(const GameState& state, const Layout& layout) override;
    void drawWinningLine(const GameState& state, const Layout& layout) override;
    void drawText(TextId id, const Layout& layout) override;

private:
    void drawX(int cx, int cy, int half);
    void drawO(int cx, int cy, int radius);

    SDL_Renderer* renderer_ = nullptr;
    bool          present_  = false;
};

}  // namespace ttt::sdl
