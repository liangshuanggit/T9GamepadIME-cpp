#include "t9/t9_keymap.h"

namespace t9 {

namespace {
// 索引 0 对应键 '2'
const std::string kKeyLetters[8] = {
    "abc",   // 2
    "def",   // 3
    "ghi",   // 4
    "jkl",   // 5
    "mno",   // 6
    "pqrs",  // 7
    "tuv",   // 8
    "wxyz",  // 9
};
const std::string kEmpty;
}  // namespace

const std::string& LettersForKey(char digit) {
    if (digit < '2' || digit > '9') return kEmpty;
    return kKeyLetters[digit - '2'];
}

char KeyForLetter(char letter) {
    if (letter < 'a' || letter > 'z') return 0;
    for (int i = 0; i < 8; ++i) {
        if (kKeyLetters[i].find(letter) != std::string::npos) {
            return static_cast<char>('2' + i);
        }
    }
    return 0;
}

std::string PinyinToDigits(const std::string& pinyin) {
    std::string digits;
    digits.reserve(pinyin.size());
    for (char c : pinyin) {
        char key = KeyForLetter(c);
        if (key == 0) return {};  // 含非拼音字符
        digits.push_back(key);
    }
    return digits;
}

}  // namespace t9
