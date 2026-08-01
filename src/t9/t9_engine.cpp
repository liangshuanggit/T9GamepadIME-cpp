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

// 标准拼音音节表：提取自词典原始词表（rawdict_utf16_65105_freq.txt）
// 的全部音节（含零声母），共 416 个。构建索引时每个音节再经
// libgooglepinyin 的 ValidateAndDecode 验证，丢弃引擎不接受者。
// 与旧实现（逐字母 DFS + 引擎验证）相比，音节级配对将搜索空间从
// 指数级的字母组合缩小为“音节铺贴”，且无需对每个叶子做 im_search。
constexpr const char* kPinyinSyllables[] = {
    "a", "ai", "an", "ang", "ao", "ba", "bai", "ban", "bang", "bao",
    "bei", "ben", "beng", "bi", "bian", "biao", "bie", "bin", "bing", "bo",
    "bu", "ca", "cai", "can", "cang", "cao", "ce", "cen", "ceng", "cha",
    "chai", "chan", "chang", "chao", "che", "chen", "cheng", "chi", "chong", "chou",
    "chu", "chua", "chuai", "chuan", "chuang", "chui", "chun", "chuo", "ci", "cong",
    "cou", "cu", "cuan", "cui", "cun", "cuo", "da", "dai", "dan", "dang",
    "dao", "de", "dei", "den", "deng", "di", "dia", "dian", "diao", "die",
    "ding", "diu", "dong", "dou", "du", "duan", "dui", "dun", "duo", "e",
    "ei", "en", "eng", "er", "fa", "fan", "fang", "fei", "fen", "feng",
    "fiao", "fo", "fou", "fu", "ga", "gai", "gan", "gang", "gao", "ge",
    "gei", "gen", "geng", "gong", "gou", "gu", "gua", "guai", "guan", "guang",
    "gui", "gun", "guo", "ha", "hai", "han", "hang", "hao", "he", "hei",
    "hen", "heng", "hm", "hng", "hong", "hou", "hu", "hua", "huai", "huan",
    "huang", "hui", "hun", "huo", "ji", "jia", "jian", "jiang", "jiao", "jie",
    "jin", "jing", "jiong", "jiu", "ju", "juan", "jue", "jun", "ka", "kai",
    "kan", "kang", "kao", "ke", "kei", "ken", "keng", "kong", "kou", "ku",
    "kua", "kuai", "kuan", "kuang", "kui", "kun", "kuo", "la", "lai", "lan",
    "lang", "lao", "le", "lei", "leng", "li", "lia", "lian", "liang", "liao",
    "lie", "lin", "ling", "liu", "lo", "long", "lou", "lu", "luan", "lue",
    "lun", "luo", "lv", "m", "ma", "mai", "man", "mang", "mao", "me",
    "mei", "men", "meng", "mi", "mian", "miao", "mie", "min", "ming", "miu",
    "mo", "mou", "mu", "n", "na", "nai", "nan", "nang", "nao", "ne",
    "nei", "nen", "neng", "ng", "ni", "nian", "niang", "niao", "nie", "nin",
    "ning", "niu", "nong", "nou", "nu", "nuan", "nue", "nuo", "nv", "o",
    "ou", "pa", "pai", "pan", "pang", "pao", "pei", "pen", "peng", "pi",
    "pian", "piao", "pie", "pin", "ping", "po", "pou", "pu", "qi", "qia",
    "qian", "qiang", "qiao", "qie", "qin", "qing", "qiong", "qiu", "qu", "quan",
    "que", "qun", "ran", "rang", "rao", "re", "ren", "reng", "ri", "rong",
    "rou", "ru", "ruan", "rui", "run", "ruo", "sa", "sai", "san", "sang",
    "sao", "se", "sen", "seng", "sha", "shai", "shan", "shang", "shao", "she",
    "shei", "shen", "sheng", "shi", "shou", "shu", "shua", "shuai", "shuan", "shuang",
    "shui", "shun", "shuo", "si", "song", "sou", "su", "suan", "sui", "sun",
    "suo", "ta", "tai", "tan", "tang", "tao", "te", "tei", "teng", "ti",
    "tian", "tiao", "tie", "ting", "tong", "tou", "tu", "tuan", "tui", "tun",
    "tuo", "wa", "wai", "wan", "wang", "wei", "wen", "weng", "wo", "wu",
    "xi", "xia", "xian", "xiang", "xiao", "xie", "xin", "xing", "xiong", "xiu",
    "xu", "xuan", "xue", "xun", "ya", "yan", "yang", "yao", "ye", "yi",
    "yin", "ying", "yo", "yong", "you", "yu", "yuan", "yue", "yun", "za",
    "zai", "zan", "zang", "zao", "ze", "zei", "zen", "zeng", "zha", "zhai",
    "zhan", "zhang", "zhao", "zhe", "zhei", "zhen", "zheng", "zhi", "zhong", "zhou",
    "zhu", "zhua", "zhuai", "zhuan", "zhuang", "zhui", "zhun", "zhuo", "zi", "zong",
    "zou", "zu", "zuan", "zui", "zun", "zuo",
};
constexpr size_t kPinyinSyllableCount =
    sizeof(kPinyinSyllables) / sizeof(kPinyinSyllables[0]);

