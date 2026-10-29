// core/include/ttt/view.hpp
// 渲染抽象：由 platform_sdl / platform_ohos 实现。
// 约束：实现方只消费"逻辑单位"坐标；本接口不含任何 SDL 类型。
#pragma once

#include <cstdint>
#include <string_view>

#include "ttt/game.hpp"
#include "ttt/layout.hpp"
#include "ttt/text_id.hpp"

namespace ttt {

class IView {
public:
    virtual ~IView() = default;

    // 保留字体的相对路径加载（经 AssetIO 解析，不得使用绝对路径）
    virtual bool loadFont(std::string_view assetName, int logicalPointSize) = 0;

    // 一帧的开始/结束；present() 只在状态变化时被调用（省电）
    virtual void beginFrame() = 0;
    virtual void present() = 0;

    virtual void drawBoard(const Layout& layout) = 0;
    virtual void drawPieces(const GameState& state, const Layout& layout) = 0;
    virtual void drawWinningLine(const GameState& state, const Layout& layout) = 0;
    // 在渲染坐标 (x, y) 处绘制一条文案。
    //
    // 只接收 TextId，不接收裸字符串：代码里不允许出现硬编码的用户可见文案，
    // 具体内容由 core 的文案表（ttt/text.hpp）提供。
    // 颜色用三个 uint8，避免 core 依赖任何平台类型（SDL_Color 之类）。
    virtual void drawText(TextId id, int x, int y,
                          std::uint8_t r, std::uint8_t g, std::uint8_t b) = 0;
};

}  // namespace ttt
