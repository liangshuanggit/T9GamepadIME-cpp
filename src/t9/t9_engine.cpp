#include "t9/t9_engine.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "t9/beam_search.h"
#include "t9/freq_table.h"
#include "t9/t9_keymap.h"

namespace t9 {

namespace {

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
constexpr size_t kPinyinSyllableCount = sizeof(kPinyinSyllables) / sizeof(kPinyinSyllables[0]);
constexpr size_t kPinyinCandidateCollect = 128;

struct FuzzyPair { const char* from; const char* to; };
constexpr FuzzyPair kFuzzyPairs[] = {
    {"zh", "z"}, {"ch", "c"}, {"sh", "s"}, {"n", "l"},
    {"ing", "in"}, {"eng", "en"}, {"ang", "an"},
};

bool StartsWith(const std::string& str, const char* prefix) {
    size_t len = 0;
    while (prefix[len]) ++len;
    return str.size() >= len && str.compare(0, len, prefix) == 0;
}

std::string ReplacePrefix(const std::string& str, const char* old_prefix, const char* new_prefix) {
    size_t old_len = 0;
    while (old_prefix[old_len]) ++old_len;
    if (str.size() < old_len || str.compare(0, old_len, old_prefix) != 0) return str;
    return std::string(new_prefix) + str.substr(old_len);
}

size_t Utf8CharCount(const std::string& s) {
    size_t count = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++count;
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

    max_syllable_len_ = 0;
    for (size_t i = 0; i < kPinyinSyllableCount; ++i) {
        const char* syl = kPinyinSyllables[i];
        std::string digits = PinyinToDigits(syl);
        if (digits.empty()) continue;

        std::string decoded;
        if (!ime_->ValidateAndDecode(syl, decoded)) continue;
        std::string canon = decoded.empty() ? syl : decoded;
        syllable_trie_.Insert(digits, canon);
        max_syllable_len_ = std::max(max_syllable_len_, digits.size());
    }
    syllable_index_built_ = true;
}

void T9Engine::ExpandSyllables(size_t pos, std::string& built,
                               std::vector<std::string>& out,
                               size_t max_results) const {
    if (out.size() >= max_results || pos >= digits_.size()) {
        if (pos == digits_.size() && !built.empty() && out.size() < max_results)
            out.push_back(built);
        return;
    }

    const T9Trie::Node* node = syllable_trie_.Root();
    std::string digits_path;
    const size_t remaining = digits_.size() - pos;
    const size_t limit = std::min(max_syllable_len_, remaining);

    for (size_t len = 1; len <= limit; ++len) {
        node = syllable_trie_.MatchPrefix(node, digits_[pos + len - 1]);
        if (!node) break;
        if (!node->terminal) continue;

        for (const std::string& syl : node->values) {
            const size_t old_size = built.size();
            if (!built.empty()) built.push_back('\'');
            built += syl;

            ExpandSyllables(pos + len, built, out, max_results);
            built.resize(old_size);
            if (out.size() >= max_results) return;
        }
    }
}

std::vector<std::string> T9Engine::FuzzySyllable(const std::string& syl) const {
    std::vector<std::string> results{ syl };
    for (const auto& fp : kFuzzyPairs) {
        if (StartsWith(syl, fp.from)) {
            std::string variant = ReplacePrefix(syl, fp.from, fp.to);
            if (variant != syl) results.push_back(variant);
        }
        if (StartsWith(syl, fp.to)) {
            std::string variant = ReplacePrefix(syl, fp.to, fp.from);
            if (variant != syl) results.push_back(variant);
        }
    }
    return results;
}

void T9Engine::ApplyFuzzyVariants(const std::string& pinyin,
                                  std::vector<std::string>& out) const {
    std::vector<std::string> syllables;
    std::string current;
    for (char c : pinyin) {
        if (c == '\'') {
            if (!current.empty()) { syllables.push_back(current); current.clear(); }
        } else current.push_back(c);
    }
    if (!current.empty()) syllables.push_back(current);
    if (syllables.empty()) return;

    std::vector<std::vector<std::string>> fuzzy_syllables;
    fuzzy_syllables.reserve(syllables.size());
    for (const auto& syl : syllables) fuzzy_syllables.push_back(FuzzySyllable(syl));

    std::vector<std::string> combos;
    for (const auto& syl : fuzzy_syllables[0]) combos.push_back(syl);
    for (size_t i = 1; i < fuzzy_syllables.size() && combos.size() < 16; ++i) {
        std::vector<std::string> next;
        for (const auto& base : combos) {
            for (const auto& syl : fuzzy_syllables[i]) {
                next.push_back(base + "'" + syl);
                if (next.size() >= 16) break;
            }
            if (next.size() >= 16) break;
        }
        combos = std::move(next);
    }

    for (const auto& combo : combos) {
        if (combo == pinyin) continue;
        std::string decoded;
        if (ime_->ValidateAndDecode(combo, decoded))
            out.push_back(decoded.empty() ? combo : decoded);
    }
}

std::vector<std::string> T9Engine::PinyinCandidates(size_t max_results) const {
    if (digits_.empty()) return {};
    if (cached_digits_ == digits_ && !cached_pinyin_.empty()) {
        if (max_results >= cached_pinyin_.size()) return cached_pinyin_;
        return {cached_pinyin_.begin(), cached_pinyin_.begin() + max_results};
    }

    EnsureSyllableIndex();

    std::vector<std::string> expanded;
    std::string built;
    // Trie 负责精确的数字前缀匹配；这里仍限制原始组合数量，避免异常输入导致
    // 后续 libgooglepinyin 查询过多。BeamSearch 再按音节数/组合复杂度做 Top-K。
    ExpandSyllables(0, built, expanded, 256);

    BeamSearch beam(64);
    for (const auto& py : expanded) {
        const size_t syllables = 1 + std::count(py.begin(), py.end(), '\'');
        // 少音节优先；同等情况下保持较短字符串优先。
        const double score = 1000.0 / static_cast<double>(syllables) -
                             0.001 * static_cast<double>(py.size());
        beam.Add(py, score);
    }
    std::vector<std::string> out = beam.Results();

    if (fuzzy_enabled_) {
        std::vector<std::string> base_pinyins = out;
        for (const auto& py : base_pinyins) ApplyFuzzyVariants(py, out);
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());

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

    std::stable_sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        const size_t sa = std::count(a.begin(), a.end(), '\'');
        const size_t sb = std::count(b.begin(), b.end(), '\'');
        return sa < sb;
    });

