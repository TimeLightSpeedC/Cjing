// core/include/ttt/view.hpp
// 渲染抽象：由 platform_sdl / platform_ohos 实现。
// 约束：实现方只消费"逻辑单位"坐标；本接口不含任何 SDL 类型。
#pragma once

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
    virtual void drawText(TextId id, const Layout& layout) = 0;
};

}  // namespace ttt
