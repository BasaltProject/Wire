#pragma once

#include <windows.h>

typedef struct mbedtls_threading_mutex_t {
    SRWLOCK mutex;
} mbedtls_threading_mutex_t;
