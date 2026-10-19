#include "sdl_audio.hpp"

#include <cstdio>
#include <utility>

namespace ttt::sdl {
namespace {

// 枚举 → 下标的合法性检查。
// 为什么必须查：枚举可以强转成任意整数，数组下标一旦越界就是未定义行为。
bool validSoundId(SoundId id) noexcept {
    const int v = static_cast<int>(id);
    return v >= 0 && v < SDLAudio::kClipCount;
}

}  // namespace

std::unique_ptr<SDLAudio> SDLAudio::create(const AssetIO& assets) {
    // 音频是【可选能力】：初始化失败不应该阻止游戏运行。
    // 这也照顾了 CI（无音频设备）与静音系统。
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("[Cjing] 音频子系统初始化失败（%s），将以静音运行", SDL_GetError());
        return nullptr;
    }

    // 打开默认播放设备；spec 传 nullptr 表示"用设备自己的格式"。
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (dev == 0) {
        SDL_Log("[Cjing] 没有可用的音频设备（%s），将以静音运行", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);     // 回滚，避免子系统泄漏
        return nullptr;
    }

    SDL_Log("[Cjing] 音频设备已打开（device id=%u）", static_cast<unsigned>(dev));
    return std::unique_ptr<SDLAudio>(new SDLAudio(assets, dev));
}

SDLAudio::SDLAudio(const AssetIO& assets, SDL_AudioDeviceID device)
    : assets_(&assets), device_(device) {}

SDLAudio::~SDLAudio() {
    for (Clip& c : clips_) {
        // 顺序很重要：先解绑（否则设备还在引用这条流），再销毁流，最后释放 PCM
        if (c.stream != nullptr) {
            SDL_UnbindAudioStream(c.stream);
            SDL_DestroyAudioStream(c.stream);
            c.stream = nullptr;
        }
        if (c.pcm != nullptr) {
            SDL_free(c.pcm);       // 由 SDL_LoadWAV_IO 分配，必须用 SDL_free
            c.pcm = nullptr;
        }
    }
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool SDLAudio::load(SoundId id, std::string_view assetName) {
    if (device_ == 0 || assets_ == nullptr || !validSoundId(id)) return false;

    // ① 通过 AssetIO 拿字节 —— 不拼路径，移动端也能读打包资源
    auto bytes = assets_->read(assetName);
    if (!bytes.has_value() || bytes->empty()) {
        SDL_Log("[Cjing] 音效资源缺失: %s", std::string(assetName).c_str());
        return false;
    }

    // ② 把内存包成流，让 SDL 解码 WAV。
    //    closeio=true：解码完成后由 SDL 关闭流（内存仍归我们管）。
    SDL_IOStream* io = SDL_IOFromConstMem(bytes->data(), bytes->size());
    if (io == nullptr) return false;

    SDL_AudioSpec spec{};
    Uint8*  pcm = nullptr;
    Uint32  len = 0;
    if (!SDL_LoadWAV_IO(io, /*closeio=*/true, &spec, &pcm, &len)) {
        SDL_Log("[Cjing] WAV 解码失败（%s）: %s", std::string(assetName).c_str(), SDL_GetError());
        return false;
    }

    // ③ 建一条"源格式 = WAV 格式"的流。
    //    dst 传 nullptr：等绑定到设备时，SDL 会自动把输出格式设成设备格式，
    //    并在这条流里完成重采样与格式转换 —— 我们不必关心设备是 48kHz 还是立体声。
    SDL_AudioStream* stream = SDL_CreateAudioStream(&spec, nullptr);
    if (stream == nullptr) {
        SDL_Log("[Cjing] 创建音频流失败: %s", SDL_GetError());
        SDL_free(pcm);
        return false;
    }

    if (!SDL_BindAudioStream(device_, stream)) {
        SDL_Log("[Cjing] 绑定音频流到设备失败: %s", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_free(pcm);
        return false;
    }

    // ④ 替换旧资源（允许重复 load 同一 ID）
    Clip& c = clips_[static_cast<std::size_t>(id)];
    if (c.stream != nullptr) {
        SDL_UnbindAudioStream(c.stream);
        SDL_DestroyAudioStream(c.stream);
    }
    if (c.pcm != nullptr) SDL_free(c.pcm);

    c.stream = stream;
    c.pcm    = pcm;
    c.bytes  = len;

    SDL_Log("[Cjing] 音效已加载: %s（%u 字节，%dHz %d 声道）",
            std::string(assetName).c_str(), len, spec.freq, spec.channels);
    return true;
}

void SDLAudio::play(SoundId id) {
    if (device_ == 0 || !validSoundId(id)) return;

    Clip& c = clips_[static_cast<size_t>(id)];
    if (c.stream == nullptr || c.pcm == nullptr || c.bytes == 0) return;

    // 先清空再灌入：让每次播放都从头开始。
    // 代价是连续快速触发时会打断上一次（井字棋不会出现）。
    SDL_ClearAudioStream(c.stream);
    if (!SDL_PutAudioStreamData(c.stream, c.pcm, static_cast<int>(c.bytes))) {
        SDL_Log("[Cjing] 播放失败（%s）", SDL_GetError());
    }
}

}  // namespace ttt::sdl
