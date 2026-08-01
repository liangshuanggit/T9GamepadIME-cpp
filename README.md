# T9GamepadIME-cpp

用手柄（XInput）驱动的 **T9 中文拼音输入法**，C++17 编写，拼音解码内核采用成熟开源框架
[libgooglepinyin](https://github.com/qgears/libgooglepinyin)（Google 拼音输入法开源核心，
即 Android 原生 PinyinIME 引擎）。

## 功能概览

- **扇形九宫格输入**：右摇杆 8 方向拨动输入 T9 数字，Overlay 界面实时显示当前输入、拼音与候选
- **候选上屏**：A 确认上屏（Unicode 注入前台窗口），十字键左右切换 / 上下翻页，长按进入字母候选
- **编辑快捷键**：按住 LB + 面键 → 全选 / 剪切 / 复制 / 粘贴（仅 IME 开启时生效，触发时界面高亮）
- **ROG Ally 硬件模式切换**：IME 开启 → 硬件游戏手柄模式；IME 关闭 → 硬件桌面（鼠标）模式
- **系统托盘**：静默后台运行，左键切换 IME，右键菜单含开机启动 / 退出
- **可选安装包**：Inno Setup 生成 `installer/T9GamepadIME-Setup.exe`

## 目录结构

```text
T9GamepadIME-cpp/
├── CMakeLists.txt              # 顶层工程（t9ime / t9_tests / dictbuilder_tool）
├── config.ini                  # 运行时配置（开关快捷键、摇杆阈值、分页）
├── install.iss                 # Inno Setup 安装脚本
├── ChineseSimplified.isl       # 安装向导简体中文语言文件
├── src/
│   ├── main.cpp                # 主循环：托盘图标 + Overlay + 模式切换
│   ├── app/
│   │   ├── config.h/.cpp       # 配置加载与按键组合解析
│   │   ├── ime_controller.*    # 开关状态机 + 输入翻译 + 候选选择 + 编辑快捷键
│   │   ├── desktop_controller.*# 软件桌面操控（非 ROG Ally / 硬件切换失败时）
│   │   ├── ally_hid_controller.* # ROG Ally 硬件模式切换（HID Feature Report）
│   │   ├── text_injector.*     # 文本/快捷键注入（SendInput）
│   │   └── log.*               # 诊断日志（OutputDebugString + 文件）
│   ├── ui/
│   │   └── overlay.*           # 屏幕覆盖层（扇形九宫格 + 候选 + 编辑快捷键栏）
│   ├── gamepad/
│   │   ├── xinput_pad.h/.cpp   # XInput 手柄轮询（按键边沿 + 摇杆）
│   │   └── stick.h/.cpp        # 右摇杆 8 方向识别与"拨动"边沿检测
│   ├── t9/
│   │   ├── keypad.h/.cpp       # 九宫格布局与 方向->数字键 映射
│   │   ├── t9_keymap.h/.cpp    # 数字键 2-9 <-> 拼音字母映射
│   │   └── t9_engine.h/.cpp    # 数字串 -> 合法拼音展开 -> 候选合并
│   └── ime/
│       └── pinyin_ime.h/.cpp   # libgooglepinyin 封装（含 UTF-16->UTF-8）
├── third_party/
│   ├── libgooglepinyin/        # 拼音解码内核（git 引入）
│   └── compat/                 # Windows POSIX 兼容 shim
├── tests/
│   └── test_main.cpp           # 单元测试（146 项断言）
└── data/
    └── dict_pinyin.dat         # 系统词典（来自 libgooglepinyin/data）
```

## 架构与数据流

```
右摇杆/十字键/A/B ─▶ ImeController(开关/翻译) ─▶ T9Engine ─▶ PinyinIme(libgooglepinyin) ─▶ 候选
```

- **ImeController**：维护"开启/关闭"状态；仅在开启时把手柄输入翻译为 T9 操作，并处理
  LB+面键 编辑快捷键（含按键时序回溯，保证组合键可靠触发）。
- **T9Engine**：内置完整汉语拼音音节表，用 DFS + 前缀剪枝把 T9 数字串展开为所有合法拼音，
  再逐一送入拼音内核检索并去重合并候选。
- **PinyinIme**：封装 libgooglepinyin 的 `im_open_decoder / im_search / im_get_candidate / im_choose`，
  自动把内核返回的 UTF-16 候选转换为 UTF-8。
- 未启用 `T9IME_USE_LIBGOOGLEPINYIN` 时，`PinyinIme` 退化为桩实现，方便先跑通骨架。

## 扇形键区布局与操作

键位以**扇形（径向）**方式围绕中心静止位排布：右摇杆回中于中心圆，拨向 8 个方向
即触发对应的弧形扇叶键。方向与数字键的映射与经典九宫格一致，仅视觉排布由 3×3
矩形改为环形扇形：

```text
          上(2 abc)
    左上(5 jkl)    右上(3 def)
左(4 ghi)   ·(中心静止)   右(6 mno)
    左下(7pqrs)    右下(9wxyz)
          下(8 tuv)
```

以下操作**仅在 IME 开启（界面显示）时生效**：

| 输入                | 功能                          |
|---------------------|-------------------------------|
| 右摇杆 8 方向拨动    | 触发对应九宫格键位一次（T9）  |
| 右摇杆长按（方向不回中）| 进入该键字母候选（如 '2' 显示 "ABC2abc"）|
| 十字键 ← / →        | 上一个 / 下一个候选           |
| 十字键 ↑ / ↓        | 候选上一页 / 下一页           |
| A                   | 确认上屏当前候选              |
| B                   | 退格 / 取消字母候选           |
| 开关快捷键（可配置）| 切换 开启/关闭（默认 Start）  |
| Ctrl+Alt+E          | 键盘切换热键（硬件桌面模式下备用）|
| Ctrl+Alt+Q          | 退出程序                      |

> 说明：本程序仅轮询 XInput、不拦截输入，故关闭时游戏仍正常读取手柄。

### 编辑快捷键（全选/剪切/复制/粘贴）

**仅在 IME 开启（界面显示并启用）时可用**：按住 **LB** 再按面键触发文本编辑快捷键。
IME 关闭（桌面操控模式）时不响应，LB 保持 PageUp 功能。

| 输入      | 功能   | 快捷键   |
|-----------|--------|----------|
| LB + A    | 全选   | Ctrl+A   |
| LB + X    | 剪切   | Ctrl+X   |
| LB + Y    | 复制   | Ctrl+C   |
| LB + B    | 粘贴   | Ctrl+V   |

注意：按住 LB 期间 A/B 的常规功能（上屏候选 / 退格）会被暂时抑制，松开 LB 后恢复。
Overlay 底部会常驻显示这四项快捷键，触发时对应项高亮约 0.8 秒。面键若先于 LB 被检测到
按下（同帧/扫描时序），会自动回溯一帧判定为组合，避免误触发常规功能。

### ROG Ally 硬件模式切换

- **开启 IME**：切换到硬件游戏手柄模式（XInput 输入可用）
- **关闭 IME**：切换到硬件桌面（鼠标）模式，由 Ally 固件接管鼠标/按键
- 桌面模式下 XInput 通常断开，手柄 Start 键快捷键失效，需用**托盘图标左键**或
  **键盘 Ctrl+Alt+E** 重新开启 IME
- 非 ROG Ally 设备或硬件切换失败时，回退为软件桌面操控（左摇杆移动鼠标、A/B 左右键、LB PageUp）

## 系统托盘

程序以托盘图标静默运行：

| 操作      | 功能                                  |
|-----------|---------------------------------------|
| 左键单击  | 切换 IME 开关（同手柄快捷键）          |
| 右键菜单  | 开机启动（可勾选）/ 退出               |

开机启动通过注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\T9GamepadIME` 实现。

## 配置（config.ini，位于运行目录）

| 键              | 说明                                  | 默认        |
|-----------------|---------------------------------------|-------------|
| `toggle_hotkey` | 开关组合键，用 `+` 连接               | `Start`     |
| `stick_activate`| 拨动触发幅度阈值 (0~1)                 | `0.8`       |
| `stick_release` | 回中阈值，低于此才能再次触发 (0~1)    | `0.15`      |
| `long_press_ms` | 长按触发字母候选的持续时间 (毫秒)      | `500`       |
| `candidate_page`| 候选每页数量                          | `8`         |
| `start_enabled` | 启动时是否已开启 (true/false)         | `false`     |
| `overlay_opacity`| 覆盖层整体不透明度 (0~1)              | `1.0`       |

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

# 运行（可省略词典参数，自动按 exe 目录及上级目录查找）
./build/Release/t9ime.exe
# 非交互自检（验证词典与候选）：./build/Release/t9ime.exe --selftest=ni
```

> 本程序为 GUI 程序（WINDOWS 子系统），启动时不显示命令行窗口。

若暂时不想编译内核，可先用桩模式验证骨架：

```powershell
cmake -S . -B build -DT9IME_USE_LIBGOOGLEPINYIN=OFF
cmake --build build
```

## 测试

纯逻辑单元测试（`tests/test_main.cpp`，146 项断言）覆盖右摇杆 8 方向识别与拨动边沿、
九宫格键位映射与布局、T9 拼音展开、模糊音、词频排序、配置解析（含非法值校验）、
`ImeController` 的开关/候选选择状态机、长按字母模式、LB 编辑快捷键（含按键时序回溯）、
以及 `DesktopController` 的开关/边沿状态机（借助 `T9IME_TESTING` 下暴露的手柄注入接口，
无需真实手柄）。测试走桩拼音内核，无需词典即可运行：

```powershell
cmake -S . -B build
cmake --build build --config Release --target t9_tests
ctest --test-dir build -C Release --output-on-failure
```

## 非交互自检

验证真实词典与候选展开（结果同时写入 `selftest_out.txt`，便于脚本解析）：

```powershell
cmake --build build --config Release --target t9ime
./build/Release/t9ime.exe --selftest=ni
```

## 安装包

使用 [Inno Setup 6](https://jrsoftware.org/isinfo.php) 生成安装程序：

```powershell
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" install.iss
```

产物：`installer/T9GamepadIME-Setup.exe`

- 用户级安装（`%LOCALAPPDATA%\Programs\T9GamepadIME`），无需管理员权限
- 安装向导支持简体中文 / English
- 卸载时删除整个应用目录（含运行时生成的用户词典/日志）

## 许可证

本项目采用 **Apache License 2.0**（见 [LICENSE](LICENSE)，第三方声明见 [NOTICE](NOTICE)）。

第三方依赖：

| 组件 | 许可证 | 版权 |
|------|--------|------|
| [libgooglepinyin](https://github.com/qgears/libgooglepinyin)（拼音解码内核） | Apache-2.0 | Copyright (C) 2009 The Android Open Source Project / 2011 Weng Xuetian |

libgooglepinyin 为 Android 开源项目（AOSP）拼音输入法内核的提取封装，源文件头均含 Apache-2.0 声明（见 `third_party/libgooglepinyin/`）。
