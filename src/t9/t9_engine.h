#pragma once
// T9 引擎：维护数字键输入缓冲，将数字串展开为合法拼音组合，
// 交由 PinyinIme 检索候选并合并结果。
//
// 拼音展开策略（两阶段验证）：
// 1. DFS 前缀剪枝：调用 libgooglepinyin 的 SpellingParser 接口
//    （im_validate_splstr）快速判断字母串是否为合法拼音前缀。
// 2. 叶子最终验证：调用 libgooglepinyin 公共 API
//    （im_search + im_get_spl_start_pos + im_get_sps_str）确认
//    整个字符串被引擎分解为完整音节，并获取规范解码拼音。
// 支持多音节：如 "64426" -> "nihao"（ni + hao 两个音节拼接）。
//
// 模糊音策略：
//   在拼音展开验证通过后，对每个音节应用模糊音映射生成变体拼音。
//   支持的模糊音对：z/zh, c/ch, s/sh, n/l, in/ing, en/eng, an/ang。
//   模糊音变体同样经过 ValidateAndDecode 验证，确保只产生合法拼音。
//   可通过 SetFuzzyEnabled() 开关控制。
//
// 词频排序策略：
//   HanziCandidates 中的候选词按以下优先级排序：
//   1. 常用字词优先（查频率表，频率高的词排名靠前）
//   2. 同频率时多字词优先（词长越长越优先，如 "你好吗" 优先于 "你"）
//   3. 同词长按拼音展开顺序（少音节优先，即更常见的组合）
//   4. 同拼音展开内按 libgooglepinyin 返回顺序（引擎内置词频）

#include <string>
#include <vector>

#include "ime/pinyin_ime.h"

namespace t9 {

class T9Engine {
public:
    explicit T9Engine(ime::PinyinIme* ime);

    // 按下数字键 '2'-'9'
    void PushKey(char digit);
    // 退格删除一个数字
    void PopKey();
    // 清空当前输入
    void Clear();

    const std::string& Digits() const { return digits_; }

    // 当前数字串可展开出的合法拼音串（按音节数排序，少音节优先）。
    // 结果会被缓存：当 digits_ 未变化时直接返回缓存，避免重复 DFS。
    // 支持多音节：如 "64426" -> "ni'hao"（ni + hao 两个音节拼接）
    // 模糊音开启时会额外包含模糊音变体（如 "zhi" -> "zi"）。
    std::vector<std::string> PinyinCandidates(size_t max_results = 16) const;

    // 汉字候选：对每个拼音展开调用引擎检索并合并结果
    // 排序策略：多字词优先，同词长按拼音展开顺序，同展开内按引擎词频。
    std::vector<std::string> HanziCandidates(size_t max_results = 30) const;

    // 模糊音开关
    void SetFuzzyEnabled(bool enabled) { fuzzy_enabled_ = enabled; InvalidateCache(); }
    bool FuzzyEnabled() const { return fuzzy_enabled_; }

private:
    // DFS 将 digits_[pos..] 展开为拼音，prefix 为已确定的全部字母。
    // 前缀阶段使用 ValidateSplstr 快速剪枝（1=合法前缀，2=完整拼音）。
    // 到达叶子时使用 ValidateAndDecode 做最终验证（im_search 公共 API）。
    void Expand(size_t pos, std::string& prefix,
                std::vector<std::string>& out, size_t max_results) const;

    // 对已验证的拼音串应用模糊音映射，生成模糊音变体。
    // 输入 pinyin 为带 ' 分隔符的拼音（如 "zhi'hao"），
    // 输出所有模糊音变体（如 "zi'hao"），不包含原始串。
    // 每个变体都经过 ValidateAndDecode 验证。
    void ApplyFuzzyVariants(const std::string& pinyin,
                            std::vector<std::string>& out) const;

    // 对单个音节应用模糊音替换，返回所有变体（含原始音节）。
    std::vector<std::string> FuzzySyllable(const std::string& syl) const;

    // 标记缓存失效（digits_ 变化时调用）
    void InvalidateCache();

    ime::PinyinIme* ime_;  // 不持有
    std::string digits_;

    // 模糊音开关
    bool fuzzy_enabled_ = false;

    // 拼音展开缓存：避免每次重绘都重新 DFS。
    // 当 digits_ 变化时通过 InvalidateCache() 清除。
    mutable std::string cached_digits_;
    mutable std::vector<std::string> cached_pinyin_;

    // 汉字候选缓存：避免每次 RefreshCandidates 都重新对所有拼音做 im_search。
    // im_search 是性能瓶颈，256 个拼音 × 1ms = 256ms 阻塞，会导致快捷键边沿丢失。
    // 当 digits_ 变化时通过 InvalidateCache() 清除。
    mutable std::string cached_hanzi_digits_;
    mutable std::vector<std::string> cached_hanzi_;
};

}  // namespace t9
