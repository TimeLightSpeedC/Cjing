// =============================================================================
//  core/src/io.cpp —— 对局存档的序列化 / 反序列化（纯文本，无第三方库）
//
//  用途：切后台时把对局落盘，重新进入时恢复（移动端的硬需求）。
//
//  格式设计（一行文本，逗号分隔）：
//
//      TTT1,X........,O,1
//      │    │        │  └─ 走子数
//      │    │        └──── 当前该谁走（X/O）
//      │    └───────────── 9 个格子，'.' 表示空
//      └────────────────── 魔数 + 格式版本
//
//  几个设计取舍，值得理解：
//
//  ① 为什么是【文本】而不是二进制
//     存档只有几十字节，文本的可读性/可调试性远超那点空间代价。
//     出问题时用 cat 就能看出对局状态，不需求助工具。
//
//  ② 为什么空位用 '.'
//     固定宽度（永远 9 个字符），解析时不必处理变长；
//     同时肉眼可读，'.' 也不会和 'X'/'O' 混淆。
//
//  ③ 为什么魔数带版本号 "TTT1"
//     将来格式若要改（比如加入走子历史），可以认出来旧存档并做兼容或拒绝，
//     而不是默默解析出错乱的状态。
//
//  ④ 为什么【不存】胜负状态
//     胜负完全由棋盘推导得出（见 deriveStatus）。存了就有"两份真相"，
//     一旦不一致（比如存档被改过、或某次生成 bug），就会出现
//     "棋盘上三连但显示还在下"的诡异现象。单一事实来源是这里的核心原则。
// =============================================================================

#include "ttt/io.hpp"

#include <charconv>   // std::from_chars：无异常、无分配、不受 locale 影响

