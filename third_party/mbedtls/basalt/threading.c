#include "basalt_mbedtls_threads.h"

#if defined(_WIN32)
#include <mbedtls/threading.h>
#include <windows.h>

static void mutex_init(mbedtls_threading_mutex_t* mutex) {
    if (mutex != NULL) {
        InitializeSRWLock(&mutex->mutex);
    }
}

static void mutex_free(mbedtls_threading_mutex_t* mutex) { (void)mutex; }

static int mutex_lock(mbedtls_threading_mutex_t* mutex) {
    if (mutex == NULL) {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    AcquireSRWLockExclusive(&mutex->mutex);
    return 0;
}

static int mutex_unlock(mbedtls_threading_mutex_t* mutex) {
    if (mutex == NULL) {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    ReleaseSRWLockExclusive(&mutex->mutex);
    return 0;
}

static BOOL CALLBACK install(PINIT_ONCE once, PVOID parameter, PVOID* context) {
    (void)once;
    (void)parameter;
    (void)context;
    mbedtls_threading_set_alt(mutex_init, mutex_free, mutex_lock, mutex_unlock);
    return TRUE;
}

void basalt_mbedtls_threading_init(void) {
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, install, NULL, NULL);
}
#else
void basalt_mbedtls_threading_init(void) {}
#endif
