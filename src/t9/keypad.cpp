#include "t9/keypad.h"

namespace t9 {

char DigitForDirection(gamepad::Direction d) {
    switch (d) {
        case gamepad::Direction::kUpLeft:    return '5';  // 左上（原 1 位置，放 5 键）
        case gamepad::Direction::kUp:        return '2';
        case gamepad::Direction::kUpRight:   return '3';
        case gamepad::Direction::kLeft:      return '4';
        case gamepad::Direction::kRight:     return '6';
        case gamepad::Direction::kDownLeft:  return '7';
        case gamepad::Direction::kDown:      return '8';
        case gamepad::Direction::kDownRight: return '9';
        case gamepad::Direction::kNone:      return 0;
    }
    return 0;
}

const KeyCell* GridCells() {
    // 行优先：左上、上、右上 / 左、中、右 / 左下、下、右下
    static const KeyCell kCells[9] = {
        {'5', "jkl"},  {'2', "abc"},  {'3', "def"},
        {'4', "ghi"},  {'\0', ""},    {'6', "mno"},
        {'7', "pqrs"}, {'8', "tuv"},  {'9', "wxyz"},
    };
    return kCells;
}

}  // namespace t9
