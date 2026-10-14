// core/include/ttt/input.hpp
// 输入抽象：只接收"渲染坐标"。
// 关键约束（路线图 F4）：窗口像素 -> 渲染坐标的换算由平台层在事件入口
// 用一次 SDL_ConvertEventToRenderCoordinates() 完成，且只允许有这一处实现。
// 本层不得出现 x*winW 之类的换算，也不得出现平台事件类型。
#pragma once

namespace ttt {

class IInput {
public:
    virtual ~IInput() = default;

    // 一次指针按下（鼠标或触摸已在上游统一）。坐标为渲染坐标。
    virtual void onPointerDown(int x, int y) = 0;

    // 一次指针抬起（供触摸反馈动画使用，可空实现）
    virtual void onPointerUp(int x, int y) = 0;
};

}  // namespace ttt
