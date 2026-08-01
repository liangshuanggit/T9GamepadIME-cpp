cmake_minimum_required(VERSION 3.16)
project(T9GamepadIME CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 是否链接 libgooglepinyin 真实拼音内核。
# 关闭时 PinyinIme 使用内置桩实现，工程零外部依赖即可编译（便于先跑通骨架）。
option(T9IME_USE_LIBGOOGLEPINYIN "Build & link the libgooglepinyin decoder" ON)

set(GP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libgooglepinyin)

# 源码含中文注释/字符串，统一按 UTF-8 编译，消除 MSVC C4819 警告
if(MSVC)
    add_compile_options(/utf-8)
endif()

if(T9IME_USE_LIBGOOGLEPINYIN)
    # 不直接使用上游 CMakeLists（其 cmake_minimum_required(2.6) 与新版 CMake 不兼容，
    # 且链接 pthread、使用 GCC 专用 flag）。这里用可移植目标直接编译其 src/*.cpp。
    file(GLOB GP_SOURCES ${GP_ROOT}/src/*.cpp)
    if(GP_SOURCES)
        add_library(googlepinyin STATIC ${GP_SOURCES})
        target_include_directories(googlepinyin PUBLIC
            ${GP_ROOT}/include
            ${GP_ROOT})
        if(WIN32)
            # Windows/MSVC 缺少 POSIX 头（unistd/pthread/sys/time 等）与 Android cutils/log.h，
            # 由 compat 兼容层补齐。
            target_include_directories(googlepinyin PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/compat)
            target_compile_definitions(googlepinyin PRIVATE _CRT_SECURE_NO_WARNINGS)
        else()
            find_package(Threads REQUIRED)
            target_link_libraries(googlepinyin PUBLIC Threads::Threads)
        endif()
    else()
        message(WARNING "libgooglepinyin 源码目录不存在 (${GP_ROOT}/src/)，将使用桩实现。")
        set(T9IME_USE_LIBGOOGLEPINYIN OFF)
    endif()
endif()

# 应用图标资源（Windows .rc → .ico）
if(WIN32)
    set(APP_RC ${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc)
    set_source_files_properties(${APP_RC} PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/app.ico)
endif()

add_executable(t9ime
    src/main.cpp
    src/t9/t9_keymap.cpp
    src/t9/t9_engine.cpp
    src/t9/keypad.cpp
    src/ime/pinyin_ime.cpp
    src/gamepad/xinput_pad.cpp
    src/gamepad/stick.cpp
    src/app/config.cpp
    src/app/ime_controller.cpp
    src/app/log.cpp
    src/app/text_injector.cpp
    src/app/desktop_controller.cpp
    src/app/ally_hid_controller.cpp
    src/ui/overlay.cpp
    ${APP_RC}
)

target_include_directories(t9ime PRIVATE src)

if(T9IME_USE_LIBGOOGLEPINYIN)
    target_compile_definitions(t9ime PRIVATE T9IME_USE_LIBGOOGLEPINYIN=1)
    target_include_directories(t9ime PRIVATE ${GP_ROOT}/include)
    target_link_libraries(t9ime PRIVATE googlepinyin)
endif()

if(WIN32)
    # XInput: 手柄输入；user32/imm32: 预留给窗口与文本注入；
    # gdi32: Overlay 窗口 GDI 绘制
    target_link_libraries(t9ime PRIVATE xinput user32 imm32 gdi32 shell32 hid setupapi)
    # WINDOWS 子系统：启动时不显示命令行窗口。
    # /ENTRY:mainCRTStartup 保留标准 main() 入口（默认 WinMain 会链接失败）。
    target_link_options(t9ime PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

# ---- 单元测试（纯逻辑，使用桩拼音内核，无需词典） ----
# 不定义 T9IME_USE_LIBGOOGLEPINYIN，PinyinIme 走桩实现；定义 T9IME_TESTING
# 以启用 XInputPad 的测试注入接口，从而在无手柄环境下验证控制器状态机。
enable_testing()
add_executable(t9_tests
    tests/test_main.cpp
    src/t9/t9_keymap.cpp
    src/t9/t9_engine.cpp
    src/t9/keypad.cpp
    src/ime/pinyin_ime.cpp
    src/gamepad/xinput_pad.cpp
    src/gamepad/stick.cpp
    src/app/config.cpp
    src/app/ime_controller.cpp
    src/app/log.cpp
    src/app/text_injector.cpp
    src/app/desktop_controller.cpp
)
target_include_directories(t9_tests PRIVATE src)
target_compile_definitions(t9_tests PRIVATE T9IME_TESTING=1)
if(WIN32)
    target_link_libraries(t9_tests PRIVATE xinput user32 imm32)
endif()
add_test(NAME t9_tests COMMAND t9_tests)

# ---- 词典构建工具 ----
# 内核的 dictdef.h 总是定义 ___BUILD_MODEL___，故 DictTrie::build_dict / save_dict
# 已编进 googlepinyin 静态库。本工具直接链接该库，把原始词表生成
# 二进制 dict_pinyin.dat。
if(T9IME_USE_LIBGOOGLEPINYIN)
    add_executable(dictbuilder_tool tools/build_dict.cpp)
    target_link_libraries(dictbuilder_tool PRIVATE googlepinyin)

    # 诊断工具：验证拼音展开与 libgooglepinyin 接口行为
    add_executable(diag_pinyin tools/diag_pinyin.cpp
        src/t9/t9_engine.cpp
        src/t9/t9_keymap.cpp
        src/t9/keypad.cpp
        src/ime/pinyin_ime.cpp
        src/app/config.cpp
    )
    target_include_directories(diag_pinyin PRIVATE src ${GP_ROOT}/include)
    target_compile_definitions(diag_pinyin PRIVATE T9IME_USE_LIBGOOGLEPINYIN=1)
    target_link_libraries(diag_pinyin PRIVATE googlepinyin)
    if(WIN32)
        target_link_libraries(diag_pinyin PRIVATE user32)
    endif()
endif()
