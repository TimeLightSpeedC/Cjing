// =============================================================================
//  platform_sdl/sdl_audio.hpp —— IAudio 的 SDL3 实现（核心音频，不用 SDL_mixer）
//
//  ▍为什么用核心音频而不是 SDL3_mixer
//    我们只需要"播放一段预解码的 WAV"，SDL 核心的 SDL_AudioStream 就够：
//      SDL_LoadWAV_IO 解码 → 挂到设备上 → 需要时把 PCM 数据塞进流
//    引入 SDL_mixer 会多一个第三方依赖（鸿蒙交叉编译时还要多适配一个库），
//    而它提供的功能（多轨混音、OGG/MP3 解码）本项目用不到。
//
//  ▍实现结构
//    每个音效 = 一份 PCM 数据 + 一条绑定到默认播放设备的 SDL_AudioStream。
//    播放时先清空流再重新灌数据 —— 简单可靠，代价是同一音效连续触发会打断上一次，
//    对井字棋（不会密集发声）完全可以接受。
// =============================================================================
#pragma once

#include <array>
#include <memory>
#include <string_view>

#include <SDL3/SDL.h>

#include <ttt/audio.hpp>
#include <ttt/io.hpp>

namespace ttt::sdl {

class SDLAudio final : public IAudio {
public:
    // 用工厂而不是构造函数：初始化可能失败（无音频设备），需要能返回 nullptr
    static std::unique_ptr<SDLAudio> create(const AssetIO& assets);

    ~SDLAudio() override;

    SDLAudio(const SDLAudio&)            = delete;
    SDLAudio& operator=(const SDLAudio&) = delete;

    bool load(SoundId id, std::string_view assetName) override;
    void play(SoundId id) override;
    [[nodiscard]] bool available() const noexcept override { return device_ != 0; }

private:
    explicit SDLAudio(const AssetIO& assets, SDL_AudioDeviceID device);

    struct Clip {
        SDL_AudioStream* stream = nullptr;   // 已绑定到 device_
        Uint8*           pcm    = nullptr;   // 解码后的 PCM（SDL_free 释放）
        Uint32           bytes  = 0;
    };

public:
    static constexpr int kClipCount = 3;     // 与 SoundId 的取值个数一致
private:

    const AssetIO* assets_ = nullptr;
    SDL_AudioDeviceID device_ = 0;           // 0 表示不可用
    std::array<Clip, kClipCount> clips_{};
};

}  // namespace ttt::sdl
