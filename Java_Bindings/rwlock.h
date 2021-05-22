
#ifndef __INCLUDED_RWLOCK
#define __INCLUDED_RWLOCK
#ifdef _WIN32
// Use Windows.h if compiling for Windows
#include <Windows.h>

#define rwlock_t SRWLOCK
#define RWLOCK_INITIALIZER SRWLOCK_INIT
static inline int rwlock_init(rwlock_t* rwlock) {
    InitializeSRWLock(rwlock);
    return 0;
}
static inline int rwlock_init(rwlock_t* rwlock) { InitializeSRWLock(rwlock); return 0; }
static inline int rwlock_read_lock(rwlock_t* rwlock) { AcquireSRWLockShared(rwlock); return 0; }
static inline int rwlock_read_unlock(rwlock_t* rwlock) { ReleaseSRWLockShared(rwlock); return 0; }
static inline int rwlock_write_lock(rwlock_t* rwlock) { AcquireSRWLockExclusive(rwlock); return 0; }
static inline int rwlock_write_unlock(rwlock_t* rwlock) { ReleaseSRWLockExclusive(rwlock); return 0; }
static inline int rwlock_destroy(rwlock_t* rwlock) { return 0; }

#else
// On other platforms use <pthread.h>
#include <pthread.h>

#define rwlock_t pthread_rwlock_t
#define RWLOCK_INITIALIZER PTHREAD_RWLOCK_INITIALIZER
static inline int rwlock_init(rwlock_t* rwlock) { return pthread_rwlock_init(rwlock, NULL); }
static inline int rwlock_read_lock(rwlock_t* rwlock) { return pthread_rwlock_rdlock(rwlock); }
static inline int rwlock_read_unlock(rwlock_t* rwlock) { return pthread_rwlock_unlock(rwlock); }
static inline int rwlock_write_lock(rwlock_t* rwlock) { return pthread_rwlock_wrlock(rwlock); }
static inline int rwlock_write_unlock(rwlock_t* rwlock) { return pthread_rwlock_unlock(rwlock); }
static inline int rwlock_destroy(rwlock_t* rwlock) { return pthread_rwlock_destroy(rwlock); }
#endif
#endif  // End rwlock include guard