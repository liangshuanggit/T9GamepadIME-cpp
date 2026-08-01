#include "ime/pinyin_ime.h"

#if T9IME_USE_LIBGOOGLEPINYIN
#include <pinyinime.h>  // libgooglepinyin 公共 API（ime_pinyin 命名空间）
#endif

namespace ime {

namespace {

#if T9IME_USE_LIBGOOGLEPINYIN
// libgooglepinyin 返回 UTF-16 候选，转成 UTF-8
std::string Utf16ToUtf8(const ime_pinyin::char16* s, size_t len) {
    std::string out;
    for (size_t i = 0; i < len && s[i] != 0; ++i) {
        unsigned int cp = s[i];
        // 处理代理对
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < len) {
            unsigned int low = s[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::vector<std::string> CollectCandidates(size_t total, size_t max_candidates) {
    std::vector<std::string> result;
    ime_pinyin::char16 buf[64];
    size_t count = total < max_candidates ? total : max_candidates;
    for (size_t i = 0; i < count; ++i) {
        if (ime_pinyin::im_get_candidate(i, buf, 63) != nullptr) {
            result.push_back(Utf16ToUtf8(buf, 63));
        }
    }
    return result;
}
#endif

}  // namespace

PinyinIme::~PinyinIme() { Close(); }

bool PinyinIme::Open(const std::string& sys_dict_path, const std::string& user_dict_path) {
#if T9IME_USE_LIBGOOGLEPINYIN
    opened_ = ime_pinyin::im_open_decoder(sys_dict_path.c_str(), user_dict_path.c_str());
    if (opened_) {
        // 禁用首字母模式（ShouZiMu），使 SpellingParser 只接受完整拼音音节
        // （如 ni、hao、tao），而非单声母（如 n、h、g）。
        // 否则 "mggam" 等无效组合会被误认为合法拼音。
        ime_pinyin::im_enable_shm_as_szm(false);
        ime_pinyin::im_enable_ym_as_szm(false);
    }
#else
    (void)sys_dict_path;
    (void)user_dict_path;
    opened_ = true;  // 桩实现：总是成功
#endif
    return opened_;
}

void PinyinIme::Close() {
    if (!opened_) return;
#if T9IME_USE_LIBGOOGLEPINYIN
    ime_pinyin::im_close_decoder();
#endif
    opened_ = false;
}

std::vector<std::string> PinyinIme::Search(const std::string& spelling, size_t max_candidates) {
    if (!opened_ || spelling.empty()) return {};
#if T9IME_USE_LIBGOOGLEPINYIN
    // im_search 是增量搜索：基于前一次结果继续。搜索不同拼音串前必须重置，
    // 否则引擎会尝试在前一次搜索结果上做增量，导致候选缺失或错误。
    ime_pinyin::im_reset_search();
    size_t total = ime_pinyin::im_search(spelling.c_str(), spelling.size());
    return CollectCandidates(total, max_candidates);
#else
    (void)max_candidates;
    return {"[stub:" + spelling + "]"};  // 桩实现：回显拼音
#endif
}

bool PinyinIme::HasPhrase(const std::string& spelling) {
    return SearchAndCheck(spelling, 1).is_phrase;
}

PinyinIme::SearchResult PinyinIme::SearchAndCheck(const std::string& spelling,
                                                  size_t max_candidates) {
    SearchResult result;
    if (!opened_ || spelling.empty()) return result;
#if T9IME_USE_LIBGOOGLEPINYIN
    // im_search 是增量搜索：基于前一次结果继续。搜索不同拼音串前必须重置，
    // 否则引擎会尝试在前一次搜索结果上做增量，导致候选缺失或错误。
    ime_pinyin::im_reset_search();
    size_t total = ime_pinyin::im_search(spelling.c_str(), spelling.size());
    if (total == 0) return result;

    // 音节数 = 分隔符数量 + 1
    size_t n_syllables = 1;
    for (char c : spelling) {
        if (c == '\'') ++n_syllables;
    }

    // 单音节（单个汉字）恒为真实汉字，直接接受。
    if (n_syllables == 1) {
        result.is_phrase = true;
        result.candidates = CollectCandidates(total, max_candidates);
        return result;
    }

    // 统计整句候选的"词条组成结构"：总词条数与多字（>=2 字）词条数。
    // 每个汉字对应一个音节，所以"跨 >=2 个音节"的词条即为多字词条。
    size_t n_lemma = 0, n_multi = 0;
    ime_pinyin::im_get_sentence_lemma_stats(&n_lemma, &n_multi);
    if (n_lemma == 0) return result;

    // 多字词条数 >= 单字词条数（即 2*多字 >= 总词条数），说明以真实词语
    // 为主；否则单字兜底占主导，属于无意义拼接（如 米+噶+哦哦+啊）。
    result.is_phrase = (n_multi * 2 >= n_lemma);
    if (result.is_phrase) {
        result.candidates = CollectCandidates(total, max_candidates);
    }
    return result;
#else
    (void)max_candidates;
    result.is_phrase = true;
    result.candidates = {"[stub:" + spelling + "]"};  // 桩实现：回显拼音
    return result;
#endif
}

std::vector<std::string> PinyinIme::Choose(size_t index, size_t max_candidates) {
    if (!opened_) return {};
#if T9IME_USE_LIBGOOGLEPINYIN
    size_t total = ime_pinyin::im_choose(index);
    return CollectCandidates(total, max_candidates);
#else
    (void)index;
    (void)max_candidates;
    return {};
#endif
}

void PinyinIme::ResetSearch() {
    if (!opened_) return;
#if T9IME_USE_LIBGOOGLEPINYIN
    ime_pinyin::im_reset_search();
#endif
}

int PinyinIme::ValidateSplstr(const std::string& spelling) {
    if (!opened_ || spelling.empty()) return 0;
#if T9IME_USE_LIBGOOGLEPINYIN
    return ime_pinyin::im_validate_splstr(spelling.c_str(), spelling.size());
#else
    return 2;  // 桩实现：总是返回完整拼音
#endif
}

bool PinyinIme::ValidateAndDecode(const std::string& spelling, std::string& decoded) {
    decoded.clear();
    if (!opened_ || spelling.empty()) return false;
#if T9IME_USE_LIBGOOGLEPINYIN
    // 使用 libgooglepinyin 公共 API 验证：
    // 1. im_search 将拼音串送入引擎，引擎内部做音节分解
    // 2. im_get_spl_start_pos 返回音节起始位置数组
    //    若 spl_start[n_syllables] == spelling.size()，说明全部字符被分解为完整音节
    // 3. 用音节边界插入 ' 分隔符（如 "nihaoma" -> "ni'hao'ma"），
    //    使拼音组成清晰可读
    ime_pinyin::im_reset_search();
    ime_pinyin::im_search(spelling.c_str(), spelling.size());

    const unsigned short* spl_start = nullptr;
    size_t n_spl = ime_pinyin::im_get_spl_start_pos(spl_start);

    bool valid = false;
    if (n_spl > 0 && spl_start) {
        if (spl_start[n_spl] == spelling.size()) {
            valid = true;
        }
    }

    if (valid) {
        // 用音节边界构建带分隔符的拼音串
        // spl_start[0]=0, spl_start[1]=第2音节起始, ..., spl_start[n_spl]=末尾
        for (size_t i = 0; i < n_spl; ++i) {
            if (i > 0) decoded.push_back('\'');
            size_t start = spl_start[i];
            size_t end = (i + 1 <= n_spl) ? spl_start[i + 1] : spelling.size();
            decoded.append(spelling, start, end - start);
        }
    }

    // 重置搜索状态，避免影响后续 HanziCandidates 中的 im_search
    ime_pinyin::im_reset_search();
    return valid;
#else
    decoded = spelling;
    return true;  // 桩实现：总是有效
#endif
}

}  // namespace ime
