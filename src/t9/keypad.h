#pragma once
// 九宫格键位布局（T9 拼音）。
//
// 与常规九宫格输入法一致，但把 5 键移到左上（原 1 键位置），中心留空：
//
//     5 jkl │ 2 abc │ 3 def          左上 │ 上  │ 右上
//     4 ghi │  ·    │ 6 mno    <==>   左   │中心 │ 右
//     7pqrs │ 8 tuv │ 9wxyz          左下 │ 下  │ 右下
//
// 右摇杆静止于中心（留空），拨向 8 个方向即触发对应键位一次。

#include "gamepad/stick.h"

namespace t9 {

// 方向 -> T9 数字键 '2'..'9'；kNone 或中心返回 0。
char DigitForDirection(gamepad::Direction d);

// 一个格子的展示信息
struct KeyCell {
    char digit;           // '2'..'9'；'\0' 表示空格子（中心）
    const char* letters;  // 该键字母，如 "abc"；空格子为 ""
};

// 3x3 布局（行优先，共 9 个格子），用于界面渲染。
const KeyCell* GridCells();

}  // namespace t9
