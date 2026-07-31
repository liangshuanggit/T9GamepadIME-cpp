// Windows 兼容 shim：替代 POSIX <pthread.h>（仅实现 userdict.cpp 用到的互斥量）。
// 采用 Windows SRWLOCK，可静态初始化（SRWLOCK_INIT），语义与 pthread 互斥量一致。
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // 避免拉入 winsock.h 的不完整 struct timeval
#endif
#include <windows.h>

typedef SRWLOCK pthread_mutex_t;

#ifndef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#endif

static inline int pthread_mutex_lock(pthread_mutex_t* m) {
    AcquireSRWLockExclusive(m);
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t* m) {
    ReleaseSRWLockExclusive(m);
    return 0;
}

// 成功返回 0，未获得锁返回非 0（与 pthread_mutex_trylock 语义一致）
static inline int pthread_mutex_trylock(pthread_mutex_t* m) {
    return TryAcquireSRWLockExclusive(m) ? 0 : 1;
}

static inline int pthread_mutex_init(pthread_mutex_t* m, const void* /*attr*/) {
    InitializeSRWLock(m);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t* /*m*/) {
    return 0;  // SRWLOCK 无需销毁
}
