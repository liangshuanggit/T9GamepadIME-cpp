// Windows 兼容 shim：替代 POSIX <unistd.h>
// libgooglepinyin/userdict.cpp 使用 open/close/read/write/lseek/ftruncate。
// MSVC 的 <io.h> 已提供 open/close/read/write/lseek（POSIX 名，带弃用警告），
// ftruncate 用 _chsize 实现。
#pragma once

#include <io.h>
#include <process.h>
#include <direct.h>
#include <stdio.h>   // SEEK_SET / SEEK_CUR / SEEK_END

#ifndef ftruncate
#define ftruncate _chsize
#endif
