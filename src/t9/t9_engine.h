#pragma once
// T9 引擎：维护数字键输入缓冲，将数字串展开为合法拼音组合，
// 交由 PinyinIme 检索候选并合并结果。
//
// 拼音展开策略：
// 1. 内置标准拼音音节表，首用时通过 libgooglepinyin 验证。
// 2. 按音节对应的 T9 数字串建立 Trie，避免每次从 unordered_map
//    重新构造 substring key。
// 3. 使用 BeamSearch 对完整音节组合做 Top-K 截断，控制组合规模。
//
// 模糊音、词频和候选缓存保持现有行为。

#include <string>
#include <unordered_map>
#include <vector>

#include "ime/pinyin_ime.h"
#include "t9/t9_trie.h"

namespace t9 {

class T9Engine {
public:
    explicit T9Engine(ime::PinyinIme* ime);

    void PushKey(char digit);
    void PopKey();
    void Clear();

    const std::string& Digits() const { return digits_; }

    std::vector<std::string> PinyinCandidates(size_t max_results = 16) const;

    std::vector<std::string> HanziCandidates(size_t max_results = 30) const;

    void SetFuzzyEnabled(bool enabled) { fuzzy_enabled_ = enabled; InvalidateCache(); }
    bool FuzzyEnabled() const { return fuzzy_enabled_; }

private:
    void EnsureSyllableIndex() const;

    // 从 digits_[pos] 开始沿 Trie 走到所有 terminal 节点。
    // built 保存已经选择的音节，音节之间用 ' 分隔。
    void ExpandSyllables(size_t pos, std::string& built,
                         std::vector<std::string>& out,
                         size_t max_results) const;

    void ApplyFuzzyVariants(const std::string& pinyin,
                            std::vector<std::string>& out) const;

    std::vector<std::string> FuzzySyllable(const std::string& syl) const;

    void InvalidateCache();

    ime::PinyinIme* ime_;  // 不持有
    std::string digits_;
    bool fuzzy_enabled_ = false;

    mutable bool syllable_index_built_ = false;
    mutable T9Trie syllable_trie_;
    mutable size_t max_syllable_len_ = 0;

    mutable std::string cached_digits_;
    mutable std::vector<std::string> cached_pinyin_;
    mutable std::unordered_map<std::string, std::vector<std::string>> cached_pinyin_cands_;
    mutable std::string cached_hanzi_digits_;
    mutable std::vector<std::string> cached_hanzi_;
};

}  // namespace t9
