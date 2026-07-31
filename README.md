# T9GamepadIME-cpp

用手柄（XInput）驱动的 **T9 中文拼音输入法**，C++17 编写，拼音解码内核采用成熟开源框架
[libgooglepinyin](https://github.com/qgears/libgooglepinyin)（Google 拼音输入法开源核心，
即 Android 原生 PinyinIME 引擎）。

## 目录结构

```
T9GamepadIME-cpp/
├── CMakeLists.txt              # 顶层工程
├── config.ini                 # 运行时配置（开关快捷键、摇杆阈值、分页）
├── src/
│   ├── main.cpp                # 事件循环 + 九宫格控制台渲染
│   ├── app/
│   │   ├── config.h/.cpp       # 配置加载与按键组合解析
│   │   └── ime_controller.*    # 开关状态机 + 输入翻译 + 候选选择
│   ├── gamepad/
│   │   ├── xinput_pad.h/.cpp   # XInput 手柄轮询（按键边沿 + 右摇杆）
│   │   └── stick.h/.cpp        # 右摇杆 8 方向识别与“拨动”边沿检测
│   ├── t9/
│   │   ├── keypad.h/.cpp       # 九宫格布局与 方向->数字键 映射
│   │   ├── t9_keymap.h/.cpp    # 数字键 2-9 <-> 拼音字母映射
│   │   └── t9_engine.h/.cpp    # 数字串 -> 合法拼音展开 -> 候选合并
│   └── ime/
│       └── pinyin_ime.h/.cpp   # libgooglepinyin 封装（含 UTF-16->UTF-8）
├── third_party/
│   ├── libgooglepinyin/        # 拼音解码内核（git 引入）
│   └── compat/                 # Windows POSIX 兼容 shim
└── data/
    └── dict_pinyin.dat         # 系统词典（来自 libgooglepinyin/data）
```

## 架构与数据流

```
右摇杆/十字键/A/B ─▶ ImeController(开关/翻译) ─▶ T9Engine ─▶ PinyinIme(libgooglepinyin) ─▶ 候选
```

- **ImeController**：维护“开启/关闭”状态；仅在开启时把手柄输入翻译为 T9 操作。
- **T9Engine**：内置完整汉语拼音音节表，用 DFS + 前缀剪枝把 T9 数字串展开为所有合法拼音，
  再逐一送入拼音内核检索并去重合并候选。
- **PinyinIme**：封装 libgooglepinyin 的 `im_open_decoder / im_search / im_get_candidate / im_choose`，
  自动把内核返回的 UTF-16 候选转换为 UTF-8。
- 未启用 `T9IME_USE_LIBGOOGLEPINYIN` 时，`PinyinIme` 退化为桩实现，方便先跑通骨架。

## 九宫格布局与操作

5 键移到左上（原 1 键位置），中心留空，作为右摇杆静止位：

```
5 jkl │ 2 abc │ 3 def          左上 │ 上  │ 右上
4 ghi │  ·    │ 6 mno    <==>   左   │中心 │ 右
7pqrs │ 8 tuv │ 9wxyz          左下 │ 下  │ 右下
```

所有下列操作**仅在“开启”状态生效**；关闭时手柄按键保持其原功能。

| 输入                | 功能                          |
|---------------------|-------------------------------|
| 右摇杆 8 方向拨动    | 触发对应九宫格键位一次（T9）  |
| 十字键 ← / →        | 上一个 / 下一个候选           |
| 十字键 ↑ / ↓        | 候选上一页 / 下一页           |
| A                   | 确认上屏当前候选              |
| B                   | 退格                          |
| 开关快捷键（可配置）| 切换 开启/关闭（默认 Back+Start）|
| 控制台 ESC          | 退出程序                      |

> 说明：本程序仅轮询 XInput、不拦截输入，故关闭时游戏仍正常读取手柄。若需在开启时
> 真正屏蔽输入到前台程序，需要 ViGEm 等更底层方案（后续扩展）。

## 配置（config.ini，位于运行目录）

| 键              | 说明                                  | 默认        |
|-----------------|---------------------------------------|-------------|
| `toggle_hotkey` | 开关组合键，用 `+` 连接               | `Back+Start`|
| `stick_activate`| 拨动触发幅度阈值 (0~1)                 | `0.6`       |
| `stick_release` | 回中阈值，低于此才能再次触发 (0~1)    | `0.35`      |
| `candidate_page`| 候选每页数量                          | `5`         |
| `start_enabled` | 启动时是否已开启 (true/false)         | `false`     |

可用按钮名：`A B X Y LB RB Start Back DpadUp DpadDown DpadLeft DpadRight`

## 构建（Windows / MSVC）

```powershell
cd T9GamepadIME-cpp
cmake -S . -B build
cmake --build build --config Release

# 首次需生成二进制词典 dict_pinyin.dat（详见 data/README.md）
cmake --build build --config Release --target dictbuilder_tool
./build/Release/dictbuilder_tool.exe `
    third_party/libgooglepinyin/data/rawdict_utf16_65105_freq.txt `
    third_party/libgooglepinyin/data/valid_utf16.txt `
    data/dict_pinyin.dat

./build/Release/t9ime.exe data/dict_pinyin.dat data/user_dict.dat
# 非交互自检（验证词典与候选）：./build/Release/t9ime.exe --selftest=ni
```

> 控制台中文乱码？本程序启动时已调用 `SetConsoleOutputCP(CP_UTF8)`；若仍乱码，
> 请确保控制台字体支持中文（建议 Windows Terminal）。

若暂时不想编译内核，可先用桩模式验证骨架：

```powershell
cmake -S . -B build -DT9IME_USE_LIBGOOGLEPINYIN=OFF
cmake --build build
```

## 测试

纯逻辑单元测试（`tests/test_main.cpp`）覆盖右摇杆 8 方向识别与拨动边沿、
九宫格键位映射与布局、T9 拼音展开、配置解析，以及 `ImeController` 的开关/
候选选择状态机（借助 `T9IME_TESTING` 下暴露的手柄注入接口，无需真实手柄）。
测试走桩拼音内核，无需词典即可运行：

```powershell
cmake -S . -B build
cmake --build build --config Release --target t9_tests
ctest --test-dir build -C Release --output-on-failure
```

## 待你修改 / 扩展的点

- `src/main.cpp` → `MapButtonToDigit`：自定义按键布局、加入连击选字/翻页。
- 上屏动作：当前是控制台打印，可替换为 `SendInput` / IMM 真正注入到前台窗口。
- `src/t9/t9_engine.cpp`：多音节分词、词频排序、模糊音等策略。
