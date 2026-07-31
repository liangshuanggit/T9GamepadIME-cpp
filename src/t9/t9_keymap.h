#pragma once
// T9 键位映射：数字键 2-9 对应的拼音字母集合

#include <string>
#include <vector>

namespace t9 {

// 返回某个 T9 数字键（'2'-'9'）对应的字母集合，如 '2' -> "abc"
const std::string& LettersForKey(char digit);

// 将单个小写字母映射回 T9 数字键，如 'a' -> '2'；非法输入返回 0
char KeyForLetter(char letter);

// 将一段拼音字符串转成 T9 数字串，如 "ni" -> "64"
std::string PinyinToDigits(const std::string& pinyin);

}  // namespace t9
