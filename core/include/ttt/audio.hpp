// =============================================================================
//  core/include/ttt/audio.hpp —— 音效播放抽象
//
//  ▍为什么这里【只有接口、没有 .cpp】
//    和 view.hpp / input.hpp 一样：它描述"能做什么"，不描述"怎么做"。
//    具体实现依赖平台（桌面 CoreAudio/WASAPI/ALSA、Android AAudio、鸿蒙 OHAudio），
//    全部放在 platform_* 里。core 不链接任何音频库。
//
//  ▍为什么用"ID + 名字"而不是直接传文件名
//    游戏逻辑只该关心"现在该响哪种声音"（落子/胜利/平局），
//    不该知道资源文件叫什么、放在哪。映射关系放在应用层，见 App::init。
// =============================================================================
#pragma once

#include <cstdint>
#include <string_view>

namespace ttt {

// 游戏里需要的音效种类。新增音效时在这里加一项即可。
enum class SoundId : std::uint8_t {
    Place = 0,   // 落子
    Win,         // 获胜
    Draw,        // 平局
};

// 音效播放器接口
//
// 实现要求（写平台实现时遵守）：
//   1. load() 必须能重复调用而不泄漏（同一 ID 覆盖旧资源）
//   2. play() 在音频设备不可用时必须【静默返回】，不能崩溃
//      —— CI、无音频设备的机器、被静音的系统都可能出现这种情况
//   3. 所有调用都在主线程发生（SDL 音频 API 的线程约定）
class IAudio {
public:
    virtual ~IAudio() = default;

    // 从资源抽象读取并解码音频数据；assetName 形如 "sounds/place.wav"
    virtual bool load(SoundId id, std::string_view assetName) = 0;

    // 播放一次。未加载或设备不可用时静默忽略。
    virtual void play(SoundId id) = 0;

    // 音频是否真正可用（用于启动时决定要不要提示用户）
    [[nodiscard]] virtual bool available() const noexcept = 0;
};

}  // namespace ttt