// PinyinCandidates 中为每个通过校验的拼音一次性收集的候选上限。
// HanziCandidates 的最大请求量（kMaxCandidates=100）不应超过此值，
// 否则缓存不足时需要回退到再次 Search。
constexpr size_t kPinyinCandidateCollect = 128;

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
    cached_pinyin_cands_.clear();
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

void T9Engine::EnsureSyllableIndex() const {
    if (syllable_index_built_) return;

    syllable_index_.clear();
    max_syllable_len_ = 0;

    for (size_t i = 0; i < kPinyinSyllableCount; ++i) {
        const char* syl = kPinyinSyllables[i];
        std::string digits = PinyinToDigits(syl);
        if (digits.empty()) continue;

        // 用引擎验证并获取规范拼音；不接受的音节直接丢弃，
        // 保证索引与引擎的合法性判定完全一致。
        std::string decoded;
        if (!ime_->ValidateAndDecode(syl, decoded)) continue;
        std::string canon = decoded.empty() ? syl : decoded;

        // 注意：同一数字串可对应多个音节（如 "64" -> mi/ni）。
        syllable_index_[digits].push_back(canon);
        if (digits.size() > max_syllable_len_) max_syllable_len_ = digits.size();
    }

    syllable_index_built_ = true;
}

void T9Engine::ExpandSyllables(size_t pos, std::string& built,
                               std::vector<std::string>& out,
                               size_t max_results) const {
    if (out.size() >= max_results) return;

    // 数字串已全部铺满：built 即为带 ' 分隔符的拼音组合。
    // 音节在索引构建时已逐个经引擎验证，直接使用，不再对整串重复解码
    // （否则会把已含分隔符的串再解一次，产生 "mi''gan''ma" 这类双重分隔符）。
    if (pos == digits_.size()) {
        out.push_back(built);
        return;
    }

    // 尝试以 1..max_syllable_len 位的数字串匹配一个完整音节
    size_t max_len = max_syllable_len_;
    if (pos + max_len > digits_.size()) max_len = digits_.size() - pos;

    for (size_t len = 1; len <= max_len; ++len) {
        auto it = syllable_index_.find(digits_.substr(pos, len));
        if (it == syllable_index_.end()) continue;

        for (const std::string& syl : it->second) {
            bool was_empty = built.empty();
            if (!was_empty) built.push_back('\'');
            built += syl;

            ExpandSyllables(pos + len, built, out, max_results);

            // 还原 built
            if (was_empty) {
                built.clear();
            } else {
                built.resize(built.size() - syl.size() - 1);
            }

            if (out.size() >= max_results) return;
        }
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

    // 懒构建音节索引（首用才验证/建立，缓存于本实例）
    EnsureSyllableIndex();

    // 音节级 DFS：将数字串铺满为完整音节序列。
    // 相比逐字母展开（3^n 组合），搜索空间缩减为音节铺贴数量，
    // 且仅在每个完整组合（而非每个字母叶子）做一次 im_search 验证。
    size_t expand_limit = 256;
    std::string built;
    ExpandSyllables(0, built, out, expand_limit);

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

    // 严格管控：只保留能在词典中组成词语/成语/话语的拼音组合。
    // 判定标准是 libgooglepinyin（成熟开源拼音引擎）整句候选的"词条组成结构"：
    // 真实短语由多字词条组成（词条数 < 音节数），如 ni'hao -> 你好（1 词条/2 音节）；
    // 而任意合法音节都能被引擎拼成"逐音节单字"（如 mi'ga'o'o'a -> 米噶哦哦啊），
    // 词条数 == 音节数，属于无意义拼接，一律丢弃。
    // 保证上屏候选与拼音提示都是真实可用的词语/成语/句子。
    // 这里用 SearchAndCheck 一次性完成"短语判定 + 候选收集"，
    // 候选缓存到 cached_pinyin_cands_ 供 HanziCandidates 复用，避免二次搜索。
    {
        std::vector<std::string> kept;
        kept.reserve(out.size());
        cached_pinyin_cands_.clear();
        for (const std::string& py : out) {
            ime::PinyinIme::SearchResult r = ime_->SearchAndCheck(py, kPinyinCandidateCollect);
            if (!r.is_phrase) continue;
            cached_pinyin_cands_[py] = std::move(r.candidates);
            kept.push_back(py);
        }
        out.swap(kept);
    }

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

    // 为每个拼音展开预先获取候选列表。
    // 优先复用 PinyinCandidates 阶段缓存的候选（SearchAndCheck 已收集），
    // 仅在缓存缺失（如请求量超过缓存上限）时回退到再次 Search。
    // 注意：pys 中存储的是带 ' 分隔符的拼音（如 "ni'hao'ma"），
    // im_search 将非 a-z 字符视为分隔符，可直接处理，无需去除。
    std::vector<std::vector<std::string>> all_cands;
    all_cands.reserve(pys.size());
    for (const std::string& py : pys) {
        auto it = cached_pinyin_cands_.find(py);
        if (it != cached_pinyin_cands_.end() && !it->second.empty()) {
            const auto& cached = it->second;
            all_cands.push_back(
                max_results < cached.size()
                    ? std::vector<std::string>(cached.begin(), cached.begin() + max_results)
                    : cached);
        } else {
            all_cands.push_back(ime_->Search(py, max_results));
        }
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
