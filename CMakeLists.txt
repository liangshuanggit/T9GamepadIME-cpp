cmake_minimum_required(VERSION 3.16)
project(T9GamepadIME CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(T9IME_USE_LIBGOOGLEPINYIN "Build & link the libgooglepinyin decoder" ON)

set(GP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libgooglepinyin)

if(MSVC)
    add_compile_options(/utf-8)
endif()

if(T9IME_USE_LIBGOOGLEPINYIN)
    file(GLOB GP_SOURCES ${GP_ROOT}/src/*.cpp)
    if(GP_SOURCES)
        add_library(googlepinyin STATIC ${GP_SOURCES})
        target_include_directories(googlepinyin PUBLIC
            ${GP_ROOT}/include
            ${GP_ROOT})
        if(WIN32)
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

if(WIN32)
    set(APP_RC ${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc)
    set_source_files_properties(${APP_RC} PROPERTIES
        OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/app.ico)
endif()

add_executable(t9ime
    src/main.cpp
    src/t9/t9_keymap.cpp
    src/t9/t9_engine.cpp
    src/t9/t9_trie.cpp
    src/t9/beam_search.cpp
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
    target_link_libraries(t9ime PRIVATE xinput user32 imm32 gdi32 shell32 hid setupapi)
    target_link_options(t9ime PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

enable_testing()
add_executable(t9_tests
    tests/test_main.cpp
    src/t9/t9_keymap.cpp
    src/t9/t9_engine.cpp
    src/t9/t9_trie.cpp
    src/t9/beam_search.cpp
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

if(T9IME_USE_LIBGOOGLEPINYIN)
    add_executable(dictbuilder_tool tools/build_dict.cpp)
    target_link_libraries(dictbuilder_tool PRIVATE googlepinyin)

    add_executable(diag_pinyin tools/diag_pinyin.cpp
        src/t9/t9_engine.cpp
        src/t9/t9_keymap.cpp
        src/t9/t9_trie.cpp
        src/t9/beam_search.cpp
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
