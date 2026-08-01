#include "t9/t9_keymap.h"

#include <cctype>

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

std::vector<std::string> LetterCandidatesForDigit(char digit) {
    std::vector<std::string> out;
    const std::string& letters = LettersForKey(digit);
    if (letters.empty()) return out;
    // 大写字母
    for (char c : letters) out.emplace_back(1, static_cast<char>(std::toupper(c)));
    // 数字
    out.emplace_back(1, digit);
    // 小写字母
    for (char c : letters) out.emplace_back(1, c);
    return out;
}

std::string LetterLabelForDigit(char digit) {
    const std::string& letters = LettersForKey(digit);
    if (letters.empty()) return {};
    std::string label;
    label.reserve(letters.size() * 2 + 1);
    for (char c : letters) label += static_cast<char>(std::toupper(c));
    label += digit;
    label += letters;
    return label;
}

}  // namespace t9
