cmake_minimum_required(VERSION 3.16)
project(ui_preview CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()
add_executable(ui_preview ui_preview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/overlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/t9/keypad.cpp)
target_include_directories(ui_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(ui_preview PRIVATE user32 gdi32)
