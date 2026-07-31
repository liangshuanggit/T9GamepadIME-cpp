// 诊断工具：使用真实 libgooglepinyin 检查拼音验证行为
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "ime/pinyin_ime.h"
#include "t9/t9_engine.h"
#include "t9/t9_keymap.h"

#if T9IME_USE_LIBGOOGLEPINYIN
#include <pinyinime.h>
#endif

int main() {
    // UTF-8 控制台
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    const char* sys_dict = "data/dict_pinyin.dat";
    const char* user_dict = "data/user_dict.dat";

    ime::PinyinIme ime;
    bool ok = ime.Open(sys_dict, user_dict);
    printf("词典加载: %s\n", ok ? "OK" : "FAILED");
    if (!ok) return 1;

    // ---- 1. 直接测试 ValidateSplstr ----
    printf("\n=== ValidateSplstr 测试 ===\n");
    const char* test_cases[] = {
        "m", "n", "o",       // 单声母/韵母
        "mg", "nh", "ni",    // 两字母
        "mga", "nha", "nih", // 三字母
        "mgam", "nihan", "nihao", // 更长
        "ga", "ha", "ma",    // 有效单音节
        "hao", "gao", "tao", // 有效单音节
        "niha", "nihao",     // 多音节
        "mggam", "mgham",    // 无效组合
    };
    for (const char* tc : test_cases) {
        int r = ime.ValidateSplstr(tc);
        const char* label = (r == 0) ? "INVALID" : (r == 1) ? "PREFIX" : "COMPLETE";
        printf("  ValidateSplstr(\"%s\") = %d (%s)\n", tc, r, label);
    }

    // ---- 2. 测试 T9 展开结果 ----
    printf("\n=== T9 拼音展开 ===\n");
    const char* digit_seqs[] = {"6442662", "64426", "826", "426", "64"};
    for (const char* ds : digit_seqs) {
        t9::T9Engine eng(&ime);
        for (const char* p = ds; *p; ++p) eng.PushKey(*p);
        auto pys = eng.PinyinCandidates(256);
        printf("\n  数字串: %s -> %zu 个拼音展开\n", ds, pys.size());
        for (size_t i = 0; i < pys.size() && i < 50; ++i) {
            printf("    [%zu] %s\n", i, pys[i].c_str());
        }
        if (pys.size() > 50) printf("    ... (共 %zu 个)\n", pys.size());

        // 检查是否有重复
        std::vector<std::string> sorted = pys;
        std::sort(sorted.begin(), sorted.end());
        for (size_t i = 1; i < sorted.size(); ++i) {
            if (sorted[i] == sorted[i-1]) {
                printf("    [DUP] \"%s\" 出现重复!\n", sorted[i].c_str());
            }
        }

        // 汉字候选
        auto cands = eng.HanziCandidates(20);
        printf("  汉字候选(%zu): ", cands.size());
        for (size_t i = 0; i < cands.size() && i < 20; ++i) {
            printf("%s ", cands[i].c_str());
        }
        printf("\n");
    }

    // ---- 3. 使用 libgooglepinyin 的 im_search 验证 ----
#if T9IME_USE_LIBGOOGLEPINYIN
    printf("\n=== im_search + im_get_spl_start_pos 验证 ===\n");
    const char* search_tests[] = {
        "nihao", "mihao", "mggam", "mgham", "tao", "hao",
        "ni", "mi", "mg", "nh"
    };
    for (const char* st : search_tests) {
        ime_pinyin::im_reset_search();
        size_t total = ime_pinyin::im_search(st, std::strlen(st));
        const unsigned short* spl_start = nullptr;
        size_t n_spl = ime_pinyin::im_get_spl_start_pos(spl_start);
        size_t decoded_len = 0;
        const char* decoded = ime_pinyin::im_get_sps_str(&decoded_len);
        printf("  im_search(\"%s\"): candidates=%zu, syllables=%zu, decoded=\"%.*s\"(%zu)",
               st, total, n_spl, (int)decoded_len, decoded ? decoded : "", decoded_len);
        if (n_spl > 0 && spl_start) {
            printf(", spl_start=[");
            for (size_t i = 0; i <= n_spl; ++i) printf("%u ", (unsigned)spl_start[i]);
            printf("]");
        }
        printf("\n");
    }
#endif

    // ---- 4. 期望的正确拼音展开 ----
    printf("\n=== 期望的正确拼音展开（参考） ===\n");
    printf("  64426 (nihao): nihao, mihao, nigan, migao, ...\n");
    printf("  826 (tao): tao, tan, sau, ...\n");
    printf("  426 (hao): hao, gao, iao(无), ...\n");

    ime.Close();
    return 0;
}
