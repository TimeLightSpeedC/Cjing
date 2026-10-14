// core/include/ttt/io.hpp
// 资源与存档抽象：上层代码不得出现平台路径。
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ttt/game.hpp"

namespace ttt {

class AssetIO {
public:
    virtual ~AssetIO() = default;

    // 读取只读资源（字体、音效）。桌面走文件路径，Android/HarmonyOS 走 assets/rawfile。
    [[nodiscard]] virtual std::optional<std::vector<std::byte>>
    read(std::string_view assetName) const = 0;
};

class SaveIO {
public:
    virtual ~SaveIO() = default;

    // 可写目录由平台提供（SDL 侧为 SDL_GetPrefPath）
    [[nodiscard]] virtual std::string writableDir() const = 0;

    virtual bool write(std::string_view name, std::string_view data) const = 0;
    [[nodiscard]] virtual std::optional<std::string> read(std::string_view name) const = 0;
};

// 存档序列化：棋盘 9 格 + 当前玩家 + 走子数（P3-T7 的切后台落盘用）
[[nodiscard]] std::string serialize(const GameState& state);
[[nodiscard]] std::optional<GameState> deserialize(std::string_view data);

}  // namespace ttt
