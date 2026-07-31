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