    cached_digits_ = digits_;
    cached_pinyin_ = out;
    if (max_results < out.size()) out.resize(max_results);
    return out;
}

std::vector<std::string> T9Engine::HanziCandidates(size_t max_results) const {
    if (!ime_ || digits_.empty()) return {};
    if (cached_hanzi_digits_ == digits_ && !cached_hanzi_.empty()) {
        if (max_results >= cached_hanzi_.size()) return cached_hanzi_;
        return {cached_hanzi_.begin(), cached_hanzi_.begin() + max_results};
    }

    auto pys = PinyinCandidates(48);
    if (pys.empty()) return {};

    std::vector<std::vector<std::string>> all_cands;
    all_cands.reserve(pys.size());
    for (const std::string& py : pys) {
        auto it = cached_pinyin_cands_.find(py);
        if (it != cached_pinyin_cands_.end() && !it->second.empty()) {
            const auto& cached = it->second;
            all_cands.push_back(max_results < cached.size()
                ? std::vector<std::string>(cached.begin(), cached.begin() + max_results)
                : cached);
        } else {
            all_cands.push_back(ime_->Search(py, max_results));
        }
    }

    struct Candidate {
        std::string text;
        int freq_score;
        size_t word_len;
        size_t expand_idx;
        size_t engine_idx;
    };

    std::vector<Candidate> all_candidates;
    std::set<std::string> seen;
    for (size_t j = 0; j < all_cands.size(); ++j) {
        for (size_t i = 0; i < all_cands[j].size(); ++i) {
            const std::string& hz = all_cands[j][i];
            if (seen.insert(hz).second)
                all_candidates.push_back({hz, GetFreqScore(hz), Utf8CharCount(hz), j, i});
        }
    }

    std::stable_sort(all_candidates.begin(), all_candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            if (a.freq_score != b.freq_score) return a.freq_score > b.freq_score;
            if (a.word_len != b.word_len) return a.word_len > b.word_len;
            if (a.expand_idx != b.expand_idx) return a.expand_idx < b.expand_idx;
            return a.engine_idx < b.engine_idx;
        });

    std::vector<std::string> result;
    result.reserve(std::min(all_candidates.size(), max_results));
    for (const auto& c : all_candidates) {
        if (result.size() >= max_results) break;
        result.push_back(c.text);
    }

    cached_hanzi_digits_ = digits_;
    cached_hanzi_ = result;
    if (max_results < result.size()) result.resize(max_results);
    return result;
}

}  // namespace t9
