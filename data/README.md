# data 目录说明

libgooglepinyin 运行需要一个二进制系统词典 `dict_pinyin.dat`，它由框架自带的
`dictbuilder` 工具从原始词表生成（原始词表位于
`third_party/libgooglepinyin/data/`）：

- `rawdict_utf16_65105_freq.txt` —— 词条与词频原始表
- `valid_utf16.txt`             —— 合法拼音音节表
- `new_words.txt`              —— 追加词表（网络梗 / 新词），UTF-16LE，格式同原始表

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

### 追加新词

1. 将新词按 `汉字 频率 标志 拼音...` 的格式（`ü` 写作 `v`，多字词词频 ≥ 60）
   追加到 `data/new_words.txt`。
2. 把 `new_words.txt` 与原始词表合并（尾行须换行），生成临时合并表：
   ```powershell
   Get-Content third_party/libgooglepinyin/data/rawdict_utf16_65105_freq.txt, data/new_words.txt |
       Set-Content -Encoding Unicode data/rawdict_merged_utf16.txt
   ```
3. 用合并表重建 `dict_pinyin.dat`：
   ```powershell
   ./build/Release/dictbuilder_tool.exe `
       data/rawdict_merged_utf16.txt `
       third_party/libgooglepinyin/data/valid_utf16.txt `
       data/dict_pinyin.dat
   ```

生成后的 `dict_pinyin.dat` 约 1 MB（含 65155 条词）。可用自检模式验证：

```powershell
cmake --build build --config Release --target t9ime
./build/Release/t9ime.exe --selftest=ni   # 应输出非空汉字候选
```

## 运行时文件

- `dict_pinyin.dat`  —— 系统词典（只读，需自行生成/放置，已纳入版本库）
- `user_dict.dat`    —— 用户词典（程序运行时自动创建，记录学习到的词）
- `rawdict_merged_utf16.txt` —— 合并中间产物（原始词表 + new_words.txt），
  仅重建 `dict_pinyin.dat` 时生成，已加入 `.gitignore` 不入库

> 未放置 `dict_pinyin.dat` 时，程序仍可启动：`PinyinIme::Open` 会返回失败并进入
> 无候选状态；若用 `-DT9IME_USE_LIBGOOGLEPINYIN=OFF` 构建，则走桩实现回显拼音。
