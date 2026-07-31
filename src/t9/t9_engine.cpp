#include "t9/t9_engine.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "t9/freq_table.h"
#include "t9/t9_keymap.h"

namespace t9 {

// ---- 模糊音映射表 ----
// 每对表示双向替换：(a, b) 表示 a 可替换为 b，b 也可替换为 a。
// 仅在音节级别应用（完整音节匹配，非子串替换）。
namespace {

struct FuzzyPair {
    const char* from;
    const char* to;
};

// 模糊音对：声母和韵母级别的替换
// 格式：from -> to（双向）
constexpr FuzzyPair kFuzzyPairs[] = {
    // 平翘舌
    {"zh", "z"},
    {"ch", "c"},
    {"sh", "s"},
    // 鼻边音
    {"n", "l"},
    // 前后鼻音
    {"ing", "in"},
    {"eng", "en"},
    {"ang", "an"},
};

// 判断字符串 str 是否以 prefix 开头
bool StartsWith(const std::string& str, const char* prefix) {
    size_t len = 0;
    while (prefix[len]) ++len;
    if (str.size() < len) return false;
    return str.compare(0, len, prefix) == 0;
}

// 将字符串 str 从开头替换 old_prefix 为 new_prefix
std::string ReplacePrefix(const std::string& str,
                          const char* old_prefix, const char* new_prefix) {
    size_t old_len = 0;
    while (old_prefix[old_len]) ++old_len;
    if (str.size() < old_len) return str;
    if (str.compare(0, old_len, old_prefix) != 0) return str;
    return std::string(new_prefix) + str.substr(old_len);
}

// 计算汉字的 UTF-8 字符数（用于词频排序时衡量词长）
size_t Utf8CharCount(const std::string& s) {
    size_t count = 0;
    for (unsigned char c : s) {
        // UTF-8 起始字节：0xxxxxxx (ASCII) 或 11xxxxxx (多字节首字节)
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

}  // namespace

T9Engine::T9Engine(ime::PinyinIme* ime) : ime_(ime) {}

void T9Engine::InvalidateCache() {
    cached_digits_.clear();
    cached_pinyin_.clear();
    cached_hanzi_digits_.clear();
    cached_hanzi_.clear();
}

void T9Engine::PushKey(char digit) {
    if (digit >= '2' && digit <= '9') {
        digits_.push_back(digit);
        InvalidateCache();
    }
}

void T9Engine::PopKey() {
    if (!digits_.empty()) {
        digits_.pop_back();
        InvalidateCache();
    }
}

void T9Engine::Clear() {
    digits_.clear();
    InvalidateCache();
}

void T9Engine::Expand(size_t pos, std::string& prefix,
                      std::vector<std::string>& out, size_t max_results) const {
    if (out.size() >= max_results) return;

    // 到达数字串末尾：使用 libgooglepinyin 公共 API 做最终验证
    if (pos == digits_.size()) {
        std::string decoded;
        if (ime_->ValidateAndDecode(prefix, decoded)) {
            // 引擎确认所有字符被分解为完整音节
            // 使用引擎解码的规范拼音（可能与输入略有不同）
            out.push_back(decoded.empty() ? prefix : decoded);
        }
        return;
    }

    // 遍历当前数字对应的所有可能字母
    const std::string& letters = LettersForKey(digits_[pos]);
    for (char c : letters) {
        prefix.push_back(c);

        // DFS 前缀剪枝：调用 libgooglepinyin 的 SpellingParser 快速判断
        //   1=合法前缀（含部分音节，可继续展开）
        //   2=完整拼音（也可继续展开，后续字母可能开启新音节）
        int valid = ime_->ValidateSplstr(prefix);
        if (valid > 0) {
            Expand(pos + 1, prefix, out, max_results);
        }

        prefix.pop_back();

        if (out.size() >= max_results) return;
    }
}

std::vector<std::string> T9Engine::FuzzySyllable(const std::string& syl) const {
    std::vector<std::string> results;
    results.push_back(syl);  // 原始音节

    // 对每个模糊音对，检查音节是否匹配
    for (const auto& fp : kFuzzyPairs) {
        // 检查 syl 是否以 fp.from 开头（声母/韵母替换）
        if (StartsWith(syl, fp.from)) {
            std::string variant = ReplacePrefix(syl, fp.from, fp.to);
            if (variant != syl) {
                results.push_back(variant);
            }
        }
        // 反向：检查 syl 是否以 fp.to 开头
        if (StartsWith(syl, fp.to)) {
            std::string variant = ReplacePrefix(syl, fp.to, fp.from);
            if (variant != syl) {
                results.push_back(variant);
            }
        }
    }

    return results;
}

void T9Engine::ApplyFuzzyVariants(const std::string& pinyin,
                                   std::vector<std::string>& out) const {
    // 将拼音串按 ' 分割为音节列表
    std::vector<std::string> syllables;
    std::string current;
    for (char c : pinyin) {
        if (c == '\'') {
            if (!current.empty()) {
                syllables.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        syllables.push_back(current);
    }

    if (syllables.empty()) return;

    // 对每个音节生成模糊音变体，然后做笛卡尔积
    // 为避免组合爆炸，限制：每个音节最多取原始 + 1 个变体，
    // 且总变体数不超过 16。
    std::vector<std::vector<std::string>> fuzzy_syllables;
    fuzzy_syllables.reserve(syllables.size());
    for (const auto& syl : syllables) {
        fuzzy_syllables.push_back(FuzzySyllable(syl));
    }

    // 笛卡尔积生成变体拼音
    // 使用迭代法：从第一个音节开始，逐步扩展
    std::vector<std::string> combos;
    for (const auto& syl : fuzzy_syllables[0]) {
        combos.push_back(syl);
    }

    for (size_t i = 1; i < fuzzy_syllables.size() && combos.size() < 16; ++i) {
        std::vector<std::string> new_combos;
        for (const auto& base : combos) {
            for (const auto& syl : fuzzy_syllables[i]) {
                new_combos.push_back(base + "'" + syl);
                if (new_combos.size() >= 16) break;
            }
            if (new_combos.size() >= 16) break;
        }
        combos = std::move(new_combos);
    }

    // 验证每个变体并添加到输出（排除原始串）
    for (const auto& combo : combos) {
        if (combo == pinyin) continue;  // 跳过原始串
        std::string decoded;
        if (ime_->ValidateAndDecode(combo, decoded)) {
            std::string final_pinyin = decoded.empty() ? combo : decoded;
            out.push_back(final_pinyin);
        }
    }
}

std::vector<std::string> T9Engine::PinyinCandidates(size_t max_results) const {
    if (digits_.empty()) return {};

    // 缓存命中：digits_ 未变化时直接返回缓存结果（截断到 max_results）
    if (cached_digits_ == digits_ && !cached_pinyin_.empty()) {
        if (max_results >= cached_pinyin_.size()) return cached_pinyin_;
        return std::vector<std::string>(cached_pinyin_.begin(),
                                        cached_pinyin_.begin() + max_results);
    }

    // 缓存未命中：重新计算
    std::vector<std::string> out;

    // 两阶段验证：
    // 1. DFS 前缀剪枝用 ValidateSplstr（SpellingParser/SpellingTrie，快速）
    // 2. 叶子最终验证用 ValidateAndDecode（im_search 公共 API，准确）
    // 展开结果均为 libgooglepinyin 引擎确认的合法拼音，带音节分隔符。
    size_t expand_limit = 256;
    std::string prefix;
    Expand(0, prefix, out, expand_limit);

    // 模糊音变体：对每个已验证拼音生成模糊音变体
    if (fuzzy_enabled_) {
        std::vector<std::string> base_pinyins = out;  // 复制原始结果
        for (const auto& py : base_pinyins) {
            ApplyFuzzyVariants(py, out);
        }
    }

    // 去重
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());

    // 按音节数排序（少音节优先），同音节数按字典序。
    // 音节数少的组合更可能对应常用词（如 "ni'hao" 优先于 "mi'ga'mo'a"）。
    std::stable_sort(out.begin(), out.end(),
        [](const std::string& a, const std::string& b) {
            size_t sa = std::count(a.begin(), a.end(), '\'');
            size_t sb = std::count(b.begin(), b.end(), '\'');
            return sa < sb;
        });

    // 保存完整缓存（不截断），后续调用按需截断
    cached_digits_ = digits_;
    cached_pinyin_ = out;

    if (max_results < out.size()) out.resize(max_results);
    return out;
}

std::vector<std::string> T9Engine::HanziCandidates(size_t max_results) const {
    if (!ime_ || digits_.empty()) return {};

    // 缓存命中：digits_ 未变化时直接返回缓存结果（截断到 max_results）
    // 这是性能关键路径：未缓存时对 256 个拼音逐个调用 im_search 可阻塞 200+ ms，
    // 导致主循环丢帧、快捷键边沿丢失。
    if (cached_hanzi_digits_ == digits_ && !cached_hanzi_.empty()) {
        if (max_results >= cached_hanzi_.size()) return cached_hanzi_;
        return std::vector<std::string>(cached_hanzi_.begin(),
                                        cached_hanzi_.begin() + max_results);
    }

    // 限制拼音展开数量：48 个已足够覆盖常用候选，
    // 更多只会增加 im_search 调用次数而无实际收益。
    auto pys = PinyinCandidates(48);
    if (pys.empty()) return {};

    // 为每个拼音展开预先获取候选列表
    // 注意：pys 中存储的是带 ' 分隔符的拼音（如 "ni'hao'ma"），
    // im_search 将非 a-z 字符视为分隔符，可直接处理，无需去除。
    std::vector<std::vector<std::string>> all_cands;
    all_cands.reserve(pys.size());
    for (const std::string& py : pys) {
        all_cands.push_back(ime_->Search(py, max_results));
    }

    // ---- 词频排序 + 常用字词加权 ----
    // 策略：
    // 1. 常用字词优先：查频率表，频率高的词排序靠前
    // 2. 同频率（含均为 0）时多字词优先：词长越长越靠前
    // 3. 同词长按拼音展开顺序（少音节展开的候选先出现）
    // 4. 同展开内按引擎返回顺序（引擎内置词频排序）
    //
    // 实现：先收集所有候选及其元数据（频率分数、词长、展开索引、引擎内索引），
    // 然后按 (频率降序, 词长降序, 展开索引升序, 引擎内索引升序) 排序。

    struct Candidate {
        std::string text;
        int freq_score;       // 常用字词频率分数（0 表示不在表中）
        size_t word_len;      // UTF-8 字符数
        size_t expand_idx;    // 拼音展开索引
        size_t engine_idx;    // 引擎内索引
    };

    std::vector<Candidate> all_candidates;
    std::set<std::string> seen;

    for (size_t j = 0; j < all_cands.size(); ++j) {
        for (size_t i = 0; i < all_cands[j].size(); ++i) {
            const std::string& hz = all_cands[j][i];
            if (seen.insert(hz).second) {
                all_candidates.push_back({
                    hz,
                    GetFreqScore(hz),
                    Utf8CharCount(hz),
                    j,
                    i
                });
            }
        }
    }

    // 排序：频率高的优先，同频率多字词优先，同词长按展开顺序，同展开按引擎顺序
    std::stable_sort(all_candidates.begin(), all_candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            if (a.freq_score != b.freq_score)
                return a.freq_score > b.freq_score;  // 频率降序
            if (a.word_len != b.word_len)
                return a.word_len > b.word_len;  // 词长降序
            if (a.expand_idx != b.expand_idx)
                return a.expand_idx < b.expand_idx;  // 展开索引升序
            return a.engine_idx < b.engine_idx;  // 引擎内索引升序
        });

    // 提取结果
    std::vector<std::string> result;
    result.reserve(std::min(all_candidates.size(), max_results));
    for (const auto& c : all_candidates) {
        if (result.size() >= max_results) break;
        result.push_back(c.text);
    }

    // 保存完整缓存（不截断），后续调用按需截断
    cached_hanzi_digits_ = digits_;
    cached_hanzi_ = result;

    if (max_results < result.size()) result.resize(max_results);
    return result;
}

}  // namespace t9
