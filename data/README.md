# data 目录说明

libgooglepinyin 运行需要一个二进制系统词典 `dict_pinyin.dat`，它由框架自带的
`dictbuilder` 工具从原始词表生成（原始词表位于
`third_party/libgooglepinyin/data/`）：

- `rawdict_utf16_65105_freq.txt` —— 词条与词频原始表
- `valid_utf16.txt`             —— 合法拼音音节表

## 生成 dict_pinyin.dat

本工程已内置一个 `dictbuilder_tool` 目标（源码 `tools/build_dict.cpp`）。
内核的 `dictdef.h` 总是定义 `___BUILD_MODEL___`，故 `DictTrie::build_dict/save_dict`
已编进 `googlepinyin` 静态库，工具直接链接该库即可。

```powershell
cmake -S . -B build
cmake --build build --config Release --target dictbuilder_tool
./build/Release/dictbuilder_tool.exe `
    third_party/libgooglepinyin/data/rawdict_utf16_65105_freq.txt `
    third_party/libgooglepinyin/data/valid_utf16.txt `
    data/dict_pinyin.dat
```

生成后的 `dict_pinyin.dat` 约 1 MB（含 65101 条词）。可用自检模式验证：

```powershell
cmake --build build --config Release --target t9ime
./build/Release/t9ime.exe --selftest=ni   # 应输出非空汉字候选
```

## 运行时文件

- `dict_pinyin.dat`  —— 系统词典（只读，需自行生成/放置，已被 .gitignore 忽略）
- `user_dict.dat`    —— 用户词典（程序运行时自动创建，记录学习到的词）

> 未放置 `dict_pinyin.dat` 时，程序仍可启动：`PinyinIme::Open` 会返回失败并进入
> 无候选状态；若用 `-DT9IME_USE_LIBGOOGLEPINYIN=OFF` 构建，则走桩实现回显拼音。