namespace ttt {
namespace {

constexpr std::string_view kMagic = "TTT1";   // 魔数 + 格式版本
constexpr char kSep = ',';                    // 字段分隔符

// -----------------------------------------------------------------------------
// 内部表示 → 文本字符
//
// switch 列出所有枚举值，最后一个 return 是防御性兜底：
// 若将来新增了 Cell 取值而忘记更新这里，会退化成 '.'（空格子），
// 而不是写出未定义字符导致解析失败。
// -----------------------------------------------------------------------------
char cellChar(Cell c) noexcept {
    switch (c) {
        case Cell::X: return 'X';
        case Cell::O: return 'O';
        case Cell::Empty: break;
    }
    return '.';
}

// 文本字符 → 内部表示。
// 采用"宽容解析"：任何非 X/O 的字符都当作空格子。
// 这样即使存档里混入了奇怪的字节，也只会让某个格子变空，
// 而不至于让整个存档作废（对井字棋来说，宽松比严格更有用）。
Cell charCell(char ch) noexcept {
    if (ch == 'X') return Cell::X;
    if (ch == 'O') return Cell::O;
    return Cell::Empty;
}

// -----------------------------------------------------------------------------
// deriveStatus —— 从棋盘反推对局状态
//
// 这是"单一事实来源"的体现：存档只记录棋盘与手数，状态现算。
//
// 顺序与 Game::applyMove 保持一致：先判胜负，再判平局。
// outLine 用出参返回获胜线，供 UI 高亮（恢复后连高亮都是对的）。
// -----------------------------------------------------------------------------
Status deriveStatus(const Board& b, std::optional<Line>& outLine) noexcept {
    if (const auto line = Game::findWin(b)) {
        outLine = line;
        // 获胜线第一个格子的符号，决定了是 X 赢还是 O 赢
        return b[static_cast<std::size_t>((*line)[0])] == Cell::X ? Status::XWins
                                                                  : Status::OWins;
    }
    return Game::isFull(b) ? Status::Draw : Status::Playing;
}

}  // namespace

// -----------------------------------------------------------------------------
// serialize —— GameState → 文本
//
// reserve(32)：预估长度约 20 字节（4 + 1 + 9 + 1 + 1 + 1 + 2），
// 预留一次分配就够，避免多次扩容（虽然这里规模极小，但这是好习惯）。
// -----------------------------------------------------------------------------
std::string serialize(const GameState& state) {
    std::string out;
    out.reserve(32);

    out.append(kMagic);           // "TTT1"
    out.push_back(kSep);          // ','

    for (const Cell c : state.board) {
        out.push_back(cellChar(c));       // 固定 9 个字符
    }
    out.push_back(kSep);

    out.push_back(state.current == Player::X ? 'X' : 'O');
    out.push_back(kSep);

    out.append(std::to_string(state.moves));
    return out;
}

// -----------------------------------------------------------------------------
// deserialize —— 文本 → GameState
//
// ⚠️ 这是一个【对抗性输入】解析函数：存档文件可能被截断、被修改、被旧版本写入。
//    因此每一个字段都做检查，任何一处不合法就返回 nullopt（让调用方决定是
//    "用默认局面"还是"提示存档损坏"），绝不允许"解析出一半就用"。
//
// 代码结构是"顺序扫描 + 逐段校验"：
//    魔数 → 9 个格子 → 当前玩家 → 手数 → 反推状态
// 每一步都在移动 pos 指针，所以每段前后都要检查边界。
// -----------------------------------------------------------------------------
std::optional<GameState> deserialize(std::string_view data) {
    // ---- ① 魔数与最短长度 ----
    // 先查长度：避免后面 data[pos] 越界。
    if (data.size() < kMagic.size() + 1) return std::nullopt;
    if (data.substr(0, kMagic.size()) != kMagic) return std::nullopt;

    std::size_t pos = kMagic.size();

    // ---- ② 第一个逗号 ----
    if (data[pos] != kSep) return std::nullopt;
    ++pos;

    // ---- ③ 9 个格子 ----
    // 先确认剩余长度足够，再逐个读，避免越界。
    if (pos + kCellCount > data.size()) return std::nullopt;
    GameState st;
    for (int i = 0; i < kCellCount; ++i) {
        st.board[static_cast<std::size_t>(i)] =
            charCell(data[pos + static_cast<std::size_t>(i)]);
    }
    pos += kCellCount;

    // ---- ④ 当前玩家 ----
    // `pos + 1 >= size` 而不是 `pos >= size`：
    // 因为后面还要读 data[pos]，必须保证至少还有一个字符。
    if (pos + 1 >= data.size() || data[pos] != kSep) return std::nullopt;
    ++pos;
    const char who = data[pos];
    if (who != 'X' && who != 'O') return std::nullopt;    // 非法玩家符号
    st.current = (who == 'X') ? Player::X : Player::O;
    ++pos;

    // ---- ⑤ 手数 ----
    if (pos >= data.size() || data[pos] != kSep) return std::nullopt;
    ++pos;

    int moves = 0;
    const char* first = data.data() + pos;
    const char* last  = data.data() + data.size();

    // 用 std::from_chars 而不是 atoi / std::stoi：
    //   · 不抛异常（本项目在 core 层不依赖异常）
    //   · 不分配内存
    //   · 不受 locale 影响（stoi 在土耳其等 locale 下行为可能不同）
    // 返回的 ptr 指向解析结束位置；ec 非 0 表示格式错误。
    const auto [ptr, ec] = std::from_chars(first, last, moves);
    (void)ptr;                       // 只关心是否成功，不关心结束位置
    if (ec != std::errc{}) return std::nullopt;

    // 手数必须落在 0..9：越界说明存档被改过或已损坏
    if (moves < 0 || moves > kCellCount) return std::nullopt;

    st.moves  = moves;

    // ---- ⑥ 状态由棋盘推导（不信任存档里的任何"结论"）----
    st.status = deriveStatus(st.board, st.winLine);
    return st;
}

}  // namespace ttt
