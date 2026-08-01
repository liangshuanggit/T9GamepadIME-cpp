#pragma once
// 拼音解码引擎封装：底层对接 libgooglepinyin（Google 拼音输入法开源核心）
// 未启用 T9IME_USE_LIBGOOGLEPINYIN 时退化为桩实现，便于骨架先行编译。

#include <string>
#include <vector>

namespace ime {

class PinyinIme {
public:
    PinyinIme() = default;
    ~PinyinIme();

    PinyinIme(const PinyinIme&) = delete;
    PinyinIme& operator=(const PinyinIme&) = delete;

    // 加载系统词典与用户词典（.dat 文件路径）
    bool Open(const std::string& sys_dict_path, const std::string& user_dict_path);
    void Close();
    bool IsOpen() const { return opened_; }

    // 以拼音字母串检索，返回候选词（UTF-8）
    std::vector<std::string> Search(const std::string& spelling, size_t max_candidates = 20);

    // 检查该拼音串在词典中是否组成"真实话语"而非"逐音节单字拼接"。
    // 判定依据：整句候选（candidate 0）的"词条组成结构"——
    // 统计整句由多少词典词条组成、其中多少是多字词条（>=2 字）：
    //   - 多字词条占主导（双字数 >= 总词条数，即多字词条 >= 单字词条）说明
    //     是真实词语/话语（如 ni'hao'ma -> 你好吗：1 个三字词条；或
    //     ni'gan'ma -> 你+干嘛：1 双字 + 1 单字，多字不占劣）；
    //   - 反之，单字词条占主导则说明是"单字兜底拼接"（如 mi'ga'o'o'a ->
    //     米+噶+哦哦+啊：1 双字 + 3 单字），是无意义组合，应被丢弃。
    // 单音节（单个汉字）恒为真实汉字，直接接受。
    bool HasPhrase(const std::string& spelling);

    // 单次搜索同时返回"是否真实话语"判定与候选列表，供 T9Engine 复用，
    // 避免 HasPhrase（一次 im_search）与 Search（再一次 im_search）重复搜索。
    struct SearchResult {
        bool is_phrase = false;
        std::vector<std::string> candidates;
    };
    SearchResult SearchAndCheck(const std::string& spelling,
                                size_t max_candidates = 128);

    // 选择第 index 个候选（用于多字词的逐段确认），返回剩余候选
    std::vector<std::string> Choose(size_t index, size_t max_candidates = 20);

    // 重置当前输入状态
    void ResetSearch();

    // 验证拼音串的合法性（调用 libgooglepinyin 的 SpellingParser）。
    // 用于 DFS 展开时的快速前缀剪枝。
    // 返回值：
    //   0 = 非法（不是有效拼音前缀）
    //   1 = 合法前缀（有完整音节 + 末尾部分音节，或整个串是单音节的前缀）
    //   2 = 完整拼音（所有字符都构成完整音节）
    int ValidateSplstr(const std::string& spelling);

    // 使用 libgooglepinyin 公共 API（im_search + im_get_spl_start_pos + im_get_sps_str）
    // 验证完整拼音串并返回引擎解码后的规范拼音。
    // 返回 true 表示该拼音被引擎接受（所有字符被分解为完整音节），
    // decoded 输出引擎实际解码的拼音串。
    bool ValidateAndDecode(const std::string& spelling, std::string& decoded);

private:
    bool opened_ = false;
};

}  // namespace ime
