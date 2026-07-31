// 词典构建工具：把 libgooglepinyin 的原始词表编译为二进制 dict_pinyin.dat。
//
// 仅在定义 ___BUILD_MODEL___ 时，DictTrie::build_dict / save_dict 才会被编译进来
// （见 dicttrie.h）。本工具连同内核源码一起以该宏编译。
//
// 用法：
//   build_dict <rawdict_utf16_65105_freq.txt> <valid_utf16.txt> <out dict_pinyin.dat>

#include <cstdio>

#include "dicttrie.h"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::printf("用法: %s <rawdict.txt> <valid_utf16.txt> <out dict_pinyin.dat>\n",
                    argv[0]);
        return 2;
    }
    const char* raw = argv[1];
    const char* valid = argv[2];
    const char* out = argv[3];

    ime_pinyin::DictTrie trie;
    std::printf("正在从原始词表构建词典...\n  raw   = %s\n  valid = %s\n", raw, valid);
    if (!trie.build_dict(raw, valid)) {
        std::printf("[错误] build_dict 失败。\n");
        return 1;
    }
    if (!trie.save_dict(out)) {
        std::printf("[错误] save_dict 失败（%s）。\n", out);
        return 1;
    }
    std::printf("[完成] 已生成 %s\n", out);
    return 0;
}
